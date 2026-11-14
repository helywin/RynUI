"""Real finite native CLI and locked official Web API, isolated in owned processes."""
from __future__ import annotations

import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
from copy import deepcopy

from .protocol import ROOT, ProtocolError, read


class RunError(ProtocolError):
    def __init__(self, code, category, message):
        super().__init__(message)
        self.code = code
        self.category = category


def _descendants(pid):
    """Linux private compositor/test deliberately start separate process groups."""
    parents = {}
    if sys.platform.startswith('linux'):
        for path in Path('/proc').glob('[0-9]*/stat'):
            try:
                fields = path.read_text().rsplit(')', 1)[1].split()
                parents[int(path.parent.name)] = int(fields[1])
            except (OSError, ValueError, IndexError):
                continue
    selected = {pid}
    while True:
        added = {child for child, parent in parents.items() if parent in selected} - selected
        if not added:
            return selected
        selected.update(added)


def stop_tree(process):
    if os.name == 'nt':
        subprocess.run(['taskkill', '/T', '/F', '/PID', str(process.pid)], capture_output=True, timeout=15)
    else:
        owned = _descendants(process.pid)
        # SIGINT allows the existing Wayland wrapper's finally blocks to retire
        # its private compositor/socket/runtime. Kill any remaining descendants.
        try:
            os.killpg(process.pid, signal.SIGINT)
            process.wait(timeout=12)
        except (ProcessLookupError, subprocess.TimeoutExpired):
            pass
        for pid in sorted(owned, reverse=True):
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
    process.wait(timeout=15)


def process(argv, directory, timeout, *, failure_code=4, launch_id=None):
    directory.mkdir(parents=True, exist_ok=True)
    from .reporting import write_json
    write_json(directory / 'argv.json', [str(arg) for arg in argv])
    options = {'start_new_session': True} if os.name != 'nt' else {
        'creationflags': subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP}
    if launch_id is not None:
        options['env']={**os.environ,'RYNUI_CONTROL_LAUNCH_ID':launch_id}
    old_mode = None
    if os.name == 'nt':
        import ctypes
        old_mode = ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    try:
        with (directory / 'stdout.log').open('wb') as stdout, (directory / 'stderr.log').open('wb') as stderr:
            try:
                child = subprocess.Popen([str(a) for a in argv], cwd=ROOT, stdout=stdout, stderr=stderr, **options)
                if launch_id is not None:
                    import time
                    write_json(directory/'launch.json',dict(launch_id=launch_id,pid=child.pid,parent_pid=os.getpid(),
                        started_ns=time.time_ns(),argv=[str(a) for a in argv]))
            except OSError as error:
                raise RunError(3, 'environment', 'cannot start adapter: ' + str(error)) from error
            try:
                code = child.wait(timeout=timeout)
            except subprocess.TimeoutExpired as error:
                stop_tree(child)
                raise RunError(4, 'protocol', 'adapter timeout; owned process tree stopped') from error
            except BaseException:
                stop_tree(child)
                raise
        if code:
            # Wrapper startup failures (no stream) are environment failures;
            # native abnormal/timeout codes retain incomplete priority.
            raise RunError(failure_code, 'environment' if failure_code == 3 else 'protocol',
                           f'adapter exited {code}; see {directory}')
    finally:
        if old_mode is not None:
            import ctypes
            ctypes.windll.kernel32.SetErrorMode(old_mode)


def native_command(binary, definition, request, output, timeout, system=None):
    system = system or platform.system()
    command = [str(binary), '--page-acceptance', '--page-visual',
               '--page=' + definition['native']['route'], '--sample=' + definition['native']['sample_id'],
               '--locale=' + definition['fixture']['locale'],
               '--format-locale=' + definition['fixture']['format_locale'],
               '--direction=' + definition['fixture']['direction'],
               '--page-width=' + str(definition['fixture']['viewport']['width']),
               '--page-timeout=' + str(timeout), '--page-output=' + str(output / 'page-result.json'),
               '--page-scenario=' + str(request), '--page-observations=' + str(output / 'observations.jsonl')]
    if system == 'Linux':
        return [sys.executable, '-B', str(ROOT / 'tools/run_background_wayland.py'), '--log-dir',
                str(output / 'wayland'), '--timeout', str(timeout), '--', *command]
    if system != 'Windows':
        raise RunError(3, 'environment', 'native differential requires Linux or Windows')
    return command


