"""Explicit maintainer operation: resolve already exact pins and hash actual upstream bytes.

This is never called by prepare/run. A changed lock requires a reviewed source change.
"""
from concurrent.futures import ThreadPoolExecutor
import base64
import hashlib
import io
import json
from pathlib import Path
import re
import tarfile
from urllib.request import urlopen, Request

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
OUT = ROOT / 'out/demo-differential/lock-downloads'
COMMIT = '4a39f54842eade4e565ab336ef6097cd7e723cdd'
DEMOS = ['button/demo/basic', 'input/demo/allowClear', 'input/demo/presuffix',
         'select/demo/basic', 'select/demo/search']


def fetch(url, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        with urlopen(url, timeout=90) as response:
            path.write_bytes(response.read())
    return path.read_bytes()


def npm_record(item):
    name, version, integrity = item
    identity = name + '@' + version
    meta = json.loads(fetch('https://registry.npmjs.org/' + name + '/' + version,
                            OUT / (identity.replace('/', '__') + '.json')))
    assert meta['name'] == name and meta['version'] == version
    assert meta['dist']['integrity'] == integrity, identity
    data = fetch(meta['dist']['tarball'], OUT / (identity.replace('/', '__') + '.tgz'))
    algorithm, expected = integrity.split('-', 1)
    assert base64.b64encode(hashlib.new(algorithm, data).digest()).decode() == expected, identity
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        package = json.load(archive.extractfile('package/package.json'))
        assert package['version'] == version and package['name'] == name
        licenses = {}
        for member in archive.getmembers():
            if member.isfile() and re.search(r'(^|/)(licen[cs]e|copying|notice)(\.|$)', member.name, re.I):
                licenses[member.name] = hashlib.sha256(archive.extractfile(member).read()).hexdigest()
    license_id = meta.get('license', package.get('license'))
    assert isinstance(license_id, str) and license_id and license_id != 'UNLICENSED', identity
    file_hashes = {}
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        for member in archive.getmembers():
            if member.isfile():
                file_hashes[member.name.split('/', 1)[1]] = hashlib.sha256(archive.extractfile(member).read()).hexdigest()
    bundled = []
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        for member in archive.getmembers():
            if member.isfile() and '/node_modules/' in member.name and member.name.endswith('/package.json'):
                nested = json.load(archive.extractfile(member))
                if nested.get('name') and nested.get('version'):
                    bundled.append({'path': member.name, 'name': nested['name'], 'version': nested['version'],
                                    'license': nested.get('license', nested.get('licenses', 'see parent archive license files'))})
    tree_hash = hashlib.sha256(json.dumps(file_hashes, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    return {'bundled_packages': bundled, 'files_sha256': tree_hash, 'name': name, 'version': version, 'url': meta['dist']['tarball'], 'integrity': integrity,
            'sha256': hashlib.sha256(data).hexdigest(), 'license': license_id,
            'license_files': licenses, 'os': package.get('os', []), 'cpu': package.get('cpu', [])}


def tree_digest(root):
    files = {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in sorted(root.rglob('*')) if p.is_file()}
    return hashlib.sha256(json.dumps(files, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def main():
    previous = json.loads((HERE / 'dependencies.lock.json').read_text())
    lock_text = (HERE / 'pnpm-lock.yaml').read_text()
    packages = lock_text.split('\npackages:\n', 1)[1].split('\nsnapshots:\n', 1)[0]
    rows = re.findall(r"^  '?([^\n']+)'?:\n    resolution: \{integrity: ([^}]+)\}", packages, re.M)
    pins = []
    for identity, integrity in rows:
        name, version = identity.rsplit('@', 1)
        pins.append((name, version, integrity))
    pnpm_meta = json.loads(fetch('https://registry.npmjs.org/pnpm/10.23.0', OUT / 'pnpm.json'))
    pins.append(('pnpm', '10.23.0', pnpm_meta['dist']['integrity']))
    with ThreadPoolExecutor(max_workers=8) as pool:
        records = list(pool.map(npm_record, pins))
    source_archive = ROOT / 'out/demo-differential/downloads/antd-4a39f548.tar.gz'
    source_out = OUT / 'official'
    if not source_out.exists():
        source_out.mkdir()
        with tarfile.open(source_archive) as archive:
            archive.extractall(source_out, filter='data')
    source = next(source_out.iterdir())
    browser_url = 'https://cdn.playwright.dev/builds/chromium/1194/chromium-headless-shell-linux.zip'
    browser_path = OUT / 'chromium-headless-shell-linux-1194.zip'
    browser = fetch(browser_url, browser_path)
    lock = {'schema_version': 1, 'modes': ['BUNDLED', 'SYSTEM'],
            'package_sha256': hashlib.sha256((HERE / 'package.json').read_bytes()).hexdigest(),
            'pnpm_lock_sha256': hashlib.sha256((HERE / 'pnpm-lock.yaml').read_bytes()).hexdigest(),
            'python': {'implementation': 'CPython', 'minimum': '3.10', 'libraries': [],
                       'reason': 'Web bootstrap/protocol use only the interpreter standard library; comparator is separately owned'},
            'node': {'version': '22.21.1', 'license': 'MIT AND bundled-third-party',
                     'checksums_url': 'https://nodejs.org/download/release/v22.21.1/SHASUMS256.txt',
                     'artifacts': {'linux-x64': {'url': 'https://nodejs.org/download/release/v22.21.1/node-v22.21.1-linux-x64.tar.xz',
                         'sha256': '680d3f30b24a7ff24b98db5e96f294c0070f8f9078df658da1bce1b9c9873c88'},
                         'win-x64': {'url': 'https://nodejs.org/download/release/v22.21.1/node-v22.21.1-win-x64.zip',
                         'sha256': '3c624e9fbe07e3217552ec52a0f84e2bdc2e6ffa7348f3fdfb9fbf8f42e23fcf'}}},
            'source': {'repository': 'https://github.com/ant-design/ant-design', 'commit': COMMIT,
                       'version': '6.6.5', 'url': 'https://codeload.github.com/ant-design/ant-design/tar.gz/' + COMMIT,
                       'sha256': hashlib.sha256(source_archive.read_bytes()).hexdigest(), 'license': 'MIT',
                       'license_sha256': hashlib.sha256((source / 'LICENSE').read_bytes()).hexdigest(),
                       'components_tree_sha256': tree_digest(source / 'components'),
                       'build_inputs': {name: hashlib.sha256((source / name).read_bytes()).hexdigest() for name in ('package.json', 'scripts/generate-version.ts')},
                       'demos': {'components/' + name + '.tsx': hashlib.sha256((source / ('components/' + name + '.tsx')).read_bytes()).hexdigest() for name in DEMOS}},
            'browser': {'name': 'chromium-headless-shell', 'revision': '1194', 'version': '141.0.7390.37',
                        'license': 'BSD-3-Clause AND bundled-third-party',
                        'artifacts': {'linux-x64': {'url': browser_url, 'sha256': hashlib.sha256(browser).hexdigest()}}},
            'font_lock': 'cmake/dependencies/RynUIDependencyLock.cmake',
            'npm': sorted(records, key=lambda x: (x['name'], x['version']))}
    with __import__('zipfile').ZipFile(browser_path) as archive:
        files = {name.split('/', 1)[1]: hashlib.sha256(archive.read(name)).hexdigest() for name in archive.namelist() if not name.endswith('/')}
        lock['browser']['artifacts']['linux-x64']['files_sha256'] = hashlib.sha256(json.dumps(files, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
        lock['browser']['artifacts']['linux-x64']['executable_sha256'] = hashlib.sha256(archive.read('chrome-linux/headless_shell')).hexdigest()
    with tarfile.open(ROOT / 'out/demo-differential/downloads/node-v22.21.1-linux-x64.tar.xz') as archive:
        lock['node']['artifacts']['linux-x64']['executable_sha256'] = hashlib.sha256(archive.extractfile('node-v22.21.1-linux-x64/bin/node').read()).hexdigest()
        lock['node']['license_sha256'] = hashlib.sha256(archive.extractfile('node-v22.21.1-linux-x64/LICENSE').read()).hexdigest()
    # Keep the separately verified Windows pins when refreshing npm/Linux data.
    # Re-read actual official archives; never carry platform hashes on trust or
    # silently reduce a complete Windows record to its URL/archive hash alone.
    for tool in ('node', 'browser'):
        record = previous[tool]['artifacts']['win-x64']
        data = fetch(record['url'], OUT / (tool + '-win-x64.zip'))
        assert len(data) == record['bytes']
        assert hashlib.sha256(data).hexdigest() == record['sha256']
        with __import__('zipfile').ZipFile(io.BytesIO(data)) as archive:
            files = {name.split('/', 1)[1]: hashlib.sha256(archive.read(name)).hexdigest()
                     for name in archive.namelist() if not name.endswith('/')}
            assert hashlib.sha256(archive.read(record['executable'])).hexdigest() == record['executable_sha256']
        assert hashlib.sha256(json.dumps(files, sort_keys=True, separators=(',', ':')).encode()).hexdigest() == record['files_sha256']
        if tool == 'node':
            assert files['LICENSE'] == record['license_sha256']
        lock[tool]['artifacts']['win-x64'] = record
    license_url = 'https://raw.githubusercontent.com/chromium/chromium/141.0.7390.37/LICENSE'
    license_bytes = fetch(license_url, OUT / 'chromium-LICENSE')
    lock['browser']['license_url'] = license_url
    lock['browser']['license_sha256'] = hashlib.sha256(license_bytes).hexdigest()
    (HERE / 'dependencies.lock.json').write_text(json.dumps(lock, indent=2) + '\n')
    print(f'Locked {len(records)} exact npm archives, source, Node, Chromium; browser SHA256 {lock["browser"]["artifacts"]["linux-x64"]["sha256"]}')


if __name__ == '__main__':
    main()
