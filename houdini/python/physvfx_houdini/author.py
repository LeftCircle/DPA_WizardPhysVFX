"""Author the USD contract understood by python/usd_sphere_scene.py.

Functions take ordinary Python dictionaries so they also work without Houdini
when the OpenUSD Python bindings are installed. Units are USD stage units.
"""
import math
import re

from .schema import FIELD_KINDS, OPERATIONS


class AuthoringError(ValueError):
    pass


def _pxr():
    from pxr import Gf, Sdf, UsdGeom, UsdLux, UsdRender
    return Gf, Sdf, UsdGeom, UsdLux, UsdRender


def _path(text, label):
    _, Sdf, _, _, _ = _pxr()
    try:
        result = Sdf.Path(str(text).strip())
        if not result.IsAbsolutePath() or not result.IsPrimPath() or result == Sdf.Path.absoluteRootPath:
            raise ValueError()
        return result
    except (ValueError, TypeError, RuntimeError) as exc:
        raise AuthoringError(f'{label} must be an absolute USD prim path') from exc


def _number(value, label, minimum=None, strict=False):
    try:
        value = float(value)
    except (ValueError, TypeError) as exc:
        raise AuthoringError(f'{label} must be a finite number') from exc
    if not math.isfinite(value) or (minimum is not None and (value <= minimum if strict else value < minimum)):
        raise AuthoringError(f'{label} is out of range')
    return value


def _vec(value, size, label):
    if not isinstance(value, (tuple, list)) or len(value) != size:
        raise AuthoringError(f'{label} must have {size} components')
    return tuple(_number(x, label) for x in value)


def _normal(value, label='Normal'):
    result = _vec(value, 3, label)
    if sum(x * x for x in result) <= 1e-20:
        raise AuthoringError(f'{label} must be nonzero')
    return result


def _bounds(values):
    lo = _vec(values['bounds_min'], 3, 'Bounds minimum')
    hi = _vec(values['bounds_max'], 3, 'Bounds maximum')
    if any(a >= b for a, b in zip(lo, hi)):
        raise AuthoringError('Each bounds minimum must be less than its maximum')
    return lo, hi


def _positive(value, name):
    return _number(value, name, 0, strict=True)


def _integer(value, name, minimum, maximum=2147483647):
    if isinstance(value, bool) or not isinstance(value, int) or not minimum <= value <= maximum:
        raise AuthoringError(f'{name} must be an integer between {minimum} and {maximum}')
    return value


def _color(value):
    rgb = _vec(value, 3, 'Color')
    if min(rgb) < 0:
        raise AuthoringError('Color components must be non-negative')
    return rgb


def _cache_name(value):
    value = str(value)
    if value and not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', value):
        raise AuthoringError('Cache name must start with a letter or digit and contain only letters, digits, _, - or .')
    return value


def _tag(prim, name):
    attr = prim.GetAttribute('pbvfx:' + name)
    if not attr:
        return None
    value = attr.Get()
    if value is None:
        raise AuthoringError(f'{prim.GetPath()}: pbvfx:{name} has no default value; fix it upstream')
    return value


def _existing_or_create(stage, path, create, type_name):
    prim = stage.GetPrimAtPath(path)
    if not prim:
        if not create:
            raise AuthoringError(f'{path}: prim does not exist')
        prim = stage.DefinePrim(path, type_name)
    if not prim:
        raise AuthoringError(f'{path}: could not define prim')
    if prim.IsInstanceProxy():
        raise AuthoringError(f'{path}: cannot edit an instance proxy; edit its source instead')
    return prim


def _attr(prim, name, type_name, value):
    _, Sdf, _, _, _ = _pxr()
    type_value = getattr(Sdf.ValueTypeNames, type_name)
    existing = prim.GetAttribute('pbvfx:' + name)
    if existing and existing.GetTypeName() != type_value:
        raise AuthoringError(f'{prim.GetPath()}: pbvfx:{name} has an incompatible USD type')
    prim.CreateAttribute('pbvfx:' + name, type_value, custom=True).Set(value)


def _float(prim, name, value):
    _attr(prim, name, 'Float', value)


def _float3(prim, name, value, point=False):
    Gf, _, _, _, _ = _pxr()
    _attr(prim, name, 'Point3f' if point else 'Float3', Gf.Vec3f(*value))


def _stamp(prim, values):
    _attr(prim, 'stamp', 'Bool', bool(values['stamp']))
    # An empty authored string also overrides an upstream cache name.
    _attr(prim, 'cacheName', 'String', _cache_name(values['cache_name']))


