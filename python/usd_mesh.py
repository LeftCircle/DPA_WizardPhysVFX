"""Convert a USD polygon mesh to an OpenVDB level-set grid or scalar field."""
import math

from pxr import Gf, Usd, UsdGeom
import physics_vfx as fx


def _positive(value, name):
    value = float(value)
    if not math.isfinite(value) or value <= 0:
        raise ValueError(f'{name} must be positive and finite')
    return value


def mesh_world_bounds(prim, volume_meters_per_unit=1.0, time=None):
    """Return world-space bounds of the authored mesh points in renderer units."""
    if not prim or not prim.IsA(UsdGeom.Mesh):
        raise ValueError('Expected a USD Mesh prim')
    scale = _positive(UsdGeom.GetStageMetersPerUnit(prim.GetStage()), 'stage metersPerUnit') / _positive(
        volume_meters_per_unit, 'volume_meters_per_unit')
    timecode = Usd.TimeCode.Default() if time is None else Usd.TimeCode(float(time))
    points = UsdGeom.Mesh(prim).GetPointsAttr().Get(timecode)
    if points is None or not len(points):
        raise ValueError(f'{prim.GetPath()}: mesh must have points')
    matrix = UsdGeom.XformCache(timecode).GetLocalToWorldTransform(prim)
    world_points = []
    for p in points:
        transformed = matrix.Transform(Gf.Vec3d(float(p[0]), float(p[1]), float(p[2])))
        point = tuple(float(v) * scale for v in transformed)
        if not all(math.isfinite(v) for v in point):
            raise ValueError(f'{prim.GetPath()}: transformed points must be finite')
        world_points.append(point)
    return (tuple(min(p[i] for p in world_points) for i in range(3)),
            tuple(max(p[i] for p in world_points) for i in range(3)))


def mesh_prim_to_grid(prim, voxel_size, half_width=3.0, volume_meters_per_unit=1.0, time=None):
    """Voxelize a USD Mesh prim in renderer world space.

    voxel_size is in renderer units; half_width is in voxels. The default
    renderer unit is one meter. The mesh must be a polygonal triangle/quad
    mesh; OpenVDB requires a closed surface for a meaningful signed distance.
    """
    if not prim or not prim.IsA(UsdGeom.Mesh):
        raise ValueError('Expected a USD Mesh prim')
    voxel_size = _positive(voxel_size, 'voxel_size')
    half_width = _positive(half_width, 'half_width')
    unit_scale = _positive(UsdGeom.GetStageMetersPerUnit(prim.GetStage()), 'stage metersPerUnit') / _positive(
        volume_meters_per_unit, 'volume_meters_per_unit')
    timecode = Usd.TimeCode.Default() if time is None else Usd.TimeCode(float(time))
    mesh = UsdGeom.Mesh(prim)
    if str(mesh.GetSubdivisionSchemeAttr().Get()) != 'none':
        raise ValueError(f'{prim.GetPath()}: subdivision meshes are not supported; export a polygon mesh with subdivisionScheme = none')
    holes = mesh.GetHoleIndicesAttr().Get(timecode)
    if holes is not None and len(holes):
        raise ValueError(f'{prim.GetPath()}: mesh holes are not supported for signed-distance conversion')
    points = mesh.GetPointsAttr().Get(timecode)
    counts = mesh.GetFaceVertexCountsAttr().Get(timecode)
    indices = mesh.GetFaceVertexIndicesAttr().Get(timecode)
    if points is None or counts is None or indices is None or len(points) == 0 or len(counts) == 0:
        raise ValueError(f'{prim.GetPath()}: mesh must have points and face topology')
    if any(int(n) not in (3, 4) for n in counts):
        raise ValueError(f'{prim.GetPath()}: only triangle and quad faces are supported; triangulate other polygons first')
    if sum(int(n) for n in counts) != len(indices):
        raise ValueError(f'{prim.GetPath()}: face counts do not match face indices')
    if any(int(index) < 0 or int(index) >= len(points) for index in indices):
        raise ValueError(f'{prim.GetPath()}: face vertex index is out of range')

    transform = UsdGeom.XformCache(timecode).GetLocalToWorldTransform(prim)
    vertices = []
    for p in points:
        world = transform.Transform(Gf.Vec3d(float(p[0]), float(p[1]), float(p[2])))
        xyz = tuple(float(v) * unit_scale for v in world)
        if not all(math.isfinite(v) for v in xyz):
            raise ValueError(f'{prim.GetPath()}: transformed points must be finite')
        vertices.append(fx.Vector(*xyz))
    triangles, quads = [], []
    offset = 0
    for count in counts:
        face = tuple(int(v) for v in indices[offset:offset + int(count)])
        (triangles if int(count) == 3 else quads).append(face)
        offset += int(count)
    return fx.mesh_to_levelset(voxel_size, vertices, triangles, quads, half_width)


def mesh_prim_to_field(prim, voxel_size, half_width=3.0, volume_meters_per_unit=1.0, time=None):
    """Return a vspf-compatible scalar field for a USD Mesh prim."""
    return fx.make_grid_field(mesh_prim_to_grid(prim, voxel_size, half_width, volume_meters_per_unit, time))


def usd_mesh_to_grid(filename, voxel_size, prim_path=None, half_width=3.0, volume_meters_per_unit=1.0, time=None):
    """Read a mesh from a USD stage. Select prim_path when there are multiple meshes."""
    stage = Usd.Stage.Open(str(filename))
    if not stage:
        raise ValueError(f'Could not open USD stage: {filename}')
    if prim_path is not None:
        prim = stage.GetPrimAtPath(str(prim_path))
        if not prim or not prim.IsA(UsdGeom.Mesh):
            raise ValueError(f'{prim_path}: expected a USD Mesh prim')
    else:
        meshes = [p for p in stage.Traverse() if p.IsA(UsdGeom.Mesh)]
        if len(meshes) != 1:
            raise ValueError(f'Expected one USD Mesh; found {len(meshes)}. Specify prim_path.')
        prim = meshes[0]
    return mesh_prim_to_grid(prim, voxel_size, half_width, volume_meters_per_unit, time)


def usd_mesh_to_field(filename, voxel_size, prim_path=None, half_width=3.0, volume_meters_per_unit=1.0, time=None):
    """Read a USD mesh and return a vspf-compatible signed-distance field."""
    return fx.make_grid_field(usd_mesh_to_grid(filename, voxel_size, prim_path, half_width, volume_meters_per_unit, time))
