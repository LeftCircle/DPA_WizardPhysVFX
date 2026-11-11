"""Test the shelf/node wiring with a small HOM stand-in."""
import sys
import os
import subprocess
import io
from contextlib import redirect_stderr
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch
import xml.etree.ElementTree as ET

from physvfx_houdini.nodes import cook, create_node, read_values, refresh_selected_nodes, save_shadow_cache, render_and_view, activate_sphere_radius_override, _resolve_renderer_python, _monitor_render_worker, _renderer_environment, _cancel_previous_render, _render_jobs
from test_authoring_fake import Stage, fake_pxr
from physvfx_houdini.schema import defaults


class Template:
    def __init__(self, name, label, *args, **kwargs):
        self._name, self._label, self.args, self.kwargs = name, label, args, kwargs
    def label(self): return self._label
    def menuItems(self): return self.kwargs.get('menu_items', ())
    def setMenuItems(self, items): self.kwargs['menu_items'] = items
    def setMenuLabels(self, labels): self.kwargs['menu_labels'] = labels
    def scriptCallback(self): return self.kwargs.get('script_callback', '')
    def setScriptCallback(self, callback): self.kwargs['script_callback'] = callback
    def setScriptCallbackLanguage(self, language): self.kwargs['script_callback_language'] = language
    def isHidden(self): return self.kwargs.get('hidden', False)
    def hide(self, hidden): self.kwargs['hidden'] = hidden


class Folder(Template):
    pass


class TemplateGroup:
    def __init__(self): self.folders = []
    def append(self, folder): self.folders.append(folder)
    def find(self, name):
        return next((item for folder in self.folders for item in (folder, *folder.kwargs["parm_templates"]) if item._name == name), None)
    def replace(self, name, template):
        for folder in self.folders:
            folder.kwargs['parm_templates'] = tuple(template if item._name == name else item for item in folder.kwargs['parm_templates'])


class Parm:
    def __init__(self, value, label=''): self.value, self._label = value, label
    def eval(self): return self.value
    def evalAsString(self): return str(self.value)
    def set(self, value): self.value = value
    def parmTemplate(self): return Template('', self._label)


class ParmTuple:
    def __init__(self, value): self.value = value
    def eval(self): return self.value


class Node:
    def __init__(self, network, name):
        self.network, self.name = network, name
        self.groups, self.values = TemplateGroup(), {'python': Parm('', 'Python Code')}
        self.tuples = {}
        self.inputs = {}
        self.user_data = {}
        self.selected = self.display = self.destroyed = False
    def parent(self): return self.network
    def path(self): return '/stage/' + self.name
    def parmTemplateGroup(self): return self.groups
    def setParmTemplateGroup(self, group):
        self.groups = group
        for folder in group.folders:
            for spec in folder.kwargs['parm_templates']:
                if spec.kwargs.get('look') == 'color' and spec.kwargs.get('naming_scheme') != 'rgba':
                    raise RuntimeError('ColorSquares must use an RGBA naming scheme')
                default = spec.kwargs.get('default_value')
                if isinstance(default, tuple) and len(default) > 1:
                    self.tuples.setdefault(spec._name, ParmTuple(default))
                else:
                    if isinstance(default, tuple): default = default[0]
                    self.values.setdefault(spec._name, Parm(default))
    def parm(self, name): return self.values.get(name)
    def parmTuple(self, name): return self.tuples.get(name)
    def parms(self): return list(self.values.values())
    def setInput(self, index, node): self.inputs[index] = node
    def setUserData(self, name, value): self.user_data[name] = value
    def userData(self, name): return self.user_data.get(name)
    def moveToGoodPosition(self): pass
    def setSelected(self, value, clear_all_selected=False): self.selected = value
    def setDisplayFlag(self, value): self.display = value
    def destroy(self): self.destroyed = True


class Network:
    def __init__(self, category): self.category, self.nodes = category, []
    def childTypeCategory(self): return self.category
    def createNode(self, type_name, name):
        assert type_name == 'pythonscript'
        node = Node(self, name); self.nodes.append(node)
        return node


