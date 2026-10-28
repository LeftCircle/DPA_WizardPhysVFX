"""Dependency-free contract tests for scene translation and dispatch."""
import importlib.util
import math
from pathlib import Path
import sys
from types import ModuleType, SimpleNamespace
import unittest
from unittest.mock import patch


class Attr:
    def __init__(self, value): self.value = value
    def Get(self): return self.value


class Matrix:
    def __init__(self, translation=(0, 0, 0), scale=(1, 1, 1), shear=0):
        self.translation, self.scale, self.shear = translation, scale, shear
    def ExtractTranslation(self): return self.translation
    def TransformDir(self, v):
        return (v[0] * self.scale[0] + v[1] * self.shear,
                v[1] * self.scale[1], v[2] * self.scale[2])


class Prim:
    def __init__(self, kind, path, values=None, matrix=None):
        self.kind, self.path = kind, path
        self.values, self.matrix = values or {}, matrix or Matrix()
    def IsA(self, kind): return self.kind is kind
    def GetPath(self): return self.path
    def GetAttribute(self, name):
        return Attr(self.values[name]) if name in self.values else None


class Schema:
    def __init__(self, prim): self.prim = prim
    def __getattr__(self, name):
        if name == 'GetCameraRel':
            return lambda: SimpleNamespace(GetTargets=lambda: ['/camera'])
        if name.startswith('Get') and name.endswith('Attr'):
            key = name[3:-4]
            return lambda: Attr(self.prim.values[key])
        raise AttributeError(name)


class Sphere(Schema): pass
class Camera(Schema): pass
class Light(Schema): pass
class Settings(Schema): pass


class XformCache:
    def __init__(self, _): pass
    def GetLocalToWorldTransform(self, prim): return prim.matrix


class SettingsObject:
    def __init__(self):
        self.kappa, self.shadow_kappa = 3, 3
        self.min_step, self.max_step, self.shadow_step = 0.01, 0.05, 0.01
        self.density_voxel_size = self.color_voxel_size = self.shadow_voxel_size = 0.025


def fixture():
    camera = Prim(Camera, '/camera', dict(Projection='perspective', HorizontalApertureOffset=0,
        VerticalApertureOffset=0, FocalLength=50, HorizontalAperture=20,
        VerticalAperture=10, ClippingRange=(0.1, 100)), Matrix((0, 0, 8)))
    sphere = Prim(Sphere, '/sphere', dict(Radius=2), Matrix((4, 2, -1), (2, 2, 2)))
    light = Prim(Light, '/light', dict(Normalize=False, EnableColorTemperature=False,
        Color=(0.5, 1, 0.25), Intensity=2, Exposure=1,
        **{'pbvfx:shadowKappa': 0.7}), Matrix((3, 4, 5)))
    settings = Prim(Settings, '/settings', dict(Resolution=(200, 100), PixelAspectRatio=1,
        **{'pbvfx:densityVoxelSize': 0.2, 'pbvfx:shadowVoxelSize': 0.4}))
    prims = [camera, sphere, light, settings]
    stage = SimpleNamespace(Traverse=lambda: prims,
                            GetPrimAtPath=lambda path: next((p for p in prims if p.path == path), None))
    calls = []
    fx = ModuleType('physics_vfx')
    fx.Vector = lambda *v: tuple(v)
    fx.sphere = lambda center, radius: ('sphere', center, radius)
    fx.RenderSettings = SettingsObject
    fx.render_usd_sphere = lambda *args: calls.append(args)
    pxr = ModuleType('pxr')
    pxr.Gf = SimpleNamespace(Vec3d=lambda *v: v)
    pxr.Usd = SimpleNamespace(Stage=SimpleNamespace(Open=lambda _: stage),
                              TimeCode=SimpleNamespace(Default=lambda: None))
    pxr.UsdGeom = SimpleNamespace(Sphere=Sphere, Camera=Camera, Gprim=object, XformCache=XformCache,
                                  GetStageMetersPerUnit=lambda _: 0.5)
    pxr.UsdLux = SimpleNamespace(SphereLight=Light, LightAPI=Light)
    pxr.UsdRender = SimpleNamespace(Settings=Settings)
    spec = importlib.util.spec_from_file_location('usd_sphere_scene_test',
                                                  Path(__file__).parent / 'usd_sphere_scene.py')
    module = importlib.util.module_from_spec(spec)
    with patch.dict(sys.modules, {'pxr': pxr, 'physics_vfx': fx, spec.name: module}):
        spec.loader.exec_module(module)
    return module, sphere, camera, light, settings, calls


class TranslationTests(unittest.TestCase):
    def test_scene_parameters_and_render_dispatch(self):
        module, _, _, _, _, calls = fixture()
        scene = module.load_scene('scene.usda')
        self.assertEqual(scene.center, (2, 1, -0.5))
        self.assertEqual(scene.radius, 2)
        self.assertEqual(scene.field, ('sphere', scene.center, 2))
        self.assertEqual(scene.eye, (0, 0, 4))
        self.assertEqual(scene.view, (0, 0, -1))
        self.assertAlmostEqual(scene.horizontal_fov, math.degrees(2 * math.atan(0.2)))
        self.assertEqual(scene.light_position, (1.5, 2, 2.5))
        self.assertEqual(scene.light_color, (2, 4, 1))
        self.assertEqual(scene.light_shadow_kappa, 0.7)
        self.assertEqual(scene.settings.density_voxel_size, 0.1)
        self.assertEqual(scene.settings.shadow_voxel_size, 0.2)
        scene.render('out.exr')
        self.assertEqual(calls[0][0], scene.field)
        self.assertEqual(calls[0][-1], 'out.exr')

    def test_rejects_unsupported_transforms_and_aspect(self):
        module, sphere, camera, _, settings, _ = fixture()
        sphere.matrix = Matrix(scale=(1, 2, 1))
        with self.assertRaisesRegex(ValueError, 'non-uniform'): module.load_scene('scene')
        sphere.matrix = Matrix(shear=0.5)
        with self.assertRaisesRegex(ValueError, 'shear'): module.load_scene('scene')
        sphere.matrix = Matrix()
        settings.values['Resolution'] = (100, 100)
        with self.assertRaisesRegex(ValueError, 'aspect ratios'): module.load_scene('scene')
        settings.values['Resolution'] = (200, 100)
        camera.values['Projection'] = 'orthographic'
        with self.assertRaisesRegex(ValueError, 'perspective'): module.load_scene('scene')

    def test_rotated_camera_basis(self):
        module, _, camera, _, _, _ = fixture()
        class RotatedMatrix(Matrix):
            def TransformDir(self, v):
                # A 90-degree rotation around Y in world space.
                return (v[2], v[1], -v[0])
        camera.matrix = RotatedMatrix((0, 0, 8))
        scene = module.load_scene('scene')
        self.assertEqual(scene.view, (-1, 0, 0))
        self.assertEqual(scene.up, (0, 1, 0))


if __name__ == '__main__': unittest.main()
