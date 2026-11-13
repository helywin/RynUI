"""CPU-only platform/ABI controls; never render or run Node package scripts."""
from copy import deepcopy
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.error
import urllib.request
import unittest
from unittest.mock import patch
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_reference_web import bootstrap as boot
from demo_differential import comparison_dependencies as dependencies
from demo_differential import comparison_runtime as runtime
from demo_differential.protocol import ProtocolError

OUT = ROOT / 'out/demo-differential/windows-preparation-tests' / uuid.uuid4().hex
OUT.mkdir(parents=True)


class PlatformControls(unittest.TestCase):
    def test_only_declared_os_arch_and_platform_optional_packages(self):
        lock = boot.load_lock()
        for system, machine, expected in [('Windows', 'AMD64', 'win-x64'), ('Linux', 'x86_64', 'linux-x64')]:
            with patch.object(platform, 'system', return_value=system), patch.object(platform, 'machine', return_value=machine):
                self.assertEqual(boot.host_platform(lock), expected)
        for system, machine in [('Darwin', 'x86_64'), ('Windows', 'ARM64'), ('Windows', 'x86')]:
            with patch.object(platform, 'system', return_value=system), patch.object(platform, 'machine', return_value=machine):
                with self.assertRaises(boot.PreparationError):
                    boot.host_platform(lock)
                with self.assertRaises(ProtocolError):
                    dependencies.wheel_profile()
                with self.assertRaises(ProtocolError):
                    runtime.runtime_profile()
        names = {r['name'] for r in boot.platform_packages(lock, 'win-x64')}
        self.assertIn('@esbuild/win32-x64', names)
        self.assertNotIn('@esbuild/linux-x64', names)
        self.assertNotIn('fsevents', names)
        self.assertNotIn('pnpm', names)

    def test_offline_and_corrupt_archives_never_request_network(self):
        row = boot.load_lock()['browser']['artifacts']['win-x64']
        cache = OUT / 'empty-cache'
        with patch.object(boot, 'urlopen', side_effect=AssertionError('undeclared network')) as network:
            with self.assertRaisesRegex(boot.PreparationError, 'offline archive missing'):
                boot.fetch(row, cache, True)
            (cache / row['sha256']).write_bytes(b'partial')
            for offline in (False, True):
                with self.assertRaisesRegex(boot.PreparationError, 'corrupt archive cache'):
                    boot.fetch(row, cache, offline)
            network.assert_not_called()

    def test_no_implicit_system_tool_fallback(self):
        with patch.object(boot, 'urlopen', side_effect=AssertionError('network')), patch.dict(os.environ, {
                'NODE_PATH': str(OUT / 'global-node'), 'ESBUILD_BINARY_PATH': 'global-esbuild',
                'PLAYWRIGHT_BROWSERS_PATH': 'global-chrome'}):
            with self.assertRaisesRegex(boot.PreparationError, 'SYSTEM requires explicit'):
                boot.prepare(OUT / 'missing-system-tools', mode='SYSTEM')
            with self.assertRaisesRegex(boot.PreparationError, 'BUNDLED cannot'):
                boot.prepare(OUT / 'bundled-override', mode='BUNDLED', node='global-node')

    def test_windows_wheel_key_and_reject_wrong_abi(self):
        from collections import namedtuple
        version = namedtuple('Version', 'major minor micro')
        with patch.object(platform, 'system', return_value='Windows'), patch.object(platform, 'machine', return_value='AMD64'):
            with patch.object(sys, 'version_info', version(3, 14, 999)), patch.object(dependencies.sysconfig, 'get_config_var', return_value=0):
                self.assertEqual(dependencies.wheel_profile()['filename'], 'pillow-12.0.0-cp314-cp314-win_amd64.whl')
            with patch.object(sys, 'version_info', version(3, 10, 11)):
                with self.assertRaisesRegex(ProtocolError, 'ABI'):
                    dependencies.wheel_profile()
            with patch.object(sys, 'version_info', version(3, 14, 0)), patch.object(dependencies.sysconfig, 'get_config_var', return_value=1):
                with self.assertRaisesRegex(ProtocolError, 'ABI'):
                    dependencies.wheel_profile()
            with patch.object(sys, 'version_info', version(3, 14, 0)), patch.object(dependencies.struct, 'calcsize', return_value=4):
                with self.assertRaisesRegex(ProtocolError, 'ABI'):
                    dependencies.wheel_profile()

    def test_missing_platform_optional_package_fails_even_when_direct_dependencies_exist(self):
        install = OUT / 'closure'
        cache = OUT / 'closure-cache'
        cache.mkdir()
        lock = deepcopy(boot.load_lock())
        lock['npm'] = []
        for name in ('react', '@esbuild/win32-x64'):
            metadata = json.dumps({'name': name, 'version': '1.0.0'}).encode()
            files = {'package.json': hashlib.sha256(metadata).hexdigest()}
            stream = io.BytesIO()
            with tarfile.open(fileobj=stream, mode='w:gz') as archive:
                member = tarfile.TarInfo('package/package.json')
                member.size = len(metadata)
                archive.addfile(member, io.BytesIO(metadata))
            payload = stream.getvalue()
            archive_hash = hashlib.sha256(payload).hexdigest()
            (cache / archive_hash).write_bytes(payload)
            row = {'name': name, 'version': '1.0.0', 'sha256': archive_hash,
                   'files_sha256': boot.digest(files), 'url': 'https://registry.npmjs.org/locked-only'}
            if name.startswith('@esbuild'):
                row.update(os=['win32'], cpu=['x64'])
            lock['npm'].append(row)
            directory = install / 'node_modules/.pnpm' / (name.replace('/', '+') + '@1.0.0') / 'node_modules' / name
            directory.mkdir(parents=True)
            (directory / 'package.json').write_bytes(metadata)
        direct = install / 'node_modules/react'
        direct.mkdir()
        (direct / 'package.json').write_text(json.dumps({'version': '1.0.0'}))
        original_read = boot.read
        def read(path):
            if Path(path) == boot.HERE / 'package.json':
                return {'dependencies': {'react': '1.0.0'}}
            return original_read(path)
        with patch.object(boot, 'read', side_effect=read), patch.object(boot, 'host_platform', return_value='win-x64'):
            self.assertEqual(len(boot.verify_packages(install, lock, cache, True)), 2)
            shutil.rmtree(install / 'node_modules/.pnpm/@esbuild+win32-x64@1.0.0')
            with self.assertRaisesRegex(boot.PreparationError, 'missing: @esbuild/win32-x64'):
                boot.verify_packages(install, lock, cache, True)


