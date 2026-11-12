"""Locked source/dependency/cache controls; artifacts remain in this checkout out/."""
from copy import deepcopy
import json
import os
from pathlib import Path
import shutil
import sys
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_reference_web import bootstrap as boot

PREPARED = ROOT / os.environ.get('RYNUI_WEB_PREPARED', 'out/demo-differential/prepared')
OUT = ROOT / 'out/demo-differential/bootstrap-tests' / uuid.uuid4().hex


class WebBootstrapTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared = boot.verify_prepared(PREPARED)
        cls.lock = boot.load_lock()
        OUT.mkdir(parents=True)

    def test_exact_pins_actual_archives_and_license_metadata(self):
        rows = self.lock['npm']
        self.assertEqual(len({(x['name'], x['version']) for x in rows}), len(rows))
        for row in rows:
            self.assertTrue(row['license'])
            self.assertEqual(len(row['sha256']), 64)
            self.assertTrue(row['integrity'].startswith('sha512-'))
            archive = boot.fetch(row, ROOT / 'out/demo-differential/archives', True)
            self.assertEqual(boot.sha(archive), row['sha256'])
        pnpm = next(row for row in rows if row['name'] == 'pnpm')
        self.assertGreater(len(pnpm['bundled_packages']), 10)
        self.assertEqual(self.lock['python']['libraries'], [])

    def test_fresh_frozen_offline_prepare_and_system_exact_mode(self):
        fresh = OUT / 'fresh'
        record = boot.prepare(fresh, mode='BUNDLED', offline=True)
        self.assertTrue((fresh / 'prepared.json').is_file())
        log = (fresh / 'install.log').read_text()
        self.assertIn('reused 0', log)
        self.assertIn('resolution step is skipped', log)
        self.assertEqual(record['packages'], self.prepared['packages'])
        system = boot.prepare(OUT / 'system', mode='SYSTEM', offline=True,
                              **{key: record[key] for key in ('node', 'pnpm', 'browser', 'source')})
        self.assertEqual(system['packages'], record['packages'])

    def test_wrong_versions_and_missing_system_dependency_fail(self):
        with self.assertRaisesRegex(boot.PreparationError, 'version mismatch'):
            boot.version([self.prepared['node'], '--version'], 'v22.0.0')
        with self.assertRaisesRegex(boot.PreparationError, 'explicit node'):
            boot.prepare(OUT / 'missing-system', mode='SYSTEM')
        with self.assertRaisesRegex(boot.PreparationError, 'BUNDLED cannot'):
            boot.prepare(OUT / 'implicit-fallback', node=self.prepared['node'])

    def test_offline_missing_archive_and_actual_network_failure(self):
        cache = OUT / 'network-cache'
        record = {'url': 'http://127.0.0.1:1/unavailable', 'sha256': '0' * 64}
        with self.assertRaisesRegex(boot.PreparationError, 'offline archive missing'):
            boot.fetch(record, cache, True)
        with self.assertRaises(OSError):
            boot.fetch(record, cache, False)
        self.assertFalse((cache / record['sha256']).exists())

    def test_source_demo_token_and_tag_corruption_fail(self):
        source = Path(self.prepared['source'])
        files = [source / 'components/button/demo/basic.tsx', source / 'components/theme/themes/seed.ts', source / 'package.json']
        for file in files:
            self.assertTrue(file.is_file(), str(file))
            original = file.read_bytes()
            try:
                file.chmod(0o644)
                if file.name == 'package.json':
                    value = json.loads(original)
                    value['version'] = '6.6.6'
                    file.write_text(json.dumps(value))
                else:
                    file.write_bytes(original + b'\n// corruption negative control\n')
                with self.assertRaisesRegex(boot.PreparationError, 'drift|mismatch'):
                    boot.verify_source(source, self.lock)
            finally:
                file.write_bytes(original)
                file.chmod(0o444)
        boot.verify_source(source, self.lock)

    def test_installed_module_drift_and_missing_dependency_fail(self):
        file = Path(self.prepared['install']) / 'node_modules/react/index.js'
        original = file.read_bytes()
        try:
            file.write_bytes(original + b'\n// wrong module\n')
            with self.assertRaisesRegex(boot.PreparationError, 'package bytes drift'):
                boot.verify_prepared(PREPARED)
        finally:
            file.write_bytes(original)
        file = Path(self.prepared['install']) / 'node_modules/react'
        temporary = file.with_name('react-missing-control')
        try:
            file.rename(temporary)
            with self.assertRaises((boot.PreparationError, FileNotFoundError)):
                boot.verify_prepared(PREPARED)
        finally:
            temporary.rename(file)

    def test_truncated_archive_cache_is_never_reused(self):
        cache = OUT / 'corrupt-cache'
        cache.mkdir()
        row = self.lock['source']
        (cache / row['sha256']).write_bytes(b'partial')
        with self.assertRaisesRegex(boot.PreparationError, 'corrupt archive cache'):
            boot.fetch(row, cache, True)

    def test_reference_cache_hash_and_environment_identity(self):
        cache = OUT / 'cache-entry'
        cache.mkdir()
        (cache / 'capture.png').write_bytes(b'payload')
        manifest = {'schema_version': 1, 'key': 'environment-a', 'capture_hashes': {},
                    'files': {'capture.png': boot.sha(cache / 'capture.png')}}
        (cache / 'manifest.json').write_text(json.dumps(manifest))
        boot.verify_cache(cache, 'environment-a')
        with self.assertRaisesRegex(boot.PreparationError, 'environment/input mismatch'):
            boot.verify_cache(cache, 'environment-b')
        (cache / 'capture.png').write_bytes(b'truncated')
        with self.assertRaisesRegex(boot.PreparationError, 'cache corrupt'):
            boot.verify_cache(cache, 'environment-a')
        (cache / 'capture.png').unlink()
        with self.assertRaisesRegex(boot.PreparationError, 'cache truncated'):
            boot.verify_cache(cache, 'environment-a')


if __name__ == '__main__':
    unittest.main()
