"""Exercise render/view sequencing with real child processes and fake programs."""
import sys
import os
import signal
import subprocess
import time
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from physvfx_houdini.render_worker import render_and_view, render_progressively, main


class RenderWorkerTests(unittest.TestCase):
    def test_frame_range_renders_in_order_with_separate_caches(self):
        import json
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            script = folder / 'renderer.py'
            script.write_text('import pathlib,sys\n'
                'out=pathlib.Path(sys.argv[sys.argv.index("--output")+1])\n'
                'scene=pathlib.Path(sys.argv[1])\n'
                'out.write_text(scene.read_text())\n')
            frames = []
            for n in (1, 3, 5):
                usd = folder / f'scene.{n}.usda'; usd.write_text(str(n))
                frames.append({'frame': n, 'usd': str(usd), 'image': str(folder / f'image.{n}.exr')})
            manifest = folder / 'frames.json'; manifest.write_text(json.dumps(frames))
            real_popen = subprocess.Popen
            viewer_calls = []
            def intercept(args, *a, **kw):
                if args[0] == 'viewer': viewer_calls.append(args); return object()
                return real_popen(args, *a, **kw)
            args = ['--python', sys.executable, '--script', str(script), '--usd', 'ignored',
                    '--image', 'ignored', '--viewer', 'viewer', '--cache-dir', 'unused',
                    '--sequence-manifest', str(manifest)]
            with patch('subprocess.Popen', side_effect=intercept):
                self.assertEqual(main(args), 0)
            self.assertEqual([Path(e['image']).read_text() for e in frames], ['1', '3', '5'])
            self.assertEqual(viewer_calls, [['viewer', '-image', frames[-1]['image']]])
            self.assertFalse(manifest.exists())

    def test_frame_range_stops_on_failure_and_does_not_open_viewer(self):
        import json
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            script = folder / 'renderer.py'; script.write_text('raise SystemExit(4)\n')
            output = folder / 'image.exr'; output.write_text('old')
            manifest = folder / 'frames.json'
            manifest.write_text(json.dumps([{'frame': 1, 'usd': 'scene.usda', 'image': str(output)}]))
            args = ['--python', sys.executable, '--script', str(script), '--usd', 'ignored',
                    '--image', 'ignored', '--viewer', 'viewer', '--cache-dir', 'unused',
                    '--sequence-manifest', str(manifest)]
            self.assertEqual(main(args), 1)
            self.assertEqual(output.read_text(), 'old')
            self.assertFalse(manifest.exists())
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
                def poll(self): return 0
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

    @unittest.skipUnless(os.name == 'posix', 'POSIX process-group cancellation')
    def test_cancel_stops_renderer_and_preserves_previous_image(self):
        worker = Path(__file__).resolve().parents[1] / 'python' / 'physvfx_houdini' / 'render_worker.py'
        for progressive in (False, True):
            with self.subTest(progressive=progressive), tempfile.TemporaryDirectory() as tmp:
                directory = Path(tmp); marker = directory / 'started'; script = directory / 'slow.py'
                script.write_text('import pathlib, sys, time\n'
                    'if "--preview-image" in sys.argv:\n'
                    ' p=pathlib.Path(sys.argv[sys.argv.index("--preview-image")+1])\n'
                    ' p.write_text("preview")\n'
                    ' (p.parent / ("."+p.stem+".physvfx_123_0"+p.suffix)).write_text("partial")\n'
                    'pathlib.Path(' + repr(str(marker)) + ').write_text("started")\n'
                    'time.sleep(60)\n')
                output = directory / 'image.exr'; output.write_text('old image')
                command = [sys.executable, '-u', str(worker), '--python', sys.executable,
                           '--script', str(script), '--usd', 'scene.usda', '--image', str(output),
                           '--viewer', 'viewer', '--cache-dir', 'cache']
                if progressive: command.append('--progressive')
                proc = subprocess.Popen(command, start_new_session=True, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, text=True)
                try:
                    deadline = time.monotonic() + 5
                    while not marker.exists() and proc.poll() is None and time.monotonic() < deadline:
                        time.sleep(0.05)
                    self.assertTrue(marker.exists(), 'renderer did not start')
                    os.killpg(proc.pid, signal.SIGTERM)
                    output_text, _ = proc.communicate(timeout=5)
                    self.assertEqual(proc.returncode, 130, output_text)
                    self.assertIn('Render cancelled', output_text)
                    self.assertEqual(output.read_text(), 'old image')
                    self.assertEqual(list(directory.glob('.physvfx_render_*')), [])
                    self.assertEqual(list(directory.glob('.physvfx_preview_*')), [])
                    self.assertEqual(list(directory.glob('..physvfx_preview_*')), [])
                finally:
                    if proc.poll() is None:
                        os.killpg(proc.pid, signal.SIGKILL)
                        proc.communicate(timeout=5)

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
