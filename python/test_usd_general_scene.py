"""Integration tests for the multi-object USD scene and native bindings."""
from pathlib import Path
import ast
import re
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

    def test_stage_helpers_and_legacy_render_limit(self):
        stage = Usd.Stage.Open(str(SCENE))
        camera = loader.get_camera(stage)
        settings = loader.get_render_settings(stage)
        lights = loader.get_lights(stage, settings)
        objects = loader.get_scene_objects(stage, settings)
        self.assertIsInstance(camera, fx.Camera)
        self.assertEqual((settings.width, settings.height), (960, 540))
        self.assertEqual(len(lights), 1)
        self.assertEqual(len(objects), 12)
        with self.assertRaisesRegex(ValueError, 'existing renderer supports one white sphere'):
            loader.load_scene(SCENE).render('out.exr')


if __name__ == '__main__':
    unittest.main()