def _display_color(prim, values):
    mode = values['color_mode']
    if mode == 'inherit':
        return
    if mode not in ('display', 'physvfx'):
        raise AuthoringError(f'Unknown color authoring mode: {mode}')
    Gf, Sdf, UsdGeom, _, _ = _pxr()
    color = _color(values['color'])
    if mode == 'physvfx':
        # The renderer prioritizes any authored displayColor over pbvfx:color.
        existing = prim.GetAttribute('primvars:displayColor')
        if existing and existing.Get() is not None:
            raise AuthoringError(f'{prim.GetPath()}: displayColor takes precedence; use USD Display Color or remove it upstream')
        _attr(prim, 'color', 'Color3f', Gf.Vec3f(*color))
    else:
        primvar = UsdGeom.PrimvarsAPI(prim).CreatePrimvar(
            'displayColor', Sdf.ValueTypeNames.Color3fArray, UsdGeom.Tokens.constant)
        primvar.Set([Gf.Vec3f(*color)])


def _write_bounds(prim, values):
    lo, hi = _bounds(values)
    _float3(prim, 'boundsMin', lo, point=True)
    _float3(prim, 'boundsMax', hi, point=True)


def _write_wisp(prim, v):
    _integer(v['wispDots'], 'Dot count', 1)
    _integer(v['wispSeed'], 'Seed', -2147483648)
    for name in ('wispDensity', 'wispClump', 'wispPscale'):
        _positive(v[name], name)
    lo, hi = _number(v['wispMin'], 'Sample minimum'), _number(v['wispMax'], 'Sample maximum')
    if lo >= hi:
        raise AuthoringError('Wisp sample minimum must be less than maximum')
    _attr(prim, 'wispDots', 'Int', v['wispDots'])
    _attr(prim, 'wispSeed', 'Int', v['wispSeed'])
    for name in ('wispDensity', 'wispClump', 'wispPscale', 'wispMin', 'wispMax'):
        _float(prim, name, float(v[name]))
    _float3(prim, 'wispDisplacement', _vec(v['wispDisplacement'], 3, 'Wisp displacement'))
    for prefix in ('wispNoise1', 'wispNoise2'):
        for suffix in ('Roughness', 'Octaves', 'Frequency', 'FrequencyJump'):
            name = prefix + suffix
            value = _number(v[name], name, 0, strict=suffix != 'Roughness')
            _float(prim, name, value)
        name = prefix + 'Translation'
        _float3(prim, name, _vec(v[name], 3, name))


def author_field(stage, v):
    _, _, UsdGeom, _, _ = _pxr()
    kind = v['kind']
    if kind not in dict(FIELD_KINDS):
        raise AuthoringError(f'Unsupported field type: {kind}')
    path = _path(v['path'], 'Prim path')
    # Validate these before mutating the stage.
    _cache_name(v['cache_name'])
    if v['color_mode'] != 'inherit':
        _color(v['color'])
    if kind == 'plane':
        _bounds(v)
        _normal(v['normal'])
    if kind == 'mesh' and not stage.GetPrimAtPath(path):
        raise AuthoringError(f'{path}: select an existing USD Mesh')
    prim = _existing_or_create(stage, path, bool(v['create']),
                               'Sphere' if kind == 'isf_sphere' else 'Xform')
    if _tag(prim, 'operation') is not None:
        raise AuthoringError(f'{path}: operation prims cannot also be field prims')
    old_isf, old_type = _tag(prim, 'isf_type'), _tag(prim, 'field_type')
    if kind == 'mesh':
        if not prim.IsA(UsdGeom.Mesh):
            raise AuthoringError(f'{path}: mesh mode requires a USD Mesh')
        if old_isf is not None or old_type is not None:
            raise AuthoringError(f'{path}: remove upstream field tags before using mesh mode')
        _float(prim, 'meshHalfWidth', _positive(v['meshHalfWidth'], 'Mesh half width'))
    elif kind.startswith('isf_'):
        if old_type is not None:
            raise AuthoringError(f'{path}: remove upstream pbvfx:field_type before assigning an ISF')
        if kind == 'isf_sphere' and not v['override_radius'] and _tag(prim, 'radius') is not None:
            raise AuthoringError(f'{path}: remove upstream pbvfx:radius or enable Override Sphere Radius')
        _attr(prim, 'isf_type', 'Token', kind)
        if (kind == 'isf_sphere' and v['override_radius']) or kind in ('isf_box', 'isf_cylinder'):
            _float(prim, 'radius', _positive(v['radius'], 'Radius'))
        if kind in ('isf_torus', 'isf_ellipse'):
            for name in ('majorRadius', 'minorRadius'):
                _float(prim, name, _positive(v[name], name))
        if kind in ('isf_cone', 'isf_cylinder'):
            _float(prim, 'height', _positive(v['height'], 'Height'))
        if kind in ('isf_torus', 'isf_cone', 'isf_ellipse', 'isf_cylinder'):
            _float3(prim, 'normal', _normal(v['normal']))
        if kind == 'isf_cone':
            theta = _positive(v['theta'], 'Cone angle')
            if theta >= math.pi / 2:
                raise AuthoringError('Cone angle must be less than pi/2 radians')
            _float(prim, 'theta', theta)
        if kind == 'isf_box':
            _attr(prim, 'roundingExponent', 'Int', _integer(v['roundingExponent'], 'Box exponent', 1))
    else:
        if old_isf is not None or (old_type is not None and str(old_type) != kind):
            raise AuthoringError(f'{path}: remove incompatible upstream field tags before assigning {kind}')
        _attr(prim, 'field_type', 'Token', kind)
        if kind == 'plane':
            _float3(prim, 'normal', _normal(v['normal']))
            _write_bounds(prim, v)
        else:
            _write_wisp(prim, v)
    _stamp(prim, v)
    _display_color(prim, v)
    from .preview import author_preview
    try:
        author_preview(stage, prim, kind, v)
    except ValueError as exc:
        raise AuthoringError(str(exc)) from exc
    return prim


