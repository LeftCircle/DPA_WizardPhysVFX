"""Create Solaris Python LOPs and evaluate their user-facing parameters."""
from .schema import PARAMETERS, parameters
from .author import AuthoringError, author


NODE_LABELS = {
    'render_view': 'PhysVFX Render and View',
    'shadow_cache': 'PhysVFX Shadow Cache',
    'shadow_region': 'PhysVFX Shadow Region',
    'field': 'PhysVFX Field',
    'operation': 'PhysVFX Operation',
    'camera': 'PhysVFX Camera',
    'light': 'PhysVFX Light',
    'settings': 'PhysVFX Render Settings',
}


def _template(hou, spec):
    name = 'pv_' + spec.name
    help_text = spec.help
    if spec.kind == 'button':
        callback = 'render_and_view' if spec.name == 'render_view' else 'save_shadow_cache'
        return hou.ButtonParmTemplate(name, spec.label, help=help_text,
            script_callback="import physvfx_houdini.nodes as n; n.%s(kwargs['node'])" % callback,
            script_callback_language=hou.scriptLanguage.Python)
    if spec.kind == 'menu':
        return hou.StringParmTemplate(name, spec.label, 1, default_value=(spec.default,),
            menu_items=tuple(item[0] for item in spec.choices),
            menu_labels=tuple(item[1] for item in spec.choices), help=help_text)
    if spec.kind == 'string':
        return hou.StringParmTemplate(name, spec.label, 1, default_value=(spec.default,), help=help_text)
    if spec.kind == 'toggle':
        return hou.ToggleParmTemplate(name, spec.label, default_value=spec.default, help=help_text)
    if spec.kind == 'int':
        return hou.IntParmTemplate(name, spec.label, 1, default_value=(spec.default,), help=help_text)
    if spec.kind == 'int2':
        return hou.IntParmTemplate(name, spec.label, 2, default_value=spec.default, help=help_text)
    if spec.kind in ('vector', 'vector2', 'color'):
        kwargs = {'default_value': spec.default, 'help': help_text}
        if spec.kind == 'color':
            kwargs['look'] = hou.parmLook.ColorSquare
            kwargs['naming_scheme'] = hou.parmNamingScheme.RGBA
        return hou.FloatParmTemplate(name, spec.label, len(spec.default), **kwargs)
    if spec.kind == 'float':
        return hou.FloatParmTemplate(name, spec.label, 1, default_value=(spec.default,), help=help_text)
    raise ValueError('Unknown parameter type: ' + spec.kind)


def read_values(node, kind):
    """Evaluate everything before acquiring the editable stage lock."""
    values = {}
    for spec in parameters(kind):
        name = 'pv_' + spec.name
        if spec.kind == 'button':
            values[spec.name] = None
            continue
        if spec.kind in ('vector', 'vector2', 'color', 'int2'):
            parm = node.parmTuple(name)
            values[spec.name] = tuple(parm.eval()) if parm is not None else spec.default
        else:
            parm = node.parm(name)
            if parm is None:
                # Nodes created with earlier plugin versions have no new spare
                # parameters, but should continue to cook using current defaults.
                values[spec.name] = spec.default
            elif spec.kind in ('string', 'menu'):
                values[spec.name] = parm.evalAsString()
            else:
                value = parm.eval()
                values[spec.name] = bool(value) if spec.kind == 'toggle' else value
    return values


def cook(node, kind):
    import hou
    try:
        if kind == 'render_view':
            return  # pass the upstream stage through unchanged
        values = read_values(node, kind)
        stage = node.editableStage()
        if stage is None:
            raise AuthoringError('This tool requires an editable Solaris stage')
        author(stage, kind, values)
    except AuthoringError as exc:
        raise hou.NodeError(str(exc)) from exc


def _network_from_ui(hou):
    editor = hou.ui.paneTabUnderCursor()
    if editor is None or editor.type() != hou.paneTabType.NetworkEditor:
        editor = hou.ui.paneTabOfType(hou.paneTabType.NetworkEditor)
    if editor is None:
        raise hou.OperationFailed('Open a Solaris LOP network editor before using this shelf tool.')
    return editor.pwd()


