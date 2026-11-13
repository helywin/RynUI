"""Fail-closed, test-only preparation. All writable state lives in this checkout out/."""
from __future__ import annotations

import argparse
import base64
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse
from urllib.request import urlopen
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
import sys
sys.path.insert(0, str(ROOT / 'tools'))
from demo_differential.protocol import ProtocolError, output_path, read


class PreparationError(ValueError):
    pass


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False).encode()


def digest(value):
    return hashlib.sha256(canonical(value)).hexdigest()


def require(condition, message):
    if not condition:
        raise PreparationError(message)


def load_lock():
    lock = read(HERE / 'dependencies.lock.json')
    require(lock['schema_version'] == 1, 'dependency lock version')
    for key, filename in [('package_sha256', 'package.json'), ('pnpm_lock_sha256', 'pnpm-lock.yaml')]:
        require(sha(HERE / filename) == lock[key], 'dependency lock drift: ' + filename)
    package = read(HERE / 'package.json')
    for name, version in package['dependencies'].items():
        require(re.fullmatch(r'\d+\.\d+\.\d+(?:-[\w.]+)?', version), 'non-exact dependency: ' + name)
        require(any(row['name'] == name and row['version'] == version for row in lock['npm']),
                'missing locked dependency: ' + name)
    require(platform.python_implementation() == lock['python']['implementation'], 'Python implementation')
    require(sys.version_info >= (3, 10), 'Python >= 3.10 required')
    return lock


def host_platform(lock):
    host = {'Linux': 'linux', 'Windows': 'win'}.get(platform.system(), 'unsupported') + '-' + {
        'x86_64': 'x64', 'AMD64': 'x64', 'amd64': 'x64'}.get(platform.machine(), platform.machine())
    require(host in lock['browser']['artifacts'] and host in lock['node']['artifacts'],
            'no locked tool/browser artifact for platform ' + host)
    return host


def platform_packages(lock, host):
    system, cpu = host.split('-', 1)
    system = {'win': 'win32'}.get(system, system)
    def matches(values, value):
        return value not in [v[1:] for v in values if v.startswith('!')] and (
            not any(not v.startswith('!') for v in values) or value in values)
    return [r for r in lock['npm'] if r['name'] != 'pnpm'
            and matches(r.get('os', []), system) and matches(r.get('cpu', []), cpu)]


def fetch(record, cache, offline=False):
    cache = output_path(cache)
    cache.mkdir(parents=True, exist_ok=True)
    path = cache / record['sha256']
    if path.exists():
        require(sha(path) == record['sha256'], 'corrupt archive cache: ' + str(path))
        require('bytes' not in record or path.stat().st_size == record['bytes'], 'archive byte count mismatch')
        return path
    require(not offline, 'offline archive missing: ' + record['url'])
    with tempfile.NamedTemporaryFile(dir=cache, delete=False) as stream:
        pending = Path(stream.name)
        try:
            with urlopen(record['url'], timeout=90) as response:
                shutil.copyfileobj(response, stream)
            stream.close()
            require(sha(pending) == record['sha256'], 'archive SHA256 mismatch: ' + record['url'])
            require('bytes' not in record or pending.stat().st_size == record['bytes'], 'archive byte count mismatch')
            if 'integrity' in record:
                algorithm, expected = record['integrity'].split('-', 1)
                actual = base64.b64encode(hashlib.new(algorithm, pending.read_bytes()).digest()).decode()
                require(actual == expected, 'archive integrity mismatch')
            os.replace(pending, path)
            path.chmod(0o444)
        finally:
            pending.unlink(missing_ok=True)
    return path


def extract(archive, destination):
    destination.mkdir(parents=True)
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as source:
            for member in source.infolist():
                target = (destination / member.filename).resolve()
                require(target.is_relative_to(destination.resolve()), 'archive path escape')
                require((member.external_attr >> 16) & 0o170000 != 0o120000, 'zip symlink not allowed')
            source.extractall(destination)
            for member in source.infolist():
                path = destination / member.filename
                if path.is_file():
                    path.chmod((member.external_attr >> 16) & 0o777 or 0o644)
    else:
        with tarfile.open(archive) as source:
            # Explicit data filter is also available on supported 3.10 security releases.
            source.extractall(destination, filter='data')


def file_map(root):
    return {p.relative_to(root).as_posix(): sha(p) for p in sorted(Path(root).rglob('*'))
            if p.is_file() and not p.is_symlink()}


def tree_digest(root):
    return digest(file_map(root))


def readonly(root):
    for path in Path(root).rglob('*'):
        if path.is_file():
            path.chmod(0o555 if path.stat().st_mode & 0o111 else 0o444)
    for path in sorted(Path(root).rglob('*'), reverse=True):
        if path.is_dir():
            path.chmod(0o555)
    Path(root).chmod(0o555)


