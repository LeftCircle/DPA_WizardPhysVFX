"""Dependency-free contract tests for scene translation and dispatch."""
import importlib.util
import math
from pathlib import Path
import sys
import tempfile
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
    def GetDeterminant(self): return math.prod(self.scale)
    def GetInverse(self):
        inverse = Matrix(tuple(-t / s for t, s in zip(self.translation, self.scale)),
                         tuple(1 / s for s in self.scale))
        return inverse
    def Transform(self, v):
        return tuple(a * b + t for a, b, t in zip(v, self.scale, self.translation))


class Prim:
    def __init__(self, kind, path, values=None, matrix=None):
        self.kind, self.path = kind, path
        self.values, self.matrix = values or {}, matrix or Matrix()
    def IsA(self, kind):
        if kind is object:
            return self.kind in (Sphere, Mesh)
        return self.kind is kind
    def GetPath(self): return self.path
    def GetRelationship(self, name):
        value = self.values.get(name)
        return SimpleNamespace(GetTargets=lambda: value) if value is not None else None
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
class Mesh(Schema): pass


class XformCache:
    def __init__(self, _): pass
    def GetLocalToWorldTransform(self, prim): return prim.matrix


class SettingsObject:
    def __init__(self):
        self.kappa, self.shadow_kappa = 3, 3
        self.min_step, self.max_step, self.shadow_step = 0.01, 0.05, 0.01
        self.density_voxel_size = self.color_voxel_size = self.shadow_voxel_size = 0.025


class FakeVector(tuple):
    def __new__(cls, *v): return tuple.__new__(cls, v)
    x = property(lambda self: self[0])
    y = property(lambda self: self[1])
    z = property(lambda self: self[2])


class FakeColor:
    def __init__(self, red, green, blue, alpha):
        self.red, self.green, self.blue, self.alpha = red, green, blue, alpha


class FakeCamera:
    def set_eye_view_up(self, *args): pass
    def set_fov(self, value): pass
    def set_aspect_ratio(self, value): pass


