"""Source/effective-input declarations only; no real pilot execution claim."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

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
                                 {key: case['fixture'][key] for key in ('props', 'slots', 'data', 'container', 'theme', 'parent_policy', 'fonts')})

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

    def test_select_complete_factory_context_is_expressed_without_dropping_targets(self):
        case = pilots.select_case()
        data = case['fixture']['data']['gallery.select.controlled-search']
        self.assertEqual([r['value'] for r in data['options']], ['desktop', 'service'])
        self.assertEqual(data['value'], 'desktop')
        composition = case['fixture']['props']['composition']
        controls = case['fixture']['props']['controls']
        self.assertEqual(case['fixture']['container']['layout'], 'composition-v2')
        reference.validate_composition(composition, controls, case['fixture']['slots'])
        self.assertTrue(set(case['native']['targets']) <= set(controls))
        self.assertEqual(case['fixture']['data']['gallery.select.numeric']['defaultValue'], 42)
        self.assertEqual(controls['gallery.select.component-theme']['localTheme']['components']['Select']['optionHeight'], 36)
        self.assertEqual(len(case['fixture']['data']['gallery.select.virtual']['options']), 10000)
        for name in ('multiple', 'tags', 'virtual', 'async', 'authority', 'prefix', 'popup-note', 'deliver'):
            self.assertIn('gallery.select.' + name, controls)
        self.assertEqual(case['fixture']['data']['gallery.select.multiple']['options'][1]['options'][0]['value'], 1)
        self.assertTrue(any(a['parameters'].get('values') == {'parent_policy': 'reject'} for a in case['actions']))

    def test_popup_checkpoints_preserve_legacy_order_and_require_real_finite_aliases(self):
        from demo_differential.actual_inputs import ordered_targets, validate_popup_plan
        case = pilots.select_case()
        self.assertEqual(case['native']['targets'], pilots.TARGETS['select'])
        full = ordered_targets(case)
        self.assertEqual(full[:19], pilots.TARGETS['select'])
        self.assertEqual(full[19:], ['gallery.select.' + suffix for suffix in
            ('multiple', 'authority', 'tags', 'virtual', 'prefix', 'popup-note', 'async', 'deliver')])
        validate_popup_plan(case)
        case_coverage(case, self.inventory)
        for fault in ('unknown-popup', 'unknown-slot', 'missing-legacy', 'popup-for-control', 'missing-context', 'missing-guard'):
            broken = deepcopy(case)
            if fault == 'unknown-popup':
                checkpoint = next(c for c in broken['checkpoints'] if c['step'] == 'virtual-mounted')
                checkpoint['targets'].append('gallery.select.unknown.popup')
                checkpoint['rois'].append({'identity': 'unknown', 'kind': 'popup', 'target': 'gallery.select.unknown.popup', 'guard_px': 2})
            elif fault == 'unknown-slot':
                broken['fixture']['slots']['gallery.select.virtual']['popup']['control'] = 'gallery.select.unknown'
            elif fault == 'missing-legacy':
                broken['native']['targets'].pop(4)
            elif fault == 'popup-for-control':
                broken['checkpoints'][0]['rois'][2]['kind'] = 'popup'
            else:
                kind = 'sample-context' if fault == 'missing-context' else 'guard-band'
                broken['checkpoints'][0]['rois'] = [r for r in broken['checkpoints'][0]['rois'] if r['kind'] != kind]
            with self.subTest(fault=fault), self.assertRaises(ProtocolError):
                case_coverage(broken, self.inventory)

    def test_each_popup_has_a_checkpoint_before_child_input_and_real_pointer_open_actions(self):
        from demo_differential.actual_inputs import validate_popup_plan
        for fault in ('deadline', 'open-down', 'edit-first'):
            case = pilots.select_case()
            if fault == 'deadline':
                for checkpoint in case['checkpoints']:
                    checkpoint['targets'] = [t for t in checkpoint['targets'] if t != 'gallery.select.popup-note']
            elif fault == 'open-down':
                case['actions'] = [a for a in case['actions'] if a['step'] != 'virtual-open-down']
            else:
                edit = next(a for a in case['actions'] if a['step'] == 'popup-note-edit')
                case['actions'].remove(edit)
                case['actions'].insert(next(i for i, a in enumerate(case['actions']) if a['step'] == 'virtual-mounted'), edit)
            with self.subTest(fault=fault), self.assertRaises(ProtocolError):
                validate_popup_plan(case)

    def test_composition_unknown_js_css_limits_and_missing_context_are_rejected(self):
        case = pilots.select_case()
        controls, slots = case['fixture']['props']['controls'], case['fixture']['slots']
        for tree in ({'component': 'script', 'content': 'alert(1)'},
                     {'component': 'Flex', 'props': {'style': {'color': 'red'}}, 'children': []},
                     {'component': 'Flex', 'props': {'onClick': 'arbitrary JS'}, 'children': []},
                     {'component': 'Flex', 'children': [pilots.text('x')] * 513}):
            with self.subTest(tree=tree['component']), self.assertRaises(bootstrap.PreparationError):
                reference.validate_composition(tree, controls, slots)
        tree = pilots.leaf('gallery.select.controlled-search')
        for _ in range(26): tree = {'component': 'Flex', 'children': [tree]}
        with self.assertRaises(bootstrap.PreparationError):
            reference.validate_composition(tree, controls, slots)
        for key in ('algorithm', 'components'):
            with self.assertRaises(bootstrap.PreparationError):
                reference.validate_local_theme({key: 'arbitrary JS/CSS'})
        broken = deepcopy(case['fixture']['props']['composition'])
        broken['children'].pop()
        with self.assertRaises(bootstrap.PreparationError):
            reference.validate_composition(broken, controls, slots)

    def test_merged_adapter_and_seven_file_font_binding_reject_old_inputs(self):
        case = pilots.select_case()
        case['fixture']['viewport']['dpr'] = 1
        self.assertEqual([row['weight'] for row in case['fixture']['fonts']], [400, 400, 400, 600, 600, 500, 500])
        self.assertEqual(case['fixture']['fonts'][2]['identity'], 'noto-emoji')
        request = {'schema_version': 1, 'nonce': 'source-binding-negative', 'definition': case, 'font_files': []}
        old = deepcopy(request)
        old['definition']['official']['adapter_sha256'] = '192d5a384a08c3a918d7326f5f022370d8b34a2c59a716394fed031fe92cbefc'
        with self.assertRaisesRegex(bootstrap.PreparationError, 'adapter identity/hash'):
            reference.validate_request(old, {'source': 'unused-because-hash-fails-first'})
        # Only the immutable source read is substituted here; no font file or
        # browser is admitted by this mechanism control.
        source_hash = case['official']['source_paths'][0]['sha256']
        with patch.object(reference, 'sha', side_effect=lambda path:
                          case['official']['adapter_sha256'] if Path(path).name == 'app.tsx' else source_hash):
            with self.assertRaisesRegex(bootstrap.PreparationError, 'font chain missing'):
                reference.validate_request(request, {'source': 'mechanism-source'})
        from demo_reference_web.font_resources import admit_font
        supplied = {**case['fixture']['fonts'][0], 'path': 'out/absent-negative-control.ttf'}
        for field, wrong in [('weight', 600), ('sha256', '0' * 64), ('identity', 'unlocked')]:
            expected = {**case['fixture']['fonts'][0], field: wrong}
            with self.subTest(field=field), self.assertRaisesRegex(bootstrap.PreparationError, 'descriptor mismatch'):
                admit_font(expected, supplied, 'pinned')

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
