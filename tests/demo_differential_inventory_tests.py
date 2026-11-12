"""Protocol and ordered-coverage failure controls; compiled registry is optional."""
from copy import deepcopy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_differential.protocol import ProtocolError, FIXTURES, read, loads, validate, relative_path, output_path
from demo_differential.inventory import (source_inventory, differences, require_exact, require_retained,
                                         coverage_gaps, query_binary, obligations, VECTORS)
from demo_differential.schema_definitions import schemas


class InventoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.inventory = source_inventory()
        cls.coverage = read(FIXTURES / 'coverage.json')
        cls.baseline = read(FIXTURES / 'identity-baseline.json')

    def test_source_registry_complete_and_retained(self):
        require_exact(self.inventory, self.inventory)
        require_retained(self.inventory, self.baseline)
        self.assertEqual([r['identity'] for r in self.inventory['samples']],
                         [r['identity'] for r in self.inventory['sources']])
        self.assertEqual(len({r['family'] for r in self.inventory['routes']
                              if r['component'] == 'ant.component.input'}), 5)
        self.assertEqual(sum(r['identity'] == 'ant.component.input' for r in self.inventory['catalog']), 1)
        for sample in ('gallery.sample.card.rounded-mask', 'gallery.sample.modal.backdrop'):
            self.assertIn(sample, [r['identity'] for r in self.inventory['sources']])

    def test_full_ledger_skeleton_is_not_execution_pass(self):
        expected = obligations(self.inventory)
        self.assertEqual([r['identity'] for r in expected], [r['identity'] for r in self.coverage['rows']])
        gaps = coverage_gaps(self.inventory, self.coverage)
        self.assertEqual(len(gaps), len(expected))
        self.assertEqual({r['reason'] for r in gaps}, {'planned'})

    def test_new_obligation_immediately_pending(self):
        changed = deepcopy(self.inventory)
        changed['targets'].append({'identity':'gallery.future', 'sample': changed['samples'][0]['identity'], 'alias':''})
        self.assertEqual(coverage_gaps(changed, self.coverage)[-1],
                         {'identity':'target/gallery.future', 'reason':'unmapped'})
        delta = require_retained(changed, self.baseline)
        self.assertEqual(delta['targets']['added'], ['gallery.future'])

    def test_same_count_replacement_delete_reorder_and_owner(self):
        for mutate in ('replace', 'delete', 'reorder', 'owner'):
            with self.subTest(mutate=mutate):
                changed = deepcopy(self.inventory)
                if mutate == 'replace':
                    changed['targets'][0]['identity'] = 'gallery.replaced'
                elif mutate == 'delete':
                    changed['targets'].pop()
                elif mutate == 'reorder':
                    changed['targets'][0:2] = reversed(changed['targets'][0:2])
                else:
                    changed['targets'][0]['sample'] = changed['samples'][1]['identity']
                with self.assertRaises(ProtocolError):
                    require_exact(self.inventory, changed)
                with self.assertRaises(ProtocolError):
                    require_retained(changed, self.baseline)

    def test_source_code_change_rejects_stale_binary(self):
        changed = deepcopy(self.inventory)
        changed['sources'][0]['code'] += '\n// dirty source requires rebuild'
        with self.assertRaises(ProtocolError):
            require_exact(changed, self.inventory)

    def test_alias_cycle_missing_alias_duplicate_and_unknown_owner(self):
        for mutate in ('cycle', 'missing', 'duplicate', 'owner'):
            with self.subTest(mutate=mutate):
                changed = deepcopy(self.inventory)
                if mutate == 'cycle':
                    changed['targets'][0]['alias'] = changed['targets'][1]['identity']
                    changed['targets'][1]['alias'] = changed['targets'][0]['identity']
                elif mutate == 'missing':
                    changed['targets'][0]['alias'] = 'gallery.never.registered'
                elif mutate == 'duplicate':
                    changed['targets'].append(changed['targets'][0])
                else:
                    changed['targets'][0]['sample'] = 'gallery.sample.absent'
                with self.assertRaises(ProtocolError):
                    require_exact(self.inventory, changed)

    def test_schema_unknown_fields_and_finite_values(self):
        for vector in VECTORS:
            changed = deepcopy(self.inventory)
            changed[vector][0]['surprise'] = 1
            with self.assertRaises(ProtocolError):
                validate('inventory', changed)
        for text in ('{"x":NaN}', '{"x":1,"x":2}', '{"x":1e999}'):
            with self.assertRaises(ProtocolError):
                loads(text)
        for path in ('../escape', '/absolute', 'C:/drive', 'a\\b', 'a/../b', './a', 'a//b'):
            with self.subTest(path=path), self.assertRaises(ProtocolError):
                relative_path(path)
        with self.assertRaises(ProtocolError):
            output_path(ROOT / 'README.md')

    def test_generated_schema_source_matches(self):
        for kind, expected in schemas().items():
            self.assertEqual(read(FIXTURES / f'{kind}.schema.json'), expected)

    def test_planned_missing_case_not_run_and_boundary_not_pass(self):
        identity = self.coverage['rows'][0]['identity']
        changed = deepcopy(self.coverage)
        changed['rows'][0].update(mapping='paired', implementation='ready', cases=['case.missing'])
        self.assertEqual(coverage_gaps(self.inventory, changed)[0], {'identity':identity, 'reason':'missing-case'})
        self.assertEqual(coverage_gaps(self.inventory, changed, ['case.missing'])[0],
                         {'identity':identity, 'reason':'not-pass'})
        changed['rows'][0].update(mapping='unsupported', reason='native failed')
        with self.assertRaises(ProtocolError):
            coverage_gaps(self.inventory, changed)

    @unittest.skipUnless(os.environ.get('RYNUI_DEMO_INVENTORY_BINARY'), 'use the formal CTest target for binary closure')
    def test_actual_compiled_registry_without_display(self):
        binary = Path(os.environ['RYNUI_DEMO_INVENTORY_BINARY'])
        with tempfile.TemporaryDirectory(prefix='inventory-', dir=ROOT / 'out') as directory:
            output = Path(directory) / 'compiled.json'
            # No display, compositor, Web toolchain or renderer is needed.
            env = dict(os.environ)
            for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XDG_RUNTIME_DIR'):
                env.pop(key, None)
            result = subprocess.run([str(binary), f'--page-inventory={output}'], env=env,
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            require_exact(self.inventory, read(output))
            duplicate = subprocess.run([str(binary), f'--page-inventory={output}'],
                                       capture_output=True, text=True, timeout=30)
            self.assertEqual(duplicate.returncode, 2)
            for args in (['--page-inventory=../escape.json'], ['--page-inventory'],
                         [f'--page-inventory={Path(directory)/"mix.json"}', '--page-acceptance']):
                bad = subprocess.run([str(binary), *args], capture_output=True, text=True, timeout=30)
                self.assertEqual(bad.returncode, 2)


if __name__ == '__main__':
    unittest.main()
