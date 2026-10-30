"""Integration tests for native wisp bindings and USD loading."""
from pathlib import Path
import os
import tempfile
import unittest

try:
    import physics_vfx as fx
    HAS_FX = hasattr(fx, 'wisp_uniform')
except ImportError:
    HAS_FX = False
try:
    from pxr import Usd
    import usd_sphere_scene as loader
    HAS_USD = HAS_FX
except ImportError:
    HAS_USD = False


@unittest.skipUnless(HAS_FX, 'requires the built physics_vfx module')
class WispBindingTests(unittest.TestCase):
    def test_seeded_deposits_and_bounds(self):
        noise = fx.FSVals(0.5, 2, fx.Vector(0, 0, 0), 1, 2)
        def create(seed):
            grid = fx.FloatGrid(0.25, 0)
            wisp = fx.wisp_uniform(100, -1, 1, 1, noise, noise, fx.Vector(1, 1, 1),
                                   fx.Vector(0, 0, 0), fx.CoordinateSys(), seed)
            wisp.stamp(grid)
            return grid
        first, second = create(7), create(7)
        self.assertIsNotNone(first.active_world_bounds())
        self.assertEqual(first.active_world_bounds(), second.active_world_bounds())
        values = [first.get_value(x, y, z) for x in range(-10, 11)
                  for y in range(-10, 11) for z in range(-10, 11)]
        self.assertLess(min(values), 0)
        self.assertEqual(values, [second.get_value(x, y, z) for x in range(-10, 11)
                                  for y in range(-10, 11) for z in range(-10, 11)])

    def test_float_grid_cache_round_trip(self):
        grid = fx.FloatGrid(0.25, 0)
        grid.set_value(1, 2, 3, -0.75)
        with tempfile.TemporaryDirectory() as tmp:
            path = str(Path(tmp) / 'wisp.vdb')
            fx.save_to_disk(grid, path)
            restored = fx.load_float_grid(path)
            self.assertAlmostEqual(restored.get_value(1, 2, 3), -0.75)
            self.assertAlmostEqual(restored.voxel_size, 0.25)
            self.assertEqual(restored.active_world_bounds(), grid.active_world_bounds())


@unittest.skipUnless(HAS_USD, 'requires OpenUSD and the built physics_vfx module')
class WispUSDTests(unittest.TestCase):
    def test_load_example_and_density(self):
        scene = loader.load_scene(Path(__file__).parent / 'inputs' / 'wisps.usda')
        self.assertEqual(len(scene.objects), 1)
        self.assertEqual(scene.objects[0].kind, 'wisp')
        self.assertTrue(scene.objects[0].is_density)
        self.assertTrue(scene.objects[0].stamped)
        self.assertIs(scene.density, scene.objects[0].field)

    @unittest.skipUnless(os.environ.get('PBVFX_RUN_RENDER_TESTS') == '1', 'opt in to CPU render test')
    def test_render_wisp(self):
        try:
            import OpenImageIO as oiio
            import numpy as np
            from pxr import Gf
        except ImportError:
            self.skipTest('requires OpenImageIO Python bindings and NumPy')
        source = Path(__file__).parent / 'inputs' / 'wisps.usda'
        with tempfile.TemporaryDirectory() as tmp:
            stage_path = Path(tmp) / 'wisps.usda'
            stage_path.write_text(source.read_text())
            stage = Usd.Stage.Open(str(stage_path))
            settings = stage.GetPrimAtPath('/World/Settings')
            settings.GetAttribute('resolution').Set(Gf.Vec2i(160, 90))
            settings.GetAttribute('pbvfx:minStep').Set(0.08)
            settings.GetAttribute('pbvfx:maxStep').Set(0.08)
            settings.GetAttribute('pbvfx:shadowStep').Set(0.2)
            settings.GetAttribute('pbvfx:densityVoxelSize').Set(0.2)
            settings.GetAttribute('pbvfx:shadowVoxelSize').Set(0.3)
            stage.GetPrimAtPath('/World/Wisp').GetAttribute('pbvfx:wispDots').Set(20000)
            stage.GetRootLayer().Save()
            output = Path(tmp) / 'wisp.exr'
            loader.load_scene(stage_path).render(output)
            image = oiio.ImageInput.open(str(output))
            self.assertIsNotNone(image)
            pixels = np.asarray(image.read_image(format=oiio.FLOAT))
            image.close()
            self.assertGreater(float(pixels.max()), 0)


if __name__ == '__main__':
    unittest.main()
