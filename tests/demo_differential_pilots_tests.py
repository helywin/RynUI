"""Source/effective-input declarations only; no real pilot execution claim."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_differential import pilots
from demo_differential.runner import case_coverage, select
from demo_differential.protocol import FIXTURES, ProtocolError, read, validate
from demo_differential.adapters import web_definition
from demo_differential.inventory import source_inventory
from demo_reference_web import reference, bootstrap


class PilotDefinitionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.inventory = source_inventory()

    def test_three_strict_cases_preserve_actual_samples_and_all_ordered_targets(self):
        for case in pilots.definitions():
            with self.subTest(case=case['identity']):
                validate('case', case)
                case_coverage(case, self.inventory)
                official = case['official']
                self.assertEqual(official['adapter_sha256'], reference.adapter_sha256())
                self.assertEqual(official['demo_sha256'], bootstrap.load_lock()['source']['demos'][official['demo_path']])
                self.assertEqual(case['fixture']['fonts'], [{k: f[k] for k in ('identity', 'sha256', 'face_index', 'weight')}
                                                          for f in reference.central_fonts()])
                self.assertEqual({r['field']: r['value'] for r in official['adaptation']},
                                 {key: case['fixture'][key] for key in ('props', 'slots', 'data', 'container', 'theme', 'parent_policy')})

    def test_pilot_ledger_and_uncalibrated_template_do_not_claim_ready_or_pass(self):
        ledger = validate('coverage', read(FIXTURES / 'pilot-coverage.json'))
        for case in pilots.definitions():
            rows = [r for r in ledger['rows'] if case['identity'] in r['cases']]
            self.assertEqual({r['identity'] for r in rows}, {'sample/' + case['native']['sample_id']} |
                             {'target/' + target for target in case['native']['targets']})
            self.assertTrue(all(r['mapping'] == 'paired' and r['implementation'] == 'planned' for r in rows))
        profile = validate('profile', read(FIXTURES / 'profiles/pilot-pinned-font.uncalibrated.json'))
        self.assertFalse(profile['calibrated'])

    def test_button_small_middle_local_default_and_actual_labels(self):
        case = pilots.button()
        controls = case['fixture']['props']['controls']
        for target in ('gallery.state.default', 'gallery.state.primary'):
            self.assertEqual(controls[target]['size'], 'small')
        self.assertEqual(controls['gallery.state.disabled']['size'], 'middle')
        self.assertTrue(controls['gallery.state.disabled']['disabled'])
        self.assertTrue(controls['gallery.state.loading']['loading'])
        self.assertEqual(case['fixture']['slots']['gallery.state.default']['content'], '默认')
        reference.validate_composition(case['fixture']['props']['composition'], controls, case['fixture']['slots'])

    def test_input_prefix_suffix_limit_data_warning_and_accept_reject_are_declared(self):
        case = pilots.input_case()
        controls = case['fixture']['props']['controls']
        self.assertEqual(controls['gallery.input.controlled']['maxLength'], 32)
        self.assertEqual(controls['gallery.input.controlled']['status'], 'warning')
        self.assertEqual(case['fixture']['slots']['gallery.input.controlled']['prefix'], '前')
        self.assertEqual(case['fixture']['slots']['gallery.input.controlled']['suffix'], '32')
        self.assertEqual(case['fixture']['data']['gallery.input.controlled']['value'], 'seed')
        self.assertTrue(any(a['parameters'].get('values') == {'parent_policy': 'reject'} for a in case['actions']))
        reference.validate_composition(case['fixture']['props']['composition'], controls, case['fixture']['slots'])

    def test_select_complete_context_is_required_and_limited_web_grammar_rejects(self):
        case = pilots.select_case()
        data = case['fixture']['data']['gallery.select.controlled-search']
        self.assertEqual([r['value'] for r in data['options']], ['desktop', 'service'])
        self.assertEqual(data['value'], 'desktop')
        composition = case['fixture']['props']['composition']
        self.assertEqual(composition['required_nested_function'], 'add_complete_select_samples')
        self.assertEqual(composition['local_theme']['optionHeight'], 36)
        with self.assertRaises(bootstrap.PreparationError):
            reference.validate_composition(composition, case['fixture']['props']['controls'], {})

    def test_same_count_identity_swap_reorder_and_new_target_fail_instead_of_count_cap(self):
        case = pilots.button()
        for fault in ('swap', 'reorder', 'new'):
            inventory = deepcopy(self.inventory)
            targets = [r for r in inventory['targets'] if r['sample'] == case['native']['sample_id']]
            if fault == 'swap':
                targets[0]['identity'] += '-replacement'
            elif fault == 'reorder':
                i, j = inventory['targets'].index(targets[0]), inventory['targets'].index(targets[1])
                inventory['targets'][i], inventory['targets'][j] = inventory['targets'][j], inventory['targets'][i]
            else:
                inventory['targets'].append({'identity': 'gallery.state.added', 'sample': case['native']['sample_id'], 'alias': ''})
            with self.subTest(fault=fault), self.assertRaises(ProtocolError):
                case_coverage(case, inventory)

    def test_selection_is_ordered_and_browser_layer_projection_preserves_inputs_oracles(self):
        cases = pilots.definitions()
        self.assertEqual(select(cases, {'cases': [cases[1]['identity'], cases[0]['identity']]}), cases[:2])
        for case in cases:
            projected = web_definition(case)
            self.assertEqual(projected['fixture'], case['fixture'])
            self.assertEqual(projected['oracle'], case['oracle'])
            self.assertEqual([a['step'] for a in projected['actions']], [a['step'] for a in case['actions']])
        with self.assertRaises(ProtocolError):
            select(cases, {'cases': ['antd665.button.unknown.v1']})


if __name__ == '__main__':
    unittest.main()
