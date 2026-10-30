"""Create Solaris Python LOPs and evaluate their user-facing parameters."""
from .schema import PARAMETERS, parameters
from .author import AuthoringError, author


NODE_LABELS = {
    'field': 'PhysVFX Field',
    'operation': 'PhysVFX Operation',
    'camera': 'PhysVFX Camera',
    'light': 'PhysVFX Light',
    'settings': 'PhysVFX Render Settings',
}


def _template(hou, spec):
    name = 'pv_' + spec.name
    help_text = spec.help
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