def create_node(kind, network=None, source=None):
    """Create a Python Script LOP with a PhysVFX parameter interface.

    With no explicit network, use the active network editor and connect the
    selected node in that network, if there is one.
    """
    import hou
    if kind not in PARAMETERS:
        raise ValueError('Unknown PhysVFX node type: ' + str(kind))
    network = network or _network_from_ui(hou)
    if network.childTypeCategory() != hou.lopNodeTypeCategory():
        raise hou.OperationFailed('PhysVFX shelf tools must be used in a Solaris LOP network.')
    if source is None:
        selected = [n for n in hou.selectedNodes() if n.parent() == network]
        source = selected[-1] if selected else None
    if source is not None and source.parent() != network:
        raise hou.OperationFailed('The input node must be in the selected LOP network.')
    node = network.createNode('pythonscript', 'physvfx_' + kind)
    try:
        group = node.parmTemplateGroup()
        for index, (title, specs) in enumerate(PARAMETERS[kind]):
            folder = hou.FolderParmTemplate('pv_folder_' + str(index), title,
                parm_templates=tuple(_template(hou, spec) for spec in specs))
            group.append(folder)
        node.setParmTemplateGroup(group)
        if kind == 'shadow_cache' and source is not None and source.userData('physvfx_node_kind') == 'shadow_region':
            source_path = source.parm('pv_path')
            if source_path is not None:
                node.parm('pv_region_path').set(source_path.evalAsString())
        code_parm = node.parm('python')
        if code_parm is None:
            # The displayed label is stable even if a Houdini release renames
            # the underlying Python Script parameter.
            code_parm = next((p for p in node.parms()
                              if p.parmTemplate().label() == 'Python Code'), None)
        if code_parm is None:
            raise hou.OperationFailed('Could not find the Python Code parameter on Python Script LOP.')
        code_parm.set("import hou\nfrom physvfx_houdini import cook\ncook(hou.pwd(), %r)\n" % kind)
        node.setUserData('physvfx_node_kind', kind)
        if source is not None:
            node.setInput(0, source)
        node.moveToGoodPosition()
        node.setSelected(True, clear_all_selected=True)
        node.setDisplayFlag(True)
        return node
    except Exception:
        node.destroy()
        raise


def refresh_selected_nodes():
    """Add missing PhysVFX parameters to selected nodes without replacing existing values."""
    import hou
    nodes = hou.selectedNodes()
    if not nodes:
        raise hou.OperationFailed('Select one or more PhysVFX nodes first.')
    updated = 0
    for node in nodes:
        kind = node.userData('physvfx_node_kind')
        if kind not in PARAMETERS:
            continue
        group = node.parmTemplateGroup()
        missing = [spec for spec in parameters(kind) if group.find('pv_' + spec.name) is None]
        code_parm = node.parm('python')
        if code_parm is not None and not code_parm.evalAsString().strip():
            code_parm.set("import hou\nfrom physvfx_houdini import cook\ncook(hou.pwd(), %r)\n" % kind)
        if missing:
            folder_name = 'pv_folder_updates'
            suffix = 2
            while group.find(folder_name) is not None:
                folder_name = 'pv_folder_updates_' + str(suffix)
                suffix += 1
            group.append(hou.FolderParmTemplate(folder_name, 'PhysVFX Updates',
                parm_templates=tuple(_template(hou, spec) for spec in missing)))
            node.setParmTemplateGroup(group)
        updated += 1
    if not updated:
        raise hou.OperationFailed('No selected nodes are PhysVFX nodes.')

def save_shadow_cache(node):
    """Export the final stage and invoke the external renderer for one region."""
    import os
    import subprocess
    import tempfile
    from pathlib import Path
    import hou

    values = read_values(node, 'shadow_cache')
    requested = values['cache_scene_lop'].strip()
    scene_node = hou.node(requested) if requested else node.parent().displayNode()
    if scene_node is None:
        raise hou.OperationFailed('Set Final Scene LOP or select a display node with the complete scene.')
    stage = scene_node.stage()
    if stage is None or not stage.GetPrimAtPath(values['region_path']):
        raise hou.OperationFailed('The final scene stage must contain this shadow region.')
    region = stage.GetPrimAtPath(values['region_path'])
    cache_dir = region.GetAttribute('pbvfx:shadowCacheDir')
    if not cache_dir or cache_dir.Get() != values['shadow_cache_dir'].strip():
        raise hou.OperationFailed('Final Scene LOP must include this cache node and its cache directory.')
    script = Path(values['cache_renderer_script']) if values['cache_renderer_script'] else Path(__file__).resolve().parents[3] / 'python' / 'usd_sphere_scene.py'
    if not script.is_file():
        raise hou.OperationFailed('Set Renderer Script to your repository python/usd_sphere_scene.py.')
    if not values['shadow_cache_dir'].strip():
        raise hou.OperationFailed('Set a Shadow Cache Directory first.')
    repo = script.resolve().parent.parent
    env = os.environ.copy()
    extra = [str(repo / 'build' / 'python'), str(repo / 'python')]
    env['PYTHONPATH'] = os.pathsep.join(extra + ([env['PYTHONPATH']] if env.get('PYTHONPATH') else []))
    with tempfile.TemporaryDirectory(prefix='physvfx_shadow_') as tmp:
        usd = str(Path(tmp) / 'scene.usda')
        if not stage.Flatten().Export(usd):
            raise hou.OperationFailed('Could not export the composed scene for shadow caching.')
        command = [values['cache_python'], str(script), usd, '--save-shadow-cache', values['region_path'],
                   '--cache-dir', values['cache_stamp_dir']]
        try:
            result = subprocess.run(command, env=env, check=False)
        except OSError as exc:
            raise hou.OperationFailed(f'Could not start renderer Python: {exc}') from exc
        if result.returncode:
            raise hou.OperationFailed(f'Shadow cache failed (exit {result.returncode}); see the console for details.')
    if hou.isUIAvailable():
        hou.ui.displayMessage('Shadow maps saved. Enable Load from Disk to use the cache.')


