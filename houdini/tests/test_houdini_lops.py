"""Headless Houdini smoke test: run using hython, not in an artist session."""
import unittest

try:
    import hou
    from pxr import Sdf
    HAS_HOUDINI = True
except ImportError:
    HAS_HOUDINI = False

from physvfx_houdini import create_node


@unittest.skipUnless(HAS_HOUDINI, 'requires hython and Houdini USD bindings')
class HoudiniLOPTests(unittest.TestCase):
    def setUp(self):
        if hou.isUIAvailable():
            self.skipTest('run in headless hython, not an interactive artist session')
        self.network = hou.node('/stage')
        if self.network is None:
            self.skipTest('Houdini session has no /stage LOP network')
        self.created = []

    def tearDown(self):
        for node in reversed(self.created):
            node.destroy()

    def add(self, kind, source=None):
        node = create_node(kind, network=self.network, source=source)
        self.created.append(node)
        return node

    def test_shadow_cache_node_authors_region_and_button(self):
        region = self.add('shadow_region')
        region.parm('pv_path').set('/World/ShadowHead')
        cache = self.add('shadow_cache', region)
        cache.parm('pv_shadow_cache_dir').set('/tmp/physvfx_shadow_test')
        cache.parm('pv_shadow_cache_load').set(1)
        stage = cache.stage()
        prim = stage.GetPrimAtPath('/World/ShadowHead')
        self.assertTrue(prim.GetAttribute('pbvfx:shadowCacheLoad').Get())
        self.assertEqual(prim.GetAttribute('pbvfx:shadowCacheDir').Get(), '/tmp/physvfx_shadow_test')
        self.assertIsNotNone(cache.parm('pv_save_shadow_cache'))
        guide = stage.GetPrimAtPath('/World/ShadowHead/__physvfx_bounds')
        self.assertEqual(tuple(guide.GetAttribute('primvars:displayColor').Get()[0]), (1, 0, 0))

    def test_wisp_spline_ramp_and_guide_stage(self):
        node = self.add('wisp_spline')
        node.parm('pv_path').set('/World/Waterfall')
        node.parm('pv_splineMin_density').set(1)
        node.parm('pv_splineMax_density').set(3)
        node.parm('pv_ramp_subdivisions').set(4)
        node.parmTuple('pv_guide_position2').set((0, 4, 0))
        stage = node.stage()
        spline = stage.GetPrimAtPath('/World/Waterfall')
        self.assertEqual(spline.GetAttribute('pbvfx:field_type').Get(), 'wisp_spline')
        density = spline.GetAttribute('pbvfx:spline_density').Get()
        self.assertEqual(len(density), 5)
        self.assertAlmostEqual(density[2], 2)
        guide = stage.GetPrimAtPath('/World/Waterfall/__physvfx_spline_guide')
        self.assertTrue(guide.GetAttribute('pbvfx:preview').Get())
        self.assertEqual(len(guide.GetAttribute('points').Get()), 20)

    def test_render_view_lop_passes_stage_and_has_button(self):
        camera = self.add('camera')
        node = self.add('render_view', camera)
        self.assertTrue(node.stage().GetPrimAtPath('/World/Camera'))
        self.assertIsNotNone(node.parm('pv_render_view'))
        self.assertIsNotNone(node.parm('pv_usd_path'))
        self.assertIsNotNone(node.parm('pv_image_path'))
        self.assertIsNotNone(node.parm('pv_progressive_preview'))
        self.assertIsNotNone(node.parm('pv_continuous_preview'))
        self.assertFalse(node.parm('pv_continuous_preview').eval())

    def test_node_chain_authors_composed_usd(self):
        camera = self.add('camera')
        light = self.add('light', camera)
        a = self.add('field', light)
        a.parm('pv_path').set('/World/A')
        a.parm('pv_stamp').set(1)
        a.parm('pv_cache_name').set('houdini_a')
        b = self.add('field', a)
        b.parm('pv_path').set('/World/B')
        op = self.add('operation', b)
        op.parm('pv_path').set('/World/Cut')
        op.parm('pv_input_a').set('/World/A')
        op.parm('pv_input_b').set('/World/B')
        settings = self.add('settings', op)
        stage = settings.stage()
        self.assertIsNotNone(stage)
        preview = stage.GetPrimAtPath('/World/A/__physvfx_bounds')
        self.assertTrue(preview)
        self.assertEqual(preview.GetTypeName(), 'BasisCurves')
        self.assertTrue(preview.GetAttribute('pbvfx:preview').Get())
        self.assertEqual(len(preview.GetAttribute('points').Get()), 24)
        self.assertEqual(stage.GetPrimAtPath('/World/A').GetAttribute('pbvfx:stamp').Get(), True)
        self.assertEqual(stage.GetPrimAtPath('/World/A').GetAttribute('pbvfx:cacheName').Get(), 'houdini_a')
        self.assertEqual(stage.GetPrimAtPath('/World/Cut').GetAttribute('pbvfx:operation').Get(), 'cutout')
        self.assertEqual([str(p) for p in stage.GetPrimAtPath('/World/Cut').GetRelationship('pbvfx:inputs').GetTargets()],
                         ['/World/A', '/World/B'])
        self.assertEqual(stage.GetPrimAtPath('/World/Settings').GetAttribute('pbvfx:minStep').GetTypeName(),
                         Sdf.ValueTypeNames.Float)


if __name__ == '__main__': unittest.main()
