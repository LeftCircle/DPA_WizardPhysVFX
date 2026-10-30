"""Translate USD stages into PhysVFX settings, cameras, lights and fields."""
import argparse
import math
import os
import re
import tempfile
from dataclasses import dataclass
from pathlib import Path

from pxr import Gf, Usd, UsdGeom, UsdLux, UsdRender
import physics_vfx as fx


def _vec(value, scale=1.0):
    result = tuple(float(x) * scale for x in value)
    if not all(math.isfinite(x) for x in result):
        raise ValueError('Scene vectors must have finite components')
    return result


def _fxvec(value):
    return fx.Vector(*value)


def _dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def _length(v):
    return math.sqrt(_dot(v, v))


def _basis(matrix, path, uniform=False):
    axes = [_vec(matrix.TransformDir(Gf.Vec3d(*axis))) for axis in
            ((1, 0, 0), (0, 1, 0), (0, 0, 1))]
    lengths = [_length(axis) for axis in axes]
    if not all(math.isfinite(x) and x > 1e-10 for x in lengths):
        raise ValueError(f'{path}: singular or non-finite transform')
    if any(abs(_dot(axes[i], axes[j])) > 1e-5 * lengths[i] * lengths[j]
           for i in range(3) for j in range(i)):
        raise ValueError(f'{path}: shear is not supported')
    if uniform and max(lengths) - min(lengths) > 1e-5 * max(lengths):
        raise ValueError(f'{path}: non-uniform scale is not supported for spheres')
    return [tuple(x / length for x in axis) for axis, length in zip(axes, lengths)], lengths


def _positive(value, label, allow_zero=False):
    value = float(value)
    if not math.isfinite(value) or (value < 0 if allow_zero else value <= 0):
        raise ValueError(f"{label} must be {'non-negative' if allow_zero else 'positive'} and finite")
    return value


def _custom(prim, name, default):
    attr = prim.GetAttribute('pbvfx:' + name)
    if not attr:
        return default
    value = attr.Get()
    if value is None:
        raise ValueError(f'{prim.GetPath()}: pbvfx:{name} has no value')
    return value


def _one(items, description):
    items = list(items)
    if len(items) != 1:
        raise ValueError(f'Expected exactly one {description}, found {len(items)}')
    return items[0]


def _unit_scale(stage, volume_meters_per_unit):
    return _positive(UsdGeom.GetStageMetersPerUnit(stage), 'metersPerUnit') / _positive(
        volume_meters_per_unit, 'volume_meters_per_unit')


def _settings_prim(stage):
    return _one((p for p in stage.Traverse() if p.IsA(UsdRender.Settings)), 'RenderSettings')


@dataclass
class CameraInfo:
    native: object
    eye: tuple
    view: tuple
    up: tuple
    horizontal_fov: float
    near: float
    far: float


def get_camera_info(stage, volume_meters_per_unit=1.0):
    """Construct a camera and its clip distances from the settings' camera relationship."""
    scale = _unit_scale(stage, volume_meters_per_unit)
    settings = UsdRender.Settings(_settings_prim(stage))
    path = _one(settings.GetCameraRel().GetTargets(), 'camera relationship target')
    prim = stage.GetPrimAtPath(path)
    if not prim or not prim.IsA(UsdGeom.Camera):
        raise ValueError(f'{path}: camera relationship must target a Camera')
    camera = UsdGeom.Camera(prim)
    if str(camera.GetProjectionAttr().Get()) != 'perspective':
        raise ValueError('Only perspective cameras are supported')
    for attr in (camera.GetHorizontalApertureOffsetAttr(), camera.GetVerticalApertureOffsetAttr()):
        if abs(float(attr.Get())) > 1e-8:
            raise ValueError('Camera aperture offsets are not supported')
    focal = _positive(camera.GetFocalLengthAttr().Get(), 'focal length')
    horizontal = _positive(camera.GetHorizontalApertureAttr().Get(), 'horizontal aperture')
    vertical = _positive(camera.GetVerticalApertureAttr().Get(), 'vertical aperture')
    resolution = settings.GetResolutionAttr().Get()
    if not resolution or resolution[0] <= 0 or resolution[1] <= 0:
        raise ValueError('Render resolution must be positive')
    if not math.isclose(float(settings.GetPixelAspectRatioAttr().Get()), 1.0, rel_tol=1e-5):
        raise ValueError('Only square pixels are supported')
    aspect = int(resolution[0]) / int(resolution[1])
    if not math.isclose(horizontal / vertical, aspect, rel_tol=1e-4):
        raise ValueError('Camera aperture and image aspect ratios must match')
    fov = math.degrees(2 * math.atan(horizontal / (2 * focal)))
    matrix = UsdGeom.XformCache(Usd.TimeCode.Default()).GetLocalToWorldTransform(prim)
    basis, _ = _basis(matrix, prim.GetPath())
    eye, up = _vec(matrix.ExtractTranslation(), scale), basis[1]
    view = tuple(-x for x in basis[2])
    near, far = camera.GetClippingRangeAttr().Get()
    near = _positive(near, 'near clip', allow_zero=True) * scale
    far = _positive(far, 'far clip') * scale
    if far <= near:
        raise ValueError('Far clip must be greater than near clip')
    native = fx.Camera()
    native.set_eye_view_up(_fxvec(eye), _fxvec(view), _fxvec(up))
    native.set_fov(fov)
    native.set_aspect_ratio(aspect)
    return CameraInfo(native, eye, view, up, fov, near, far)