def verify_source(source, lock):
    source = Path(source)
    require(read(source / 'package.json')['version'] == lock['source']['version'], 'official package/tag mismatch')
    require(sha(source / 'LICENSE') == lock['source']['license_sha256'], 'official license drift')
    for name, expected in lock['source']['build_inputs'].items():
        require(sha(source / name) == expected, 'official build input drift: ' + name)
    for name, expected in lock['source']['demos'].items():
        require(sha(source / name) == expected, 'official demo drift: ' + name)
    require(tree_digest(source / 'components') == lock['source']['components_tree_sha256'],
            'official component/import/Token source drift')
    return source


def run(argv, env, logfile, cwd=None):
    with Path(logfile).open('ab') as stream:
        result = subprocess.run([str(x) for x in argv], cwd=cwd, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=300)
    require(result.returncode == 0, 'dependency command failed; see ' + str(logfile))


def version(argv, expected):
    actual = subprocess.check_output([str(x) for x in argv], text=True, timeout=20).strip()
    require(actual == expected, f'version mismatch: {actual!r}, expected {expected!r}')


def verify_windows_node(node, artifact):
    node = Path(node)
    require(node.name == Path(artifact['executable']).name, 'Node executable path mismatch')
    require(tree_digest(node.parent) == artifact['files_sha256'], 'Node resource drift')
    require(sha(node.parent / 'LICENSE') == artifact['license_sha256'], 'Node license drift')


def verify_packages(install, lock, cache, offline):
    # Verify actual installed bytes against independently SHA256+integrity checked tarballs.
    verified = {}
    rows = {(r['name'], r['version']): r for r in lock['npm']}
    for package_json in sorted((install / 'node_modules/.pnpm').glob('*/node_modules/*/package.json')) + sorted(
            (install / 'node_modules/.pnpm').glob('*/node_modules/@*/*/package.json')):
        if package_json.parent.is_symlink():
            continue
        meta = read(package_json)
        key = (meta['name'], meta['version'])
        require(key in rows, 'unlocked installed package: ' + str(key))
        row = rows[key]
        archive = fetch(row, cache, offline)
        with tarfile.open(archive) as source:
            expected = {}
            for member in source.getmembers():
                if member.isfile():
                    name = member.name.split('/', 1)[1]
                    expected[name] = hashlib.sha256(source.extractfile(member).read()).hexdigest()
        for name, expected_hash in expected.items():
            path = package_json.parent / name
            require(path.is_file() and sha(path) == expected_hash, 'installed package bytes drift: ' + str(path))
        actual_files = {k: v for k, v in file_map(package_json.parent).items() if not k.startswith('node_modules/.bin/')}
        require(digest(actual_files) == row['files_sha256'], 'installed package tree drift: ' + str(key))
        verified[row['name'] + '@' + row['version']] = digest(expected)
    for name, expected in read(HERE / 'package.json')['dependencies'].items():
        require(read(install / 'node_modules' / name / 'package.json')['version'] == expected,
                'missing/wrong direct dependency: ' + name)
    expected = {r['name'] + '@' + r['version'] for r in platform_packages(lock, host_platform(lock))}
    require(set(verified) == expected, 'installed platform dependency closure differs; missing: ' +
            ', '.join(sorted(expected - set(verified))) + '; extra: ' + ', '.join(sorted(set(verified) - expected)))
    return verified


def library_fingerprints(executable):
    if platform.system() == 'Windows':
        from demo_reference_web.windows_dependencies import fingerprints
        try:
            return fingerprints(executable)
        except (OSError, ValueError) as error:
            raise PreparationError('Windows browser dependency closure: ' + str(error)) from error
    if platform.system() != 'Linux':
        raise PreparationError('shared-library provenance is not implemented on this platform')
    text = subprocess.check_output(['ldd', str(executable)], text=True, timeout=20)
    require('not found' not in text, 'missing browser runtime library')
    paths = re.findall(r'(?:=>\s+|^\s*)(/[^\s]+)', text, re.M)
    return {str(Path(path).resolve()): sha(Path(path).resolve()) for path in paths}


def seed_downloads(directory, lock, cache):
    if not directory:
        return
    directory = Path(directory)
    rows = [lock['source'], *lock['node']['artifacts'].values(), *lock['browser']['artifacts'].values(), *lock['npm']]
    expected = {r['sha256'] for r in rows}
    for path in directory.iterdir():
        if path.is_file():
            actual = sha(path)
            if actual in expected:
                target = cache / actual
                if not target.exists():
                    shutil.copy2(path, target)
                    target.chmod(0o444)