def make_hou():
    hou = types.ModuleType('hou')
    hou.lopNodeTypeCategory = lambda: 'lop'
    hou.selectedNodes = lambda: []
    hou.OperationFailed = type('OperationFailed', (Exception,), {})
    hou.NodeError = type('NodeError', (Exception,), {})
    hou.scriptLanguage = types.SimpleNamespace(Python='python')
    hou.parmLook = types.SimpleNamespace(ColorSquare='color')
    hou.parmNamingScheme = types.SimpleNamespace(RGBA='rgba')
    for name in ('StringParmTemplate', 'IntParmTemplate', 'ToggleParmTemplate', 'FloatParmTemplate', 'ButtonParmTemplate'):
        setattr(hou, name, Template)
    hou.FolderParmTemplate = Folder
    return hou


class NodeConstructionTests(unittest.TestCase):
    def test_editing_sphere_radius_enables_override(self):
        hou = make_hou(); network = Network('lop')
        with patch.dict(sys.modules, {'hou': hou}):
            node = create_node('field', network=network)
            activate_sphere_radius_override(node)
            self.assertTrue(node.parm('pv_override_radius').eval())
            node.parm('pv_override_radius').set(False)
            node.parm('pv_kind').set('isf_box')
            activate_sphere_radius_override(node)
            self.assertFalse(node.parm('pv_override_radius').eval())
            radius = node.groups.find('pv_radius')
            self.assertIn('activate_sphere_radius_override', radius.scriptCallback())
            radius.setScriptCallback('')
            hou.selectedNodes = lambda: [node]
            refresh_selected_nodes()
            self.assertIn('activate_sphere_radius_override', node.groups.find('pv_radius').scriptCallback())

    def test_each_shelf_tool_creates_parameterized_python_lop(self):
        hou = make_hou(); network = Network('lop')
        with patch.dict(sys.modules, {'hou': hou}):
            for kind in ('render_view', 'shadow_cache', 'shadow_region', 'field', 'operation', 'camera', 'light', 'settings'):
                with self.subTest(kind=kind):
                    node = create_node(kind, network=network)
                    self.assertEqual(read_values(node, kind), defaults(kind))
                    self.assertIn("cook(hou.pwd(), '%s')" % kind, node.parm('python').value)
                    self.assertEqual(node.user_data['physvfx_node_kind'], kind)
                    self.assertTrue(node.display)
                    self.assertTrue(node.selected)

    def test_source_is_connected_and_wrong_context_is_rejected(self):
        hou = make_hou(); network = Network('lop')
        source = Node(network, 'source')
        with patch.dict(sys.modules, {'hou': hou}):
            node = create_node('field', network=network, source=source)
            self.assertIs(node.inputs[0], source)
            with self.assertRaises(hou.OperationFailed):
                create_node('field', network=Network('sop'))
            with self.assertRaises(hou.OperationFailed):
                create_node('field', network=network, source=Node(Network('lop'), 'other'))

    def test_spare_parameters_are_authored_during_cook(self):
        hou = make_hou(); network = Network('lop'); stage = Stage()
        with patch.dict(sys.modules, {'hou': hou, 'pxr': fake_pxr()}):
            node = create_node('field', network=network)
            node.values['pv_kind'].set('wisp')
            node.values['pv_path'].set('/World/Wisp')
            node.values['pv_wispDots'].set(25)
            node.values['pv_cache_name'].set('wisp_25')
            node.editableStage = lambda: stage
            cook(node, 'field')
            prim = stage.GetPrimAtPath('/World/Wisp')
            self.assertEqual(prim.attrs['pbvfx:field_type'].value, 'wisp')
            self.assertEqual(prim.attrs['pbvfx:wispDots'].value, 25)
            self.assertEqual(prim.attrs['pbvfx:cacheName'].value, 'wisp_25')
            node.values['pv_cache_name'].set('../bad')
            with self.assertRaises(hou.NodeError):
                cook(node, 'field')

    def test_existing_nodes_use_defaults_for_new_preview_parameters(self):
        hou = make_hou(); network = Network('lop')
        with patch.dict(sys.modules, {'hou': hou}):
            node = create_node('field', network=network)
            del node.values['pv_show_preview']
            del node.values['pv_preview_wisp_radius']
            values = read_values(node, 'field')
            self.assertTrue(values['show_preview'])
            self.assertEqual(values['preview_wisp_radius'], 0.0)

    def test_render_view_exports_stage_and_launches_worker(self):
        hou = make_hou(); network = Network('lop')
        hou.isUIAvailable = lambda: False
        class FlatLayer:
            def Export(self, path):
                Path(path).write_text('#usda 1.0')
                return True
        stage = types.SimpleNamespace(Flatten=lambda: FlatLayer())
        with tempfile.TemporaryDirectory() as tmp, patch.dict(sys.modules, {'hou': hou}), \
                patch.dict(os.environ, {'PYTHONHOME': '/invalid/houdini/python'}):
            folder = Path(tmp) / 'project with spaces'; folder.mkdir()
            script = folder / 'python' / 'usd_sphere_scene.py'
            script.parent.mkdir(); script.write_text('')
            viewer = folder / 'dpa_imgviewer'; viewer.write_text(''); viewer.chmod(0o755)
            node = create_node('render_view', network=network)
            node.values['pv_usd_path'].set(str(folder / 'output' / 'scene.usda'))
            node.values['pv_image_path'].set(str(folder / 'output' / 'image.exr'))
            node.values['pv_render_script'].set(str(script))
            node.values['pv_viewer_path'].set(str(viewer))
            node.stage = lambda: stage
            with patch('subprocess.Popen') as popen, patch('physvfx_houdini.nodes._monitor_render_worker') as monitor:
                popen.return_value.poll.return_value = 0
                render_and_view(node)
                args, kwargs = popen.call_args
                monitor.assert_called_once_with(popen.return_value)
                self.assertEqual((folder / 'output' / 'scene.usda').read_text(), '#usda 1.0')
                self.assertIn(str(viewer), args[0])
                self.assertIn(str(folder / 'output' / 'image.exr'), args[0])
                self.assertIn(str(script), args[0])
                self.assertEqual(kwargs['cwd'], str(folder))
                self.assertEqual(kwargs['start_new_session'], os.name == 'posix')
                self.assertNotIn('PYTHONHOME', kwargs['env'])
                self.assertEqual(os.environ['PYTHONHOME'], '/invalid/houdini/python')
                self.assertNotIn('--progressive', args[0])
                node.values['pv_progressive_preview'].set(True)
                render_and_view(node)
                self.assertIn('--progressive', popen.call_args.args[0])

    def test_rejects_extension_module_as_renderer_python_before_export(self):
        hou = make_hou(); network = Network('lop')
        with tempfile.TemporaryDirectory() as tmp, patch.dict(sys.modules, {'hou': hou}):
            folder = Path(tmp); script = folder / 'render.py'; script.write_text('')
            viewer = folder / 'viewer'; viewer.write_text(''); viewer.chmod(0o755)
            extension = folder / 'physics_vfx.cpython-312-x86_64-linux-gnu.so'
            extension.write_text('fake'); extension.chmod(0o755)
            usd = folder / 'scene.usda'; usd.write_text('old stage')
            node = create_node('render_view', network=network)
            node.values['pv_usd_path'].set(str(usd))
            node.values['pv_image_path'].set(str(folder / 'render.exr'))
            node.values['pv_render_script'].set(str(script))
            node.values['pv_viewer_path'].set(str(viewer))
            node.values['pv_render_python'].set(str(extension))
            with patch('subprocess.Popen') as popen:
                with self.assertRaisesRegex(hou.OperationFailed, 'interpreter executable'):
                    render_and_view(node)
                popen.assert_not_called()
            self.assertEqual(usd.read_text(), 'old stage')

    def test_shadow_cache_rejects_extension_as_renderer_python(self):
        hou = make_hou(); network = Network('lop')
        with patch.dict(sys.modules, {'hou': hou}):
            node = create_node('shadow_cache', network=network)
            node.values['pv_cache_python'].set('/tmp/physics_vfx.cpython-312-x86_64-linux-gnu.so')
            with patch('subprocess.run') as run:
                with self.assertRaisesRegex(hou.OperationFailed, 'interpreter executable'):
                    save_shadow_cache(node)
                run.assert_not_called()

    def test_renderer_python_resolution_and_worker_crash_reporting(self):
        hou = make_hou()
        with patch.dict(sys.modules, {'hou': hou}):
            self.assertTrue(Path(_resolve_renderer_python(sys.executable)).is_file())
            with self.assertRaisesRegex(hou.OperationFailed, 'not found or not executable'):
                _resolve_renderer_python('/no/such/python')
            with self.assertRaisesRegex(hou.OperationFailed, 'not the interpreter'):
                _resolve_renderer_python('/path/to/physics_vfx.so')
            stream = io.StringIO()
            with redirect_stderr(stream):
                thread = _monitor_render_worker(types.SimpleNamespace(wait=lambda: -11))
                thread.join(timeout=2)
            self.assertFalse(thread.is_alive())
            self.assertIn('signal 11', stream.getvalue())
            stream = io.StringIO()
            with redirect_stderr(stream):
                thread = _monitor_render_worker(types.SimpleNamespace(wait=lambda: -15, _physvfx_cancelled=True))
                thread.join(timeout=2)
            self.assertFalse(thread.is_alive())
            self.assertEqual(stream.getvalue(), '')

    @unittest.skipUnless(os.name == 'posix', 'process-group cancellation is POSIX-specific')
    def test_second_click_stops_previous_render_group(self):
        import signal
        hou = make_hou(); network = Network('lop')
        with patch.dict(sys.modules, {'hou': hou}):
            node = create_node('render_view', network=network)
            process = types.SimpleNamespace(pid=12345, poll=lambda: None, wait=lambda timeout: 0)
            _render_jobs[node.path()] = process
            with patch('os.killpg') as kill_group:
                _cancel_previous_render(node)
            kill_group.assert_called_once_with(12345, signal.SIGTERM)
            self.assertTrue(process._physvfx_cancelled)
            self.assertNotIn(node.path(), _render_jobs)

    @unittest.skipUnless(os.name == 'posix', 'POSIX process-group cancellation')
    def test_restart_stops_previous_worker_before_export_and_launch(self):
        hou = make_hou(); network = Network('lop'); hou.isUIAvailable = lambda: False
        events = []
        class Layer:
            def Export(self, path):
                events.append('export')
                Path(path).write_text('#usda 1.0')
                return True
        stage = types.SimpleNamespace(Flatten=lambda: Layer())
        with tempfile.TemporaryDirectory() as tmp, patch.dict(sys.modules, {'hou': hou}):
            folder = Path(tmp); script = folder / 'python' / 'usd_sphere_scene.py'
            script.parent.mkdir(); script.write_text('')
            viewer = folder / 'viewer'; viewer.write_text(''); viewer.chmod(0o755)
            node = create_node('render_view', network=network)
            node.values['pv_usd_path'].set(str(folder / 'scene.usda'))
            node.values['pv_image_path'].set(str(folder / 'image.exr'))
            node.values['pv_render_script'].set(str(script))
            node.values['pv_viewer_path'].set(str(viewer))
            node.stage = lambda: stage
            old = types.SimpleNamespace(pid=12345, poll=lambda: None,
                                        wait=lambda timeout: events.append('wait'))
            _render_jobs[node.path()] = old
            def start(*args, **kwargs):
                events.append('launch')
                return types.SimpleNamespace()
            with patch('os.killpg', side_effect=lambda *args: events.append('stop')), \
                 patch('subprocess.Popen', side_effect=start), \
                 patch('physvfx_houdini.nodes._monitor_render_worker'):
                render_and_view(node)
            self.assertEqual(events, ['stop', 'wait', 'export', 'launch'])
            self.assertTrue(old._physvfx_cancelled)
            _render_jobs.pop(node.path(), None)

    @unittest.skipUnless(os.name == 'posix', 'uses POSIX process-group stubs')
    def test_failed_cancellation_keeps_tracking_the_previous_worker(self):
        hou = make_hou(); network = Network('lop')
        with patch.dict(sys.modules, {'hou': hou}):
            node = create_node('render_view', network=network)
            process = types.SimpleNamespace(pid=12345, poll=lambda: None, wait=lambda timeout: 0)
            _render_jobs[node.path()] = process
            with patch('os.killpg', side_effect=PermissionError('denied')):
                with self.assertRaisesRegex(hou.OperationFailed, 'Could not stop'):
                    _cancel_previous_render(node)
            self.assertIs(_render_jobs[node.path()], process)
            self.assertFalse(process._physvfx_cancelled)
            _render_jobs.pop(node.path(), None)

    def test_missing_viewer_does_not_export_or_start_render(self):
        hou = make_hou(); network = Network('lop')
        with tempfile.TemporaryDirectory() as tmp, patch.dict(sys.modules, {'hou': hou}):
            folder = Path(tmp); script = folder / 'render.py'; script.write_text('')
            usd = folder / 'scene.usda'; usd.write_text('old stage')
            node = create_node('render_view', network=network)
            node.values['pv_usd_path'].set(str(usd))
            node.values['pv_image_path'].set(str(folder / 'render.exr'))
            node.values['pv_render_script'].set(str(script))
            node.values['pv_viewer_path'].set(str(folder / 'missing_viewer'))
            with patch('subprocess.Popen') as popen:
                with self.assertRaisesRegex(hou.OperationFailed, 'viewer not found'):
                    render_and_view(node)
                popen.assert_not_called()
            self.assertEqual(usd.read_text(), 'old stage')

    def test_external_interpreter_starts_with_houdini_pythonhome_set(self):
        with tempfile.TemporaryDirectory() as tmp, patch.dict(os.environ, {'PYTHONHOME': '/invalid/houdini/python'}):
            env = _renderer_environment(Path(tmp))
            result = subprocess.run([sys.executable, '-c', 'import encodings; print("started")'],
                                    env=env, capture_output=True, text=True, check=True)
            self.assertEqual(result.stdout.strip(), 'started')
            self.assertNotIn('PYTHONHOME', env)
            self.assertEqual(os.environ['PYTHONHOME'], '/invalid/houdini/python')

    def test_render_node_is_passthrough_on_cook(self):
        hou = make_hou(); network = Network('lop')
        with patch.dict(sys.modules, {'hou': hou}):
            node = create_node('render_view', network=network)
            cook(node, 'render_view')  # no editable stage is requested

    def test_cache_node_button_uses_final_stage_and_target(self):
        hou = make_hou(); network = Network('lop')
        hou.isUIAvailable = lambda: False
        class FlatLayer:
            def Export(self, path):
                Path(path).write_text('composed stage')
                return True
        region = types.SimpleNamespace(GetAttribute=lambda key: types.SimpleNamespace(Get=lambda: '/tmp/shadow'))
        stage = types.SimpleNamespace(GetPrimAtPath=lambda path: region, Flatten=lambda: FlatLayer())
        with patch.dict(sys.modules, {'hou': hou}), patch.dict(os.environ, {'PYTHONHOME': '/invalid/houdini/python'}):
            source = create_node('shadow_region', network=network)
            source.values['pv_path'].set('/World/ShadowHead')
            node = create_node('shadow_cache', network=network, source=source)
            self.assertEqual(node.parm('pv_region_path').evalAsString(), '/World/ShadowHead')
            node.values['pv_shadow_cache_dir'].set('/tmp/shadow')
            network.displayNode = lambda: types.SimpleNamespace(stage=lambda: stage)
            with patch('subprocess.run', return_value=types.SimpleNamespace(returncode=0)) as run:
                save_shadow_cache(node)
                args, kwargs = run.call_args
                self.assertIn('--save-shadow-cache', args[0])
                self.assertIn('/World/ShadowHead', args[0])
                self.assertIn('--cache-dir', args[0])
                self.assertFalse(kwargs['check'])
                self.assertNotIn('PYTHONHOME', kwargs['env'])
                self.assertEqual(os.environ['PYTHONHOME'], '/invalid/houdini/python')

    def test_refresh_restores_empty_script_but_keeps_custom_script(self):
        hou = make_hou(); network = Network('lop')
        with patch.dict(sys.modules, {'hou': hou}):
            node = create_node('shadow_region', network=network)
            node.parm('python').set('')
            hou.selectedNodes = lambda: [node]
            refresh_selected_nodes()
            self.assertIn("cook(hou.pwd(), 'shadow_region')", node.parm('python').value)
            node.parm('python').set('custom script')
            refresh_selected_nodes()
            self.assertEqual(node.parm('python').value, 'custom script')

    def test_refresh_adds_missing_parameters_without_resetting_values(self):
        hou = make_hou(); network = Network('lop')
        with patch.dict(sys.modules, {'hou': hou}):
            node = create_node('operation', network=network)
            node.values['pv_input_a'].set('/World/Head')
            for folder in node.groups.folders:
                folder.kwargs['parm_templates'] = tuple(s for s in folder.kwargs['parm_templates']
                                                       if s._name != 'pv_use_stamp_bounds')
            node.values.pop('pv_use_stamp_bounds')
            hou.selectedNodes = lambda: [node]
            refresh_selected_nodes()
            self.assertEqual(node.values['pv_input_a'].evalAsString(), '/World/Head')
            self.assertIsNotNone(node.parm('pv_use_stamp_bounds'))
            count = len(node.groups.folders)
            refresh_selected_nodes()
            self.assertEqual(len(node.groups.folders), count)

    def test_refresh_hides_legacy_operation_bounds_without_deleting_values(self):
        hou = make_hou(); network = Network('lop')
        with patch.dict(sys.modules, {'hou': hou}):
            node = create_node('operation', network=network)
            old = Template('pv_use_pyro_bounds', 'Use Explicit Pyro Bounds', default_value=True)
            node.groups.append(Folder('old', 'Old', parm_templates=(old,)))
            node.setParmTemplateGroup(node.groups)
            hou.selectedNodes = lambda: [node]
            refresh_selected_nodes()
            self.assertTrue(node.groups.find('pv_use_pyro_bounds').isHidden())
            self.assertTrue(node.parm('pv_use_pyro_bounds').eval())

    def test_refresh_updates_field_menu_without_resetting_selection(self):
        hou = make_hou(); network = Network('lop')
        with patch.dict(sys.modules, {'hou': hou}):
            node = create_node('field', network=network)
            node.values['pv_kind'].set('plane')
            template = node.groups.find('pv_kind')
            template.setMenuItems(tuple(x for x in template.menuItems() if x != 'infinite_plane'))
            hou.selectedNodes = lambda: [node]
            refresh_selected_nodes()
            self.assertIn('infinite_plane', node.groups.find('pv_kind').menuItems())
            self.assertEqual(node.values['pv_kind'].evalAsString(), 'plane')

    def test_shelf_xml_references_all_tools(self):
        path = Path(__file__).resolve().parents[1] / 'toolbar' / 'physvfx.shelf'
        root = ET.parse(path).getroot()
        refs = {node.attrib['name'] for node in root.findall('./toolshelf/memberTool')}
        tools = {node.attrib['name'] for node in root.findall('./tool')}
        self.assertEqual(refs, tools)
        self.assertEqual(len(tools), 9)
        for script in root.findall('./tool/script'):
            self.assertTrue('physvfx_houdini.create_node(' in script.text or
                            'physvfx_houdini.refresh_selected_nodes()' in script.text)


if __name__ == '__main__': unittest.main()
