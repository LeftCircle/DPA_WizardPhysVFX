"""Exercise live preview lifecycle with real child processes and fake HOM."""
import os
from pathlib import Path
import sys
import tempfile
import time
import types
import unittest
from unittest.mock import patch

from physvfx_houdini import live_preview


class Parm:
    def __init__(self, value): self.value = value
    def eval(self): return self.value


class Node:
    next_id = 0
    def __init__(self, parent=None):
        Node.next_id += 1
        self.id, self._parent, self.callbacks = Node.next_id, parent, []
        self.children = []
        self.enabled = Parm(True)
        self.scene_text = 'first'
    def sessionId(self): return self.id
    def parent(self): return self._parent
    def allSubChildren(self):
        return tuple(self.children) + tuple(c for n in self.children for c in n.allSubChildren())
    def addEventCallback(self, events, callback): self.callbacks.append((events, callback))
    def removeEventCallback(self, events, callback): self.callbacks.remove((events, callback))
    def trigger(self, event):
        for events, callback in list(self.callbacks):
            if event in events: callback(node=self, event_type=event)
    def parm(self, name): return self.enabled if name == 'pv_continuous_preview' else None
    def stage(self):
        node = self
        class Stage:
            def Flatten(self): return self
            def Export(self, path): Path(path).write_text(node.scene_text); return True
        return Stage()


class UI:
    def __init__(self): self.callbacks = []
    def addEventLoopCallback(self, fn): self.callbacks.append(fn)
    def removeEventLoopCallback(self, fn): self.callbacks.remove(fn)


class LivePreviewTests(unittest.TestCase):
    def make_hou(self, root):
        events = types.SimpleNamespace(**{name: name for name in
            ('ParmTupleChanged', 'InputRewired', 'InputDataChanged', 'BeingDeleted', 'FlagChanged', 'ChildCreated', 'ChildDeleted')})
        return types.SimpleNamespace(nodeEventType=events, node=lambda p: root if p == '/stage' else None,
                                     frame=lambda: 1, ui=UI())

    def pump(self, session, condition, timeout=8):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            session.tick()
            if condition(): return
            time.sleep(0.02)
        self.fail('Timed out waiting for live preview')

    @unittest.skipUnless(os.name == 'posix', 'uses POSIX child process groups')
    def test_replaces_inflight_render_and_reuses_viewer(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            script = folder / 'renderer.py'
            script.write_text('import pathlib,sys,time\n'
                'value=pathlib.Path(sys.argv[1]).read_text()\n'
                'preview=pathlib.Path(sys.argv[sys.argv.index("--preview-image")+1])\n'
                'out=pathlib.Path(sys.argv[sys.argv.index("--output")+1])\n'
                'preview.write_text(value+":preview")\n'
                'time.sleep(.6)\n'
                'out.write_text(value+":final")\n')
            viewer = folder / 'viewer'
            viewer.write_text('#!/usr/bin/env python3\nimport pathlib,time\n'
                              'p=pathlib.Path(' + repr(str(folder / 'launches')) + ')\n'
                              'p.open("a").write("start\\n")\ntime.sleep(30)\n')
            viewer.chmod(0o755)
            root = Node(); render = Node(root); upstream = Node(root)
            root.children = [render, upstream]
            hou = self.make_hou(root)
            values = {'live_resolution_scale': .25, 'live_step_multiplier': 4,
                      'live_voxel_multiplier': 2, 'live_dot_fraction': .25, 'live_delay': 0}
            with patch('physvfx_houdini.nodes.read_values', return_value=values), \
                 patch('physvfx_houdini.nodes._render_programs', return_value=(sys.executable, script, folder, viewer)), \
                 patch('physvfx_houdini.nodes._renderer_environment', return_value=os.environ.copy()):
                session = live_preview.LivePreview(render, hou)
                try:
                    self.pump(session, lambda: session.image.exists() and session.viewer is not None)
                    self.assertEqual(session.image.read_text(), 'first:preview')
                    old = session.job['process']
                    viewer_pid = session.viewer.pid
                    render.scene_text = 'second'
                    upstream.trigger(hou.nodeEventType.ParmTupleChanged)
                    self.pump(session, lambda: session.image.exists() and session.image.read_text() == 'second:final')
                    self.assertIsNotNone(old.poll())
                    self.assertEqual(session.viewer.pid, viewer_pid)
                    self.assertEqual((folder / 'launches').read_text().count('start'), 1)
                    self.assertEqual(session.image.read_text(), 'second:final')
                finally:
                    session.close()
                self.assertEqual(hou.ui.callbacks, [])
                self.assertFalse(session.directory.exists())
                self.assertIsNotNone(session.viewer.poll())

    def test_bad_quality_can_be_fixed_without_restarting_preview(self):
        root = Node(); render = Node(root); upstream = Node(root)
        root.children = [render, upstream]
        hou = self.make_hou(root)
        values = {'live_delay': -1}
        with patch('physvfx_houdini.nodes.read_values', return_value=values):
            session = live_preview.LivePreview(render, hou)
            try:
                with patch('sys.stderr'):
                    session.tick()
                self.assertFalse(session.closed)
                self.assertFalse(session.dirty)
                values['live_delay'] = 0
                upstream.trigger(hou.nodeEventType.ParmTupleChanged)
                with patch.object(session, '_start_job') as start:
                    session.tick()
                    start.assert_called_once_with(values)
            finally:
                session.close()

    def test_upstream_input_changes_and_node_deletion(self):
        root = Node(); render = Node(root)
        root.children = [render]
        hou = self.make_hou(root)
        session = live_preview.LivePreview(render, hou)
        render.trigger(hou.nodeEventType.InputDataChanged)
        self.assertEqual(session.revision, 2)
        render.trigger(hou.nodeEventType.BeingDeleted)
        self.assertTrue(session.closed)
        self.assertEqual(hou.ui.callbacks, [])
        self.assertEqual(root.callbacks, [])
        self.assertFalse(session.directory.exists())


if __name__ == '__main__': unittest.main()