def author_operation(stage, v):
    _, Sdf, UsdGeom, _, _ = _pxr()
    name = v['operation']
    if name not in dict(OPERATIONS):
        raise AuthoringError(f'Unsupported field operation: {name}')
    path = _path(v['path'], 'Operation path')
    targets = [_path(v['input_a'], 'Input A')]
    if name not in ('pyro', 'noise_warp'):
        targets.append(_path(v['input_b'], 'Input B'))
    if path in targets:
        raise AuthoringError('An operation cannot reference itself')
    for target in targets:
        source = stage.GetPrimAtPath(target)
        if not source:
            raise AuthoringError(f'{target}: input prim does not exist in the upstream stage')
        if not (source.IsA(UsdGeom.Mesh) or _tag(source, 'field_type') is not None or
                _tag(source, 'isf_type') is not None or _tag(source, 'operation') is not None):
            raise AuthoringError(f'{target}: input is not a PhysVFX field, operation, or USD Mesh')
    if name in ('add', 'subtract', 'noise_warp'):
        _bounds(v)
    _cache_name(v['cache_name'])
    if v['color_mode'] != 'inherit':
        _color(v['color'])
    if name in ('pyro', 'noise_warp'):
        for key in ('octaves', 'frequency', 'frequencyJump'):
            _positive(v[key], key)
        for key in ('roughness', 'amplitude'):
            _number(v[key], key, 0)
        _vec(v['noiseTranslation'], 3, 'Noise translation')
        if name == 'pyro':
            _positive(v['gamma'], 'Gamma')
            _integer(v['iterations'], 'Iterations', 1)
        else:
            _normal(v['normal'], 'Warp direction')
    prim = _existing_or_create(stage, path, True, 'Xform')
    if prim.IsA(UsdGeom.Gprim) or _tag(prim, 'field_type') is not None or _tag(prim, 'isf_type') is not None:
        raise AuthoringError(f'{path}: choose a separate Xform for the operation')
    _attr(prim, 'operation', 'Token', name)
    prim.CreateRelationship('pbvfx:inputs', custom=True).SetTargets(targets)
    if name in ('add', 'subtract', 'noise_warp'):
        _write_bounds(prim, v)
    if name in ('pyro', 'noise_warp'):
        for key in ('roughness', 'octaves', 'frequency', 'frequencyJump', 'amplitude'):
            _float(prim, key, float(v[key]))
        _float3(prim, 'noiseTranslation', _vec(v['noiseTranslation'], 3, 'Noise translation'))
        if name == 'pyro':
            _float(prim, 'gamma', float(v['gamma']))
            _attr(prim, 'iterations', 'Int', v['iterations'])
        else:
            _float3(prim, 'normal', _normal(v['normal'], 'Warp direction'))
    _stamp(prim, v)
    _display_color(prim, v)
    return prim