@unittest.skipUnless(platform.system() == 'Windows', 'actual Windows x64 artifacts')
class WindowsRuntimeControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Requires real locked archive caches, populated by explicit preparation.
        with patch.object(boot, 'urlopen', side_effect=AssertionError('offline network')):
            cls.first = runtime.prepare_runtime(OUT / 'runtime-a', mode='BUNDLED', offline=True)

    def test_actual_fresh_offline_repeat_and_explicit_system(self):
        with patch.object(boot, 'urlopen', side_effect=AssertionError('offline network')) as network:
            other = runtime.prepare_runtime(OUT / 'runtime-b', mode='BUNDLED', offline=True)
            system = runtime.prepare_runtime(OUT / 'runtime-system', mode='SYSTEM',
                                            python=self.first['python'], offline=True)
            network.assert_not_called()
        for record in (self.first, other, system):
            self.assertEqual(record['probe']['soabi'], 'cp314-win_amd64')
            self.assertEqual(record['smoke']['pillow'], '12.0.0')
            self.assertEqual(record['smoke']['isolated'], 1)
            self.assertTrue(Path(record['smoke']['origin']).is_relative_to(record['comparison_site']))
        self.assertEqual(boot.file_map(Path(self.first['python']).parent), boot.file_map(Path(other['python']).parent))
        self.assertEqual(boot.file_map(self.first['comparison_site']), boot.file_map(other['comparison_site']))

    def test_reject_310_wrong_version_platform_arch_abi_and_executable(self):
        fake = dict(self.first['probe'])
        for field, value in [('version', [3, 10, 11]), ('platform', 'linux'), ('bits', 32),
                             ('soabi', 'cp314t-win_amd64'), ('gil_disabled', True),
                             ('executable', str(OUT / 'wrong-python.exe'))]:
            record = dict(fake, **{field: value})
            with patch.object(subprocess, 'check_output', return_value=json.dumps(record)):
                with self.assertRaises(ProtocolError):
                    runtime.probe_interpreter(self.first['python'])
        with self.assertRaises(OSError):
            runtime.probe_interpreter(OUT / 'missing-python.exe')
        with self.assertRaisesRegex(ProtocolError, 'requires explicit'):
            runtime.prepare_runtime(OUT / 'implicit-python', mode='SYSTEM', offline=True)

    def test_pth_and_interpreter_corruption_rejected_before_execution(self):
        directory = Path(self.first['python']).parent
        for relative in ('python314._pth', 'python.exe'):
            path = directory / relative
            original = path.read_bytes()
            try:
                path.write_bytes(original + b'\ncorrupt')
                with patch.object(runtime, 'probe_interpreter', side_effect=AssertionError('must verify before execute')):
                    with self.assertRaisesRegex(ProtocolError, 'file differs'):
                        runtime.verify_runtime(directory.parent)
            finally:
                path.write_bytes(original)
        runtime.verify_runtime(directory.parent)

    def test_global_pythonpath_pillow_is_not_loaded(self):
        poison = OUT / 'global-python/PIL'
        poison.mkdir(parents=True)
        (poison / '__init__.py').write_text("raise RuntimeError('global Pillow used')\n")
        with patch.dict(os.environ, {'PYTHONPATH': str(poison.parent), 'PYTHONHOME': str(poison.parent)}):
            record = runtime.verify_runtime(Path(self.first['python']).parents[1])
        self.assertEqual(record['smoke']['pillow'], '12.0.0')