def prepare(output, *, mode='BUNDLED', offline=False, downloads=None, node=None, pnpm=None,
            browser=None, source=None):
    output = output_path(output)
    require(not output.exists(), 'prepare output already exists')
    lock = load_lock()
    require(mode in lock['modes'], 'explicit BUNDLED or SYSTEM required')
    host = host_platform(lock)
    output.mkdir(parents=True)
    cache = ROOT / 'out/demo-differential/archives'
    cache.mkdir(parents=True, exist_ok=True)
    seed_downloads(downloads, lock, cache)
    environment = os.environ.copy()
    for name in list(environment):
        if name.upper().startswith(('NPM_CONFIG_', 'PNPM_', 'COREPACK_')) or name.upper() in (
                'NODE_PATH', 'NODE_OPTIONS', 'ESBUILD_BINARY_PATH', 'PLAYWRIGHT_BROWSERS_PATH'):
            environment.pop(name)
    environment.update({'PLAYWRIGHT_SKIP_BROWSER_DOWNLOAD': '1', 'CI': 'true',
                        'npm_config_cache': str(output / 'npm-cache'),
                        'npm_config_userconfig': str(output / 'npmrc'),
                        'XDG_CACHE_HOME': str(output / 'xdg-cache')})
    (output / 'npmrc').write_text('registry=https://registry.npmjs.org/\nupdate-notifier=false\n')
    if mode == 'BUNDLED':
        require(not any([node, pnpm, browser, source]), 'BUNDLED cannot use SYSTEM overrides')
        extract(fetch(lock['node']['artifacts'][host], cache, offline), output / 'node')
        node = output / 'node' / lock['node']['artifacts'][host].get('executable',
                    'node-v' + lock['node']['version'] + '-linux-x64/bin/node')
        pnpm_row = next(r for r in lock['npm'] if r['name'] == 'pnpm')
        extract(fetch(pnpm_row, cache, offline), output / 'pnpm')
        pnpm = output / 'pnpm/package/bin/pnpm.cjs'
        extract(fetch(lock['browser']['artifacts'][host], cache, offline), output / 'browser')
        browser = output / 'browser' / lock['browser']['artifacts'][host].get(
            'executable', 'chrome-linux/headless_shell')
        extract(fetch(lock['source'], cache, offline), output / 'source')
        source = next((output / 'source').iterdir())
    else:
        require(all([node, pnpm, browser, source]), 'SYSTEM requires explicit node, pnpm CLI, browser, source')
        node, pnpm, browser, source = [Path(x).resolve() for x in (node, pnpm, browser, source)]
    require(sha(node) == lock['node']['artifacts'][host]['executable_sha256'], 'Node binary drift')
    if host == 'win-x64':
        verify_windows_node(node, lock['node']['artifacts'][host])
    pnpm_row = next(r for r in lock['npm'] if r['name'] == 'pnpm')
    require(tree_digest(Path(pnpm).parents[1]) == pnpm_row['files_sha256'], 'pnpm tool source drift')
    version([node, '--version'], 'v' + lock['node']['version'])
    version([node, pnpm, '--version'], '10.23.0')
    if 'executable_sha256' in lock['browser']['artifacts'][host]:
        require(sha(browser) == lock['browser']['artifacts'][host]['executable_sha256'], 'browser binary drift')
    require(tree_digest(Path(browser).parent) == lock['browser']['artifacts'][host]['files_sha256'], 'browser resource drift')
    browser_version = subprocess.check_output([str(browser), '--version'], text=True, timeout=20).strip()
    require(browser_version == lock['browser']['artifacts'][host]['version_output'] if host == 'win-x64'
            else lock['browser']['version'] in browser_version, 'browser version mismatch')
    verify_source(source, lock)
    environment['PATH'] = str(Path(node).parent) + os.pathsep + environment.get('PATH', '')
    install = output / 'web-tools'
    install.mkdir()
    for name in ('package.json', 'pnpm-lock.yaml'):
        shutil.copy2(HERE / name, install / name)
    # Serve only independently checked locked tarballs to a fresh pnpm store.
    # No registry metadata resolution or external installation fallback is available.
    archives = {}
    for row in lock['npm']:
        archives[urlparse(row['url']).path] = fetch(row, cache, offline)
    class Registry(BaseHTTPRequestHandler):
        def do_GET(self):
            archive = archives.get(self.path)
            if archive is None:
                self.send_error(404, 'unlocked registry resource')
                return
            self.send_response(200)
            self.send_header('Content-Length', str(archive.stat().st_size))
            self.end_headers()
            with archive.open('rb') as stream:
                shutil.copyfileobj(stream, self.wfile)
        def log_message(self, *args):
            pass
    registry = ThreadingHTTPServer(('127.0.0.1', 0), Registry)
    thread = threading.Thread(target=registry.serve_forever, daemon=True)
    thread.start()
    try:
        args = [node, pnpm, '--dir', install, 'install', '--frozen-lockfile', '--ignore-scripts',
                '--store-dir', output / 'store', '--registry=http://127.0.0.1:' + str(registry.server_port),
                '--fetch-retries=0', '--ignore-workspace']
        run(args, environment, output / 'install.log')
    finally:
        registry.shutdown()
        registry.server_close()
        thread.join()
    packages = verify_packages(install, lock, cache, offline)
    run([node, '-e', "const assert = require('node:assert/strict'); "
         "assert.equal(require('esbuild').version, '0.25.12'); "
         "assert.equal(require('playwright/package.json').version, '1.56.1'); "
         "console.log('esbuild 0.25.12; playwright 1.56.1')"],
        environment, output / 'versions.log', cwd=install)
    if mode == 'BUNDLED':
        readonly(source)
    record = {'schema_version': 1, 'mode': mode, 'host': host, 'lock_sha256': sha(HERE / 'dependencies.lock.json'),
              'node': str(Path(node).resolve()), 'pnpm': str(Path(pnpm).resolve()),
              'browser': str(Path(browser).resolve()), 'source': str(Path(source).resolve()),
              'install': str(install), 'packages': packages, 'browser_version': browser_version,
              'browser_sha256': sha(browser), 'node_sha256': sha(node), 'pnpm_sha256': sha(pnpm),
              'browser_libraries': library_fingerprints(browser)}
    (output / 'prepared.json').write_bytes(canonical(record) + b'\n')
    return record


