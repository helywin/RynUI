"""Load only the locked comparison library from an owned test directory."""
from __future__ import annotations

import hashlib
import importlib
import json
from pathlib import Path
import platform
import shutil
import sys
import urllib.request
import zipfile

from .protocol import ROOT, ProtocolError, output_path, read

LOCK = Path(__file__).with_name('comparison-dependencies.lock.json')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def wheel_profile():
    system = {'Linux': 'linux', 'Windows': 'windows'}.get(platform.system())
    if system is None or platform.machine().lower() not in ('x86_64', 'amd64'):
        raise ProtocolError('comparison library has no locked wheel for this platform')
    lock = read(LOCK)
    package = lock['libraries']['Pillow'][system + '-x64']
    # This tests binary compatibility, not an exact interpreter patch version.
    abi = 'cp' + str(sys.version_info.major) + str(sys.version_info.minor)
    if sys.implementation.name != 'cpython' or package['python_abi'] != abi:
        raise ProtocolError('no compatible locked Pillow wheel for the active Python ABI')
    return package


def verify(directory, package):
    directory = Path(directory).resolve()
    for relative, expected in package['files'].items():
        path = directory / relative
        if not path.is_relative_to(directory) or not path.is_file() or sha(path) != expected:
            raise ProtocolError('comparison library file differs: ' + relative)
    actual = {p.relative_to(directory).as_posix() for p in directory.rglob('*')
              if p.is_file() and '__pycache__' not in p.parts}
    if actual != set(package['files']):
        raise ProtocolError('comparison library installed file set differs')


def prepare(output, *, archive=None):
    output = output_path(output)
    package = wheel_profile()
    if output.exists():
        raise ProtocolError('comparison preparation directory must be fresh')
    output.mkdir(parents=True)
    wheel = output / package['filename']
    if archive:
        shutil.copyfile(archive, wheel)
    else:
        with urllib.request.urlopen(package['url'], timeout=60) as source, wheel.open('wb') as target:
            shutil.copyfileobj(source, target)
    if sha(wheel) != package['sha256']:
        raise ProtocolError('comparison library archive hash differs')
    site = output / 'site-packages'
    site.mkdir()
    with zipfile.ZipFile(wheel) as contents:
        for member in contents.infolist():
            path = site / member.filename
            if not path.resolve().is_relative_to(site.resolve()):
                raise ProtocolError('comparison wheel contains an escaping path')
            if not member.is_dir():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(contents.read(member))
    verify(site, package)
    (output / 'runtime.json').write_text(json.dumps({'python': sys.version, 'platform': platform.platform(),
        'package': 'Pillow', 'version': package['version'], 'wheel_sha256': package['sha256']}, indent=2) + '\n')
    return site


def image_module(directory):
    directory = output_path(directory).resolve()
    package = wheel_profile()
    verify(directory, package)
    if 'PIL' in sys.modules and not Path(sys.modules['PIL'].__file__).resolve().is_relative_to(directory):
        raise ProtocolError('another Pillow was imported before the locked comparison library')
    sys.path.insert(0, str(directory))
    image = importlib.import_module('PIL.Image')
    if image.__version__ != package['version'] or not Path(image.__file__).resolve().is_relative_to(directory):
        raise ProtocolError('comparison library import differs from verified files')
    return image
