"""Exercise render/view sequencing with real child processes and fake programs."""
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from physvfx_houdini.render_worker import render_and_view, render_progressively


class RenderWorkerTests(unittest.TestCase):
    def test_success_replaces_image_and_launches_viewer_with_image_flag(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp) / 'path with spaces'; directory.mkdir()
            script = directory / 'renderer.py'
            script.write_text("import sys\nfrom pathlib import Path\np = sys.argv[sys.argv.index('--output') + 1]\nPath(p).write_text('new image')\n")
            output = directory / 'image.exr'; output.write_text('old image')
            import subprocess
            real_popen = subprocess.Popen
            starts = []
            def intercept(args, *a, **kw):
                if args[0] == 'viewer':
                    starts.append(args)
                    return object()
                return real_popen(args, *a, **kw)
            with patch('subprocess.Popen', side_effect=intercept):
                render_and_view(sys.executable, str(script), 'scene.usda', str(output), 'viewer', 'cache')
            self.assertEqual(output.read_text(), 'new image')
            self.assertEqual(starts, [['viewer', '-image', str(output)]])
            self.assertEqual(list(directory.glob('.physvfx_render_*')), [])

    def test_progressive_updates_use_one_watched_viewer(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp); script = directory / 'renderer.py'
            script.write_text("import os, sys, time\nfrom pathlib import Path\np = Path(sys.argv[sys.argv.index('--preview-image') + 1])\no = Path(sys.argv[sys.argv.index('--output') + 1])\nt = Path(str(p) + '.tmp')\nt.write_text('pass one'); os.replace(t, p); time.sleep(0.5)\nt.write_text('pass ten'); os.replace(t, p); o.write_text('final image')\n")
            output = directory / 'result.exr'; output.write_text('old image')
            import subprocess
            real_popen = subprocess.Popen
            starts, seen = [], []
            class Viewer:
                def __init__(self, preview): self.preview = preview
                def wait(self):
                    seen.append(Path(self.preview).read_text())
                    return 0
            def intercept(args, *a, **kw):
                if args[0] == 'viewer':
                    starts.append(args)
                    seen.append(Path(args[2]).read_text())
                    return Viewer(args[2])
                return real_popen(args, *a, **kw)
            with patch('subprocess.Popen', side_effect=intercept):
                render_progressively(sys.executable, str(script), 'scene.usda', str(output), 'viewer', 'cache')
            self.assertEqual(len(starts), 1)
            self.assertEqual(starts[0][-1], '--watch')
            self.assertEqual(seen, ['pass one', 'pass ten'])
            self.assertEqual(output.read_text(), 'final image')
            self.assertEqual(list(directory.glob('.physvfx_preview_*')), [])
            self.assertEqual(list(directory.glob('.physvfx_render_*')), [])

    def test_progressive_failure_preserves_final_image(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp); script = directory / 'renderer.py'
            script.write_text('raise SystemExit(3)\n')
            output = directory / 'result.exr'; output.write_text('old image')
            import subprocess
            real_popen = subprocess.Popen
            starts = []
            def intercept(args, *a, **kw):
                if args[0] == 'viewer': starts.append(args); return object()
                return real_popen(args, *a, **kw)
            with patch('subprocess.Popen', side_effect=intercept):
                with self.assertRaisesRegex(RuntimeError, 'exit code 3'):
                    render_progressively(sys.executable, str(script), 'scene.usda', str(output), 'viewer', 'cache')
            self.assertEqual(starts, [])
            self.assertEqual(output.read_text(), 'old image')
            self.assertEqual(list(directory.glob('.physvfx_preview_*')), [])
            self.assertEqual(list(directory.glob('.physvfx_render_*')), [])

    def test_failed_render_keeps_previous_image_and_does_not_open_viewer(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp); script = directory / 'fail.py'
            script.write_text('raise SystemExit(3)\n')
            output = directory / 'image.exr'; output.write_text('old image')
            import subprocess
            real_popen = subprocess.Popen
            starts = []
            def intercept(args, *a, **kw):
                if args[0] == 'viewer':
                    starts.append(args)
                    return object()
                return real_popen(args, *a, **kw)
            with patch('subprocess.Popen', side_effect=intercept):
                with self.assertRaisesRegex(RuntimeError, 'exit code 3'):
                    render_and_view(sys.executable, str(script), 'scene.usda', str(output), 'viewer', 'cache')
            self.assertEqual(output.read_text(), 'old image')
            self.assertEqual(starts, [])
            self.assertEqual(list(directory.glob('.physvfx_render_*')), [])


if __name__ == '__main__': unittest.main()
