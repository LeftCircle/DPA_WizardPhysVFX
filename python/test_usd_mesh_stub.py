"""Dependency-free tests of USD topology and world-space conversion."""
import importlib.util
from pathlib import Path
import sys
from types import ModuleType, SimpleNamespace
import unittest
from unittest.mock import patch


class Attr:
    def __init__(self, value): self.value = value
    def Get(self, *args): return self.value


class Mesh:
    def __init__(self, prim): self.prim = prim
    def GetPointsAttr(self): return Attr(self.prim.points)
    def GetFaceVertexCountsAttr(self): return Attr(self.prim.counts)
    def GetFaceVertexIndicesAttr(self): return Attr(self.prim.indices)
    def GetSubdivisionSchemeAttr(self): return Attr(self.prim.scheme)
    def GetHoleIndicesAttr(self): return Attr(self.prim.holes)


class Prim:
    def __init__(self):
        self.points = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1)]
        self.counts = [3, 3, 3, 3]
        self.indices = [0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3]
        self.scheme, self.holes = 'none', []
        self.stage = None
    def IsA(self, schema): return schema is Mesh
    def GetPath(self): return '/World/Mesh'
    def GetStage(self): return self.stage


class TimeCode:
    def __init__(self, value): self.value = value
    @staticmethod
    def Default(): return TimeCode('default')


class Matrix:
    def Transform(self, p): return (2 * p[0] + 10, 3 * p[1] + 20, p[2] + 30)


class Cache:
    def __init__(self, time): self.time = time
    def GetLocalToWorldTransform(self, prim): return Matrix()


def fixture():
    prim = Prim()
    stage = SimpleNamespace(Traverse=lambda: [prim], GetPrimAtPath=lambda p: prim if p == '/World/Mesh' else None)
    prim.stage = stage
    calls = []
    fx = ModuleType('physics_vfx')
    fx.Vector = lambda *args: tuple(args)
    fx.mesh_to_levelset = lambda *args: calls.append(args) or ('grid', args)
    fx.make_grid_field = lambda grid: ('field', grid)
    pxr = ModuleType('pxr')
    pxr.Gf = SimpleNamespace(Vec3d=lambda *args: args)
    pxr.Usd = SimpleNamespace(Stage=SimpleNamespace(Open=lambda _: stage), TimeCode=TimeCode)
    pxr.UsdGeom = SimpleNamespace(Mesh=Mesh, XformCache=Cache, GetStageMetersPerUnit=lambda _: 0.01)
    spec = importlib.util.spec_from_file_location('usd_mesh_test', Path(__file__).parent / 'usd_mesh.py')
    module = importlib.util.module_from_spec(spec)
    with patch.dict(sys.modules, {'pxr': pxr, 'physics_vfx': fx, spec.name: module}):
        spec.loader.exec_module(module)
    return module, prim, calls


class USDMeshTranslationTests(unittest.TestCase):
    def test_transform_units_topology_and_field_dispatch(self):
        module, prim, calls = fixture()
        result = module.usd_mesh_to_field('fixture.usda', 0.1, prim_path='/World/Mesh', time=24)
        self.assertEqual(result[0], 'field')
        voxel_size, points, triangles, quads, half_width = calls[0]
        self.assertEqual(voxel_size, 0.1)
        self.assertEqual(points[0], (0.1, 0.2, 0.3))
        self.assertAlmostEqual(points[1][0], 0.12)
        self.assertEqual(triangles[0], (0, 2, 1))
        self.assertEqual(quads, [])
        self.assertEqual(half_width, 3)
        bounds_min, bounds_max = module.mesh_world_bounds(prim)
        self.assertEqual(bounds_min, (0.1, 0.2, 0.3))
        self.assertAlmostEqual(bounds_max[0], 0.12)
        self.assertAlmostEqual(bounds_max[1], 0.23)
        self.assertAlmostEqual(bounds_max[2], 0.31)

    def test_invalid_topology_and_subdivision_are_rejected(self):
        module, prim, _ = fixture()
        prim.scheme = 'catmullClark'
        with self.assertRaisesRegex(ValueError, 'subdivision'):
            module.usd_mesh_to_field('scene', 0.1)
        prim.scheme = 'none'
        prim.counts = [5]
        with self.assertRaisesRegex(ValueError, 'triangle and quad'):
            module.usd_mesh_to_field('scene', 0.1)
        prim.counts = [3]
        prim.indices = [0, 1, 9]
        with self.assertRaisesRegex(ValueError, 'out of range'):
            module.usd_mesh_to_field('scene', 0.1)
        prim.indices = [0, 1, 2]
        prim.holes = [0]
        with self.assertRaisesRegex(ValueError, 'holes'):
            module.usd_mesh_to_field('scene', 0.1)

    def test_missing_mesh_and_invalid_voxel_size(self):
        module, _, _ = fixture()
        with self.assertRaisesRegex(ValueError, 'expected a USD Mesh'):
            module.usd_mesh_to_field('scene', 0.1, prim_path='/wrong')
        with self.assertRaisesRegex(ValueError, 'voxel_size'):
            module.usd_mesh_to_field('scene', 0)


if __name__ == '__main__': unittest.main()
