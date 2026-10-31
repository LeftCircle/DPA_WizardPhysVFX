"""Viewport-only wireframe bounds for PhysVFX USD fields."""
import math

PREVIEW_CHILD = '__physvfx_bounds'
SURFACE_CHILD = '__physvfx_surface'
CAMERA_CHILD = '__physvfx_camera_range'


def camera_range_lines(near, far, horizontal, vertical, focal, steps=12):
    """Camera-local lines on spherical ray-distance boundaries and their sides."""
    hx, hy = horizontal / (2.0 * focal), vertical / (2.0 * focal)
    def point(u, v, distance):
        x, y = u * hx, v * hy
        factor = distance / math.sqrt(x * x + y * y + 1.0)
        return (x * factor, y * factor, -factor)
    lines = []
    for radius in (near, far):
        if radius == 0:
            continue
        for t in (-1.0, 0.0, 1.0):
            for axis in (0, 1):
                for i in range(steps):
                    a, b = -1.0 + 2.0 * i / steps, -1.0 + 2.0 * (i + 1) / steps
                    lines.append((point(a, t, radius), point(b, t, radius)) if axis == 0
                                 else (point(t, a, radius), point(t, b, radius)))
    for u in (-1.0, 0.0, 1.0):
        for v in (-1.0, 0.0, 1.0):
            lines.append((point(u, v, near), point(u, v, far)))
    return lines


def author_camera_preview(stage, prim, values):
    """Author an excluded viewport guide under the camera."""
    from pxr import Gf, Sdf, UsdGeom
    path = str(prim.GetPath()) + '/' + CAMERA_CHILD
    proxy = stage.GetPrimAtPath(path)
    if proxy:
        marker = proxy.GetAttribute('pbvfx:preview')
        if not marker or marker.Get() is not True or not proxy.IsA(UsdGeom.BasisCurves):
            raise ValueError(f'{path}: reserved preview path is already in use')
    if not values.get('show_camera_preview', True):
        if proxy:
            UsdGeom.Imageable(proxy).CreateVisibilityAttr().Set(UsdGeom.Tokens.invisible)
        return
    if not proxy:
        proxy = stage.DefinePrim(path, 'BasisCurves')
    if not proxy:
        raise ValueError(f'{path}: could not create camera range preview')
    proxy.CreateAttribute('pbvfx:preview', Sdf.ValueTypeNames.Bool, custom=True).Set(True)
    near, far = values['clipping']
    lines = camera_range_lines(near, far, values['horizontal_aperture'],
                               values['vertical_aperture'], values['focal_length'])
    points = [point for line in lines for point in line]
    geom = UsdGeom.BasisCurves(proxy)
    geom.CreateTypeAttr().Set(UsdGeom.Tokens.linear)
    geom.CreateCurveVertexCountsAttr().Set([2] * len(lines))
    geom.CreatePointsAttr().Set([Gf.Vec3f(*p) for p in points])
    width = max(far * 0.0005, 0.0001)
    geom.CreateWidthsAttr().Set([width])
    geom.SetWidthsInterpolation(UsdGeom.Tokens.constant)
    geom.CreateExtentAttr().Set([Gf.Vec3f(*(min(p[i] for p in points) - width / 2 for i in range(3))),
                                 Gf.Vec3f(*(max(p[i] for p in points) + width / 2 for i in range(3)))])
    UsdGeom.PrimvarsAPI(proxy).CreatePrimvar(
        'displayColor', Sdf.ValueTypeNames.Color3fArray, UsdGeom.Tokens.constant
    ).Set([Gf.Vec3f(0.3, 1.0, 0.4)])
    UsdGeom.Imageable(proxy).CreateVisibilityAttr().Set(UsdGeom.Tokens.inherited)


def _cube(extent):
    return (-extent, -extent, -extent), (extent, extent, extent)