def render_and_view(node):
    """Export this LOP's stage, then start the renderer and viewer in a worker."""
    import os
    import subprocess
    import tempfile
    from pathlib import Path
    import hou

    v = read_values(node, 'render_view')
    if not v['usd_path'].strip() or not v['image_path'].strip():
        raise hou.OperationFailed('Set both USD Save Path and Image Save Path.')
    usd = Path(v['usd_path']).expanduser().resolve()
    image = Path(v['image_path']).expanduser().resolve()
    if usd.suffix.lower() != '.usda':
        raise hou.OperationFailed('USD Save Path must end in .usda.')
    if not image.suffix or usd == image:
        raise hou.OperationFailed('Choose a distinct Image Save Path with a file extension.')
    script = Path(v['render_script']).expanduser().resolve() if v['render_script'].strip() else Path(__file__).resolve().parents[3] / 'python' / 'usd_sphere_scene.py'
    if not script.is_file():
        raise hou.OperationFailed('Set Renderer Script to your repository python/usd_sphere_scene.py.')
    repo = script.resolve().parent.parent
    if v['viewer_path'].strip():
        viewer = Path(v['viewer_path']).expanduser().resolve()
    else:
        candidates = [repo / 'build' / 'DPA_ImageProcessing' / 'dpa_imgviewer',
                      repo / 'build' / 'DPA_ImageProcessing' / 'dpa_image_viewer',
                      repo / 'build' / 'dpa_imgviewer', repo / 'build' / 'dpa_image_viewer']
        viewer = next((p for p in candidates if p.is_file() and os.access(p, os.X_OK)), candidates[0])
    if not viewer.is_file() or not os.access(viewer, os.X_OK):
        raise hou.OperationFailed(f'Image viewer not found or not executable: {viewer}. Build dpa_imgviewer or set Image Viewer.')
    if not v['render_python'].strip():
        raise hou.OperationFailed('Set Renderer Python.')
    stage = node.stage()
    if stage is None:
        raise hou.OperationFailed('Connect the complete Solaris scene to this node.')
    try:
        usd.parent.mkdir(parents=True, exist_ok=True)
        image.parent.mkdir(parents=True, exist_ok=True)
        fd, temp_path = tempfile.mkstemp(prefix='.physvfx_', suffix='.usda', dir=str(usd.parent))
        os.close(fd)
        try:
            if not stage.Flatten().Export(temp_path):
                raise hou.OperationFailed('Could not export the composed USD stage.')
            os.replace(temp_path, usd)
        finally:
            if os.path.exists(temp_path): os.unlink(temp_path)
    except OSError as exc:
        raise hou.OperationFailed(f'Could not save the USD stage: {exc}') from exc
    env = os.environ.copy()
    env['PYTHONPATH'] = os.pathsep.join([str(repo / 'build' / 'python'), str(repo / 'python')] +
        ([env['PYTHONPATH']] if env.get('PYTHONPATH') else []))
    worker = Path(__file__).resolve().parent / 'render_worker.py'
    command = [v['render_python'], '-u', str(worker), '--python', v['render_python'],
               '--script', str(script), '--usd', str(usd), '--image', str(image),
               '--viewer', str(viewer), '--cache-dir', v['render_stamp_dir']]
    try:
        subprocess.Popen(command, cwd=str(repo), env=env)
    except OSError as exc:
        raise hou.OperationFailed(f'Could not start renderer Python: {exc}') from exc
    print(f'[PhysVFX] Saved {usd}; render started. Follow progress in the Houdini console.', flush=True)
    if hou.isUIAvailable():
        hou.ui.setStatusMessage('PhysVFX render started; see the Houdini console for progress.')