def get_camera(stage, volume_meters_per_unit=1.0):
    """Return a native fx.Camera from the stage."""
    return get_camera_info(stage, volume_meters_per_unit).native


def get_render_settings(stage, volume_meters_per_unit=1.0, camera_info=None):
    """Build fx.RenderSettings from a stage and its render camera."""
    scale = _unit_scale(stage, volume_meters_per_unit)
    prim = _settings_prim(stage)
    settings = UsdRender.Settings(prim)
    camera = camera_info or get_camera_info(stage, volume_meters_per_unit)
    width, height = settings.GetResolutionAttr().Get()
    rs = fx.RenderSettings()
    rs.width, rs.height = int(width), int(height)
    rs.near_dist, rs.far_dist = camera.near, camera.far
    for name, attr in (('minStep', 'min_step'), ('maxStep', 'max_step'),
                       ('shadowStep', 'shadow_step'), ('densityVoxelSize', 'density_voxel_size'),
                       ('colorVoxelSize', 'color_voxel_size'), ('shadowVoxelSize', 'shadow_voxel_size')):
        value = _custom(prim, name, None)
        if value is not None:
            setattr(rs, attr, _positive(value, 'pbvfx:' + name) * scale)
    rs.kappa = _positive(_custom(prim, 'kappa', rs.kappa), 'pbvfx:kappa')
    rs.shadow_kappa = _positive(_custom(prim, 'shadowKappa', rs.shadow_kappa), 'pbvfx:shadowKappa', allow_zero=True)
    if rs.max_step < rs.min_step:
        raise ValueError('pbvfx:maxStep must be at least pbvfx:minStep')
    return rs


def get_lights(stage, settings=None, volume_meters_per_unit=1.0):
    """Construct one fx.PointLight per USD SphereLight."""
    scale = _unit_scale(stage, volume_meters_per_unit)
    settings = settings or get_render_settings(stage, volume_meters_per_unit)
    cache = UsdGeom.XformCache(Usd.TimeCode.Default())
    lights = []
    for prim in stage.Traverse():
        if not prim.IsA(UsdLux.SphereLight):
            continue
        api = UsdLux.LightAPI(prim)
        if api.GetNormalizeAttr().Get() or api.GetEnableColorTemperatureAttr().Get():
            raise ValueError('Light normalization and color temperature are not supported')
        color = _vec(api.GetColorAttr().Get())
        if not all(c >= 0 for c in color):
            raise ValueError('Light color must be finite and non-negative')
        intensity = _positive(api.GetIntensityAttr().Get(), 'light intensity', allow_zero=True)
        exposure = float(api.GetExposureAttr().Get())
        if not math.isfinite(exposure):
            raise ValueError('Light exposure must be finite')
        try:
            rgb = tuple(c * intensity * 2.0 ** exposure for c in color)
        except OverflowError as exc:
            raise ValueError('Light exposure is too large') from exc
        if not all(math.isfinite(c) for c in rgb):
            raise ValueError('Light color and intensity are too large')
        pos = _vec(cache.GetLocalToWorldTransform(prim).ExtractTranslation(), scale)
        kappa = _positive(_custom(prim, 'shadowKappa', settings.shadow_kappa),
                          'light pbvfx:shadowKappa', allow_zero=True)
        lights.append(fx.PointLight(fx.Color(*rgb, 0), _fxvec(pos), kappa))
    return lights


