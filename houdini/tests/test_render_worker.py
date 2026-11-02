"""Exercise render/view sequencing with real child processes and fake programs."""
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from physvfx_houdini.render_worker import render_and_view


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
