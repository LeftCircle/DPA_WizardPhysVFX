"""Viewport-only wireframe bounds for PhysVFX USD fields."""
import math

PREVIEW_CHILD = '__physvfx_bounds'


def _cube(extent):
    return (-extent, -extent, -extent), (extent, extent, extent)


def local_bounds(prim, kind, values):
    """Return conservative local bounds, except wisps which use an estimate."""
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
    if not values.get('show_preview', True):
        if proxy:
            UsdGeom.Imageable(proxy).CreateVisibilityAttr().Set(UsdGeom.Tokens.invisible)
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
