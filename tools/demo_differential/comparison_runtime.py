"""Explicit Windows CPython/Pillow preparation, runnable from the Web's Python 3.10.

Discovery reports ABI-verified installed interpreters; it never chooses SYSTEM
implicitly. BUNDLED owns an official embeddable interpreter entirely under out/.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys

from .comparison_dependencies import LOCK, sha, verify
from .protocol import ROOT, ProtocolError, output_path, read


def runtime_profile():
    if platform.system() != 'Windows' or platform.machine().lower() not in ('amd64', 'x86_64'):
        raise ProtocolError('portable comparison interpreter is locked only for Windows x64')
    return read(LOCK)['python_runtime']['win-x64']


def probe_interpreter(executable):
    executable = Path(executable).resolve(strict=True)
    script = ("import json,sys,sysconfig,struct; print(json.dumps({"
              "'executable':sys.executable,'implementation':sys.implementation.name,"
              "'version':list(sys.version_info[:3]),'platform':sys.platform,"
              "'bits':struct.calcsize('P')*8,'soabi':sysconfig.get_config_var('SOABI'),"
              "'gil_disabled':bool(sysconfig.get_config_var('Py_GIL_DISABLED'))}))")
    flags = {'creationflags': subprocess.CREATE_NO_WINDOW} if sys.platform == 'win32' else {}
    try:
        result = json.loads(subprocess.check_output([str(executable), '-I', '-S', '-B', '-c', script],
                            text=True, timeout=30, **flags))
        if (result['implementation'] != 'cpython' or result['version'][:2] != [3, 14]
                or result['platform'] != 'win32' or result['bits'] != 64
                or result['soabi'] != runtime_profile()['soabi'] or result['gil_disabled']
                or Path(result['executable']).resolve() != executable):
            raise ProtocolError('comparison interpreter requires CPython 3.14 / cp314-win_amd64 ABI')
    except (subprocess.SubprocessError, OSError, ValueError, KeyError) as error:
        raise ProtocolError('comparison interpreter probe failed: ' + str(error)) from error
    return result


def discover_interpreters():
    """Only current interpreter and PythonCore registrations; no global PATH search."""
    runtime_profile()
    import winreg
    candidates = [Path(sys.executable)]
    for hive in (winreg.HKEY_CURRENT_USER, winreg.HKEY_LOCAL_MACHINE):
        for key in ('3.14', '3.14-64'):
            try:
                with winreg.OpenKey(hive, 'Software\\Python\\PythonCore\\' + key + '\\InstallPath') as handle:
                    try:
                        candidate = winreg.QueryValueEx(handle, 'ExecutablePath')[0]
                    except OSError:
                        candidate = str(Path(winreg.QueryValueEx(handle, None)[0]) / 'python.exe')
                    candidates.append(Path(candidate))
            except OSError:
                continue
    found = []
    for candidate in dict.fromkeys(candidates):
        try:
            found.append(probe_interpreter(candidate))
        except (ProtocolError, OSError):
            continue
    return found


def path_configuration():
    # No import site, user site, environment, registry or unverified Pillow path.
    # Only the stdlib, extension DLL directory, and this checkout's tool modules.
    return ('python314.zip\n.\n' + str(ROOT / 'tools') + '\n').encode('utf-8')


def runner_configuration():
    # An explicitly selected SYSTEM embed may have another application's _pth.
    # Bind imports to this checkout without editing that interpreter's files.
    return ("import runpy, sys\n"
            "sys.path.insert(0, " + repr(str(ROOT / 'tools')) + ")\n"
            "sys.argv[0] = " + repr(str(ROOT / 'tools/run_demo_differential.py')) + "\n"
            "runpy.run_path(sys.argv[0], run_name='__main__')\n").encode('utf-8')


def verify_interpreter(directory, *, configured):
    package = runtime_profile()
    expected = dict(package['files'])
    if configured:
        expected['python314._pth'] = hashlib.sha256(path_configuration()).hexdigest()
    verify(directory, {'files': expected})


def _smoke(executable, site):
    script = ("import sys,io,json; sys.path.insert(0," + repr(str(ROOT / 'tools')) + "); "
              "from demo_differential.comparison_dependencies import image_module; "
              "Image=image_module(" + repr(str(site)) + "); "
              "im=Image.new('RGBA',(3,2),(12,34,56,255)); "
              "buffers=[io.BytesIO(),io.BytesIO()]; "
              "[(im.save(b,format=f),b.seek(0)) for b,f in zip(buffers,('PNG','BMP'))]; "
              "assert all(Image.open(b).convert('RGBA').tobytes()==im.tobytes() for b in buffers); "
              "print(json.dumps({'pillow':Image.__version__,'origin':Image.__file__,"
              "'python':sys.version,'isolated':sys.flags.isolated,'path':sys.path}))")
    flags = {'creationflags': subprocess.CREATE_NO_WINDOW} if sys.platform == 'win32' else {}
    try:
        return json.loads(subprocess.check_output([str(executable), '-I', '-S', '-B', '-c', script],
                          text=True, timeout=60, **flags))
    except (subprocess.SubprocessError, OSError, ValueError) as error:
        raise ProtocolError('locked Pillow CPU import/PNG/BMP smoke failed: ' + str(error)) from error


def prepare_runtime(output, *, mode, python=None, offline=False, downloads=None):
    package = runtime_profile()
    if mode not in ('BUNDLED', 'SYSTEM'):
        raise ProtocolError('explicit BUNDLED or SYSTEM interpreter mode required')
    if (mode == 'BUNDLED' and python) or (mode == 'SYSTEM' and not python):
        raise ProtocolError('BUNDLED forbids interpreter override; SYSTEM requires explicit python executable')
    output = output_path(output)
    if output.exists():
        raise ProtocolError('comparison runtime directory must be fresh')
    from demo_reference_web.bootstrap import fetch, extract, PreparationError
    wheel = read(LOCK)['libraries']['Pillow']['win-x64']
    cache = ROOT / 'out/demo-differential/comparison-archives'
    cache.mkdir(parents=True, exist_ok=True)
    if downloads:
        for path in Path(downloads).iterdir():
            if path.is_file() and sha(path) in (package['sha256'], wheel['sha256']):
                target = cache / sha(path)
                if not target.exists():
                    shutil.copyfile(path, target)
                    target.chmod(0o444)
    # Validate an explicit SYSTEM interpreter before any network is attempted.
    if mode == 'SYSTEM':
        probe_interpreter(python)
    try:
        interpreter_archive = fetch(package, cache, offline) if mode == 'BUNDLED' else None
        wheel_archive = fetch(wheel, cache, offline)
    except PreparationError as error:
        raise ProtocolError(str(error)) from error
    output.mkdir(parents=True)
    if mode == 'BUNDLED':
        interpreter = output / 'python'
        extract(interpreter_archive, interpreter)
        verify_interpreter(interpreter, configured=False)
        (interpreter / 'python314._pth').write_bytes(path_configuration())
        verify_interpreter(interpreter, configured=True)
        python = interpreter / package['executable']
    python = Path(python).resolve()
    probe = probe_interpreter(python)
    if mode == 'BUNDLED' and '.'.join(map(str, probe['version'])) != package['version']:
        raise ProtocolError('bundled interpreter version differs')
    script = ("import sys; sys.path.insert(0," + repr(str(ROOT / 'tools')) + "); "
              "from demo_differential.comparison_dependencies import prepare; "
              "prepare(" + repr(str(output / 'comparison')) + ",archive=" + repr(str(wheel_archive)) +
              ",offline=True)")
    flags = {'creationflags': subprocess.CREATE_NO_WINDOW} if sys.platform == 'win32' else {}
    subprocess.run([str(python), '-I', '-S', '-B', '-c', script], check=True, timeout=60, **flags)
    site = output / 'comparison/site-packages'
    runner = output / 'run_demo_differential.py'
    runner.write_bytes(runner_configuration())
    record = {'schema_version': 1, 'mode': mode, 'host': 'win-x64', 'lock_sha256': sha(LOCK),
              'python': str(python), 'python_sha256': sha(python), 'probe': probe,
              'python_flags': ['-E', '-s', '-S', '-B'], 'comparison_site': str(site),
              'runner': str(runner), 'runner_sha256': sha(runner),
              'smoke': _smoke(python, site)}
    (output / 'runtime.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    verify_runtime(output)
    return record


def verify_runtime(output):
    output = output_path(output)
    record = read(output / 'runtime.json')
    if record['lock_sha256'] != sha(LOCK) or record['host'] != 'win-x64':
        raise ProtocolError('comparison runtime lock/platform drift')
    if record['mode'] not in ('BUNDLED', 'SYSTEM'):
        raise ProtocolError('comparison runtime mode drift')
    runner = output / 'run_demo_differential.py'
    if (record['runner'] != str(runner) or runner.read_bytes() != runner_configuration()
            or record['runner_sha256'] != sha(runner) or record['python_flags'] != ['-E', '-s', '-S', '-B']):
        raise ProtocolError('comparison runner path/configuration drift')
    python = Path(record['python'])
    if record['mode'] == 'BUNDLED':
        if python != output / 'python/python.exe':
            raise ProtocolError('bundled comparison executable path drift')
        verify_interpreter(python.parent, configured=True)
    if sha(python) != record['python_sha256'] or probe_interpreter(python) != record['probe']:
        raise ProtocolError('comparison interpreter drift')
    site = output_path(record['comparison_site'])
    if site != output / 'comparison/site-packages':
        raise ProtocolError('comparison site path drift')
    verify(site, read(LOCK)['libraries']['Pillow']['win-x64'])
    _smoke(python, site)
    return record