def verify_prepared(path):
    path = output_path(path)
    record = read(path / 'prepared.json')
    lock = load_lock()
    require(record['host'] == host_platform(lock), 'prepared host platform differs')
    require(record['lock_sha256'] == sha(HERE / 'dependencies.lock.json'), 'prepared dependency lock drift')
    for key in ('node', 'pnpm', 'browser'):
        require(sha(record[key]) == record[key + '_sha256'], key + ' prepared binary drift')
    require(sha(record['node']) == lock['node']['artifacts'][record['host']]['executable_sha256'], 'locked Node drift')
    if record['host'] == 'win-x64':
        verify_windows_node(record['node'], lock['node']['artifacts']['win-x64'])
    require(sha(record['browser']) == lock['browser']['artifacts'][record['host']]['executable_sha256'], 'locked browser drift')
    require(tree_digest(Path(record['browser']).parent) == lock['browser']['artifacts'][record['host']]['files_sha256'], 'locked browser resource drift')
    pnpm_row = next(r for r in lock['npm'] if r['name'] == 'pnpm')
    require(tree_digest(Path(record['pnpm']).parents[1]) == pnpm_row['files_sha256'], 'locked pnpm source drift')
    require(verify_packages(Path(record['install']), lock, ROOT / 'out/demo-differential/archives', True) == record['packages'],
            'prepared installed package identities drift')
    require(library_fingerprints(record['browser']) == record['browser_libraries'], 'browser shared library environment drift')
    verify_source(record['source'], lock)
    require(sha(Path(record['install']) / 'pnpm-lock.yaml') == lock['pnpm_lock_sha256'], 'installed lock drift')
    require(sha(Path(record['install']) / 'package.json') == lock['package_sha256'], 'installed package drift')
    return record


def verify_cache(directory, key):
    manifest = read(directory / 'manifest.json')
    require(manifest['key'] == key, 'cache environment/input mismatch')
    expected = manifest['files']
    actual_names = {p.relative_to(directory).as_posix() for p in directory.rglob('*') if p.is_file()} - {'manifest.json'}
    require(actual_names == set(expected), 'reference cache truncated/extra artifacts')
    for name, expected_hash in expected.items():
        require(sha(directory / name) == expected_hash, 'reference cache corrupt: ' + name)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    parser.add_argument('--mode', choices=['BUNDLED', 'SYSTEM'], required=True)
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--downloads')
    for key in ('node', 'pnpm', 'browser', 'source'):
        parser.add_argument('--' + key)
    args = parser.parse_args()
    try:
        result = prepare(**vars(args))
        print(json.dumps({'prepared': str(Path(args.output).resolve()), 'browser': result['browser_version']}))
    except (PreparationError, ProtocolError, OSError, subprocess.SubprocessError, ValueError) as error:
        print('Web preparation failed: ' + str(error), file=sys.stderr)
        raise SystemExit(3)


if __name__ == '__main__':
    main()
