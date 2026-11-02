"""Background renderer started by the Solaris Render and View node."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def render_and_view(python, script, usd, image, viewer, cache_dir):
    image = Path(image)
    fd, temp = tempfile.mkstemp(prefix='.physvfx_render_', suffix=image.suffix, dir=str(image.parent))
    os.close(fd)
    try:
        command = [python, '-u', script, usd, '--output', temp, '--cache-dir', cache_dir]
        print(f'[PhysVFX] Rendering {usd} to {image}', flush=True)
        result = subprocess.run(command, check=False)
        if result.returncode:
            raise RuntimeError(f'renderer failed with exit code {result.returncode}')
        if not os.path.isfile(temp) or os.path.getsize(temp) == 0:
            raise RuntimeError('renderer did not produce a non-empty image')
        os.replace(temp, image)
        print(f'[PhysVFX] Wrote {image}; opening image viewer.', flush=True)
        subprocess.Popen([viewer, '-image', str(image)])
    finally:
        if os.path.exists(temp): os.unlink(temp)


def main(argv=None):
    parser = argparse.ArgumentParser()
    for name in ('python', 'script', 'usd', 'image', 'viewer', 'cache-dir'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args(argv)
    try:
        render_and_view(args.python, args.script, args.usd, args.image, args.viewer, args.cache_dir)
    except (OSError, RuntimeError) as exc:
        print(f'[PhysVFX] Render and View failed: {exc}', file=sys.stderr, flush=True)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
