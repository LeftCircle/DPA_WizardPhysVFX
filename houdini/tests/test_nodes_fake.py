"""Test the shelf/node wiring with a small HOM stand-in."""
import sys
import types
import unittest
from pathlib import Path
from unittest.mock import patch
import xml.etree.ElementTree as ET

from physvfx_houdini.nodes import cook, create_node, read_values, refresh_selected_nodes, save_shadow_cache
from test_authoring_fake import Stage, fake_pxr
from physvfx_houdini.schema import defaults


class Template:
    def __init__(self, name, label, *args, **kwargs):
        self._name, self._label, self.args, self.kwargs = name, label, args, kwargs
    def label(self): return self._label


class Folder(Template):
    pass


class TemplateGroup:
    def __init__(self): self.folders = []
    def append(self, folder): self.folders.append(folder)
    def find(self, name):
        return next((item for folder in self.folders for item in (folder, *folder.kwargs["parm_templates"]) if item._name == name), None)


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
    def test_each_shelf_tool_creates_parameterized_python_lop(self):
        hou = make_hou(); network = Network('lop')
        with patch.dict(sys.modules, {'hou': hou}):
            for kind in ('shadow_cache', 'shadow_region', 'field', 'operation', 'camera', 'light', 'settings'):
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

    def test_cache_node_button_uses_final_stage_and_target(self):
        hou = make_hou(); network = Network('lop')
        hou.isUIAvailable = lambda: False
        class FlatLayer:
            def Export(self, path):
                Path(path).write_text('composed stage')
                return True
        region = types.SimpleNamespace(GetAttribute=lambda key: types.SimpleNamespace(Get=lambda: '/tmp/shadow'))
        stage = types.SimpleNamespace(GetPrimAtPath=lambda path: region, Flatten=lambda: FlatLayer())
        with patch.dict(sys.modules, {'hou': hou}):
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

    def test_shelf_xml_references_all_tools(self):
        path = Path(__file__).resolve().parents[1] / 'toolbar' / 'physvfx.shelf'
        root = ET.parse(path).getroot()
        refs = {node.attrib['name'] for node in root.findall('./toolshelf/memberTool')}
        tools = {node.attrib['name'] for node in root.findall('./tool')}
        self.assertEqual(refs, tools)
        self.assertEqual(len(tools), 8)
        for script in root.findall('./tool/script'):
            self.assertTrue('physvfx_houdini.create_node(' in script.text or
                            'physvfx_houdini.refresh_selected_nodes()' in script.text)


if __name__ == '__main__': unittest.main()
