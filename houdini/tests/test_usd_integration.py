"""Run with OpenUSD (e.g. hython); no Houdini UI is required."""
from pathlib import Path
import tempfile
import unittest

from physvfx_houdini.author import author, AuthoringError
from physvfx_houdini.schema import defaults

try:
    from pxr import Gf, Sdf, Usd, UsdGeom
    HAS_USD = True
except ImportError:
    HAS_USD = False

try:
    import physics_vfx as fx
    import usd_sphere_scene as loader
    HAS_RENDERER = HAS_USD
except ImportError:
    HAS_RENDERER = False


@unittest.skipUnless(HAS_USD, 'requires OpenUSD Python bindings')
class RealUSDTests(unittest.TestCase):
    def test_real_usd_types_relationships_and_layer_composition(self):
        with tempfile.TemporaryDirectory() as tmp:
            layout_path = Path(tmp) / 'layout.usda'
            ops_path = Path(tmp) / 'operations.usda'
            stage = Usd.Stage.CreateNew(str(layout_path))
            UsdGeom.SetStageMetersPerUnit(stage, 1.0)
            author(stage, 'camera', defaults('camera'))
            author(stage, 'light', defaults('light'))
            author(stage, 'settings', defaults('settings'))
            a = defaults('field'); a.update(path='/World/A', stamp=True, cache_name='object_a', color_mode='display', color=(1, 0, 0))
            b = defaults('field'); b.update(path='/World/B')
            author(stage, 'field', a)
            prim_b = author(stage, 'field', b)
            UsdGeom.XformCommonAPI(prim_b).SetTranslate(Gf.Vec3d(0.5, 0, 0))
            stage.GetRootLayer().Save()
            stage = Usd.Stage.CreateNew(str(ops_path))
            stage.GetRootLayer().subLayerPaths.append(str(layout_path))
            values = defaults('operation'); values.update(path='/World/Cut', input_a='/World/A', input_b='/World/B')
            author(stage, 'operation', values)
            stage.GetRootLayer().Save()
            composed = Usd.Stage.Open(str(ops_path))
            self.assertEqual(composed.GetPrimAtPath('/World/A').GetAttribute('pbvfx:isf_type').Get(), 'isf_sphere')
            self.assertEqual(composed.GetPrimAtPath('/World/A').GetAttribute('pbvfx:cacheName').Get(), 'object_a')
            self.assertEqual(composed.GetPrimAtPath('/World/A').GetAttribute('primvars:displayColor').GetTypeName(), Sdf.ValueTypeNames.Color3fArray)
            self.assertEqual([str(x) for x in composed.GetPrimAtPath('/World/Cut').GetRelationship('pbvfx:inputs').GetTargets()],
                             ['/World/A', '/World/B'])
            self.assertEqual(composed.GetPrimAtPath('/World/Settings').GetAttribute('pbvfx:minStep').GetTypeName(), Sdf.ValueTypeNames.Float)

    def test_wisp_and_other_field_attributes_use_real_usd_types(self):
        stage = Usd.Stage.CreateInMemory()
        v = defaults('field'); v.update(kind='wisp', path='/World/Wisp', cache_name='wisp_01')
        prim = author(stage, 'field', v)
        self.assertEqual(prim.GetAttribute('pbvfx:field_type').Get(), 'wisp')
        preview = stage.GetPrimAtPath('/World/Wisp/__physvfx_bounds')
        self.assertTrue(preview.GetAttribute('pbvfx:preview').Get())
        self.assertEqual(preview.GetTypeName(), 'BasisCurves')
        self.assertEqual(len(preview.GetAttribute('points').Get()), 24)
        self.assertEqual(prim.GetAttribute('pbvfx:wispDots').GetTypeName(), Sdf.ValueTypeNames.Int)
        self.assertEqual(prim.GetAttribute('pbvfx:wispNoise1Translation').GetTypeName(), Sdf.ValueTypeNames.Float3)
        v.update(kind='plane', path='/World/Plane')
        plane = author(stage, 'field', v)
        self.assertEqual(plane.GetAttribute('pbvfx:boundsMin').GetTypeName(), Sdf.ValueTypeNames.Point3f)
        self.assertEqual(plane.GetAttribute('pbvfx:normal').GetTypeName(), Sdf.ValueTypeNames.Float3)

    @unittest.skipUnless(HAS_RENDERER, 'requires the built physics_vfx module')
    def test_authored_usd_loads_into_physvfx(self):
        with tempfile.TemporaryDirectory() as tmp:
            filename = Path(tmp) / 'scene.usda'
            stage = Usd.Stage.CreateNew(str(filename))
            UsdGeom.SetStageMetersPerUnit(stage, 1.0)
            author(stage, 'camera', defaults('camera'))
            author(stage, 'light', defaults('light'))
            author(stage, 'settings', defaults('settings'))
            a = defaults('field'); a.update(path='/World/A')
            b = defaults('field'); b.update(path='/World/B')
            author(stage, 'field', a)
            bprim = author(stage, 'field', b)
            UsdGeom.XformCommonAPI(bprim).SetTranslate(Gf.Vec3d(0.5, 0, 0))
            operation = defaults('operation'); operation.update(path='/World/Cut', input_a='/World/A', input_b='/World/B')
            author(stage, 'operation', operation)
            stage.GetRootLayer().Save()
            scene = loader.load_scene(filename, cache_dir=Path(tmp) / 'cache')
            self.assertEqual([obj.kind for obj in scene.objects], ['cutout'])
            self.assertLess(scene.sdf.eval(fx.Vector(-0.75, 0, 0)), 0)
            self.assertGreater(scene.sdf.eval(fx.Vector(0.5, 0, 0)), 0)


if __name__ == '__main__': unittest.main()