def local_bounds(prim, kind, values):
    """Return conservative local bounds, except wisps which use an estimate."""
    if kind == 'shadow_region':
        return values['bounds_min'], values['bounds_max']
    if kind == 'isf_sphere':
        from pxr import UsdGeom
        attr = prim.GetAttribute('radius') if prim.IsA(UsdGeom.Sphere) else None
        radius = values['radius'] if values['override_radius'] else (attr.Get() if attr and attr.Get() is not None else 1.0)
        return _cube(float(radius))
    if kind == 'isf_torus':
        return _cube(values['majorRadius'] + values['minorRadius'])
    if kind == 'isf_cone':
        return _cube(values['height'] * (1.0 + math.tan(values['theta'])))
    if kind == 'isf_box':
        return _cube(values['radius'])
    if kind == 'isf_icosahedron':
        return _cube(math.pi * 1.8)
    if kind == 'isf_steiner_patch':
        return _cube(1.0)
    if kind == 'isf_ellipse':
        return _cube(max(values['majorRadius'], values['minorRadius']))
    if kind == 'isf_cylinder':
        return _cube(values['radius'] + values['height'] / 2.0)
    if kind == 'wisp':
        radius = float(values['preview_wisp_radius'])
        if not math.isfinite(radius) or radius < 0:
            raise ValueError('Wisp preview half size must be finite and non-negative')
        if radius == 0:
            # This is a display estimate only. Actual wisp bounds come from the
            # generated density grid and can differ with noise and seed.
            radius = max(abs(values['wispMin']), abs(values['wispMax'])) * max(1.0, values['wispPscale']) + 2.0
        return _cube(radius)
    if kind == 'mesh':
        attr = prim.GetAttribute('points')
        points = attr.Get() if attr else None
        if points is None or len(points) == 0:
            raise ValueError('Mesh preview requires authored points; turn off Show Bounds Preview if needed')
        return (tuple(min(float(p[i]) for p in points) for i in range(3)),
                tuple(max(float(p[i]) for p in points) for i in range(3)))
    if kind == 'plane':
        from pxr import Gf, Usd, UsdGeom
        matrix = UsdGeom.XformCache(Usd.TimeCode.Default()).GetLocalToWorldTransform(prim)
        determinant = float(matrix.GetDeterminant())
        if not math.isfinite(determinant) or abs(determinant) < 1e-12:
            raise ValueError('Cannot preview plane bounds with a singular transform')
        inverse = matrix.GetInverse()
        lo, hi = values['bounds_min'], values['bounds_max']
        points = [inverse.Transform(Gf.Vec3d(x, y, z))
                  for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
        return (tuple(min(float(p[i]) for p in points) for i in range(3)),
                tuple(max(float(p[i]) for p in points) for i in range(3)))
    raise ValueError('Unsupported preview field type: ' + str(kind))


def author_preview(stage, prim, kind, values):
    """Create or hide a tagged USD BasisCurves child of the field prim."""
    from pxr import Gf, Sdf, UsdGeom
    path = str(prim.GetPath()) + '/' + PREVIEW_CHILD
    proxy = stage.GetPrimAtPath(path)
    if proxy:
        marker = proxy.GetAttribute('pbvfx:preview')
        if not marker or marker.Get() is not True or not proxy.IsA(UsdGeom.BasisCurves):
            raise ValueError(f'{path}: reserved preview path is already in use')
    surface_path = str(prim.GetPath()) + '/' + SURFACE_CHILD
    surface = stage.GetPrimAtPath(surface_path)
    if surface:
        marker = surface.GetAttribute('pbvfx:preview')
        if not marker or marker.Get() is not True or not surface.IsA(UsdGeom.Mesh):
            raise ValueError(f'{surface_path}: reserved preview path is already in use')
    if not values.get('show_preview', True):
        for part in (proxy, surface):
            if part:
                UsdGeom.Imageable(part).CreateVisibilityAttr().Set(UsdGeom.Tokens.invisible)
        return
    lo, hi = local_bounds(prim, kind, values)
    if not all(math.isfinite(x) for x in (*lo, *hi)):
        raise ValueError('Preview bounds must be finite')
    if not proxy:
        proxy = stage.DefinePrim(path, 'BasisCurves')
    if not proxy:
        raise ValueError(f'{path}: could not create preview; edit instance source or disable preview')
    proxy.CreateAttribute('pbvfx:preview', Sdf.ValueTypeNames.Bool, custom=True).Set(True)
    geom = UsdGeom.BasisCurves(proxy)
    geom.CreateTypeAttr().Set(UsdGeom.Tokens.linear)
    geom.CreateCurveVertexCountsAttr().Set([2] * 12)
    vertices = [(lo[0], lo[1], lo[2]), (hi[0], lo[1], lo[2]),
                (lo[0], hi[1], lo[2]), (hi[0], hi[1], lo[2]),
                (lo[0], lo[1], hi[2]), (hi[0], lo[1], hi[2]),
                (lo[0], hi[1], hi[2]), (hi[0], hi[1], hi[2])]
    edges = ((0, 1), (2, 3), (4, 5), (6, 7),
             (0, 2), (1, 3), (4, 6), (5, 7),
             (0, 4), (1, 5), (2, 6), (3, 7))
    geom.CreatePointsAttr().Set([Gf.Vec3f(*vertices[i]) for edge in edges for i in edge])
    width = max(max(hi[i] - lo[i] for i in range(3)) * 0.005, 0.002)
    geom.CreateWidthsAttr().Set([width])
    geom.SetWidthsInterpolation(UsdGeom.Tokens.constant)
    geom.CreateExtentAttr().Set([Gf.Vec3f(*(x - width / 2 for x in lo)),
                                 Gf.Vec3f(*(x + width / 2 for x in hi))])
    UsdGeom.PrimvarsAPI(proxy).CreatePrimvar(
        'displayColor', Sdf.ValueTypeNames.Color3fArray, UsdGeom.Tokens.constant
    ).Set([Gf.Vec3f(1.0, 0.65, 0.1)])
    UsdGeom.Imageable(proxy).CreateVisibilityAttr().Set(UsdGeom.Tokens.inherited)
    from .preview_surface import ISF_KINDS, polygonize
    # A native USD sphere already provides an exact viewport surface unless
    # PhysVFX's radius override differs from the native sphere radius.
    native_sphere = kind == 'isf_sphere' and prim.IsA(UsdGeom.Sphere) and not values['override_radius']
    if kind not in ISF_KINDS or native_sphere:
        if surface:
            UsdGeom.Imageable(surface).CreateVisibilityAttr().Set(UsdGeom.Tokens.invisible)
        return
    triangles = polygonize(kind, values, float(hi[0]), max(abs(x) for x in (*lo, *hi)))
    if not surface:
        surface = stage.DefinePrim(surface_path, 'Mesh')
    if not surface:
        raise ValueError(f'{surface_path}: could not create preview surface')
    surface.CreateAttribute('pbvfx:preview', Sdf.ValueTypeNames.Bool, custom=True).Set(True)
    mesh = UsdGeom.Mesh(surface)
    mesh.CreatePointsAttr().Set([Gf.Vec3f(*p) for p in triangles])
    mesh.CreateFaceVertexCountsAttr().Set([3] * (len(triangles)//3))
    mesh.CreateFaceVertexIndicesAttr().Set(list(range(len(triangles))))
    mesh.CreateSubdivisionSchemeAttr().Set('none')
    mesh.CreateDoubleSidedAttr().Set(True)
    if triangles:
        mesh.CreateExtentAttr().Set([
            Gf.Vec3f(*(min(p[i] for p in triangles) for i in range(3))),
            Gf.Vec3f(*(max(p[i] for p in triangles) for i in range(3)))])
    UsdGeom.PrimvarsAPI(surface).CreatePrimvar(
        'displayColor', Sdf.ValueTypeNames.Color3fArray, UsdGeom.Tokens.constant
    ).Set([Gf.Vec3f(0.2, 0.7, 1.0)])
    UsdGeom.Imageable(surface).CreateVisibilityAttr().Set(UsdGeom.Tokens.inherited)


def author_stamp_preview(stage, prim, values):
    """Display the world-axis-aligned stamp box using a local child guide."""
    from pxr import Gf, Sdf, Usd, UsdGeom
    path = str(prim.GetPath()) + '/__physvfx_stamp_bounds'
    proxy = stage.GetPrimAtPath(path)
    if proxy:
        marker = proxy.GetAttribute('pbvfx:preview')
        if not marker or marker.Get() is not True or not proxy.IsA(UsdGeom.BasisCurves):
            raise ValueError(f'{path}: reserved preview path is already in use')
    if not values.get('show_stamp_bounds', False):
        if proxy:
            UsdGeom.Imageable(proxy).CreateVisibilityAttr().Set(UsdGeom.Tokens.invisible)
        return
    lo = values.get('stamp_bounds_min', (-2, -2, -2))
    hi = values.get('stamp_bounds_max', (2, 2, 2))
    matrix = UsdGeom.XformCache(Usd.TimeCode.Default()).GetLocalToWorldTransform(prim)
    det = float(matrix.GetDeterminant())
    if not math.isfinite(det) or abs(det) < 1e-12:
        raise ValueError('Cannot show stamp bounds with a singular transform')
    world = [matrix.Transform(Gf.Vec3d(x, y, z)) for x in (lo[0], hi[0])
             for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    wlo = tuple(min(p[i] for p in world) for i in range(3))
    whi = tuple(max(p[i] for p in world) for i in range(3))
    inverse = matrix.GetInverse()
    vertices = [inverse.Transform(Gf.Vec3d(x, y, z)) for z in (wlo[2], whi[2])
                for y in (wlo[1], whi[1]) for x in (wlo[0], whi[0])]
    edges = ((0,1),(2,3),(4,5),(6,7),(0,2),(1,3),(4,6),(5,7),
             (0,4),(1,5),(2,6),(3,7))
    if not proxy:
        proxy = stage.DefinePrim(path, 'BasisCurves')
    if not proxy:
        raise ValueError(f'{path}: could not create stamp preview')
    proxy.CreateAttribute('pbvfx:preview', Sdf.ValueTypeNames.Bool, custom=True).Set(True)
    geom = UsdGeom.BasisCurves(proxy)
    geom.CreateTypeAttr().Set(UsdGeom.Tokens.linear)
    geom.CreateCurveVertexCountsAttr().Set([2] * 12)
    geom.CreatePointsAttr().Set([Gf.Vec3f(*vertices[i]) for edge in edges for i in edge])
    width = max(max(hi[i] - lo[i] for i in range(3)) * 0.005, 0.002)
    geom.CreateWidthsAttr().Set([width])
    geom.SetWidthsInterpolation(UsdGeom.Tokens.constant)
    geom.CreateExtentAttr().Set([Gf.Vec3f(*(min(p[i] for p in vertices) - width for i in range(3))),
                                 Gf.Vec3f(*(max(p[i] for p in vertices) + width for i in range(3)))])
    UsdGeom.PrimvarsAPI(proxy).CreatePrimvar('displayColor', Sdf.ValueTypeNames.Color3fArray,
                                            UsdGeom.Tokens.constant).Set([Gf.Vec3f(1, 0, 0)])
    UsdGeom.Imageable(proxy).CreateVisibilityAttr().Set(UsdGeom.Tokens.inherited)

def author_shadow_region_preview(stage, prim, values):
    # The generic guide is a tagged local-space wireframe. The renderer uses
    # its enclosing world-axis-aligned box when the prim is rotated.
    author_preview(stage, prim, 'shadow_region', values)
