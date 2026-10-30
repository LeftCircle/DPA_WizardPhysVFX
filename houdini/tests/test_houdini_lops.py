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