def web_definition(definition):
    """Input-layer mapping only; fixture/expected/layout values are untouched."""
    result = deepcopy(definition)
    for action in result['actions']:
        if action['layer'] in ('native-core-dispatch', 'native-sdl-mapped'):
            action['layer'] = 'browser-input'
        elif action['layer'] == 'native-composition-seam':
            action['layer'] = 'browser-composition-seam'
        if action['kind'] == 'navigate' and action['parameters']['route'] == 'overview':
            # The reference has no Gallery shell. Leaving the tested sample
            # means unmounting its host, rather than inventing an overview UI.
            action['parameters'] = {'route': 'unmount', 'sample_id': ''}
    return result


class RealAdapters:
    """No fallback renderer, browser session, desktop, or hand-written reference."""
    def prepare(self, directory, options):
        from demo_reference_web import bootstrap, reference
        prepared = options.get('prepared')
        if prepared:
            prepared = Path(prepared)
            try:
                bootstrap.verify_prepared(prepared)
            except bootstrap.PreparationError as error:
                raise RunError(2, 'reference-drift', str(error)) from error
        else:
            prepared = directory / 'prepared'
            argv = [sys.executable, '-B', str(ROOT / 'tools/demo_reference_web/bootstrap.py'),
                    '--mode', 'BUNDLED', '--output', str(prepared)]
            if options.get('offline'):
                argv.append('--offline')
            process(argv, directory / 'prepare-process', 600, failure_code=3)
            bootstrap.verify_prepared(prepared)
        fonts = options.get('font_files')
        if fonts:
            fonts = read(fonts)
        else:
            try:
                fonts = reference.prepare_fonts(directory / 'fonts')
            except bootstrap.PreparationError as error:
                raise RunError(3, 'environment', str(error)) from error
        return {'prepared': str(prepared), 'fonts': fonts}

    def inventory(self, binary, output):
        from .inventory import require_exact, source_inventory
        process([binary, '--page-inventory=' + str(output)], output.parent / 'inventory-process', 30)
        if not output.is_file() or not output.stat().st_size:
            raise RunError(4, 'protocol', 'zero-exit inventory returned no current artifact')
        actual = read(output)
        require_exact(source_inventory(), actual)
        return actual

    def font_preflight(self, definition, prepared):
        from .font_requirements import preflight
        preflight(definition, prepared['fonts'])

    def native(self, definition, nonce, directory, prepared, options):
        from .reporting import write_json
        from .meta_test import fault_for_case
        directory.mkdir(parents=True)
        request = directory / 'request.json'
        write_json(request, {'schema_version': 1, 'nonce': nonce, 'definition': definition,
                             'font_files': prepared['fonts'],
                             'fault': fault_for_case(options.get('fault', 'none'), definition['identity'])})
        try:
            process(native_command(options['binary'], definition, request, directory, options['timeout']),
                    directory / 'process', options['timeout'] + 25)
        except RunError as error:
            log = directory / 'process/stderr.log'
            message = log.read_text(encoding='utf-8', errors='replace') if log.is_file() else ''
            if ('Background Wayland unavailable' in message or
                    'FileNotFoundError' in message and any(tool in message for tool in ('mutter', 'dbus-run-session'))):
                raise RunError(3, 'environment', 'private Wayland unavailable; no desktop fallback') from error
            raise
        return directory / 'observations.jsonl'

    def web_preflight(self, definition, nonce, prepared):
        from demo_reference_web import bootstrap, reference
        try:
            reference.validate_request({'schema_version': 1, 'nonce': nonce, 'definition': web_definition(definition),
                                        'font_files': prepared['fonts']}, bootstrap.verify_prepared(prepared['prepared']))
        except bootstrap.PreparationError as error:
            raise RunError(2, 'reference-drift', str(error)) from error

    def web(self, definition, nonce, directory, prepared, options):
        from .reporting import write_json
        from .meta_test import reference_cache_publish_allowed
        request = directory.parent / 'web-request.json'
        write_json(request, {'schema_version': 1, 'nonce': nonce, 'definition': web_definition(definition), 'font_files': prepared['fonts']})
        allow_publish = reference_cache_publish_allowed(options.get('fault', 'none')) and options.get('reference_publish', True)
        argv = [sys.executable, '-B', str(ROOT / 'tools/demo_reference_web/reference.py'), 'run',
                '--request', str(request), '--prepared', prepared['prepared'], '--output', str(directory),
                '--reference', options['reference']]
        if not allow_publish:
            argv.append('--no-cache-publish')
        process(argv, directory.parent / 'web-process', options['timeout'] + 120)
        result = read(directory / 'result.json')
        published = result.get('cache_published')
        if type(published) is not bool or published and not allow_publish:
            raise ProtocolError('Web reference cache publication violates run policy')
        self.reference_cache_published = getattr(self, 'reference_cache_published', False) or published
        return directory / 'observations.jsonl'

    def cleanup(self):
        # Each synchronous process owns and closes its own browser/compositor.
        # Timeout/cancellation is handled at process(), including grandchildren.
        pass