def author_camera(stage, v):
    Gf, _, UsdGeom, _, _ = _pxr()
    path = _path(v['path'], 'Camera path')
    focal = _positive(v['focal_length'], 'Focal length')
    horizontal = _positive(v['horizontal_aperture'], 'Horizontal aperture')
    vertical = _positive(v['vertical_aperture'], 'Vertical aperture')
    near, far = _vec(v['clipping'], 2, 'Clipping range')
    if near < 0 or far <= near:
        raise AuthoringError('Clipping requires 0 <= near < far')
    prim = _existing_or_create(stage, path, bool(v['create']), 'Camera')
    if not prim.IsA(UsdGeom.Camera):
        raise AuthoringError(f'{path}: expected a USD Camera')
    camera = UsdGeom.Camera(prim)
    camera.CreateProjectionAttr().Set(UsdGeom.Tokens.perspective)
    camera.CreateFocalLengthAttr().Set(focal)
    camera.CreateHorizontalApertureAttr().Set(horizontal)
    camera.CreateVerticalApertureAttr().Set(vertical)
    camera.CreateHorizontalApertureOffsetAttr().Set(0.0)
    camera.CreateVerticalApertureOffsetAttr().Set(0.0)
    camera.CreateClippingRangeAttr().Set(Gf.Vec2f(near, far))
    return prim


def author_light(stage, v):
    Gf, _, _, UsdLux, _ = _pxr()
    path = _path(v['path'], 'Light path')
    color = _color(v['color'])
    intensity = _number(v['intensity'], 'Intensity', 0)
    exposure = _number(v['exposure'], 'Exposure')
    if v['override_shadow']:
        _number(v['shadowKappa'], 'Shadow kappa', 0)
    prim = _existing_or_create(stage, path, bool(v['create']), 'SphereLight')
    if not prim.IsA(UsdLux.SphereLight):
        raise AuthoringError(f'{path}: expected a USD SphereLight')
    api = UsdLux.LightAPI(prim)
    api.CreateColorAttr().Set(Gf.Vec3f(*color))
    api.CreateIntensityAttr().Set(intensity)
    api.CreateExposureAttr().Set(exposure)
    if v['override_shadow']:
        _float(prim, 'shadowKappa', float(v['shadowKappa']))
    return prim


def author_settings(stage, v):
    Gf, _, UsdGeom, _, UsdRender = _pxr()
    path = _path(v['path'], 'Settings path')
    camera_path = _path(v['camera'], 'Camera path')
    camera = stage.GetPrimAtPath(camera_path)
    if not camera or not camera.IsA(UsdGeom.Camera):
        raise AuthoringError(f'{camera_path}: select an existing USD Camera')
    resolution = v['resolution']
    if not isinstance(resolution, (tuple, list)) or len(resolution) != 2:
        raise AuthoringError('Resolution requires width and height')
    width, height = (_integer(x, 'Resolution', 1) for x in resolution)
    for key in ('minStep', 'maxStep', 'shadowStep', 'densityVoxelSize', 'colorVoxelSize',
                'shadowVoxelSize', 'kappa'):
        _positive(v[key], key)
    _number(v['shadowKappa'], 'shadowKappa', 0)
    if float(v['maxStep']) < float(v['minStep']):
        raise AuthoringError('Maximum ray step must be at least the minimum')
    prim = _existing_or_create(stage, path, True, 'RenderSettings')
    if not prim.IsA(UsdRender.Settings):
        raise AuthoringError(f'{path}: expected a USD RenderSettings prim')
    settings = UsdRender.Settings(prim)
    settings.CreateCameraRel().SetTargets([camera_path])
    settings.CreateResolutionAttr().Set(Gf.Vec2i(width, height))
    settings.CreatePixelAspectRatioAttr().Set(1.0)
    for key in ('minStep', 'maxStep', 'shadowStep', 'densityVoxelSize', 'colorVoxelSize',
                'shadowVoxelSize', 'kappa', 'shadowKappa'):
        _float(prim, key, float(v[key]))
    return prim


AUTHORS = {'field': author_field, 'operation': author_operation, 'camera': author_camera,
           'light': author_light, 'settings': author_settings}


def author(stage, kind, values):
    if kind not in AUTHORS:
        raise AuthoringError(f'Unknown PhysVFX node type: {kind}')
    return AUTHORS[kind](stage, values)