@dataclass
class SceneObject:
    path: str
    kind: str
    field: object
    color: object
    rgb: tuple
    center: tuple = None
    radius: float = None
    bounds_min: tuple = None
    bounds_max: tuple = None
    stamped: bool = False
    is_density: bool = False
    cache_hit: bool = False


def _display_color(prim):
    # Constant displayColor is also readable by common USD viewers.
    attr = prim.GetAttribute('primvars:displayColor')
    values = attr.Get() if attr else None
    rgb = _vec(values[0]) if values is not None and len(values) == 1 else None
    if values is not None and len(values) != 1:
        raise ValueError(f'{prim.GetPath()}: only a single constant displayColor is supported')
    if rgb is None:
        rgb = _vec(_custom(prim, 'color', (1, 1, 1)))
    if not all(x >= 0 for x in rgb):
        raise ValueError(f'{prim.GetPath()}: color must be non-negative')
    return rgb


def _normal(prim):
    v = _vec(_custom(prim, 'normal', (0, 1, 0)))
    length = _length(v)
    if length <= 1e-10:
        raise ValueError(f'{prim.GetPath()}: normal must be nonzero')
    return _fxvec(tuple(x / length for x in v))


def _world_to_local(field, matrix, scale, path):
    determinant = float(matrix.GetDeterminant())
    if not math.isfinite(determinant) or abs(determinant) < 1e-12:
        raise ValueError(f'{path}: singular or non-finite transform')
    inverse = matrix.GetInverse()
    origin = _vec(inverse.Transform(Gf.Vec3d(0, 0, 0)))
    axes = []
    for axis in ((1, 0, 0), (0, 1, 0), (0, 0, 1)):
        p = _vec(inverse.Transform(Gf.Vec3d(*(x / scale for x in axis))))
        axes.append(tuple(p[i] - origin[i] for i in range(3)))
    return fx.affine_transform_field(field, _fxvec(origin), *(_fxvec(x) for x in axes))


ISF_TYPES = ('isf_sphere', 'isf_torus', 'isf_cone', 'isf_box', 'isf_icosahedron',
             'isf_steiner_patch', 'isf_ellipse', 'isf_cylinder')


def _transformed_bounds(matrix, half_extent, unit_scale):
    """Conservatively transform a local box into world-space bounds."""
    corners = []
    for x in (-half_extent, half_extent):
        for y in (-half_extent, half_extent):
            for z in (-half_extent, half_extent):
                corners.append(_vec(matrix.Transform(Gf.Vec3d(x, y, z)), unit_scale))
    return (tuple(min(p[i] for p in corners) for i in range(3)),
            tuple(max(p[i] for p in corners) for i in range(3)))


def get_isf_object(prim, cache, unit_scale):
    """Convert a prim tagged with pbvfx:isf_type to an analytic field."""
    kind = _custom(prim, 'isf_type', None)
    if kind is None:
        return None
    kind = str(kind)
    if kind not in ISF_TYPES:
        raise ValueError(f'{prim.GetPath()}: unknown pbvfx:isf_type {kind!r}')
    origin, normal = fx.Vector(0, 0, 0), _normal(prim)
    radius = _positive(_custom(prim, 'radius', 1.0), 'pbvfx:radius')
    major = _positive(_custom(prim, 'majorRadius', 1.0), 'pbvfx:majorRadius')
    minor = _positive(_custom(prim, 'minorRadius', 0.4), 'pbvfx:minorRadius')
    height = _positive(_custom(prim, 'height', 2.0), 'pbvfx:height')
    matrix = cache.GetLocalToWorldTransform(prim)
    center = radius_world = None
    if kind == 'isf_sphere':
        if prim.IsA(UsdGeom.Sphere) and not prim.GetAttribute('pbvfx:radius'):
            radius = _positive(UsdGeom.Sphere(prim).GetRadiusAttr().Get(), 'sphere radius')
        _, lengths = _basis(matrix, prim.GetPath(), uniform=True)
        center = _vec(matrix.ExtractTranslation(), unit_scale)
        radius_world = radius * lengths[0] * unit_scale
        field = fx.isf_sphere(_fxvec(center), radius_world)
        bounds_min = tuple(v - radius_world for v in center)
        bounds_max = tuple(v + radius_world for v in center)
    else:
        if kind == 'isf_torus':
            field = fx.isf_torus(origin, major, minor, normal)
            extent = major + minor
        elif kind == 'isf_cone':
            theta = _positive(_custom(prim, 'theta', 0.5), 'pbvfx:theta')
            if theta >= math.pi / 2:
                raise ValueError('pbvfx:theta must be less than pi/2 radians')
            field = fx.isf_cone(origin, normal, height, theta)
            extent = height * (1.0 + math.tan(theta))
        elif kind == 'isf_box':
            exponent = int(_custom(prim, 'roundingExponent', 3))
            if exponent <= 0:
                raise ValueError('pbvfx:roundingExponent must be positive')
            field = fx.isf_box(origin, radius, exponent)
            extent = radius
        elif kind == 'isf_icosahedron':
            field = fx.isf_icosahedron(origin)
            extent = math.pi * 1.8
        elif kind == 'isf_steiner_patch':
            field = fx.isf_steiner_patch(origin)
            extent = 1.0
        elif kind == 'isf_ellipse':
            field = fx.isf_ellipse(origin, normal, major, minor)
            extent = max(major, minor)
        else:
            field = fx.isf_cylinder(origin, normal, radius, height)
            extent = radius + height / 2.0
        field = _world_to_local(field, matrix, unit_scale, prim.GetPath())
        bounds_min, bounds_max = _transformed_bounds(matrix, extent, unit_scale)
    rgb = _display_color(prim)
    return SceneObject(str(prim.GetPath()), kind, field, fx.Color(*rgb, 0), rgb,
                       center, radius_world, bounds_min, bounds_max)


