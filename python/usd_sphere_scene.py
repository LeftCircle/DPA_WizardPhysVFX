"""Load a single-sphere USD scene and send it to the PBVFX CPU renderer."""
import argparse
import math
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
    """Return orthogonal world basis vectors and reject shear/degeneracy."""
    axes = [_vec(matrix.TransformDir(Gf.Vec3d(*axis))) for axis in
            ((1, 0, 0), (0, 1, 0), (0, 0, 1))]
    lengths = [_length(axis) for axis in axes]
    if not all(math.isfinite(x) and x > 1e-10 for x in lengths):
        raise ValueError(f"{path}: singular or non-finite transform")
    if any(abs(_dot(axes[i], axes[j])) > 1e-5 * lengths[i] * lengths[j]
           for i in range(3) for j in range(i)):
        raise ValueError(f"{path}: shear is not supported")
    if uniform and max(lengths) - min(lengths) > 1e-5 * max(lengths):
        raise ValueError(f"{path}: non-uniform scale is not supported for spheres")
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
        raise ValueError(f"{prim.GetPath()}: pbvfx:{name} has no value")
    return value


def _one(items, description):
    items = list(items)
    if len(items) != 1:
        raise ValueError(f"Expected exactly one {description}, found {len(items)}")
    return items[0]


@dataclass
class SphereScene:
    field: object
    center: tuple
    radius: float
    eye: tuple
    view: tuple
    up: tuple
    horizontal_fov: float
    light_position: tuple
    light_color: tuple
    light_shadow_kappa: float
    settings: object

    def render(self, filename):
        fx.render_usd_sphere(
            self.field, _fxvec(self.center), self.radius,
            _fxvec(self.eye), _fxvec(self.view), _fxvec(self.up), self.horizontal_fov,
            _fxvec(self.light_position), self.light_color, self.light_shadow_kappa,
            self.settings, str(filename))


