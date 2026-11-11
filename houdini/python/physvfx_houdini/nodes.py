"""Create Solaris Python LOPs and evaluate their user-facing parameters."""
from .schema import PARAMETERS, parameters
from .author import AuthoringError, author
from .spline_channels import CHANNELS, resolve_guides


# Running background render workers keyed by their Houdini node path.
_render_jobs = {}

NODE_LABELS = {
    'render_view': 'PhysVFX Render and View',
    'shadow_cache': 'PhysVFX Shadow Cache',
    'shadow_region': 'PhysVFX Shadow Region',
    'field': 'PhysVFX Field',
    'wisp_spline': 'PhysVFX Wisp Spline',
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
    if spec.kind == 'ramp':
        return hou.RampParmTemplate(name, spec.label, hou.rampParmType.Float, help=help_text)
    if spec.kind == 'guides':
        parts = [hou.FloatParmTemplate('pv_guide_position#', 'Position', 3,
                                       default_value=(0.0, 0.0, 0.0),
                                       help='Local-space guide position.')]
        for channel, label, channel_kind, default in CHANNELS:
            size = 3 if channel_kind == 'vector' else 1
            defaults = default if size == 3 else (default,)
            parts.append(hou.FloatParmTemplate('pv_guide_' + channel + '#', label, size,
                                               default_value=defaults,
                                               help='Used in Per Guide mode.'))
        folder = hou.FolderParmTemplate(name, spec.label, parm_templates=tuple(parts),
                                       folder_type=hou.folderType.MultiparmBlock)
        folder.setDefaultValue(2)
        return folder
    if spec.kind == 'menu':
        return hou.StringParmTemplate(name, spec.label, 1, default_value=(spec.default,),
            menu_items=tuple(item[0] for item in spec.choices),
            menu_labels=tuple(item[1] for item in spec.choices), help=help_text)
    if spec.kind == 'string':
        return hou.StringParmTemplate(name, spec.label, 1, default_value=(spec.default,), help=help_text)
    if spec.kind == 'toggle':
        kwargs = {'default_value': spec.default, 'help': help_text}
        if spec.name == 'continuous_preview':
            kwargs.update(script_callback="import physvfx_houdini.nodes as n; n.set_continuous_preview(kwargs['node'])",
                          script_callback_language=hou.scriptLanguage.Python)
        return hou.ToggleParmTemplate(name, spec.label, **kwargs)
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
        kwargs = {'default_value': (spec.default,), 'help': help_text}
        if spec.name == 'radius':
            kwargs.update(script_callback="import physvfx_houdini.nodes as n; n.activate_sphere_radius_override(kwargs['node'])",
                          script_callback_language=hou.scriptLanguage.Python)
        return hou.FloatParmTemplate(name, spec.label, 1, **kwargs)
    raise ValueError('Unknown parameter type: ' + spec.kind)


def activate_sphere_radius_override(node):
    """Editing Radius on a sphere should immediately change the field."""
    kind, override = node.parm('pv_kind'), node.parm('pv_override_radius')
    if kind is not None and override is not None and kind.evalAsString() == 'isf_sphere':
        override.set(True)


def set_continuous_preview(node):
    """Start or stop the interactive preview from the toggle callback."""
    import hou
    from . import live_preview
    parm = node.parm('pv_continuous_preview')
    if parm is not None and parm.eval():
        if not hou.isUIAvailable():
            raise hou.OperationFailed('Continuous Preview requires an interactive Houdini session.')
        live_preview.start(node)
    else:
        live_preview.stop(node)


def read_values(node, kind):
    """Evaluate everything before acquiring the editable stage lock."""
    values = {}
    for spec in parameters(kind):
        name = 'pv_' + spec.name
        if spec.kind == 'guides':
            count_parm = node.parm(name)
            count = int(count_parm.eval()) if count_parm is not None else 0
            guides = []
            for index in range(1, count + 1):
                point = node.parmTuple('pv_guide_position' + str(index))
                if point is None:
                    raise AuthoringError('Missing wisp spline guide position parameter')
                guide = {'position': tuple(point.eval())}
                for channel, _, channel_kind, default in CHANNELS:
                    parm_name = 'pv_guide_' + channel + str(index)
                    parm = node.parmTuple(parm_name) if channel_kind == 'vector' else node.parm(parm_name)
                    guide[channel] = (tuple(parm.eval()) if channel_kind == 'vector' else parm.eval()) if parm is not None else default
                guides.append(guide)
            values[spec.name] = guides
            continue
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
            # Recreate the in-memory session after loading a HIP with the
            # toggle enabled. The first export occurs later on the UI loop.
            parm = node.parm('pv_continuous_preview')
            if parm is not None and parm.eval() and getattr(hou, 'isUIAvailable', lambda: False)():
                from .live_preview import start
                start(node)
            return  # pass the upstream stage through unchanged
        values = read_values(node, kind)
        if kind == 'wisp_spline':
            try:
                values['guide_positions'], values['spline_samples'] = resolve_guides(values)
            except (KeyError, ValueError, TypeError) as exc:
                raise AuthoringError(str(exc)) from exc
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
        if kind == 'wisp_spline':
            # Multiparm instances share a template default; give the second
            # guide a distinct position on initial node creation.
            second = node.parmTuple('pv_guide_position2')
            if second is not None:
                second.set((0.0, 2.0, 0.0))
            ramp = hou.Ramp((hou.rampBasis.Linear, hou.rampBasis.Linear), (0.0, 1.0), (0.0, 1.0))
            for channel, _, _, _ in CHANNELS:
                parm = node.parm('pv_splineRamp_' + channel)
                if parm is not None:
                    parm.set(ramp)
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
        menus_changed = False
        if kind == 'operation':
            # Keep legacy values and animation, but hide controls that no
            # longer affect analytic operations.
            for name in ('pv_bounds_min', 'pv_bounds_max', 'pv_use_pyro_bounds'):
                old = group.find(name)
                if old is not None and not old.isHidden():
                    old.hide(True)
                    group.replace(name, old)
                    menus_changed = True
        for spec in parameters(kind):
            if spec.kind != 'menu':
                continue
            existing = group.find('pv_' + spec.name)
            if existing is not None and tuple(existing.menuItems()) != tuple(x[0] for x in spec.choices):
                existing.setMenuItems(tuple(x[0] for x in spec.choices))
                existing.setMenuLabels(tuple(x[1] for x in spec.choices))
                group.replace('pv_' + spec.name, existing)
                menus_changed = True
        if kind == 'field':
            radius = group.find('pv_radius')
            if radius is not None and not radius.scriptCallback():
                radius.setScriptCallback("import physvfx_houdini.nodes as n; n.activate_sphere_radius_override(kwargs['node'])")
                radius.setScriptCallbackLanguage(hou.scriptLanguage.Python)
                group.replace('pv_radius', radius)
                menus_changed = True
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
        if missing or menus_changed:
            node.setParmTemplateGroup(group)
        updated += 1
    if not updated:
        raise hou.OperationFailed('No selected nodes are PhysVFX nodes.')


def _resolve_renderer_python(value):
    """Resolve an executable without confusing a Python extension for Python."""
    import os
    import re
    import shutil
    import hou

    candidate = os.path.expanduser(str(value).strip())
    guidance = ('Renderer Python must be a Python interpreter executable, such as python3 '
                'or /path/to/venv/bin/python. The physics_vfx .so is an importable module, '
                'not the interpreter.')
    if not candidate:
        raise hou.OperationFailed(guidance)
    if re.search(r'\.(?:so(?:\.\d+)*|pyd|dll|dylib)$', candidate, re.IGNORECASE):
        raise hou.OperationFailed(guidance)
    resolved = shutil.which(candidate)
    if not resolved:
        raise hou.OperationFailed(f'Renderer Python executable not found or not executable: {candidate}. {guidance}')
    if re.search(r'\.(?:so(?:\.\d+)*|pyd|dll|dylib)$', os.path.realpath(resolved), re.IGNORECASE):
        raise hou.OperationFailed(guidance)
    # Keep venv symlinks intact: resolving them can change Python's environment.
    return os.path.abspath(resolved)


def _monitor_render_worker(process):
    """Report an early child crash without blocking Houdini's interface."""
    import sys
    import threading

    def report():
        try:
            code = process.wait()
            if code and not getattr(process, '_physvfx_cancelled', False):
                reason = f'signal {-code}' if code < 0 else f'exit code {code}'
                print(f'[PhysVFX] Render worker ended with {reason}; check the preceding console output. '
                      'Verify Renderer Python is the interpreter for the built extension and OpenUSD.',
                      file=sys.stderr, flush=True)
        except Exception as exc:
            print(f'[PhysVFX] Could not monitor render worker: {exc}', file=sys.stderr, flush=True)

    thread = threading.Thread(target=report, name='physvfx-render-monitor', daemon=True)
    thread.start()
    return thread



def _renderer_environment(repo):
    """Keep the selected external interpreter's standard library independent of Houdini."""
    import os
    env = os.environ.copy()
    # Houdini sets PYTHONHOME to its bundled runtime. Passing that to an
    # external Python can prevent even the encodings module from loading.
    env.pop('PYTHONHOME', None)
    extra = [str(repo / 'build' / 'python'), str(repo / 'python')]
    env['PYTHONPATH'] = os.pathsep.join(extra + ([env['PYTHONPATH']] if env.get('PYTHONPATH') else []))
    return env


def save_shadow_cache(node):
    """Export the final stage and invoke the external renderer for one region."""
    import os
    import subprocess
    import tempfile
    from pathlib import Path
    import hou

    values = read_values(node, 'shadow_cache')
    renderer_python = _resolve_renderer_python(values['cache_python'])
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
    env = _renderer_environment(repo)
    with tempfile.TemporaryDirectory(prefix='physvfx_shadow_') as tmp:
        usd = str(Path(tmp) / 'scene.usda')
        if not stage.Flatten().Export(usd):
            raise hou.OperationFailed('Could not export the composed scene for shadow caching.')
        command = [renderer_python, str(script), usd, '--save-shadow-cache', values['region_path'],
                   '--cache-dir', values['cache_stamp_dir']]
        try:
            result = subprocess.run(command, env=env, check=False)
        except OSError as exc:
            raise hou.OperationFailed(f'Could not start renderer Python: {exc}') from exc
        if result.returncode:
            raise hou.OperationFailed(f'Shadow cache failed (exit {result.returncode}); see the console for details.')
    if hou.isUIAvailable():
        hou.ui.displayMessage('Shadow maps saved. Enable Load from Disk to use the cache.')



def _cancel_previous_render(node):
    """Stop a prior job from this node before writing its scene again."""
    import os
    import signal
    import subprocess
    import hou

    key = node.path()
    previous = _render_jobs.pop(key, None)
    if previous is None or previous.poll() is not None:
        return
    previous._physvfx_cancelled = True
    print(f'[PhysVFX] Stopping previous render from {key}.', flush=True)
    try:
        try:
            if os.name == 'posix':
                # The worker is a session leader; its renderer and preview viewer
                # share this group, so no superseded renderer can keep writing.
                os.killpg(previous.pid, signal.SIGTERM)
            elif os.name == 'nt':
                # Terminating only the worker on Windows would leave its renderer running.
                subprocess.run(['taskkill', '/PID', str(previous.pid), '/T', '/F'],
                               check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            else:
                previous.terminate()
        except ProcessLookupError:
            pass  # The previous worker exited between poll and signal.
        try:
            previous.wait(timeout=5)
        except subprocess.TimeoutExpired:
            if os.name == 'posix':
                os.killpg(previous.pid, signal.SIGKILL)
            else:
                previous.kill()
            previous.wait(timeout=5)
    except (OSError, subprocess.TimeoutExpired, subprocess.CalledProcessError) as exc:
        previous._physvfx_cancelled = False
        _render_jobs[key] = previous
        raise hou.OperationFailed(f'Could not stop the previous render: {exc}') from exc


def _render_programs(values):
    """Resolve the shared external renderer and watched image viewer."""
    import os
    from pathlib import Path
    import hou
    script = Path(values['render_script']).expanduser().resolve() if values['render_script'].strip() else Path(__file__).resolve().parents[3] / 'python' / 'usd_sphere_scene.py'
    if not script.is_file():
        raise hou.OperationFailed('Set Renderer Script to your repository python/usd_sphere_scene.py.')
    repo = script.resolve().parent.parent
    if values['viewer_path'].strip():
        viewer = Path(values['viewer_path']).expanduser().resolve()
    else:
        candidates = [repo / 'build' / 'DPA_ImageProcessing' / 'dpa_imgviewer',
                      repo / 'build' / 'DPA_ImageProcessing' / 'dpa_image_viewer',
                      repo / 'build' / 'dpa_imgviewer', repo / 'build' / 'dpa_image_viewer']
        viewer = next((p for p in candidates if p.is_file() and os.access(p, os.X_OK)), candidates[0])
    if not viewer.is_file() or not os.access(viewer, os.X_OK):
        raise hou.OperationFailed(f'Image viewer not found or not executable: {viewer}. Build dpa_imgviewer or set Image Viewer.')
    return _resolve_renderer_python(values['render_python']), script, repo, viewer


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
    renderer_python, script, repo, viewer = _render_programs(v)
    stage = node.stage()
    if stage is None:
        raise hou.OperationFailed('Connect the complete Solaris scene to this node.')
    _cancel_previous_render(node)
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
    env = _renderer_environment(repo)
    worker = Path(__file__).resolve().parent / 'render_worker.py'
    command = [renderer_python, '-u', str(worker), '--python', renderer_python,
               '--script', str(script), '--usd', str(usd), '--image', str(image),
               '--viewer', str(viewer), '--cache-dir', v['render_stamp_dir']]
    if v['progressive_preview']:
        command.append('--progressive')
    try:
        process = subprocess.Popen(command, cwd=str(repo), env=env,
                                   start_new_session=(os.name == 'posix'))
    except OSError as exc:
        raise hou.OperationFailed(f'Could not start renderer Python: {exc}') from exc
    _render_jobs[node.path()] = process
    _monitor_render_worker(process)
    print(f'[PhysVFX] Saved {usd}; render worker launched with {renderer_python}. '
          'Follow progress and errors in the Houdini console.', flush=True)
    if hou.isUIAvailable():
        hou.ui.setStatusMessage('PhysVFX render worker launched; see the Houdini console for progress.')
