"""Closed independent semantic oracle. Observations never generate expected values."""
from __future__ import annotations

import math
import re

from .protocol import ProtocolError, finite


def pointer(value, path):
    if not isinstance(path, str) or not path.startswith('/'):
        raise ProtocolError('oracle paths must use a non-root JSON Pointer')
    for component in path[1:].split('/'):
        # Reject invalid RFC 6901 escapes instead of silently changing identity.
        if re.search(r'~(?![01])', component):
            raise ProtocolError('invalid oracle pointer escape')
        component = component.replace('~1', '/').replace('~0', '~')
        if isinstance(value, list):
            if not component.isascii() or not component.isdecimal() or (len(component) > 1 and component[0] == '0'):
                raise ProtocolError('invalid oracle array index')
            value = value[int(component)]
        elif isinstance(value, dict):
            value = value[component]
        else:
            raise ProtocolError('oracle pointer traverses a scalar')
    return value


def exact(actual, expected):
    # bool is not a numeric zero/one, and no rounding or callback deduplication.
    if type(actual) in (int, float) and type(expected) in (int, float):
        return math.isfinite(actual) and math.isfinite(expected) and actual == expected
    if type(actual) is not type(expected):
        return False
    if isinstance(actual, list):
        return len(actual) == len(expected) and all(exact(a, b) for a, b in zip(actual, expected))
    if isinstance(actual, dict):
        return set(actual) == set(expected) and all(exact(actual[key], expected[key]) for key in actual)
    return actual == expected


def evaluate(case, observation, side):
    if side not in ('native', 'web'):
        raise ProtocolError('oracle side must name one actual adapter')
    finite(case['oracle'])
    if not case['oracle'] or any(row['origin'] not in ('independent-contract', 'web-observation') for row in case['oracle']):
        raise ProtocolError('missing oracle or untrusted expected origin')
    failures = []
    action_order = {row['step']: index for index, row in enumerate(case['actions'])}
    for index, predicate in enumerate(case['oracle']):
        if predicate['side'] not in ('both', side):
            continue
        step = predicate['step']
        record = observation['checkpoints'].get(step, observation['receipts'].get(step))
        if record is None or step not in action_order:
            failures.append({'category': 'input', 'step': step, 'oracle_index': index,
                             'message': 'oracle step was never observed'})
            continue
        op = predicate['op']
        actual = None
        expected = None
        passed = False
        try:
            if op == 'equals':
                actual = pointer(record, predicate['path']); expected = predicate['value']
                passed = exact(actual, expected)
            elif op == 'unchanged':
                previous = observation['checkpoints'].get(predicate['from_step'],
                            observation['receipts'].get(predicate['from_step']))
                if (previous is None or predicate['from_step'] not in action_order or
                        action_order[predicate['from_step']] >= action_order[step]):
                    raise ProtocolError('unchanged has no earlier causative observation')
                actual = pointer(record, predicate['path']); expected = pointer(previous, predicate['path'])
                passed = exact(actual, expected)
            elif op == 'count':
                if predicate['stream'] == 'callbacks':
                    actual = sum(row['step'] == step and row['target'] == predicate['target'] and
                                 row['event'] == predicate['event'] for row in observation['callbacks'])
                else:
                    kinds = {action['step']: action['kind'] for action in case['actions']}
                    actual = sum(row['step'] == step and row['receipt']['hit'] == predicate['target'] and
                                 kinds[row['step']] == predicate['event'] and row['receipt']['dispatched']
                                 for row in observation['receipts'].values())
                expected = predicate['equals']; passed = actual == expected
            elif op == 'ordered_events':
                rows = observation['callbacks'] if predicate['stream'] == 'callbacks' else observation['receipts'].values()
                actual = [row['event'] if 'event' in row else row['step'] for row in rows if row['step'] == step]
                expected = predicate['events']; passed = actual == expected
            elif op == 'happens_before':
                events = [row['event'] for row in observation['callbacks']]
                first = [i for i, value in enumerate(events) if value == predicate['first']]
                second = [i for i, value in enumerate(events) if value == predicate['second']]
                actual = {'first': first, 'second': second}; expected = 'all first events precede all second events'
                passed = bool(first and second and max(first) < min(second))
            elif op == 'focused':
                actual = record['state']['focused_target']; expected = predicate['target']; passed = actual == expected
            elif op == 'contains_keys_in_order':
                actual = pointer(record, predicate['path']); expected = predicate['keys']
                if not isinstance(actual, list):
                    raise ProtocolError('ordered keys require an actual array')
                cursor = iter(actual); passed = all(any(exact(item, key) for item in cursor) for key in expected)
            elif op == 'within':
                actual = pointer(record, predicate['path']); expected = [predicate['minimum'], predicate['maximum']]
                passed = type(actual) in (int, float) and math.isfinite(actual) and expected[0] <= actual <= expected[1]
            elif op == 'consumed_by':
                actual = record['receipt']; expected = predicate['target']
                passed = actual['dispatched'] and actual['consumed'] and actual['hit'] == expected
            elif op == 'retired':
                actual = record['resources']['retired_owners']; expected = predicate['owner']; passed = expected in actual
            elif op == 'idle_after':
                actual = record['resources']['passive_idle'][predicate['owner']]
                expected = predicate['duration_us']
                passed = (type(actual['duration_us']) is int and actual['duration_us'] >= expected and
                          all(type(actual[key]) is int and actual[key] == 0 for key in
                              ('callbacks', 'capture', 'session', 'deadlines', 'autonomous_submissions', 'uploads')))
            else:
                raise ProtocolError('unknown oracle predicate')
        except (KeyError, IndexError, TypeError, ValueError) as error:
            failures.append({'category': 'logic', 'step': step, 'oracle_index': index, 'op': op,
                             'message': f'missing/invalid oracle evidence: {error}'})
            continue
        if not passed:
            category = 'lifecycle' if op in ('retired', 'idle_after') else 'input' if op == 'consumed_by' else 'state' if op in ('focused', 'unchanged') else 'logic'
            failures.append({'category': category, 'step': step, 'oracle_index': index,
                             'op': op, 'actual': actual, 'expected': expected})
    return failures


def projected_trace(observation):
    # Only adapter-local time/owner/layer are omitted. Stable key, target, step,
    # duplicate callbacks, and every proposed/committed/accepted payload survive.
    return [{key: row[key] for key in ('step', 'target', 'key', 'event', 'payload')} for row in observation['callbacks']]
