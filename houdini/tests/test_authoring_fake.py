"""Offline contract tests using small USD stand-ins (not a USD implementation)."""
import sys
import types
import unittest
from unittest.mock import patch

from physvfx_houdini.author import AuthoringError, author
from physvfx_houdini.schema import FIELD_KINDS, OPERATIONS, defaults, parameters


class Path(str):
    absoluteRootPath = '/'
    def IsAbsolutePath(self): return self.startswith('/')
    def IsPrimPath(self):
        return bool(self) and all(part and part.replace('_', 'a').isalnum() for part in self[1:].split('/'))


class Attribute:
    def __init__(self, name, typ): self.name, self.typ, self.value = name, typ, None
    def Set(self, value): self.value = value
    def Get(self): return self.value
    def GetTypeName(self): return self.typ


class Relationship:
    def __init__(self): self.targets = []
    def SetTargets(self, targets): self.targets = list(targets)
    def GetTargets(self): return self.targets


class Prim:
    def __init__(self, path, type_name):
        self.path, self.type_name, self.attrs, self.rels = str(path), type_name, {}, {}
    def IsInstanceProxy(self): return False
    def GetPath(self): return self.path
    def GetAttribute(self, name): return self.attrs.get(name)
    def CreateAttribute(self, name, typ, custom=True):
        return self.attrs.setdefault(name, Attribute(name, typ))
    def CreateRelationship(self, name, custom=True): return self.rels.setdefault(name, Relationship())
    def IsA(self, cls):
        if cls is Gprim: return self.type_name in ('Sphere', 'Mesh', 'Cube', 'Cone', 'BasisCurves')
        return self.type_name == cls.type_name


class Stage:
    def __init__(self): self.prims = {}
    def GetPrimAtPath(self, path): return self.prims.get(str(path))
    def DefinePrim(self, path, typ):
        return self.prims.setdefault(str(path), Prim(path, typ))


class Schema:
    type_name = ''
    def __init__(self, prim): self.prim = prim
    def __getattr__(self, method):
        if method.startswith('Create') and method.endswith('Attr'):
            return lambda: self.prim.CreateAttribute(method[6:-4], 'native')
        if method.startswith('Create') and method.endswith('Rel'):
            return lambda: self.prim.CreateRelationship(method[6:-3])
        raise AttributeError(method)

class Sphere(Schema): type_name = 'Sphere'
class Mesh(Schema): type_name = 'Mesh'
class Gprim(Schema): pass
class BasisCurves(Schema):
    type_name = 'BasisCurves'
    def SetWidthsInterpolation(self, value): pass
class Imageable(Schema): pass
class Matrix:
    def GetDeterminant(self): return 1.0
    def GetInverse(self): return self
    def Transform(self, value): return value
class XformCache:
    def __init__(self, time): pass
    def GetLocalToWorldTransform(self, prim): return Matrix()
class Camera(Schema): type_name = 'Camera'
class SphereLight(Schema): type_name = 'SphereLight'
class LightAPI(Schema): pass
class Settings(Schema): type_name = 'RenderSettings'

class PrimvarsAPI:
    def __init__(self, prim): self.prim = prim
    def CreatePrimvar(self, name, typ, interpolation):
        return self.prim.CreateAttribute('primvars:' + name, typ)