@unittest.skipUnless(platform.system() == 'Windows', 'actual Windows x64 artifacts')
class WindowsWebArtifactControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lock = boot.load_lock()
        cls.assets = OUT / 'web-artifacts'
        cache = ROOT / 'out/demo-differential/archives'
        for key in ('node', 'browser'):
            row = cls.lock[key]['artifacts']['win-x64']
            boot.extract(boot.fetch(row, cache, True), cls.assets / key)
            setattr(cls, key, cls.assets / key / row['executable'])
        row = next(r for r in cls.lock['npm'] if r['name'] == 'pnpm')
        boot.extract(boot.fetch(row, cache, True), cls.assets / 'pnpm')
        cls.pnpm = cls.assets / 'pnpm/package/bin/pnpm.cjs'
        boot.extract(boot.fetch(cls.lock['source'], cache, True), cls.assets / 'source')
        cls.source = next((cls.assets / 'source').iterdir())

    def test_actual_archive_tree_licenses_versions_and_pe_closure(self):
        boot.verify_windows_node(self.node, self.lock['node']['artifacts']['win-x64'])
        self.assertEqual(boot.tree_digest(self.browser.parent), self.lock['browser']['artifacts']['win-x64']['files_sha256'])
        boot.version([self.node, '--version'], 'v22.21.1')
        boot.version([self.browser, '--version'], 'Chromium 141.0.7390.37')
        closure = boot.library_fingerprints(self.browser)
        self.assertEqual(closure['method'], 'pe-eager-imports-and-bundled-dlls-v1')
        self.assertIn(str(self.browser.resolve()), closure['files'])
        self.assertTrue(closure['api_sets'])
        boot.verify_source(self.source, self.lock)

    def test_wrong_executable_and_browser_version_rejected(self):
        override = {key: getattr(self, key) for key in ('node', 'pnpm', 'browser', 'source')}
        bad = OUT / 'wrong-node.exe'
        bad.write_bytes(b'not locked Windows x64 PE')
        with patch.object(boot.subprocess, 'check_output', side_effect=AssertionError('do not execute bad bytes')):
            with self.assertRaisesRegex(boot.PreparationError, 'Node binary drift'):
                boot.prepare(OUT / 'bad-node', mode='SYSTEM', offline=True, **dict(override, node=bad))
        with patch.object(boot, 'version'), patch.object(boot.subprocess, 'check_output', side_effect=AssertionError('do not execute bad bytes')):
            with self.assertRaisesRegex(boot.PreparationError, 'browser binary drift'):
                boot.prepare(OUT / 'bad-browser', mode='SYSTEM', offline=True, **dict(override, browser=bad))
        with patch.object(boot, 'version'), patch.object(boot.subprocess, 'check_output', return_value='Chromium 141.0.0.0'):
            with self.assertRaisesRegex(boot.PreparationError, 'browser version mismatch'):
                boot.prepare(OUT / 'bad-browser-version', mode='SYSTEM', offline=True, **override)

    def test_registry_rejects_undeclared_request_without_running_node_scripts(self):
        def audit_registry(argv, env, logfile, cwd=None):
            registry = next(str(a).split('=', 1)[1] for a in argv if str(a).startswith('--registry='))
            self.assertNotIn('NODE_OPTIONS', env)
            self.assertNotIn('ESBUILD_BINARY_PATH', env)
            self.assertNotIn('PNPM_HOME', env)
            self.assertNotIn('NPM_CONFIG_OPTIONAL', env)
            self.assertEqual(env['PATH'].split(os.pathsep)[0], str(self.node.parent))
            with self.assertRaises(urllib.error.HTTPError) as rejected:
                urllib.request.urlopen(registry + '/undeclared-package', timeout=10)
            self.assertEqual(rejected.exception.code, 404)
            rejected.exception.close()
            raise boot.PreparationError('CPU stop before pnpm installation')
        with patch.object(boot, 'version'), patch.object(boot, 'run', side_effect=audit_registry), patch.dict(os.environ, {
                'NODE_OPTIONS': '--require global-hook', 'ESBUILD_BINARY_PATH': 'global-esbuild',
                'PNPM_HOME': 'global-pnpm', 'NPM_CONFIG_OPTIONAL': 'false'}):
            with self.assertRaisesRegex(boot.PreparationError, 'CPU stop before pnpm'):
                boot.prepare(OUT / 'registry-audit', mode='SYSTEM', offline=True,
                             **{key: getattr(self, key) for key in ('node', 'pnpm', 'browser', 'source')})
        self.assertFalse((OUT / 'registry-audit/prepared.json').exists())


if __name__ == '__main__':
    unittest.main()