class StampCache:
    """Named VDB caches; names are explicit keys and are not auto-invalidated."""
    def __init__(self, directory):
        self.directory = Path(directory)
        self.owners = {}

    def path_for(self, prim):
        name = _custom(prim, 'cacheName', None)
        if name is None or name == '':
            return None
        if not isinstance(name, str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', name):
            raise ValueError(f'{prim.GetPath()}: invalid pbvfx:cacheName')
        path = str(prim.GetPath())
        if name in self.owners and self.owners[name] != path:
            raise ValueError(f'{path}: duplicate pbvfx:cacheName {name!r}')
        self.owners[name] = path
        return self.directory / (name + '.vdb')

    def load(self, prim):
        path = self.path_for(prim)
        return fx.load_float_grid(str(path)) if path is not None and path.exists() else None

    def save(self, prim, grid):
        path = self.path_for(prim)
        if path is None:
            return
        self.directory.mkdir(parents=True, exist_ok=True)
        fd, temp = tempfile.mkstemp(prefix='.stamp-', suffix='.vdb', dir=self.directory)
        os.close(fd)
        try:
            fx.save_to_disk(grid, temp)
            os.replace(temp, path)
        finally:
            if os.path.exists(temp):
                os.unlink(temp)


def _include_grid_bounds(item, grid):
    bounds = grid.active_world_bounds()
    if bounds is not None:
        pad = grid.voxel_size
        item.bounds_min = tuple(min(item.bounds_min[i], bounds[0][i] - pad) for i in range(3))
        item.bounds_max = tuple(max(item.bounds_max[i], bounds[1][i] + pad) for i in range(3))


def get_mesh_object(prim, settings, volume_meters_per_unit=1.0, stamp_cache=None):
    """Convert a USD Mesh to a grid-backed field, optionally reusing a cache."""
    from usd_mesh import mesh_prim_to_field, mesh_world_bounds
    half_width = _positive(_custom(prim, 'meshHalfWidth', 3.0), 'pbvfx:meshHalfWidth')
    name = _custom(prim, 'cacheName', None)
    grid = stamp_cache.load(prim) if stamp_cache and name else None
    hit = grid is not None
    if hit:
        field = fx.make_grid_field(grid)
    elif stamp_cache and name:
        from usd_mesh import mesh_prim_to_grid
        grid = mesh_prim_to_grid(prim, settings.density_voxel_size, half_width, volume_meters_per_unit)
        field = fx.make_grid_field(grid)
        if not _custom(prim, 'stamp', False):
            stamp_cache.save(prim, grid)
    else:
        field = mesh_prim_to_field(prim, settings.density_voxel_size, half_width, volume_meters_per_unit)
    rgb = _display_color(prim)
    bounds_min, bounds_max = mesh_world_bounds(prim, volume_meters_per_unit)
    item = SceneObject(str(prim.GetPath()), 'mesh', field, fx.Color(*rgb, 0), rgb,
                       bounds_min=bounds_min, bounds_max=bounds_max, cache_hit=hit)
    if hit:
        _include_grid_bounds(item, grid)
    return item


def _bounds(prim, scale):
    lo, hi = _custom(prim, 'boundsMin', None), _custom(prim, 'boundsMax', None)
    if lo is None or hi is None:
        raise ValueError(f'{prim.GetPath()}: explicit pbvfx:boundsMin and pbvfx:boundsMax are required')
    lo, hi = _vec(lo, scale), _vec(hi, scale)
    if any(a >= b for a, b in zip(lo, hi)):
        raise ValueError(f'{prim.GetPath()}: invalid operation bounds')
    return lo, hi


def _clip_field(field, lo, hi):
    for axis in range(3):
        for value, sign in ((lo[axis], -1), (hi[axis], 1)):
            point = [0.0, 0.0, 0.0]
            normal = [0.0, 0.0, 0.0]
            point[axis], normal[axis] = value, sign
            field = fx.intersection(field, fx.make_plane(_fxvec(point), _fxvec(normal)))
    return field


def get_plane_object(prim, cache, scale):
    """Create a transformed plane clipped to authored world-space bounds."""
    lo, hi = _bounds(prim, scale)
    matrix = cache.GetLocalToWorldTransform(prim)
    plane = fx.make_plane(fx.Vector(0, 0, 0), _normal(prim))
    field = _clip_field(_world_to_local(plane, matrix, scale, prim.GetPath()), lo, hi)
    rgb = _display_color(prim)
    return SceneObject(str(prim.GetPath()), 'plane', field, fx.Color(*rgb, 0), rgb,
                       bounds_min=lo, bounds_max=hi)


def get_wisp_object(prim, cache, settings, unit_scale, stamp_cache=None):
    """Create a seeded wisp density grid in local coordinates."""
    if _custom(prim, 'field_type', None) != 'wisp':
        return None
    dots = int(_custom(prim, 'wispDots', 100000))
    if dots <= 0 or dots > 2147483647:
        raise ValueError(f'{prim.GetPath()}: pbvfx:wispDots must be between 1 and 2147483647')
    density = _positive(_custom(prim, 'wispDensity', 1.0), 'pbvfx:wispDensity')
    clump = _positive(_custom(prim, 'wispClump', 1.0), 'pbvfx:wispClump')
    pscale = _positive(_custom(prim, 'wispPscale', 1.0), 'pbvfx:wispPscale')
    minimum = float(_custom(prim, 'wispMin', -1.0))
    maximum = float(_custom(prim, 'wispMax', 1.0))
    if not math.isfinite(minimum) or not math.isfinite(maximum) or minimum >= maximum:
        raise ValueError(f'{prim.GetPath()}: invalid wisp sampling range')
    def noise(prefix):
        return fx.FSVals(_positive(_custom(prim, prefix + 'Roughness', 0.5), prefix + 'Roughness', allow_zero=True),
                         _positive(_custom(prim, prefix + 'Octaves', 2.0), prefix + 'Octaves'),
                         _fxvec(_vec(_custom(prim, prefix + 'Translation', (0, 0, 0)))),
                         _positive(_custom(prim, prefix + 'Frequency', 1.0), prefix + 'Frequency'),
                         _positive(_custom(prim, prefix + 'FrequencyJump', 1.0), prefix + 'FrequencyJump'))
    seed = int(_custom(prim, 'wispSeed', 1))
    displacement = _fxvec(_vec(_custom(prim, 'wispDisplacement', (1, 1, 1))))
    voxel = _positive(settings.density_voxel_size, 'density voxel size') / unit_scale
    grid = stamp_cache.load(prim) if stamp_cache else None
    hit = grid is not None
    if not hit:
        grid = fx.FloatGrid(voxel, 0.0)
        wisp = fx.wisp_uniform(dots, -density, clump, pscale, noise('wispNoise1'),
                               noise('wispNoise2'), displacement, fx.Vector(0, 0, 0),
                               fx.CoordinateSys(), seed, fx.Color(1, 1, 1, 1), minimum, maximum)
        wisp.stamp(grid)
        if stamp_cache:
            stamp_cache.save(prim, grid)
    else:
        voxel = grid.voxel_size
    bounds = grid.active_world_bounds()
    if bounds is None:
        raise ValueError(f'{prim.GetPath()}: wisp generated an empty grid')
    lo, hi = bounds
    matrix = cache.GetLocalToWorldTransform(prim)
    corners = [_vec(matrix.Transform(Gf.Vec3d(x, y, z)), unit_scale)
               for x in (lo[0] - voxel, hi[0] + voxel)
               for y in (lo[1] - voxel, hi[1] + voxel)
               for z in (lo[2] - voxel, hi[2] + voxel)]
    bounds_min = tuple(min(p[i] for p in corners) for i in range(3))
    bounds_max = tuple(max(p[i] for p in corners) for i in range(3))
    field = _world_to_local(fx.make_grid_field(grid), matrix, unit_scale, prim.GetPath())
    rgb = _display_color(prim)
    return SceneObject(str(prim.GetPath()), 'wisp', field, fx.Color(*rgb, 0), rgb,
                       bounds_min=bounds_min, bounds_max=bounds_max, stamped=True, is_density=True, cache_hit=hit)


def _prepare_object(prim, item, settings, stamp_cache=None):
    """Optionally sample an object field into its own world-space grid."""
    stamp = _custom(prim, 'stamp', False)
    if not isinstance(stamp, bool):
        raise ValueError(f'{prim.GetPath()}: pbvfx:stamp must be a bool')
    if stamp and not item.is_density:
        if item.cache_hit:
            item.stamped = True
            return item
        grid = stamp_cache.load(prim) if stamp_cache else None
        if grid is not None:
            item.field = fx.make_grid_field(grid)
            item.stamped = True
            item.cache_hit = True
            _include_grid_bounds(item, grid)
            return item
        voxel_size = _positive(settings.density_voxel_size, 'density voxel size')
        padding = 2.0 * voxel_size
        lo = tuple(x - padding for x in item.bounds_min)
        hi = tuple(x + padding for x in item.bounds_max)
        bounds = fx.world_space_to_bounds(_fxvec(lo), _fxvec(hi), voxel_size)
        grid = fx.stamp_grid_parallel(voxel_size, 3.0 * voxel_size, item.field, bounds)
        item.field = fx.make_grid_field(grid)
        item.stamped = True
        if stamp_cache:
            stamp_cache.save(prim, grid)
    return item


# Operation callbacks take (prim, input SceneObjects, stage-to-renderer unit scale).
# Applications may register additional scalar-field operations here. Custom
# operations must author explicit bounds with pbvfx:boundsMin/Max.
FIELD_OPERATIONS = {}


def _binary_operation(name):
    return lambda prim, inputs, scale: getattr(fx, name)(inputs[0].field, inputs[1].field)


def _noise_warp_operation(prim, inputs, scale):
    roughness = _positive(_custom(prim, 'roughness', 0.5), 'pbvfx:roughness', allow_zero=True)
    octaves = _positive(_custom(prim, 'octaves', 3.0), 'pbvfx:octaves')
    frequency = _positive(_custom(prim, 'frequency', 1.0), 'pbvfx:frequency') / scale
    jump = _positive(_custom(prim, 'frequencyJump', 2.0), 'pbvfx:frequencyJump')
    translation = _fxvec(_vec(_custom(prim, 'noiseTranslation', (0, 0, 0)), scale))
    amplitude = _positive(_custom(prim, 'amplitude', 0.5), 'pbvfx:amplitude', allow_zero=True) * scale
    direction = _normal(prim)
    noise = fx.make_fspn(roughness, octaves, translation, frequency, jump)
    return fx.noise_warp_field(inputs[0].field, noise, direction, amplitude)


def _pyro_operation(prim, inputs, scale):
    roughness = _positive(_custom(prim, 'roughness', 0.5), 'pbvfx:roughness', allow_zero=True)
    octaves = _positive(_custom(prim, 'octaves', 3.0), 'pbvfx:octaves')
    frequency = _positive(_custom(prim, 'frequency', 1.0), 'pbvfx:frequency')
    jump = _positive(_custom(prim, 'frequencyJump', 2.0), 'pbvfx:frequencyJump')
    amplitude = float(_custom(prim, 'amplitude', 0.25))
    if not math.isfinite(amplitude):
        raise ValueError('pbvfx:amplitude must be finite')
    amplitude *= scale
    gamma = _positive(_custom(prim, 'gamma', 1.0), 'pbvfx:gamma')
    iters = int(_custom(prim, 'iterations', 1))
    if iters < 1:
        raise ValueError(f'{prim.GetPath()}: pbvfx:iterations must be positive')
    translation = _fxvec(_vec(_custom(prim, 'noiseTranslation', (0, 0, 0)), scale))
    return fx.make_pyro(inputs[0].field, roughness, octaves, translation,
                        frequency, jump, amplitude, gamma, iters)


# Entries are (input count, callable, bounds policy).
# 'inputs' is safe for CSG operations; pyro uses separate bounds handling;
# 'explicit' requires bounds supplied by the USD author.
FIELD_OPERATIONS.update({
    'union': (2, _binary_operation('union_fields'), 'inputs'),
    'intersection': (2, _binary_operation('intersection'), 'inputs'),
    'cutout': (2, _binary_operation('cutout'), 'inputs'),
    'pyro': (1, _pyro_operation, 'pyro'),
    'noise_warp': (1, _noise_warp_operation, 'explicit'),
    'add': (2, _binary_operation('add'), 'explicit'),
    'subtract': (2, _binary_operation('subtract'), 'explicit'),
})


def get_scene_objects(stage, settings, volume_meters_per_unit=1.0, cache_dir="cache"):
    """Resolve geometry and operation relationships; return unconsumed roots."""
    scale = _unit_scale(stage, volume_meters_per_unit)
    cache = UsdGeom.XformCache(Usd.TimeCode.Default())
    stamp_cache = StampCache(cache_dir)
    # Houdini viewport guides are geometry but are not renderer scene objects.
    prims = [p for p in stage.Traverse() if not _custom(p, 'preview', False)]
    operations = {}
    objects = {}
    # Reject ambiguous names before creating any cache files.
    for prim in prims:
        if (_custom(prim, 'field_type', None) == 'wisp' or
            prim.IsA(UsdGeom.Mesh) or _custom(prim, 'stamp', False) is True):
            stamp_cache.path_for(prim)
    for prim in prims:
        path = str(prim.GetPath())
        op = _custom(prim, 'operation', None)
        if op is not None:
            operations[path] = (prim, str(op))
            continue
        field_type = _custom(prim, 'field_type', None)
        if field_type is not None and field_type not in ('wisp', 'plane'):
            raise ValueError(f'{path}: unknown pbvfx:field_type {field_type!r}')
        if field_type and _custom(prim, 'isf_type', None) is not None:
            raise ValueError(f'{path}: choose either pbvfx:field_type or pbvfx:isf_type')
        if field_type == 'wisp':
            item = get_wisp_object(prim, cache, settings, scale, stamp_cache)
        elif field_type == 'plane':
            item = get_plane_object(prim, cache, scale)
        else:
            item = get_isf_object(prim, cache, scale)
        if item is None and prim.IsA(UsdGeom.Mesh):
            item = get_mesh_object(prim, settings, volume_meters_per_unit, stamp_cache)
        elif item is None and prim.IsA(UsdGeom.Gprim):
            raise ValueError(f'{path}: untagged geometry must be a USD Mesh')
        if item is not None:
            objects[path] = _prepare_object(prim, item, settings, stamp_cache)

    consumed, active = set(), set()

    def resolve(path):
        if path in objects:
            return objects[path]
        if path in active:
            raise ValueError(f'{path}: cyclic field operation')
        if path not in operations:
            raise ValueError(f'{path}: field operation input does not reference a field prim')
        prim, name = operations[path]
        if name not in FIELD_OPERATIONS:
            raise ValueError(f'{path}: unknown field operation {name!r}')
        count, callback, policy = FIELD_OPERATIONS[name]
        rel = prim.GetRelationship('pbvfx:inputs')
        targets = [str(target) for target in rel.GetTargets()] if rel else []
        if len(targets) != count:
            raise ValueError(f'{path}: {name} requires {count} input(s)')
        active.add(path)
        inputs = [resolve(target) for target in targets]
        active.remove(path)
        consumed.update(targets)
        field = callback(prim, inputs, scale)
        if policy == 'explicit':
            lo, hi = _bounds(prim, scale)
            field = _clip_field(field, lo, hi)
        elif policy == 'pyro':
            if _custom(prim, 'usePyroBounds', False):
                lo, hi = _bounds(prim, scale)
                field = _clip_field(field, lo, hi)
            else:
                # An estimate, not a mathematical bound for arbitrary fields.
                amplitude = float(_custom(prim, 'amplitude', 0.25)) * scale
                padding = 4.0 * max(amplitude, 0.0)
                lo = tuple(x - padding for x in inputs[0].bounds_min)
                hi = tuple(x + padding for x in inputs[0].bounds_max)
        elif policy == 'inputs':
            lo = tuple(min(obj.bounds_min[i] for obj in inputs) for i in range(3))
            hi = tuple(max(obj.bounds_max[i] for obj in inputs) for i in range(3))
        else:
            raise ValueError(f'{path}: unknown operation bounds policy {policy!r}')
        rgb = _display_color(prim) if (prim.GetAttribute('primvars:displayColor') or
                                      prim.GetAttribute('pbvfx:color')) else inputs[0].rgb
        item = SceneObject(path, name, field, fx.Color(*rgb, 0), rgb,
                           bounds_min=lo, bounds_max=hi)
        objects[path] = _prepare_object(prim, item, settings, stamp_cache)
        return objects[path]

    for path in operations:
        resolve(path)
    return [objects[str(p.GetPath())] for p in prims
            if str(p.GetPath()) in objects and str(p.GetPath()) not in consumed]


@dataclass
class Scene:
    settings: object
    camera_info: CameraInfo
    lights: list
    objects: list
    sdf: object
    density: object
    color: object
    bounds_min: tuple
    bounds_max: tuple

    @property
    def camera(self): return self.camera_info.native
    @property
    def eye(self): return self.camera_info.eye
    @property
    def view(self): return self.camera_info.view
    @property
    def up(self): return self.camera_info.up
    @property
    def horizontal_fov(self): return self.camera_info.horizontal_fov
    @property
    def field(self): return self.sdf
    @property
    def center(self): return self.objects[0].center if len(self.objects) == 1 else None
    @property
    def radius(self): return self.objects[0].radius if len(self.objects) == 1 else None
    @property
    def light_position(self):
        light = _one(self.lights, 'light')
        return (light.position.x, light.position.y, light.position.z)
    @property
    def light_color(self):
        light = _one(self.lights, 'light')
        return (light.color.red, light.color.green, light.color.blue)
    @property
    def light_shadow_kappa(self): return _one(self.lights, 'light').kappa

    def render(self, filename):
        fx.render_usd_scene(self.density, self.color, self.camera, self.lights,
                            self.settings, _fxvec(self.bounds_min), _fxvec(self.bounds_max), str(filename))


def load_scene(filename, volume_meters_per_unit=1.0, cache_dir="cache"):
    """Return PhysVFX components and combined fields for a USD stage."""
    stage = Usd.Stage.Open(str(filename))
    if not stage:
        raise ValueError(f'Could not open USD stage: {filename}')
    camera = get_camera_info(stage, volume_meters_per_unit)
    settings = get_render_settings(stage, volume_meters_per_unit, camera)
    lights = get_lights(stage, settings, volume_meters_per_unit)
    objects = get_scene_objects(stage, settings, volume_meters_per_unit, cache_dir)
    if not objects:
        raise ValueError('No tagged ISFs or USD meshes found')
    sdf = objects[0].field
    for obj in objects[1:]:
        sdf = fx.union_fields(sdf, obj.field)
    densities = [obj.field if obj.is_density else fx.negate(fx.mask(obj.field)) for obj in objects]
    density = densities[0]
    for field in densities[1:]:
        density = fx.union_fields(density, field)
    color = fx.make_scene_color_field([obj.field for obj in objects], [obj.color for obj in objects])
    padding = 3 * max(settings.density_voxel_size, settings.color_voxel_size, settings.shadow_voxel_size)
    bounds_min = tuple(min(obj.bounds_min[i] for obj in objects) - padding for i in range(3))
    bounds_max = tuple(max(obj.bounds_max[i] for obj in objects) + padding for i in range(3))
    return Scene(settings, camera, lights, objects, sdf, density, color, bounds_min, bounds_max)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('scene', help='USD scene (.usd, .usda or .usdc)')
    parser.add_argument('--output', required=True, help='Output image path, e.g. sphere.exr')
    parser.add_argument('--volume-meters-per-unit', type=float, default=1.0)
    parser.add_argument('--cache-dir', default='cache', help='Directory for named stamp caches (default: cache)')
    args = parser.parse_args(argv)
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        load_scene(args.scene, args.volume_meters_per_unit, args.cache_dir).render(output)
    except (ValueError, RuntimeError) as exc:
        parser.exit(1, f'error: {exc}\n')
    print(f'Wrote {output}')


if __name__ == '__main__':
    main()
