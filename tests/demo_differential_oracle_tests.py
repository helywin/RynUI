"""Independent behavior controls, including equal-but-wrong adapter results."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_differential.oracle import evaluate, exact, pointer, projected_trace
from demo_differential.protocol import ProtocolError, validate
from demo_differential_protocol_tests import valid_case


class OracleTests(unittest.TestCase):
    def setUp(self):
        self.case = valid_case()
        self.record = {
            'step': 'initial', 'receipt': {'dispatched': True, 'consumed': True, 'hit': 'control'},
            'state': {'value': 'accepted', 'focused_target': 'control', 'keys': ['a', 'b', 'c']},
            'resources': {'retired_owners': ['retired'], 'passive_idle': {
                'retired': {'duration_us': 1000, 'callbacks': 0, 'capture': 0, 'session': 0,
                            'deadlines': 0, 'autonomous_submissions': 0, 'uploads': 0}}}}
        self.observed = {'checkpoints': {'initial': self.record},
                         'receipts': {'initial': self.record}, 'callbacks': []}

    def predicate(self, op, **fields):
        return {'op': op, 'step': 'initial', 'side': 'both',
                'origin': 'independent-contract', **fields}

    def evaluate(self, *predicates, side='native', observation=None):
        self.case['oracle'] = list(predicates)
        return evaluate(self.case, observation or self.observed, side)

    def callback(self, seq, event='change', **fields):
        return {'seq': seq, 'step': 'initial', 'time_us': seq, 'target': 'control', 'key': 'stable',
                'event': event, 'payload': {'proposed': 'new', 'committed': 'accepted', 'accepted': False},
                'owner_generation': 1, 'layer': 'native-core-dispatch', **fields}

    def test_exact_business_values_have_no_rounding_or_normalization(self):
        for a, b in [(True, 1), (False, 0), ('e\u0301', 'é'), ([1, 2], [2, 1]),
                     ({'a': 1}, {'a': 1, 'b': None}), (1.000001, 1.0), (float('nan'), float('nan'))]:
            with self.subTest(a=a, b=b):
                self.assertFalse(exact(a, b))
        self.assertTrue(exact({'a': [1, '值', None]}, {'a': [1.0, '值', None]}))

    def test_pointer_escaped_identity_and_invalid_paths(self):
        self.assertEqual(pointer({'a/b': {'~key': [3]}}, '/a~1b/~0key/0'), 3)
        for path in ('/a/01', '/a/-1', '/a/١', '/a/~2', 'a/0', '/a/0/x'):
            with self.subTest(path=path), self.assertRaises((ProtocolError, KeyError, IndexError)):
                pointer({'a': [3]}, path)

    def test_double_wrong_sides_fail_and_missing_evidence_is_red(self):
        predicate = self.predicate('equals', path='/state/value', value='contract')
        self.assertEqual(len(self.evaluate(predicate, side='native')), 1)
        self.assertEqual(len(self.evaluate(predicate, side='web')), 1)
        self.assertEqual(len(self.evaluate(self.predicate('equals', path='/absent', value=None))), 1)
        self.assertEqual(len(self.evaluate({**predicate, 'step': 'never-executed'})), 1)

    def test_payload_order_duplicates_and_stable_identity_survive_projection(self):
        self.observed['callbacks'] = [self.callback(0), self.callback(1)]
        projected = projected_trace(self.observed)
        self.assertEqual(len(projected), 2)
        changed = deepcopy(self.observed)
        changed['callbacks'][0].update(time_us=99, owner_generation=88, layer='browser-input')
        self.assertEqual(projected_trace(changed), projected)
        for mutate in ('key', 'target', 'step', 'payload', 'duplicate'):
            changed = deepcopy(self.observed)
            if mutate == 'payload':
                changed['callbacks'][0]['payload']['accepted'] = True
            elif mutate == 'duplicate':
                changed['callbacks'].pop()
            else:
                changed['callbacks'][0][mutate] = 'different'
            with self.subTest(mutate=mutate):
                self.assertNotEqual(projected_trace(changed), projected)

    def test_exact_callback_counts_and_order_never_deduplicate(self):
        self.observed['callbacks'] = [self.callback(0, 'open-change'), self.callback(1, 'change')]
        predicates = [self.predicate('count', stream='callbacks', target='control', event='change', equals=1),
                      self.predicate('ordered_events', stream='callbacks', events=['open-change', 'change']),
                      self.predicate('happens_before', first='open-change', second='change')]
        self.assertEqual(self.evaluate(*predicates), [])
        self.observed['callbacks'].append(self.callback(2))
        self.assertEqual(len(self.evaluate(*predicates)), 2)
        self.observed['callbacks'].reverse()
        self.assertEqual(len(self.evaluate(*predicates)), 3)

    def test_receipt_event_filters_and_consumption(self):
        predicates = [self.predicate('count', stream='receipts', target='control', event='checkpoint', equals=1),
                      self.predicate('ordered_events', stream='receipts', events=['initial']),
                      self.predicate('consumed_by', target='control')]
        self.assertEqual(self.evaluate(*predicates), [])
        self.assertEqual(len(self.evaluate({**predicates[0], 'event': 'pointer-up'})), 1)
        self.record['receipt']['dispatched'] = False
        self.assertEqual(len(self.evaluate(*predicates)), 2)

    def test_focus_ordered_keys_domain_interval_and_unchanged(self):
        self.observed['checkpoints']['previous'] = deepcopy(self.record)
        self.case['actions'].insert(0, {**self.case['actions'][0], 'step': 'previous'})
        predicates = [self.predicate('focused', target='control'),
                      self.predicate('contains_keys_in_order', path='/state/keys', keys=['a', 'c']),
                      self.predicate('within', path='/resources/passive_idle/retired/duration_us', minimum=1000, maximum=1000),
                      self.predicate('unchanged', path='/state/value', from_step='previous')]
        self.assertEqual(self.evaluate(*predicates), [])
        self.record['state'].update(focused_target='background', keys=['c', 'a'], value='changed')
        self.record['resources']['passive_idle']['retired']['duration_us'] = 1001
        self.assertEqual(len(self.evaluate(*predicates)), 4)

    def test_unchanged_cannot_read_future_or_unselected_observation(self):
        self.observed['checkpoints']['future'] = deepcopy(self.record)
        self.case['actions'].append({**self.case['actions'][0], 'step': 'future'})
        self.assertEqual(len(self.evaluate(self.predicate('unchanged', path='/state/value', from_step='future'))), 1)
        self.assertEqual(len(self.evaluate(self.predicate('unchanged', path='/state/value', from_step='initial'))), 1)
        self.case['actions'].pop()
        self.assertEqual(len(self.evaluate({**self.predicate('equals', path='/state/value', value='accepted'), 'step': 'future'})), 1)

    def test_retirement_idle_counters_and_deadline_are_strict(self):
        predicates = [self.predicate('retired', owner='retired'),
                      self.predicate('idle_after', owner='retired', duration_us=1000)]
        self.assertEqual(self.evaluate(*predicates), [])
        for field in ('callbacks', 'capture', 'session', 'deadlines', 'autonomous_submissions', 'uploads'):
            for value in (1, False):
                idle = self.record['resources']['passive_idle']['retired']
                idle[field] = value
                with self.subTest(field=field, value=value):
                    self.assertEqual(len(self.evaluate(*predicates)), 1)
                idle[field] = 0
        self.record['resources']['passive_idle']['retired']['duration_us'] = 999
        self.assertEqual(len(self.evaluate(*predicates)), 1)

    def test_side_and_expected_provenance_cannot_be_silently_accepted(self):
        predicate = self.predicate('equals', path='/state/value', value='accepted')
        self.assertEqual(self.evaluate({**predicate, 'side': 'web'}), [])
        with self.assertRaises(ProtocolError):
            self.evaluate({**predicate, 'origin': 'native-observation'})
        self.case['oracle'] = [{**predicate, 'value': 'native.observed-value'}]
        with self.assertRaises(ProtocolError):
            validate('case', self.case)
        with self.assertRaises(ProtocolError):
            self.evaluate(predicate, side='invented')


if __name__ == '__main__':
    unittest.main()