def fixture():
    camera = Prim(Camera, '/camera', dict(Projection='perspective', HorizontalApertureOffset=0,
        VerticalApertureOffset=0, FocalLength=50, HorizontalAperture=20,
        VerticalAperture=10, ClippingRange=(0.1, 100)), Matrix((0, 0, 8)))
    sphere = Prim(Sphere, '/sphere', dict(Radius=2, **{'pbvfx:isf_type': 'isf_sphere'}), Matrix((4, 2, -1), (2, 2, 2)))
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
    fx.Vector = FakeVector
    fx.Color = FakeColor
    fx.Camera = FakeCamera
    fx.PointLight = lambda color, position, kappa: SimpleNamespace(color=color, position=position, kappa=kappa)
    fx.sphere = lambda center, radius: ('sphere', center, radius)
    fx.isf_sphere = fx.sphere
    fx.mask = lambda field: ('mask', field)
    fx.negate = lambda field: ('negate', field)
    fx.make_scene_color_field = lambda fields, colors: ('color', fields, colors)
    fx.union_fields = lambda a, b: ('union', a, b)
    for name in ('isf_torus', 'isf_cone', 'isf_box', 'isf_icosahedron', 'isf_steiner_patch', 'isf_ellipse', 'isf_cylinder'):
        setattr(fx, name, lambda *args, name=name: (name, args))
    fx.affine_transform_field = lambda field, origin, x, y, z: ('transformed', field, origin, x, y, z)
    fx.RenderSettings = SettingsObject
    fx.render_usd_scene = lambda *args: calls.append(args)
    pxr = ModuleType('pxr')
    pxr.Gf = SimpleNamespace(Vec3d=lambda *v: v)
    pxr.Usd = SimpleNamespace(Stage=SimpleNamespace(Open=lambda _: stage),
                              TimeCode=SimpleNamespace(Default=lambda: None))
    pxr.UsdGeom = SimpleNamespace(Sphere=Sphere, Camera=Camera, Mesh=Mesh, Gprim=object, XformCache=XformCache,
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
        self.assertEqual(calls[0][0], scene.density)
        self.assertEqual(calls[0][1], scene.color)
        self.assertEqual(calls[0][3], scene.lights)
        self.assertEqual(calls[0][-1], 'out.exr')
        self.assertLess(scene.bounds_min[0], scene.center[0] - scene.radius)
        self.assertGreater(scene.bounds_max[0], scene.center[0] + scene.radius)

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

    def test_general_scene_dispatch_and_colors(self):
        module, sphere, _, _, _, calls = fixture()
        stage = module.Usd.Stage.Open('scene')
        original = list(stage.Traverse())
        sphere.values['primvars:displayColor'] = [(1, 0, 0)]
        tagged = [Prim(Sphere, '/isf/' + kind, {'pbvfx:isf_type': kind,
                  'primvars:displayColor': [(1, 0, 0)]}, Matrix((i, 0, 0)))
                  for i, kind in enumerate(module.ISF_TYPES[1:], 1)]
        meshes = [Prim(Mesh, '/mesh/' + str(i), {'primvars:displayColor': [(0, 1, 0)]}) for i in range(4)]
        stage.Traverse = lambda: original + tagged + meshes
        fake_mesh = ModuleType('usd_mesh')
        fake_mesh.mesh_prim_to_field = lambda prim, voxel, half_width, unit: ('mesh', prim.path)
        fake_mesh.mesh_world_bounds = lambda prim, unit: ((-10, -5, -1), (10, -4, 1))
        with patch.dict(sys.modules, {'usd_mesh': fake_mesh}):
            scene = module.load_scene('scene')
        self.assertEqual(len(scene.objects), 12)
        self.assertEqual({o.kind for o in scene.objects[:8]}, set(module.ISF_TYPES))
        self.assertTrue(all(o.rgb == (1, 0, 0) for o in scene.objects[:8]))
        self.assertTrue(all(o.kind == 'mesh' and o.rgb == (0, 1, 0) for o in scene.objects[8:]))
        self.assertEqual(scene.objects[1].field[0], 'transformed')
        self.assertEqual(scene.objects[1].field[2], (-1, 0, 0))
        self.assertEqual(len(scene.lights), 1)
        self.assertEqual(scene.density[0], 'union')
        self.assertEqual(scene.color[0], 'color')
        scene.render('mixed.exr')
        self.assertEqual(calls[0][0], scene.density)
        self.assertEqual(calls[0][1], scene.color)
        self.assertEqual(calls[0][3], scene.lights)
        self.assertLess(scene.bounds_min[0], -10)
        self.assertGreater(scene.bounds_max[0], 10)

    def test_object_stamping_before_union(self):
        module, sphere, _, _, _, _ = fixture()
        stage = module.Usd.Stage.Open('scene')
        original = list(stage.Traverse())
        other = Prim(Sphere, '/other', {'pbvfx:isf_type': 'isf_sphere', 'Radius': 1,
                                      'pbvfx:stamp': False}, Matrix((12, 0, 0)))
        sphere.values['pbvfx:stamp'] = True
        stage.Traverse = lambda: original + [other]
        stamps = []
        module.fx.world_space_to_bounds = lambda lo, hi, size: (lo, hi, size)
        module.fx.stamp_grid_parallel = lambda *args: stamps.append(args) or ('grid', args)
        module.fx.make_grid_field = lambda grid: ('grid_field', grid)
        scene = module.load_scene('scene')
        self.assertEqual(len(stamps), 1)
        self.assertTrue(scene.objects[0].stamped)
        self.assertFalse(scene.objects[1].stamped)
        self.assertEqual(scene.sdf, ('union', scene.objects[0].field, scene.objects[1].field))
        self.assertEqual(scene.color[1], [obj.field for obj in scene.objects])
        self.assertGreater(stamps[0][1], 0)
        sphere.values['pbvfx:stamp'] = 'yes'
        with self.assertRaisesRegex(ValueError, 'must be a bool'):
            module.load_scene('scene')

    def test_operation_graph_consumes_inputs_and_supports_chaining(self):
        module, sphere, _, _, _, _ = fixture()
        stage = module.Usd.Stage.Open('scene')
        original = list(stage.Traverse())
        cutter = Prim(Sphere, '/cutter', {'Radius': 1, 'pbvfx:isf_type': 'isf_sphere'}, Matrix((4, 2, -1)))
        cut = Prim(None, '/cut', {'pbvfx:operation': 'cutout', 'pbvfx:inputs': ['/sphere', '/cutter']})
        pyro = Prim(None, '/pyro', {'pbvfx:operation': 'pyro', 'pbvfx:inputs': ['/cut']})
        stage.Traverse = lambda: original + [pyro, cut, cutter]
        module.fx.cutout = lambda a, b: ('cutout', a, b)
        module.fx.make_pyro = lambda *args: ('pyro', args)
        scene = module.load_scene('scene')
        self.assertEqual([o.path for o in scene.objects], ['/pyro'])
        self.assertEqual(scene.sdf[0], 'pyro')
        self.assertEqual(scene.sdf[1][0][0], 'cutout')
        self.assertEqual(scene.objects[0].rgb, (1, 1, 1))
        cut.values['pbvfx:inputs'] = ['/sphere', '/missing']
        with self.assertRaisesRegex(ValueError, 'does not reference'):
            module.load_scene('scene')
        cut.values['pbvfx:inputs'] = ['/sphere', '/pyro']
        with self.assertRaisesRegex(ValueError, 'cyclic'):
            module.load_scene('scene')
        cut.values['pbvfx:inputs'] = ['/sphere']
        with self.assertRaisesRegex(ValueError, 'requires 2'):
            module.load_scene('scene')

    def test_arithmetic_operation_requires_explicit_bounds(self):
        module, sphere, _, _, _, _ = fixture()
        stage = module.Usd.Stage.Open('scene')
        original = list(stage.Traverse())
        op = Prim(None, '/sum', {'pbvfx:operation': 'add', 'pbvfx:inputs': ['/sphere', '/sphere']})
        stage.Traverse = lambda: original + [op]
        module.fx.add = lambda a, b: ('add', a, b)
        module.fx.make_plane = lambda p, n: ('plane', p, n)
        module.fx.intersection = lambda a, b: ('intersection', a, b)
        with self.assertRaisesRegex(ValueError, 'explicit'):
            module.load_scene('scene')
        op.values.update({'pbvfx:boundsMin': (-10, -10, -10), 'pbvfx:boundsMax': (10, 10, 10)})
        scene = module.load_scene('scene')
        self.assertEqual(scene.objects[0].bounds_min, (-5, -5, -5))
        self.assertEqual(scene.sdf[0], 'intersection')

    def test_wisp_uses_density_and_transformed_grid_bounds(self):
        module, sphere, _, _, _, _ = fixture()
        stage = module.Usd.Stage.Open('scene')
        original = list(stage.Traverse())
        prim = Prim(None, '/wisp', {'pbvfx:field_type': 'wisp', 'pbvfx:wispDots': 10,
                                  'pbvfx:wispDensity': 2}, Matrix((10, 0, 0)))
        stage.Traverse = lambda: original + [prim]
        calls = []
        class Grid:
            def __init__(self, *args): calls.append(('grid', args))
            def active_world_bounds(self): return ((-1, -1, -1), (1, 1, 1))
        class Wisp:
            def __init__(self, *args): calls.append(('wisp', args))
            def stamp(self, grid): calls.append(('stamp', grid))
        module.fx.FloatGrid, module.fx.wisp_uniform = Grid, Wisp
        module.fx.FSVals = lambda *args: args
        module.fx.CoordinateSys = lambda: 'axes'
        module.fx.make_grid_field = lambda grid: ('grid_field', grid)
        scene = module.load_scene('scene')
        wisp = scene.objects[1]
        self.assertTrue(wisp.is_density)
        self.assertTrue(wisp.stamped)
        self.assertEqual(calls[1][1][1], -2)
        self.assertEqual(scene.density[2], wisp.field)
        self.assertLess(wisp.bounds_min[0], 4.5)
        self.assertGreater(wisp.bounds_max[0], 5.5)
        prim.values['pbvfx:wispDots'] = 0
        with self.assertRaisesRegex(ValueError, 'wispDots'):
            module.load_scene('scene')

    def test_bounded_plane_and_noise_warp(self):
        module, _, _, _, _, _ = fixture()
        stage = module.Usd.Stage.Open('scene')
        initial = [p for p in stage.Traverse() if p.path != '/sphere']
        plane = Prim(None, '/plane', {'pbvfx:field_type': 'plane',
                     'pbvfx:boundsMin': (-2, -2, -2), 'pbvfx:boundsMax': (2, 2, 2)})
        warp = Prim(None, '/warp', {'pbvfx:operation': 'noise_warp', 'pbvfx:inputs': ['/plane'],
                    'pbvfx:boundsMin': (-2, -2, -2), 'pbvfx:boundsMax': (2, 2, 2),
                    'pbvfx:amplitude': 0.5})
        stage.Traverse = lambda: initial + [warp, plane]
        module.fx.make_plane = lambda p, n: ('plane', p, n)
        module.fx.intersection = lambda a, b: ('intersection', a, b)
        module.fx.make_fspn = lambda *args: ('noise', args)
        seen = []
        module.fx.noise_warp_field = lambda *args: seen.append(args) or ('warp', args)
        scene = module.load_scene('scene')
        self.assertEqual([o.kind for o in scene.objects], ['noise_warp'])
        self.assertAlmostEqual(seen[0][3], 0.25)
        self.assertEqual(seen[0][1][0], 'noise')
        self.assertEqual(scene.objects[0].bounds_min, (-1, -1, -1))

    def test_named_stamp_is_reused_and_unnamed_stamp_is_not_cached(self):
        module, sphere, _, _, _, _ = fixture()
        sphere.values.update({'pbvfx:stamp': True, 'pbvfx:cacheName': 'sphere_cache'})
        events = []
        class Grid:
            voxel_size = 0.1
            def active_world_bounds(self): return ((0, -1, -3), (4, 3, 2))
        grid = Grid()
        module.fx.world_space_to_bounds = lambda *args: args
        module.fx.stamp_grid_parallel = lambda *args: events.append('stamp') or grid
        module.fx.make_grid_field = lambda value: ('grid_field', value)
        module.fx.save_to_disk = lambda value, path: (events.append('save'), Path(path).write_text('grid'))
        module.fx.load_float_grid = lambda path: events.append('load') or grid
        with tempfile.TemporaryDirectory() as tmp:
            cache = Path(tmp) / 'caches'
            first = module.load_scene('scene', cache_dir=cache)
            self.assertFalse(first.objects[0].cache_hit)
            self.assertEqual(events, ['stamp', 'save'])
            self.assertTrue((cache / 'sphere_cache.vdb').is_file())
            second = module.load_scene('scene', cache_dir=cache)
            self.assertTrue(second.objects[0].cache_hit)
            self.assertEqual(events, ['stamp', 'save', 'load'])
            sphere.values.pop('pbvfx:cacheName')
            other = Path(tmp) / 'unnamed'
            module.load_scene('scene', cache_dir=other)
            self.assertFalse(other.exists())
            self.assertEqual(events[-1], 'stamp')

    def test_cache_rejects_duplicate_and_invalid_names(self):
        module, sphere, _, _, _, _ = fixture()
        stage = module.Usd.Stage.Open('scene')
        original = list(stage.Traverse())
        sphere.values.update({'pbvfx:stamp': True, 'pbvfx:cacheName': 'same'})
        other = Prim(Sphere, '/second', {'pbvfx:isf_type': 'isf_sphere', 'Radius': 1,
                     'pbvfx:stamp': True, 'pbvfx:cacheName': 'same'})
        stage.Traverse = lambda: original + [other]
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, 'duplicate'):
                module.load_scene('scene', cache_dir=tmp)
            self.assertEqual(list(Path(tmp).iterdir()), [])
            sphere.values['pbvfx:cacheName'] = '../unsafe'
            with self.assertRaisesRegex(ValueError, 'invalid pbvfx:cacheName'):
                module.load_scene('scene', cache_dir=tmp)

    def test_wisp_cache_skips_generation(self):
        module, sphere, _, _, _, _ = fixture()
        stage = module.Usd.Stage.Open('scene')
        original = list(stage.Traverse())
        prim = Prim(None, '/wisp', {'pbvfx:field_type': 'wisp', 'pbvfx:cacheName': 'wisp'})
        stage.Traverse = lambda: original + [prim]
        events = []
        class Grid:
            voxel_size = 0.1
            def __init__(self, *args): pass
            def active_world_bounds(self): return ((-1, -1, -1), (1, 1, 1))
        class Wisp:
            def __init__(self, *args): events.append('construct')
            def stamp(self, grid): events.append('stamp')
        module.fx.FloatGrid, module.fx.wisp_uniform = Grid, Wisp
        module.fx.FSVals = lambda *args: args
        module.fx.CoordinateSys = lambda: 'axes'
        module.fx.make_grid_field = lambda value: ('grid', value)
        module.fx.save_to_disk = lambda value, path: (events.append('save'), Path(path).write_text('grid'))
        module.fx.load_float_grid = lambda path: events.append('load') or Grid()
        with tempfile.TemporaryDirectory() as tmp:
            first = module.load_scene('scene', cache_dir=tmp)
            second = module.load_scene('scene', cache_dir=tmp)
            self.assertFalse(first.objects[1].cache_hit)
            self.assertTrue(second.objects[1].cache_hit)
            self.assertEqual(events, ['construct', 'stamp', 'save', 'load'])

    def test_named_mesh_cache_skips_voxelization(self):
        module, _, _, _, _, _ = fixture()
        stage = module.Usd.Stage.Open('scene')
        original = list(stage.Traverse())
        prim = Prim(Mesh, '/mesh', {'pbvfx:cacheName': 'mesh'})
        stage.Traverse = lambda: original + [prim]
        events = []
        class Grid:
            voxel_size = 0.1
            def active_world_bounds(self): return ((-1, -1, -1), (1, 1, 1))
        grid = Grid()
        fake = ModuleType('usd_mesh')
        fake.mesh_prim_to_field = lambda *args: self.fail('unexpected uncached mesh path')
        fake.mesh_prim_to_grid = lambda *args: events.append('voxelize') or grid
        fake.mesh_world_bounds = lambda *args: ((-1, -1, -1), (1, 1, 1))
        module.fx.make_grid_field = lambda value: ('grid', value)
        module.fx.save_to_disk = lambda value, path: (events.append('save'), Path(path).write_text('grid'))
        module.fx.load_float_grid = lambda path: events.append('load') or grid
        with tempfile.TemporaryDirectory() as tmp, patch.dict(sys.modules, {'usd_mesh': fake}):
            first = module.load_scene('scene', cache_dir=tmp)
            second = module.load_scene('scene', cache_dir=tmp)
            self.assertFalse(first.objects[1].cache_hit)
            self.assertTrue(second.objects[1].cache_hit)
            self.assertEqual(events, ['voxelize', 'save', 'load'])

    def test_unknown_tags_and_untagged_primitives_are_rejected(self):
        module, sphere, _, _, _, _ = fixture()
        sphere.values['pbvfx:isf_type'] = 'not_a_type'
        with self.assertRaisesRegex(ValueError, 'unknown pbvfx:isf_type'):
            module.load_scene('scene')
        del sphere.values['pbvfx:isf_type']
        with self.assertRaisesRegex(ValueError, 'untagged geometry must be a USD Mesh'):
            module.load_scene('scene')

    def test_multiple_lights_are_passed_to_renderer(self):
        module, _, _, light, _, calls = fixture()
        stage = module.Usd.Stage.Open('scene')
        prims = list(stage.Traverse())
        second = Prim(Light, '/light2', dict(light.values), Matrix((-3, 0, 2)))
        stage.Traverse = lambda: prims + [second]
        scene = module.load_scene('scene')
        scene.render('two_lights.exr')
        self.assertEqual(len(scene.lights), 2)
        self.assertEqual(calls[0][3], scene.lights)

    def test_cli_dispatches_general_renderer(self):
        module, _, _, _, _, calls = fixture()
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'nested' / 'render.exr'
            module.main(['scene.usda', '--output', str(output)])
            self.assertTrue(output.parent.is_dir())
            self.assertEqual(calls[0][-1], str(output))


if __name__ == '__main__': unittest.main()