def fake_pxr():
    pxr = types.ModuleType('pxr')
    names = types.SimpleNamespace(**{x: x for x in
        ('Float', 'Float3', 'Point3f', 'Color3f', 'Color3fArray', 'Int', 'Bool', 'String', 'Token')})
    pxr.Sdf = types.SimpleNamespace(Path=Path, ValueTypeNames=names)
    pxr.Gf = types.SimpleNamespace(Vec3f=lambda *x: tuple(x), Vec3d=lambda *x: tuple(x), Vec2f=lambda *x: tuple(x), Vec2i=lambda *x: tuple(x))
    pxr.Usd = types.SimpleNamespace(TimeCode=types.SimpleNamespace(Default=lambda: None))
    pxr.UsdGeom = types.SimpleNamespace(Sphere=Sphere, Mesh=Mesh, Gprim=Gprim, Camera=Camera,
                                       BasisCurves=BasisCurves, Imageable=Imageable, XformCache=XformCache,
                                       PrimvarsAPI=PrimvarsAPI,
                                       Tokens=types.SimpleNamespace(constant='constant', perspective='perspective',
                                                                    linear='linear', inherited='inherited', invisible='invisible'))
    pxr.UsdLux = types.SimpleNamespace(SphereLight=SphereLight, LightAPI=LightAPI)
    pxr.UsdRender = types.SimpleNamespace(Settings=Settings)
    return pxr


