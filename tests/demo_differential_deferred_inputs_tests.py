"""Independent timeline/gate controls; no C++ or browser execution claims."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from demo_differential.actual_inputs import NATIVE_SOURCE, WEB_SOURCE, ordered_targets, validate_timeline
from demo_differential.observations import IncompleteRun
from demo_differential.protocol import ProtocolError


def case():
    return {'native': {'targets': ['parent']}, 'fixture': {
        'props': {'controls': {'parent': {'component': 'Select', 'width': 260},
                               'note': {'component': 'Input', 'placeholder': 'popup draft'}},
                  'composition': {'component': 'Flex', 'props': {'vertical': True, 'gap': 12},
                                  'children': [{'component': 'control', 'target': 'parent'}]}},
        'data': {'parent': {'value': 'desktop', 'options': []}},
        'slots': {'parent': {'popup': {'control': 'note'}}},
        'theme': {'algorithm': 'default', 'token': {'colorBgContainer': '#ffffff'}},
        'locale': 'zh-CN', 'format_locale': 'zh-CN', 'direction': 'ltr'},
        'actions': [{'step': 'open-down', 'kind': 'pointer-down', 'target': 'parent'},
                    {'step': 'open', 'kind': 'pointer-up', 'target': 'parent'},
                    {'step': 'mounted', 'kind': 'checkpoint', 'target': 'parent'},
                    {'step': 'edit', 'kind': 'text-commit', 'target': 'note'}],
        'checkpoints': [{'step': 'mounted', 'targets': ['parent', 'note']}]}


def snapshot(side, seq, step, mounted=True):
    initial = seq == 1
    ids = ['parent', 'note'] if mounted else ['parent']
    source = NATIVE_SOURCE if side == 'native' else WEB_SOURCE
    result = {'version': 1, 'source': source, 'phase': 'initial' if initial else 'settled',
        'query_seq': seq, 'step': step, 'page_generation': 1, 'ordered_targets': ['parent', 'note'],
        'instances': {}, 'controls': {}, 'data': {}, 'slots': {}, 'themes': {}, 'contexts': {},
        'slot_targets': {}, 'slot_composition': {}, 'current': {}, 'aliases': {},
        'composition': {'source': 'retained component tree and applied layout models',
                        'host': {'vertical': True, 'gap': 12},
                        'roots': [{'component': 'control', 'target': 'parent'}]}}
    for target in ['parent', 'note']:
        result['instances'][target] = {'status': 'mounted' if target in ids else 'deferred',
            'instance': target + '-1' if target in ids else '', 'owner_generation': 1,
            'parent_target': 'parent' if target == 'note' and mounted else '',
            'slot': 'popup' if target == 'note' and mounted else '',
            'first_observed_query': (1 if target == 'parent' or side == 'native' else 2) if target in ids else 0,
            **({'reason': 'closed-bound-popup'} if target not in ids else {})}
    for target in ids:
        result['controls'][target] = deepcopy(case()['fixture']['props']['controls'][target])
        result['data'][target] = {'value': 'desktop' if initial else 'service', 'options': []} if target == 'parent' else {}
        result['slots'][target] = {'popup': {'control': 'note'}} if target == 'parent' and side == 'web' else {}
        result['themes'][target] = {'source': 'retained ThemeScope applied config and resolved snapshot',
            'algorithms': ['default'], 'colorBgContainer': '#ffffff'}
        result['contexts'][target] = {'config_scope': 'config-1', 'theme_scope': 'theme-1',
            'config_generation': 1, 'theme_generation': 1, 'locale': 'zh-CN', 'format_locale': 'zh-CN', 'direction': 'ltr'}
        result['current'][target] = {'focused': target == 'note' and step == 'edit', 'value': ''}
    result['current']['parent'].update(value='desktop' if initial else 'service', open=not initial)
    if mounted:
        result['slot_targets'] = {'parent': {'popup': [{'control': 'note'}]}}
        result['slot_composition'] = {'parent': {'popup': {'component': 'Flex',
            'props': {'vertical': True, 'gap': 4, 'wrap': False, 'align': 'stretch'},
            'children': [{'component': 'origin'}, {'component': 'control', 'target': 'note'}]}}}
    return result


def stream(side):
    initial = snapshot(side, 1, 'setup', mounted=side == 'native')
    rows = [{'type': 'hello', 'step': '', 'resources': {} if side == 'native' else {
        'actual_initial_inputs': deepcopy(initial), 'actual_inputs': deepcopy(initial)}}]
    for seq, step in enumerate(('open-down', 'open', 'mounted', 'edit'), 2):
        query = snapshot(side, seq, step)
        row = {'type': 'receipt', 'step': step, 'receipt': {'dispatched': True},
               'resources': {'actual_initial_inputs': deepcopy(initial), 'actual_inputs': query}}
        rows.append(row)
        if step == 'mounted':
            rows.append({**deepcopy(row), 'type': 'checkpoint'})
    rows.append({**deepcopy(rows[-1]), 'type': 'complete', 'step': ''})
    return {'records': rows}


class DeferredInputTests(unittest.TestCase):
    def feedback_stream(self, side):
        definition = case()
        definition['fixture']['props']['composition']['children'].append(
            {'component': 'source-feedback', 'identity': 'status', 'content': 'initial feedback'})
        definition['oracle'] = [{'op': 'equals', 'step': 'edit', 'side': 'both', 'origin': 'independent-contract',
            'path': '/resources/actual_inputs/feedbacks/status', 'value': 'changed feedback'}]
        observed = stream(side)
        for row in observed['records']:
            for name in ('actual_initial_inputs', 'actual_inputs'):
                query = row['resources'].get(name)
                if not query:
                    continue
                value = 'changed feedback' if name == 'actual_inputs' and query['query_seq'] == 5 else 'initial feedback'
                query['composition']['roots'].append({'component': 'source-feedback', 'identity': 'status', 'content': value})
                query['feedbacks'] = {'status': value}
        return definition, observed

    def test_live_feedback_uses_an_independent_step_literal_and_retained_text(self):
        for side in ('native', 'web'):
            definition, observed = self.feedback_stream(side)
            before = deepcopy(observed)
            validate_timeline(definition, observed, side)
            self.assertEqual(observed, before)
            for row in observed['records']:
                query = row['resources'].get('actual_inputs')
                if query and query['query_seq'] == 5:
                    query['feedbacks']['status'] = 'same wrong feedback'
                    query['composition']['roots'][-1]['content'] = 'same wrong feedback'
            with self.subTest(side=side), self.assertRaisesRegex(ProtocolError, 'authored composition'):
                validate_timeline(definition, observed, side)

    def test_feedback_cannot_disagree_with_its_actual_text_or_change_before_the_action(self):
        for side in ('native', 'web'):
            for early in (False, True):
                definition, observed = self.feedback_stream(side)
                query = observed['records'][2]['resources']['actual_inputs']
                query['feedbacks']['status'] = 'changed feedback'
                if early:
                    query['composition']['roots'][-1]['content'] = 'changed feedback'
                with self.subTest(side=side, early=early), self.assertRaises(ProtocolError):
                    validate_timeline(definition, observed, side)

    def test_feedback_literal_cannot_be_learned_from_the_other_adapter(self):
        definition, observed = self.feedback_stream('web')
        definition['oracle'][0]['origin'] = 'web-observation'
        with self.assertRaisesRegex(ProtocolError, 'independent literal'):
            validate_timeline(definition, observed, 'web')

    def test_eager_native_and_web_deferred_close_with_real_action_bound_queries(self):
        for side in ('native', 'web'):
            observed = stream(side)
            before = deepcopy(observed)
            self.assertEqual(set(validate_timeline(case(), observed, side)), {'parent', 'note'})
            self.assertEqual(observed, before)

    def test_initial_seed_stays_desktop_while_live_controlled_value_is_service(self):
        validate_timeline(case(), stream('native'), 'native')
        broken = stream('native')
        broken['records'][1]['resources']['actual_initial_inputs']['data']['parent']['value'] = 'service'
        with self.assertRaisesRegex(ProtocolError, 'seed/data'):
            validate_timeline(case(), broken, 'native')

    def test_correct_public_seed_does_not_hide_wrong_actual_initial_selection(self):
        for side in ('native', 'web'):
            observed = stream(side)
            for row in observed['records']:
                initial = row['resources'].get('actual_initial_inputs')
                if initial: initial['current']['parent']['value'] = 'wrong actual selection'
            with self.assertRaisesRegex(ProtocolError, 'actual editor/selection'):
                validate_timeline(case(), observed, side)

    def test_new_instance_rechecks_seed_without_freezing_ongoing_controlled_value(self):
        for side in ('native', 'web'):
            observed = stream(side)
            for row in observed['records'][2:]:
                query = row['resources']['actual_inputs']
                query['instances']['parent'].update(instance='parent-2', first_observed_query=3)
                if query['query_seq'] == 3:
                    query['data']['parent']['value'] = 'desktop'
                    query['current']['parent']['value'] = 'desktop'
            validate_timeline(case(), observed, side)
            observed['records'][2]['resources']['actual_inputs']['data']['parent']['value'] = 'wrong seed'
            with self.subTest(side=side), self.assertRaisesRegex(ProtocolError, 'seed/data'):
                validate_timeline(case(), observed, side)

    def test_same_instance_cannot_truncate_static_options_after_initial_validation(self):
        for side in ('native', 'web'):
            definition = case()
            options = [{'value': index, 'label': str(index), 'disabled': False, 'key': ''} for index in range(25)]
            definition['fixture']['data']['parent']['options'] = deepcopy(options)
            observed = stream(side)
            for row in observed['records']:
                for name in ('actual_initial_inputs', 'actual_inputs'):
                    query = row['resources'].get(name)
                    if query:
                        query['data']['parent']['options'] = deepcopy(options)
            validate_timeline(definition, observed, side)
            observed['records'][2]['resources']['actual_inputs']['data']['parent']['options'] = deepcopy(options[:20])
            with self.subTest(side=side), self.assertRaisesRegex(ProtocolError, 'static data'):
                validate_timeline(definition, observed, side)

    def test_later_retirement_of_only_a_static_caption_is_rejected(self):
        for side in ('native', 'web'):
            definition = case()
            definition['fixture']['props']['composition']['children'].insert(0,
                {'component': 'Typography.Text', 'content': 'Required static context'})
            observed = stream(side)
            for row in observed['records']:
                for name in ('actual_initial_inputs', 'actual_inputs'):
                    query = row['resources'].get(name)
                    if query:
                        query['composition']['roots'].insert(0,
                            {'component': 'Typography.Text', 'content': 'Required static context'})
            validate_timeline(definition, observed, side)
            observed['records'][2]['resources']['actual_inputs']['composition']['roots'].pop(0)
            with self.subTest(side=side), self.assertRaisesRegex(ProtocolError, 'authored composition'):
                validate_timeline(definition, observed, side)

    def test_mounted_wrong_placeholder_theme_slot_order_and_owner_fail_each_side(self):
        faults = [lambda q: q['controls']['note'].update(placeholder='wrong'),
                  lambda q: q['themes']['note'].update(algorithms=['dark']),
                  lambda q: q['contexts']['note'].update(locale='en-US'),
                  lambda q: q['instances']['note'].update(parent_target='other'),
                  lambda q: q['slot_composition']['parent']['popup']['children'].reverse(),
                  lambda q: q['slot_composition']['parent']['popup']['children'].pop(0)]
        for side in ('native', 'web'):
            for fault in faults:
                observed = stream(side)
                fault(observed['records'][1 if side == 'native' else 1]['resources']['actual_inputs'])
                with self.subTest(side=side, fault=fault), self.assertRaises(ProtocolError):
                    validate_timeline(case(), observed, side)

    def test_never_open_missing_query_missing_deadline_and_not_dispatched_are_incomplete(self):
        for fault in ('never-open', 'query', 'deadline', 'dispatch', 'native-missing'):
            observed = stream('web' if fault != 'native-missing' else 'native')
            definition = case()
            if fault == 'never-open':
                for row in observed['records'][1:]:
                    q = row['resources']['actual_inputs']
                    pending = snapshot('web', q['query_seq'], q['step'], mounted=False)
                    pending['current']['parent']['open'] = False
                    row['resources']['actual_inputs'] = pending
            elif fault == 'query':
                del observed['records'][1]['resources']['actual_inputs']
            elif fault == 'deadline':
                definition['checkpoints'][0]['targets'] = ['parent']
            elif fault == 'dispatch':
                observed['records'][1]['receipt']['dispatched'] = False
            else:
                observed['records'][1]['resources']['actual_inputs']['instances']['note'].update(
                    status='missing', instance='', first_observed_query=0, reason='no-live-host')
                for key in ('controls', 'data', 'slots', 'themes', 'contexts', 'current'):
                    observed['records'][1]['resources']['actual_inputs'][key].pop('note')
            with self.subTest(fault=fault), self.assertRaises(IncompleteRun):
                validate_timeline(definition, observed, 'native' if fault == 'native-missing' else 'web')

    def test_same_sequence_changed_snapshot_stale_owner_and_rewritten_initial_fail(self):
        for fault in ('sequence', 'generation', 'initial', 'first', 'unknown', 'bool'):
            observed = stream('web')
            q = observed['records'][2]['resources']['actual_inputs']
            if fault == 'sequence': q['query_seq'] = 2
            if fault == 'generation': q['instances']['note']['owner_generation'] = 2
            if fault == 'initial': observed['records'][2]['resources']['actual_initial_inputs']['step'] = 'changed'
            if fault == 'first': q['instances']['note']['first_observed_query'] = 3
            if fault == 'unknown': q['instances']['note']['recipe_is_evidence'] = True
            if fault == 'bool': q['query_seq'] = True
            with self.subTest(fault=fault), self.assertRaises(ProtocolError):
                validate_timeline(case(), observed, 'web')

    def test_both_sides_same_wrong_input_are_not_blessed(self):
        for side in ('native', 'web'):
            observed = stream(side)
            observed['records'][1]['resources']['actual_inputs']['controls']['note']['placeholder'] = 'same wrong'
            with self.assertRaises(ProtocolError):
                validate_timeline(case(), observed, side)

    def test_live_controlled_values_use_an_independent_action_oracle(self):
        from demo_differential.oracle import evaluate
        definition = case()
        definition['oracle'] = [{'op': 'equals', 'step': 'mounted', 'side': 'both', 'origin': 'independent-contract',
            'path': '/resources/actual_inputs/current/parent/value', 'value': 'service'}]
        for side in ('native', 'web'):
            observed = stream(side)
            observed.update(checkpoints={r['step']: r for r in observed['records'] if r['type'] == 'checkpoint'}, receipts={}, callbacks=[])
            self.assertFalse(evaluate(definition, observed, side))
            # Both sides can agree on the same wrong current value. The t0 seed
            # still remains desktop; the independent service oracle must fail.
            observed['checkpoints']['mounted']['resources']['actual_inputs']['current']['parent']['value'] = 'same wrong'
            self.assertTrue(evaluate(definition, observed, side))

    def test_wrong_first_mounted_button_label_is_rejected_on_each_side(self):
        definition = case()
        definition['fixture']['props']['controls']['note'] = {'component': 'Button'}
        definition['fixture']['slots']['note'] = {'content': 'Deliver actual options'}
        for side in ('native', 'web'):
            observed = stream(side)
            for row in observed['records']:
                for key in ('actual_inputs', 'actual_initial_inputs'):
                    q = row['resources'].get(key)
                    if q and 'note' in q['controls']:
                        q['controls']['note'] = {'component': 'Button'}
                        q['slots']['note'] = {'content': ['wrong label'] if side == 'native' else 'wrong label'}
            with self.assertRaises(ProtocolError): validate_timeline(definition, observed, side)

    def test_retired_instance_token_cannot_be_reused_after_a_new_generation(self):
        observed = stream('web')
        for index in (2, 3, 4):
            q = observed['records'][index]['resources']['actual_inputs']
            q['instances']['note'].update(instance='note-2', first_observed_query=3)
        with self.assertRaises(IncompleteRun):
            validate_timeline(case(), observed, 'web')

    def test_schema_rejects_unknown_nested_slot_theme_and_nonfinite_or_boolean_numbers(self):
        faults = [lambda q: q['slots']['note'].update(prefix={'recipe': 'fake'}),
                  lambda q: q['themes']['note'].update(Select={'unknown': True}),
                  lambda q: q['controls']['note'].update(width=True),
                  lambda q: q['controls']['note'].update(width=float('inf')),
                  lambda q: q['aliases'].update({'unknown.popup': {'instance': 'fake', 'parent_target': 'unknown', 'owner_generation': 1}})]
        for mutate in faults:
            observed = stream('web')
            mutate(observed['records'][1]['resources']['actual_inputs'])
            with self.assertRaises(ProtocolError): validate_timeline(case(), observed, 'web')

    def test_popup_roi_requires_the_actual_alias_instance(self):
        definition = case()
        definition['checkpoints'][0]['targets'].append('parent.popup')
        observed = stream('web')
        checkpoint = next(r for r in observed['records'] if r['type'] == 'checkpoint')
        checkpoint['resources']['actual_inputs']['aliases'] = {
            'parent.popup': {'instance': 'real-popup-1', 'parent_target': 'parent', 'owner_generation': 1}}
        checkpoint['targets'] = [{'identity': 'parent.popup', 'state': {'instance': 'fake-popup'}, 'owner_generation': 1}]
        with self.assertRaises(IncompleteRun): validate_timeline(definition, observed, 'web')

    def test_complete_rejects_pending_or_a_witness_from_an_old_instance(self):
        for fault in ('pending', 'new-instance-after-deadline'):
            observed = stream('web')
            for row in observed['records'][-2:]:
                q = row['resources']['actual_inputs']
                if fault == 'pending':
                    q['instances']['note'].update(status='deferred', instance='', first_observed_query=0,
                        parent_target='', slot='', reason='closed-bound-popup')
                    q['current']['parent']['open'] = False
                    for key in ('controls', 'data', 'slots', 'themes', 'contexts', 'current'):
                        q[key].pop('note')
                else:
                    q['instances']['note'].update(instance='note-new', first_observed_query=q['query_seq'])
            with self.subTest(fault=fault), self.assertRaises(IncompleteRun):
                validate_timeline(case(), observed, 'web')

    def test_legacy_prefix_and_full_stable_union_keep_slot_identity(self):
        self.assertEqual(ordered_targets(case()), ['parent', 'note'])
        broken = case()
        broken['fixture']['props']['controls']['unconsumed'] = {'component': 'Input'}
        with self.assertRaises(ProtocolError): ordered_targets(broken)


if __name__ == '__main__':
    unittest.main()
