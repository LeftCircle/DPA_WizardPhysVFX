"""Integration tests for the real USD bindings and PBVFX module."""
import math
import os
from pathlib import Path
import tempfile
import unittest

try:
    from pxr import Gf, Usd, UsdGeom
    import physics_vfx as fx
    import usd_sphere_scene as loader
    HAS_DEPS = True
except ImportError:
    HAS_DEPS = False

SCENE = Path(__file__).parent / 'inputs' / 'sphere.usda'


@unittest.skipUnless(HAS_DEPS, 'requires pxr and the built physics_vfx module')
class SphereSceneTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'scene.usda'
        self.path.write_text(SCENE.read_text())

    def test_loads_authored_scene_and_field(self):
        scene = loader.load_scene(self.path)
        self.assertEqual(scene.center, (0.0, 0.0, 0.0))
        self.assertAlmostEqual(scene.radius, 1)
        self.assertLess(scene.field.eval(fx.Vector(0, 0, 0)), 0)
        self.assertAlmostEqual(scene.field.eval(fx.Vector(1, 0, 0)), 0, places=5)
        self.assertGreater(scene.field.eval(fx.Vector(2, 0, 0)), 0)
        self.assertEqual(scene.eye, (0.0, 0.0, 8.0))
        self.assertEqual(scene.view, (0.0, 0.0, -1.0))
        self.assertAlmostEqual(scene.horizontal_fov, math.degrees(2 * math.atan(20.955 / 100)), places=4)
        self.assertEqual((scene.settings.width, scene.settings.height), (640, 360))
        self.assertAlmostEqual(scene.settings.shadow_voxel_size, 0.1, places=5)
        self.assertEqual(scene.light_color, (2.0, 2.0, 2.0))

    def test_parent_transform_and_stage_units(self):
        stage = Usd.Stage.Open(str(self.path))
        stage.SetMetadata('metersPerUnit', 0.01)
        root = UsdGeom.Xformable(stage.GetPrimAtPath('/World'))
        root.AddTranslateOp().Set(Gf.Vec3d(10, 20, 30))
        sphere = UsdGeom.Xformable(stage.GetPrimAtPath('/World/Sphere'))
        sphere.AddScaleOp().Set(Gf.Vec3f(2, 2, 2))
        stage.GetRootLayer().Save()
        scene = loader.load_scene(self.path)
        for actual, expected in zip(scene.center, (0.1, 0.2, 0.3)):
            self.assertAlmostEqual(actual, expected, places=5)
        self.assertAlmostEqual(scene.radius, 0.02, places=5)
        self.assertAlmostEqual(scene.eye[2], 0.38, places=5)
        self.assertAlmostEqual(scene.settings.min_step, 0.0002, places=6)

    def test_rejects_nonuniform_sphere(self):
        stage = Usd.Stage.Open(str(self.path))
        sphere = UsdGeom.Xformable(stage.GetPrimAtPath('/World/Sphere'))
        sphere.AddScaleOp().Set(Gf.Vec3f(1, 2, 1))
        stage.GetRootLayer().Save()
        with self.assertRaisesRegex(ValueError, 'non-uniform scale'):
            loader.load_scene(self.path)

    @unittest.skipUnless(os.environ.get('PBVFX_RUN_RENDER_TESTS') == '1', 'opt in to the CPU render test')
    def test_render_changes_with_position_and_light(self):
        try:
            import OpenImageIO as oiio
            import numpy as np
        except ImportError:
            self.skipTest('requires OpenImageIO Python bindings and NumPy')

        stage = Usd.Stage.Open(str(self.path))
        stage.GetPrimAtPath('/World/Settings').GetAttribute('resolution').Set(Gf.Vec2i(96, 54))
        stage.GetRootLayer().Save()

        def render_image(name):
            output = Path(self.temp.name) / name
            loader.load_scene(self.path).render(output)
            image = oiio.ImageInput.open(str(output))
            self.assertIsNotNone(image)
            pixels = np.asarray(image.read_image(format=oiio.FLOAT))
            image.close()
            return pixels.reshape(54, 96, -1)

        original = render_image('original.exr')
        self.assertGreater(float(original[:, :, :3].max()), 0)
        stage.GetPrimAtPath('/World/Sphere').GetAttribute('xformOp:translate').Set(Gf.Vec3d(1, 0, 0))
        stage.GetPrimAtPath('/World/Light').GetAttribute('inputs:color').Set(Gf.Vec3f(1, 0, 0))
        stage.GetRootLayer().Save()
        changed = render_image('changed.exr')
        self.assertGreater(float(changed[:, :, 0].max()), 0)
        self.assertLess(float(changed[:, :, 1].max()), 1e-5)
        x = np.arange(96)[None, :]
        self.assertGreater(float((changed[:, :, 0] * x).sum() / changed[:, :, 0].sum()),
                           float((original[:, :, 0] * x).sum() / original[:, :, 0].sum()) + 3)
        stage.GetPrimAtPath('/World/Light').GetAttribute('xformOp:translate').Set(Gf.Vec3d(-3, -3, -4))
        stage.GetRootLayer().Save()
        moved_light = render_image('moved_light.exr')
        self.assertGreater(float(np.abs(moved_light[:, :, 0] - changed[:, :, 0]).sum()),
                           0.05 * float(changed[:, :, 0].sum()))


if __name__ == '__main__':
    unittest.main()