def load_scene(filename, volume_meters_per_unit=1.0):
    """Load a single-sphere scene; renderer coordinates are meters by default."""
    stage = Usd.Stage.Open(str(filename))
    if not stage:
        raise ValueError(f"Could not open USD stage: {filename}")
    unit_scale = _positive(UsdGeom.GetStageMetersPerUnit(stage), 'metersPerUnit') / _positive(
        volume_meters_per_unit, 'volume_meters_per_unit')
    cache = UsdGeom.XformCache(Usd.TimeCode.Default())
    prims = list(stage.Traverse())
    for prim in prims:
        if prim.IsA(UsdGeom.Gprim) and not prim.IsA(UsdGeom.Sphere):
            raise ValueError(f"{prim.GetPath()}: only Sphere geometry is supported")
    settings_prim = _one((p for p in prims if p.IsA(UsdRender.Settings)), 'RenderSettings')
    settings_schema = UsdRender.Settings(settings_prim)
    targets = settings_schema.GetCameraRel().GetTargets()
    camera_path = _one(targets, 'camera relationship target')
    camera_prim = stage.GetPrimAtPath(camera_path)
    if not camera_prim or not camera_prim.IsA(UsdGeom.Camera):
        raise ValueError(f"{camera_path}: camera relationship must target a Camera")
    sphere_prim = _one((p for p in prims if p.IsA(UsdGeom.Sphere)), 'Sphere')
    light_prim = _one((p for p in prims if p.IsA(UsdLux.SphereLight)), 'SphereLight')

    sphere_matrix = cache.GetLocalToWorldTransform(sphere_prim)
    _, lengths = _basis(sphere_matrix, sphere_prim.GetPath(), uniform=True)
    center = _vec(sphere_matrix.ExtractTranslation(), unit_scale)
    radius = _positive(UsdGeom.Sphere(sphere_prim).GetRadiusAttr().Get(), 'sphere radius') * lengths[0] * unit_scale
    field = fx.sphere(_fxvec(center), radius)

    camera = UsdGeom.Camera(camera_prim)
    if str(camera.GetProjectionAttr().Get()) != 'perspective':
        raise ValueError('Only perspective cameras are supported')
    for attr in (camera.GetHorizontalApertureOffsetAttr(), camera.GetVerticalApertureOffsetAttr()):
        if abs(float(attr.Get())) > 1e-8:
            raise ValueError('Camera aperture offsets are not supported')
    focal = _positive(camera.GetFocalLengthAttr().Get(), 'focal length')
    horizontal = _positive(camera.GetHorizontalApertureAttr().Get(), 'horizontal aperture')
    vertical = _positive(camera.GetVerticalApertureAttr().Get(), 'vertical aperture')
    fov = math.degrees(2.0 * math.atan(horizontal / (2.0 * focal)))
    camera_matrix = cache.GetLocalToWorldTransform(camera_prim)
    basis, _ = _basis(camera_matrix, camera_prim.GetPath())
    eye = _vec(camera_matrix.ExtractTranslation(), unit_scale)
    up = basis[1]
    view = tuple(-x for x in basis[2])

    resolution = settings_schema.GetResolutionAttr().Get()
    if not resolution or resolution[0] <= 0 or resolution[1] <= 0:
        raise ValueError('Render resolution must be positive')
    width, height = int(resolution[0]), int(resolution[1])
    pixel_aspect = float(settings_schema.GetPixelAspectRatioAttr().Get())
    if not math.isclose(pixel_aspect, 1.0, rel_tol=1e-5):
        raise ValueError('Only square pixels are supported')
    if not math.isclose(horizontal / vertical, width / height, rel_tol=1e-4):
        raise ValueError('Camera aperture and image aspect ratios must match')
    near, far = camera.GetClippingRangeAttr().Get()
    near = _positive(near, 'near clip', allow_zero=True) * unit_scale
    far = _positive(far, 'far clip') * unit_scale
    if far <= near:
        raise ValueError('Far clip must be greater than near clip')

    rs = fx.RenderSettings()
    rs.width, rs.height = width, height
    rs.near_dist, rs.far_dist = near, far
    # Defaults are specified in renderer units. Authored distance values are in stage units.
    for name, attr in (('minStep', 'min_step'), ('maxStep', 'max_step'),
                       ('shadowStep', 'shadow_step'), ('densityVoxelSize', 'density_voxel_size'),
                       ('colorVoxelSize', 'color_voxel_size'), ('shadowVoxelSize', 'shadow_voxel_size')):
        value = _custom(settings_prim, name, None)
        if value is not None:
            setattr(rs, attr, _positive(value, 'pbvfx:' + name) * unit_scale)
    rs.kappa = _positive(_custom(settings_prim, 'kappa', rs.kappa), 'pbvfx:kappa')
    rs.shadow_kappa = _positive(_custom(settings_prim, 'shadowKappa', rs.shadow_kappa), 'pbvfx:shadowKappa', allow_zero=True)
    if rs.max_step < rs.min_step:
        raise ValueError('pbvfx:maxStep must be at least pbvfx:minStep')

    light_api = UsdLux.LightAPI(light_prim)
    if light_api.GetNormalizeAttr().Get() or light_api.GetEnableColorTemperatureAttr().Get():
        raise ValueError('Light normalization and color temperature are not supported')
    color = _vec(light_api.GetColorAttr().Get())
    if not all(math.isfinite(c) and c >= 0 for c in color):
        raise ValueError('Light color must be finite and non-negative')
    intensity = _positive(light_api.GetIntensityAttr().Get(), 'light intensity', allow_zero=True)
    exposure = float(light_api.GetExposureAttr().Get())
    if not math.isfinite(exposure):
        raise ValueError('Light exposure must be finite')
    try:
        light_color = tuple(c * intensity * 2.0 ** exposure for c in color)
    except OverflowError as exc:
        raise ValueError('Light exposure is too large') from exc
    if not all(math.isfinite(c) for c in light_color):
        raise ValueError('Light color and intensity are too large')
    light_matrix = cache.GetLocalToWorldTransform(light_prim)
    light_position = _vec(light_matrix.ExtractTranslation(), unit_scale)
    shadow_kappa = _positive(_custom(light_prim, 'shadowKappa', rs.shadow_kappa),
                             'light pbvfx:shadowKappa', allow_zero=True)
    return SphereScene(field, center, radius, eye, view, up, fov, light_position,
                       light_color, shadow_kappa, rs)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('scene', help='USD scene (.usd, .usda or .usdc)')
    parser.add_argument('--output', required=True, help='Output image path, e.g. sphere.exr')
    parser.add_argument('--volume-meters-per-unit', type=float, default=1.0,
                        help='Meters per renderer unit (default: 1)')
    args = parser.parse_args(argv)
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        scene = load_scene(args.scene, args.volume_meters_per_unit)
        scene.render(output)
    except (ValueError, RuntimeError) as exc:
        parser.exit(1, f'error: {exc}\n')
    print(f'Wrote {output}')


if __name__ == '__main__':
    main()
