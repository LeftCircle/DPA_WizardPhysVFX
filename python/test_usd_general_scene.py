"""Integration tests for the multi-object USD scene and native bindings."""
from pathlib import Path
import ast
import os
import re
import tempfile
import unittest

try:
    from pxr import Usd
    import physics_vfx as fx
    import usd_sphere_scene as loader
    HAS_DEPS = True
except ImportError:
    HAS_DEPS = False

SCENE = Path(__file__).parent / 'inputs' / 'isf_mesh_scene.usda'


class ExampleSceneContentsTests(unittest.TestCase):
    def test_tags_colors_and_mesh_topology(self):
        source = SCENE.read_text()
        tags = re.findall(r'custom token pbvfx:isf_type = "([^"]+)"', source)
        expected = {'isf_sphere', 'isf_torus', 'isf_cone', 'isf_box', 'isf_icosahedron',
                    'isf_steiner_patch', 'isf_ellipse', 'isf_cylinder'}
        self.assertEqual(len(tags), 8)
        self.assertEqual(set(tags), expected)
        self.assertEqual(source.count('primvars:displayColor = [(1, 0, 0)]'), 8)
        self.assertEqual(source.count('primvars:displayColor = [(0, 1, 0)]'), 4)
        meshes = re.findall(r'def Mesh "([^"]+)"\s*\{([^}]*)\}', source)
        self.assertEqual(len(meshes), 4)
        topologies = set()
        for name, body in meshes:
            points = ast.literal_eval(re.search(r'point3f\[\] points = (\[[^\]]*\])', body).group(1))
            counts = ast.literal_eval(re.search(r'int\[\] faceVertexCounts = (\[[^\]]*\])', body).group(1))
            indices = ast.literal_eval(re.search(r'int\[\] faceVertexIndices = (\[[^\]]*\])', body).group(1))
            self.assertEqual(sum(counts), len(indices), name)
            self.assertTrue(all(0 <= i < len(points) for i in indices), name)
            self.assertTrue(all(n in (3, 4) for n in counts), name)
            topologies.add((len(points), tuple(counts)))
        self.assertEqual(len(topologies), 4)


@unittest.skipUnless(HAS_DEPS, 'requires OpenUSD and the built physics_vfx module')
class GeneralSceneTests(unittest.TestCase):
    def test_scattered_objects_and_physvfx_components(self):
        scene = loader.load_scene(SCENE)
        self.assertEqual(len(scene.objects), 12)
        self.assertTrue(next(o for o in scene.objects if o.kind == 'isf_sphere').stamped)
        self.assertFalse(next(o for o in scene.objects if o.kind == 'isf_torus').stamped)
        self.assertGreater(next(o for o in scene.objects if o.kind == 'isf_sphere').field.eval(fx.Vector(100, 100, 100)), 0)
        red = [o for o in scene.objects if o.kind != 'mesh']
        green = [o for o in scene.objects if o.kind == 'mesh']
        self.assertEqual({o.kind for o in red}, set(loader.ISF_TYPES))
        self.assertEqual(len(green), 4)
        self.assertTrue(all(o.rgb == (1, 0, 0) for o in red))
        self.assertTrue(all(o.rgb == (0, 1, 0) for o in green))
        self.assertIsInstance(scene.settings, fx.RenderSettings)
        self.assertIsInstance(scene.camera, fx.Camera)
        self.assertTrue(all(isinstance(light, fx.PointLight) for light in scene.lights))
        self.assertIsInstance(scene.density, fx.Field)
        self.assertIsInstance(scene.color, fx.ColorField)
        for p, rgb in [(fx.Vector(-6, 3, 0), (1, 0, 0)),
                       (fx.Vector(-6, -5, 0), (0, 1, 0))]:
            self.assertLess(scene.density.eval(p), 0)
            color = scene.color.eval(p)
            self.assertEqual((color.red, color.green, color.blue), rgb)

    def test_stage_helpers_and_derived_bounds(self):
        stage = Usd.Stage.Open(str(SCENE))
        camera = loader.get_camera(stage)
        settings = loader.get_render_settings(stage)
        lights = loader.get_lights(stage, settings)
        objects = loader.get_scene_objects(stage, settings)
        self.assertIsInstance(camera, fx.Camera)
        self.assertEqual((settings.width, settings.height), (960, 540))
        self.assertEqual(len(lights), 1)
        self.assertEqual(len(objects), 12)
        scene = loader.load_scene(SCENE)
        self.assertLess(scene.bounds_min[0], -7)
        self.assertGreater(scene.bounds_max[0], 7)
        self.assertLess(scene.bounds_min[1], -6)
        self.assertGreater(scene.bounds_max[1], 4)

    @unittest.skipUnless(os.environ.get('PBVFX_RUN_RENDER_TESTS') == '1', 'opt in to CPU render test')
    def test_render_contains_red_isfs_and_green_meshes(self):
        try:
            import OpenImageIO as oiio
            import numpy as np
        except ImportError:
            self.skipTest('requires OpenImageIO Python bindings and NumPy')
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'scene.usda'
            path.write_text(SCENE.read_text())
            stage = Usd.Stage.Open(str(path))
            from pxr import Gf
            settings = stage.GetPrimAtPath('/World/Settings')
            settings.GetAttribute('resolution').Set(Gf.Vec2i(160, 90))
            settings.GetAttribute('pbvfx:minStep').Set(0.08)
            settings.GetAttribute('pbvfx:maxStep').Set(0.08)
            settings.GetAttribute('pbvfx:shadowStep').Set(0.2)
            settings.GetAttribute('pbvfx:densityVoxelSize').Set(0.2)
            settings.GetAttribute('pbvfx:colorVoxelSize').Set(0.2)
            settings.GetAttribute('pbvfx:shadowVoxelSize').Set(0.4)
            stage.GetRootLayer().Save()
            output = Path(tmp) / 'scene.exr'
            loader.load_scene(path).render(output)
            image = oiio.ImageInput.open(str(output))
            self.assertIsNotNone(image)
            pixels = np.asarray(image.read_image(format=oiio.FLOAT)).reshape(90, 160, -1)
            image.close()
            red, green = pixels[:, :, 0], pixels[:, :, 1]
            self.assertGreater(float(red.max()), 0)
            self.assertGreater(float(green.max()), 0)
            self.assertGreater(int(np.count_nonzero((red > 0.005) & (red > 2 * green))), 10)
            self.assertGreater(int(np.count_nonzero((green > 0.005) & (green > 2 * red))), 10)


if __name__ == '__main__':
    unittest.main()
