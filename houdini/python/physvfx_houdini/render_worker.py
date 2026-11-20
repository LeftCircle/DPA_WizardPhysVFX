"""Background renderer started by the Solaris Render and View node."""
import argparse
import glob
import json
import math
import os
import signal
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def viewer_command(viewer, image, gamma=1.0, watch=False):
    gamma = float(gamma)
    if not math.isfinite(gamma) or gamma <= 0:
        raise ValueError('Viewer Gamma must be finite and greater than zero')
    command = [str(viewer), '-image', str(image)]
    if watch: command.append('--watch')
    if gamma != 1.0: command.extend(['--display-gamma', str(gamma)])
    return command


def render_and_view(python, script, usd, image, viewer, cache_dir, open_viewer=True, static_cache_dir=None, gamma=1.0):
    image = Path(image)
    fd, temp = tempfile.mkstemp(prefix='.physvfx_render_', suffix=image.suffix, dir=str(image.parent))
    os.close(fd)
    try:
        command = [python, '-u', script, usd, '--output', temp, '--cache-dir', cache_dir]
        if static_cache_dir is not None:
            command += ['--static-cache-dir', static_cache_dir]
        print(f'[PhysVFX] Rendering {usd} to {image}', flush=True)
        result = subprocess.run(command, check=False)
        if result.returncode:
            raise RuntimeError(f'renderer failed with exit code {result.returncode}')
        if not os.path.isfile(temp) or os.path.getsize(temp) == 0:
            raise RuntimeError('renderer did not produce a non-empty image')
        os.replace(temp, image)
        print(f'[PhysVFX] Wrote {image}.', flush=True)
        if open_viewer:
            subprocess.Popen(viewer_command(viewer, image, gamma))
    finally:
        if os.path.exists(temp): os.unlink(temp)


def render_progressively(python, script, usd, image, viewer, cache_dir, gamma=1.0):
    """Show atomic preview updates without replacing the previous final image."""
    image = Path(image)
    output_fd, output = tempfile.mkstemp(prefix='.physvfx_render_', suffix=image.suffix, dir=str(image.parent))
    os.close(output_fd)
    preview = None
    viewer_proc = None
    renderer = None
    viewer_attempted = False
    try:
        preview_fd, preview = tempfile.mkstemp(prefix='.physvfx_preview_', suffix=image.suffix, dir=str(image.parent))
        os.close(preview_fd)
        os.unlink(preview)  # The first atomic publish creates this unique path.
        command = [python, '-u', script, usd, '--output', output, '--cache-dir', cache_dir,
                   '--preview-image', preview]
        print(f'[PhysVFX] Preparing progressive render of {usd}; the viewer opens after the first pass.', flush=True)
        renderer = subprocess.Popen(command)
        while True:
            if not viewer_attempted and os.path.isfile(preview) and os.path.getsize(preview) > 0:
                viewer_attempted = True
                try:
                    viewer_proc = subprocess.Popen(viewer_command(viewer, preview, gamma, watch=True))
                except OSError as exc:
                    print(f'[PhysVFX] Could not open preview viewer: {exc}', file=sys.stderr, flush=True)
            result = renderer.poll()
            if result is not None:
                break
            time.sleep(0.1)
        if result:
            raise RuntimeError(f'renderer failed with exit code {result}')
        if not os.path.isfile(output) or os.path.getsize(output) == 0:
            raise RuntimeError('renderer did not produce a non-empty image')
        os.replace(output, image)
        print(f'[PhysVFX] Wrote {image}.', flush=True)
        if not viewer_attempted:
            print('[PhysVFX] No progressive image was published; opening the final image.', flush=True)
            subprocess.Popen(viewer_command(viewer, image, gamma))
        # Keep the watched file available while its viewer is open. This worker
        # is separate from Houdini, which remains responsive.
        if viewer_proc is not None:
            viewer_proc.wait()
    finally:
        for child in (renderer, viewer_proc):
            if child is not None and child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()
        for path in (output, preview):
            if path and os.path.exists(path): os.unlink(path)
        if preview:
            preview_path = Path(preview)
            # A cancelled native writer may leave its own uniquely named
            # atomic-write temporary file behind.
            pattern = glob.escape('.' + preview_path.stem + '.physvfx_') + '*' + glob.escape(preview_path.suffix)
            for partial in preview_path.parent.glob(pattern):
                partial.unlink(missing_ok=True)


class RenderCancelled(Exception):
    pass


def _cancel_on_signal(signum, frame):
    raise RenderCancelled()


def main(argv=None):
    parser = argparse.ArgumentParser()
    for name in ('python', 'script', 'usd', 'image', 'viewer', 'cache-dir'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--progressive', action='store_true')
    parser.add_argument('--sequence-manifest')
    parser.add_argument('--display-gamma', type=float, default=1.0)
    args = parser.parse_args(argv)
    try:
        viewer_command(args.viewer, args.image, args.display_gamma)
    except ValueError as exc:
        parser.error(str(exc))
    signal.signal(signal.SIGTERM, _cancel_on_signal)
    try:
        if args.sequence_manifest:
            try:
                with open(args.sequence_manifest, encoding='utf-8') as source:
                    frames = json.load(source)
                for entry in frames:
                    print(f"[PhysVFX] Rendering frame {entry['frame']}", flush=True)
                    # Dynamic caches are frame-local. Explicitly static named
                    # caches use the persistent directory chosen by the user.
                    with tempfile.TemporaryDirectory(prefix='physvfx_frame_cache_') as cache:
                        render_and_view(args.python, args.script, entry['usd'], entry['image'], args.viewer, cache, False, args.cache_dir, args.display_gamma)
                if frames:
                    subprocess.Popen(viewer_command(args.viewer, frames[-1]['image'], args.display_gamma))
            finally:
                os.unlink(args.sequence_manifest)
        else:
            if args.progressive:
                render_progressively(args.python, args.script, args.usd, args.image, args.viewer, args.cache_dir, args.display_gamma)
            else:
                render_and_view(args.python, args.script, args.usd, args.image, args.viewer, args.cache_dir, gamma=args.display_gamma)
    except RenderCancelled:
        print('[PhysVFX] Render cancelled.', flush=True)
        return 130
    except (OSError, RuntimeError) as exc:
        print(f'[PhysVFX] Render and View failed: {exc}', file=sys.stderr, flush=True)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
