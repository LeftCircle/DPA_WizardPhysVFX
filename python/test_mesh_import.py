"""Integration tests for OBJ and USD mesh-to-field conversion."""
from pathlib import Path
import tempfile
import unittest

import physics_vfx as fx

try:
    from pxr import Usd
    from usd_mesh import usd_mesh_to_field
    from usd_to_pbvfx import create_volume_from_usd
    HAS_USD = True
except ImportError:
    HAS_USD = False

OBJ = '''\nv 3 -1 -1\nv 5 -1 -1\nv 5 1 -1\nv 3 1 -1\nv 3 -1 1\nv 5 -1 1\nv 5 1 1\nv 3 1 1\nf -8 -5 -6 -7\nf 1 2 3 4\nf 1 5 6 2\nf 2 6 7 3\nf 3 7 8 4\nf 4 8 5 1\n'''
USD = '''#usda 1.0
(
    metersPerUnit = 0.01
    upAxis = "Y"
)
def Xform "World" {
    double3 xformOp:translate = (200, 0, 0)
    uniform token[] xformOpOrder = ["xformOp:translate"]
    def Mesh "Box" {
        uniform token subdivisionScheme = "none"
        point3f[] points = [(-100,-100,-100), (100,-100,-100), (100,100,-100), (-100,100,-100),
                           (-100,-100,100), (100,-100,100), (100,100,100), (-100,100,100)]
        int[] faceVertexCounts = [4,4,4,4,4,4]
        int[] faceVertexIndices = [0,3,2,1, 4,5,6,7, 0,1,5,4, 1,2,6,5, 2,3,7,6, 3,0,4,7]
    }
}
'''


class MeshImportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.obj = self.directory / 'box.obj'
        self.obj.write_text(OBJ)

    def test_obj_field_preserves_position_and_can_recenter(self):
        field = fx.obj_mesh_to_grid_field(str(self.obj), 0.1)
        self.assertLess(field.eval(fx.Vector(4, 0, 0)), 0)
        self.assertGreater(field.eval(fx.Vector(0, 0, 0)), 0)
        centered = fx.obj_mesh_to_grid_field(str(self.obj), 0.1, recenter=True)
        self.assertLess(centered.eval(fx.Vector(0, 0, 0)), 0)
        grid = fx.obj_mesh_to_level_set_f(str(self.obj), 0.1)
        self.assertLess(fx.make_grid_field(grid).eval(fx.Vector(4, 0, 0)), 0)

    def test_invalid_obj_rejected(self):
        with self.assertRaises(RuntimeError):
            fx.obj_mesh_to_field(str(self.directory / 'missing.obj'), 0.1)
        self.obj.write_text('v 0 0 0\nf 1 1 1 1 1\n')
        with self.assertRaisesRegex(RuntimeError, 'triangles or quads'):
            fx.obj_mesh_to_field(str(self.obj), 0.1)
        with self.assertRaises(ValueError):
            fx.obj_mesh_to_field(str(self.obj), 0)

    def test_invalid_mesh_indices_rejected(self):
        points = [fx.Vector(0, 0, 0), fx.Vector(1, 0, 0), fx.Vector(0, 1, 0)]
        with self.assertRaisesRegex(ValueError, 'index out of range'):
            fx.mesh_to_levelset(0.1, points, [(0, 1, 9)])
        with self.assertRaisesRegex(ValueError, 'mesh must contain'):
            fx.mesh_to_levelset(0.1, [], [])

    def test_helper_bounds_stamping_and_deposit(self):
        lower, upper = fx.Vector(3, -1, -1), fx.Vector(5, 1, 1)
        grid = fx.create_float_grid(1.0)
        bounds = fx.world_space_to_bounds(lower, upper, grid)
        self.assertEqual(tuple(bounds.minimum), (3, -1, -1))
        self.assertEqual(tuple(bounds.maximum), (5, 1, 1))
        fx.stamp_grid(grid, fx.constant(2.0), bounds)
        self.assertEqual(grid.get_value(4, 0, 0), 2.0)
        fx.trilinear_deposit(grid, fx.Vector(10, 0, 0), 3.0)
        self.assertEqual(grid.get_value(10, 0, 0), 3.0)
        self.assertEqual(fx.create_grid(1.0, 5.0).get_value(0, 0, 0), 5.0)
        color_grid = fx.create_grid(1.0, fx.Color(0.25, 0.5, 0.75))
        self.assertAlmostEqual(color_grid.get_value(0, 0, 0).green, 0.5)
        parallel = fx.stamp_grid_parallel(1.0, 0.0, fx.constant(-1.0), bounds, negative_only=True)
        self.assertEqual(parallel.get_value(4, 0, 0), -1.0)
        output = self.directory / 'volume.vdb'
        fx.save_to_disk(parallel, str(output))
        self.assertGreater(output.stat().st_size, 0)

    @unittest.skipUnless(HAS_USD, 'requires OpenUSD Python bindings')
    def test_usd_mesh_world_transform_and_stage_units(self):
        path = self.directory / 'scene.usda'
        path.write_text(USD)
        field = usd_mesh_to_field(path, 0.1, prim_path='/World/Box')
        self.assertLess(field.eval(fx.Vector(2, 0, 0)), 0)
        self.assertGreater(field.eval(fx.Vector(4, 0, 0)), 0)
        field2, names = create_volume_from_usd(str(path), mesh_voxel_size=0.1)
        self.assertEqual(names, ['/World/Box'])
        self.assertLess(field2.eval(fx.Vector(2, 0, 0)), 0)

    @unittest.skipUnless(HAS_USD, 'requires OpenUSD Python bindings')
    def test_usd_subdivision_and_unsupported_polygon_rejected(self):
        path = self.directory / 'scene.usda'
        path.write_text(USD.replace('uniform token subdivisionScheme = "none"',
                                    'uniform token subdivisionScheme = "catmullClark"'))
        with self.assertRaisesRegex(ValueError, 'subdivision'):
            usd_mesh_to_field(path, 0.1)
        path = self.directory / 'polygon.usda'
        path.write_text(USD.replace('int[] faceVertexCounts = [4,4,4,4,4,4]',
                                    'int[] faceVertexCounts = [5,4,4,4,4,3]'))
        with self.assertRaisesRegex(ValueError, 'triangle and quad'):
            usd_mesh_to_field(path, 0.1)


if __name__ == '__main__':
    unittest.main()