class AuthoringContractTests(unittest.TestCase):
    def setUp(self):
        self.modules = patch.dict(sys.modules, {'pxr': fake_pxr()})
        self.modules.start()
        self.addCleanup(self.modules.stop)
        self.stage = Stage()

    def test_interfaces_cover_all_loader_fields(self):
        field = defaults('field')
        self.assertEqual({x[0] for x in FIELD_KINDS}, {
            'isf_sphere', 'isf_torus', 'isf_cone', 'isf_box', 'isf_icosahedron',
            'isf_steiner_patch', 'isf_ellipse', 'isf_cylinder', 'mesh', 'plane', 'wisp'})
        for prefix in ('wispNoise1', 'wispNoise2'):
            for suffix in ('Roughness', 'Octaves', 'Translation', 'Frequency', 'FrequencyJump'):
                self.assertIn(prefix + suffix, field)
        for kind in ('field', 'operation', 'camera', 'light', 'settings'):
            names = [p.name for p in parameters(kind)]
            self.assertEqual(len(names), len(set(names)), kind)

    def test_each_field_kind_authors_correct_tags_and_options(self):
        for kind, _ in FIELD_KINDS:
            with self.subTest(kind=kind):
                stage = Stage()
                v = defaults('field'); v.update(kind=kind, path='/World/Thing',
                    color_mode='display', color=(0.2, 0.4, 0.6), stamp=True, cache_name='thing_1')
                if kind == 'mesh':
                    stage.DefinePrim('/World/Thing', 'Mesh').CreateAttribute('points', 'Point3fArray').Set([(-1, -2, -3), (1, 2, 3)])
                prim = author(stage, 'field', v)
                tag = 'pbvfx:isf_type' if kind.startswith('isf_') else 'pbvfx:field_type'
                if kind != 'mesh': self.assertEqual(prim.attrs[tag].value, kind)
                else: self.assertEqual(prim.attrs['pbvfx:meshHalfWidth'].value, 3.0)
                self.assertEqual(prim.attrs['pbvfx:stamp'].value, True)
                self.assertEqual(prim.attrs['pbvfx:cacheName'].value, 'thing_1')
                self.assertEqual(prim.attrs['primvars:displayColor'].value, [(0.2, 0.4, 0.6)])
                if kind == 'plane':
                    self.assertEqual(prim.attrs['pbvfx:boundsMin'].typ, 'Point3f')
                if kind == 'wisp':
                    self.assertEqual(prim.attrs['pbvfx:wispDots'].value, 100000)
                    self.assertIn('pbvfx:wispNoise2Translation', prim.attrs)

    def test_preview_geometry_tracks_field_bounds_and_can_be_hidden(self):
        v = defaults('field'); v.update(kind='isf_torus', path='/World/Torus', majorRadius=2.0, minorRadius=0.5)
        prim = author(self.stage, 'field', v)
        proxy = self.stage.GetPrimAtPath('/World/Torus/__physvfx_bounds')
        surface = self.stage.GetPrimAtPath('/World/Torus/__physvfx_surface')
        self.assertEqual(surface.type_name, 'Mesh')
        self.assertTrue(surface.attrs['pbvfx:preview'].value)
        self.assertGreater(len(surface.attrs['Points'].value), 0)
        self.assertEqual(proxy.type_name, 'BasisCurves')
        self.assertTrue(proxy.attrs['pbvfx:preview'].value)
        points = proxy.attrs['Points'].value
        self.assertEqual(len(points), 24)
        self.assertEqual(min(point[0] for point in points), -2.5)
        self.assertEqual(max(point[0] for point in points), 2.5)
        v['majorRadius'] = 3.0
        author(self.stage, 'field', v)
        self.assertEqual(max(point[0] for point in proxy.attrs['Points'].value), 3.5)
        v['show_preview'] = False
        author(self.stage, 'field', v)
        self.assertEqual(proxy.attrs['Visibility'].value, 'invisible')
        self.assertEqual(surface.attrs['Visibility'].value, 'invisible')
        self.assertEqual(prim.attrs['pbvfx:isf_type'].value, 'isf_torus')

    def test_torus_surface_tracks_authored_axis(self):
        v = defaults('field'); v.update(kind='isf_torus', path='/World/T', majorRadius=2.0, minorRadius=0.5)
        author(self.stage, 'field', v)
        surface = self.stage.GetPrimAtPath('/World/T/__physvfx_surface')
        points = surface.attrs['Points'].value
        extent = [max(abs(p[i]) for p in points) for i in range(3)]
        self.assertGreater(extent[0], 2.2)
        self.assertLess(extent[1], 0.65)
        v['normal'] = (1.0, 0.0, 0.0)
        author(self.stage, 'field', v)
        points = surface.attrs['Points'].value
        extent = [max(abs(p[i]) for p in points) for i in range(3)]
        self.assertLess(extent[0], 0.65)
        self.assertGreater(extent[1], 2.2)

    def test_reserved_preview_path_is_not_overwritten(self):
        self.stage.DefinePrim('/World/Thing/__physvfx_bounds', 'Mesh')
        v = defaults('field'); v.update(path='/World/Thing', kind='isf_torus')
        with self.assertRaisesRegex(AuthoringError, 'reserved preview path'):
            author(self.stage, 'field', v)

    def test_all_wisp_parameters_are_authored(self):
        v = defaults('field'); v.update(kind='wisp', path='/World/Wisp', cache_name='named')
        prim = author(self.stage, 'field', v)
        expected = ['wispDots', 'wispDensity', 'wispClump', 'wispPscale', 'wispSeed',
                    'wispMin', 'wispMax', 'wispDisplacement']
        for prefix in ('wispNoise1', 'wispNoise2'):
            expected.extend(prefix + suffix for suffix in
                            ('Roughness', 'Octaves', 'Translation', 'Frequency', 'FrequencyJump'))
        for name in expected:
            self.assertIn('pbvfx:' + name, prim.attrs)

    def test_sphere_radius_inherits_usd_unless_overridden(self):
        v = defaults('field')
        prim = author(self.stage, 'field', v)
        self.assertNotIn('pbvfx:radius', prim.attrs)
        v.update(override_radius=True, radius=2.5)
        self.assertEqual(author(self.stage, 'field', v).attrs['pbvfx:radius'].value, 2.5)

    def test_operations_write_ordered_relationships_and_correct_parameters(self):
        a = defaults('field'); a.update(path='/World/A')
        b = defaults('field'); b.update(path='/World/B')
        author(self.stage, 'field', a); author(self.stage, 'field', b)
        for op, _ in OPERATIONS:
            with self.subTest(operation=op):
                v = defaults('operation'); v.update(path='/World/Op_' + op, operation=op,
                    input_a='/World/A', input_b='/World/B')
                prim = author(self.stage, 'operation', v)
                self.assertEqual(prim.attrs['pbvfx:operation'].value, op)
                expected = ['/World/A'] if op in ('pyro', 'noise_warp') else ['/World/A', '/World/B']
                self.assertEqual(prim.rels['pbvfx:inputs'].targets, expected)
                if op in ('add', 'subtract', 'noise_warp'):
                    self.assertEqual(prim.attrs['pbvfx:boundsMax'].typ, 'Point3f')
                if op == 'pyro': self.assertIn('pbvfx:iterations', prim.attrs)
                if op == 'noise_warp': self.assertIn('pbvfx:normal', prim.attrs)

    def test_camera_light_settings_and_shadow_override(self):
        camera = author(self.stage, 'camera', defaults('camera'))
        self.assertEqual(camera.attrs['Projection'].value, 'perspective')
        self.assertEqual(camera.attrs['ClippingRange'].value, (0.1, 100.0))
        guide = self.stage.GetPrimAtPath('/World/Camera/__physvfx_camera_range')
        self.assertTrue(guide.attrs['pbvfx:preview'].value)
        self.assertEqual(guide.type_name, 'BasisCurves')
        v = defaults('camera'); v['show_camera_preview'] = False
        author(self.stage, 'camera', v)
        self.assertEqual(guide.attrs['Visibility'].value, 'invisible')
        v = defaults('light'); v.update(override_shadow=True, shadowKappa=0.7)
        light = author(self.stage, 'light', v)
        self.assertEqual(light.attrs['pbvfx:shadowKappa'].value, 0.7)
        settings = author(self.stage, 'settings', defaults('settings'))
        self.assertEqual(settings.rels['Camera'].targets, ['/World/Camera'])
        self.assertEqual(settings.attrs['Resolution'].value, (960, 540))
        self.assertEqual(settings.attrs['pbvfx:densityVoxelSize'].value, 0.025)

    def test_signed_pyro_and_explicit_bounds(self):
        v = defaults('field'); v['path'] = '/World/A'
        author(self.stage, 'field', v)
        op = defaults('operation')
        op.update(operation='pyro', input_a='/World/A', amplitude=-0.5,
                  use_pyro_bounds=True, bounds_min=(-4, -3, -2), bounds_max=(4, 3, 2))
        prim = author(self.stage, 'operation', op)
        self.assertEqual(prim.attrs['pbvfx:amplitude'].value, -0.5)
        self.assertTrue(prim.attrs['pbvfx:usePyroBounds'].value)
        self.assertEqual(prim.attrs['pbvfx:boundsMin'].value, (-4, -3, -2))

    def test_camera_guide_points_use_ray_distance(self):
        import math
        from physvfx_houdini.preview import camera_range_lines
        lines = camera_range_lines(2, 8, 20, 10, 25)
        for line in lines:
            for point in line:
                self.assertTrue(any(math.isclose(math.sqrt(sum(x*x for x in point)), r)
                                    for r in (2, 8)))

    def test_custom_color_and_validation_errors(self):
        v = defaults('field'); v.update(color_mode='physvfx', color=(1, 0.2, 0.3))
        prim = author(self.stage, 'field', v)
        self.assertEqual(prim.attrs['pbvfx:color'].typ, 'Color3f')
        prim.CreateAttribute('primvars:displayColor', 'Color3fArray').Set([(1, 1, 1)])
        with self.assertRaisesRegex(AuthoringError, 'takes precedence'):
            author(self.stage, 'field', v)
        v.update(path='/World/Bad', cache_name='../unsafe')
        with self.assertRaisesRegex(AuthoringError, 'Cache name'):
            author(self.stage, 'field', v)
        v.update(path='/World/Bad', cache_name='', kind='plane', bounds_min=(1, 0, 0), bounds_max=(0, 1, 1))
        with self.assertRaisesRegex(AuthoringError, 'bounds minimum'):
            author(self.stage, 'field', v)
        op = defaults('operation'); op.update(input_a='/World/Missing', input_b='/World/PhysVFXField')
        with self.assertRaisesRegex(AuthoringError, 'does not exist'):
            author(self.stage, 'operation', op)


if __name__ == '__main__': unittest.main()
