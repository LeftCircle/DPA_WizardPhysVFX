"""Interactive Houdini preview: debounce edits and reuse one watched viewer.

Houdini APIs and USD export run only on the UI event loop. Rendering happens in
an external process using unique temporary files for each scene revision.
"""
import math
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time


_sessions = {}


def _signal_process(process, force=False):
    if process is None or process.poll() is not None:
        return
    try:
        if os.name == 'posix':
            os.killpg(process.pid, signal.SIGKILL if force else signal.SIGTERM)
        elif force:
            process.kill()
        else:
            process.terminate()
    except ProcessLookupError:
        pass


def _positive_setting(value, name, lower, upper=None):
    value = float(value)
    if not math.isfinite(value) or value < lower or (upper is not None and value > upper):
        raise ValueError(f'{name} is out of range')
    return value


class LivePreview:
    def __init__(self, node, hou):
        self.node, self.hou = node, hou
        self.key = node.sessionId()
        self.directory = Path(tempfile.mkdtemp(prefix='physvfx_live_'))
        self.image = self.directory / 'live.exr'
        self.revision = 1
        self.changed_at = time.monotonic()
        self.dirty = True
        self.job = None
        self.viewer = None
        self.viewer_path = None
        self.watched = {}
        self.needs_scan = True
        self.snapshotting = False
        self.closed = False
        self.frame = hou.frame()
        # Store bound methods, so the identical callback can be removed.
        self._event_callback = self._on_event
        self._idle_callback = self.tick
        self._sync_watchers()
        hou.ui.addEventLoopCallback(self._idle_callback)

    def _sync_watchers(self):
        roots = [self.node.parent()]
        for path in ('/stage', '/obj', '/mat', '/ch'):
            root = self.hou.node(path)
            if root is not None:
                roots.append(root)
        event_types = self.hou.nodeEventType
        events = tuple(getattr(event_types, name) for name in
                       ('ParmTupleChanged', 'InputRewired', 'BeingDeleted',
                        'FlagChanged', 'ChildCreated', 'ChildDeleted') if hasattr(event_types, name))
        for root in roots:
            try:
                nodes = (root,) + tuple(root.allSubChildren())
            except Exception:
                nodes = (root,)
            for node in nodes:
                try:
                    key = node.sessionId()
                    if key not in self.watched:
                        watched_events = events
                        if key == self.key and hasattr(event_types, 'InputDataChanged'):
                            watched_events += (event_types.InputDataChanged,)
                        node.addEventCallback(watched_events, self._event_callback)
                        self.watched[key] = (node, watched_events)
                except Exception as exc:
                    print(f'[PhysVFX] Could not watch a node for preview: {exc}', file=sys.stderr, flush=True)
        self.needs_scan = False

    def _on_event(self, node=None, event_type=None, **kwargs):
        if self.closed:
            return
        types = self.hou.nodeEventType
        if event_type == types.BeingDeleted:
            try:
                if node.sessionId() == self.key:
                    self.close()
                    return
                self.watched.pop(node.sessionId(), None)
            except Exception:
                pass
            self.needs_scan = True
        if event_type in (types.ChildCreated, types.ChildDeleted, types.InputRewired):
            self.needs_scan = True
        # Setting the toggle off invokes its callback as well as this event.
        if self.snapshotting:
            return
        self.mark_dirty()

    def mark_dirty(self):
        self.revision += 1
        self.changed_at = time.monotonic()
        self.dirty = True

    def _publish(self, source, viewer_path):
        if not source.is_file() or source.stat().st_size == 0:
            return
        temp = self.directory / 'display_next.exr'
        shutil.copyfile(source, temp)
        os.replace(temp, self.image)
        if self.viewer is not None and self.viewer.poll() is None and self.viewer_path != viewer_path:
            _signal_process(self.viewer)
            self.viewer = None
        if self.viewer is None or self.viewer.poll() is not None:
            self.viewer = subprocess.Popen([str(viewer_path), '-image', str(self.image), '--watch'],
                                           start_new_session=(os.name == 'posix'))
            self.viewer_path = viewer_path

    def _start_job(self, values):
        from .nodes import _render_programs, _renderer_environment
        resolution = _positive_setting(values['live_resolution_scale'], 'Resolution Scale', 1e-6, 1.0)
        steps = _positive_setting(values['live_step_multiplier'], 'Step Multiplier', 1.0)
        voxel = _positive_setting(values['live_voxel_multiplier'], 'Voxel Multiplier', 1.0)
        dots = _positive_setting(values['live_dot_fraction'], 'Wisp Dot Fraction', 1e-6, 1.0)
        python, script, repo, viewer = _render_programs(values)
        generation = self.revision
        usd = self.directory / f'scene_{generation}.usda'
        final = self.directory / f'final_{generation}.exr'
        preview = self.directory / f'preview_{generation}.exr'
        self.snapshotting = True
        try:
            stage = self.node.stage()
            if stage is None or not stage.Flatten().Export(str(usd)):
                raise RuntimeError('Could not export the composed Solaris stage')
        finally:
            self.snapshotting = False
        command = [python, '-u', str(script), str(usd), '--output', str(final),
                   '--preview-image', str(preview), '--live-preview',
                   '--live-resolution-scale', str(resolution), '--live-step-multiplier', str(steps),
                   '--live-voxel-multiplier', str(voxel), '--live-dot-fraction', str(dots)]
        try:
            process = subprocess.Popen(command, cwd=str(repo), env=_renderer_environment(repo),
                                       start_new_session=(os.name == 'posix'))
        except Exception:
            usd.unlink(missing_ok=True)
            raise
        self.job = {'process': process, 'revision': generation, 'usd': usd, 'final': final,
                    'preview': preview, 'viewer': viewer, 'preview_time': None, 'cancel_at': None}
        self.dirty = False
        print(f'[PhysVFX] Continuous preview rendering revision {generation}.', flush=True)

    def tick(self):
        if self.closed:
            return
        try:
            parm = self.node.parm('pv_continuous_preview')
            if parm is None or not parm.eval():
                self.close()
                return
            frame = self.hou.frame()
            if frame != self.frame:
                self.frame = frame
                self.mark_dirty()
            if self.needs_scan:
                self._sync_watchers()
            now = time.monotonic()
            if self.job:
                job = self.job
                process = job['process']
                if job['revision'] != self.revision and process.poll() is None:
                    if job['cancel_at'] is None:
                        _signal_process(process)
                        job['cancel_at'] = now
                    elif now - job['cancel_at'] > 1.0:
                        _signal_process(process, force=True)
                if job['revision'] == self.revision and job['preview'].is_file():
                    stamp = job['preview'].stat().st_mtime_ns
                    if stamp != job['preview_time']:
                        self._publish(job['preview'], job['viewer'])
                        job['preview_time'] = stamp
                code = process.poll()
                if code is not None:
                    if job['revision'] == self.revision:
                        if code == 0 and job['final'].is_file() and job['final'].stat().st_size:
                            self._publish(job['final'], job['viewer'])
                        elif code != 0:
                            print(f'[PhysVFX] Continuous preview renderer exited with {code}; see the console.', file=sys.stderr, flush=True)
                        else:
                            print('[PhysVFX] Continuous preview produced no image.', file=sys.stderr, flush=True)
                    for key in ('usd', 'preview', 'final'):
                        job[key].unlink(missing_ok=True)
                    for partial in self.directory.glob('.' + job['preview'].stem + '.physvfx_*' + job['preview'].suffix):
                        partial.unlink(missing_ok=True)
                    self.job = None
            if self.dirty and self.job is None:
                try:
                    from .nodes import read_values
                    values = read_values(self.node, 'render_view')
                    delay = _positive_setting(values['live_delay'], 'Delay After Edit', 0.0)
                    if now - self.changed_at >= delay:
                        self._start_job(values)
                except Exception as exc:
                    # Do not spawn a failure loop; the next edit retries.
                    self.dirty = False
                    print(f'[PhysVFX] Continuous preview failed to start: {exc}', file=sys.stderr, flush=True)
        except Exception as exc:
            print(f'[PhysVFX] Continuous preview stopped: {exc}', file=sys.stderr, flush=True)
            self.close()

    def close(self):
        if self.closed:
            return
        self.closed = True
        _sessions.pop(self.key, None)
        try:
            self.hou.ui.removeEventLoopCallback(self._idle_callback)
        except Exception:
            pass
        for node, events in list(self.watched.values()):
            try:
                node.removeEventCallback(events, self._event_callback)
            except Exception:
                pass
        self.watched.clear()
        for process in ((self.job or {}).get('process'), self.viewer):
            if process is None:
                continue
            try:
                _signal_process(process)
                try:
                    process.wait(timeout=0.5)
                except subprocess.TimeoutExpired:
                    _signal_process(process, force=True)
                    process.wait(timeout=0.5)
            except (OSError, subprocess.TimeoutExpired):
                pass
        shutil.rmtree(self.directory, ignore_errors=True)
        print('[PhysVFX] Continuous preview stopped.', flush=True)


def start(node):
    import hou
    key = node.sessionId()
    session = _sessions.get(key)
    if session is None or session.closed:
        session = LivePreview(node, hou)
        _sessions[key] = session
    return session


def stop(node):
    session = _sessions.get(node.sessionId())
    if session is not None:
        session.close()
