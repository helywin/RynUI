"""Strict, bounded JSON protocol validation using the schemas shipped with the runner.

This interpreter deliberately supports only the JSON Schema 2020-12 vocabulary
used by our own schemas. Unsupported schema keywords fail closed; it is not a
replacement for a general-purpose JSON Schema implementation.
"""
from __future__ import annotations

import json
import math
from pathlib import Path, PurePosixPath
import re

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / 'tests/fixtures/demo-differential'
MAX_BYTES = 16 * 1024 * 1024


class ProtocolError(ValueError):
    pass


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ProtocolError(f'duplicate JSON field: {key}')
        result[key] = value
    return result


def finite(value, depth=0):
    if depth > 48:
        raise ProtocolError('JSON nesting exceeds 48 levels')
    if isinstance(value, float) and not math.isfinite(value):
        raise ProtocolError('non-finite number')
    if isinstance(value, dict):
        for item in value.values():
            finite(item, depth + 1)
    if isinstance(value, list):
        for item in value:
            finite(item, depth + 1)


def loads(text):
    if len(text.encode('utf-8')) > MAX_BYTES:
        raise ProtocolError('JSON exceeds protocol byte limit')
    try:
        value = json.loads(text, object_pairs_hook=_pairs,
                           parse_constant=lambda word: (_ for _ in ()).throw(ProtocolError(f'non-finite {word}')))
    except (json.JSONDecodeError, RecursionError) as error:
        raise ProtocolError(f'invalid JSON: {error}') from error
    finite(value)
    return value


def read(path):
    path = Path(path)
    if path.stat().st_size > MAX_BYTES:
        raise ProtocolError('JSON exceeds protocol byte limit')
    return loads(path.read_text(encoding='utf-8'))


def relative_path(value):
    if (not isinstance(value, str) or not value or '\\' in value or ':' in value or '\x00' in value
            or value.startswith('/') or any(part in ('', '.', '..') for part in value.split('/'))):
        raise ProtocolError(f'unsafe relative path: {value!r}')
    return PurePosixPath(value)


def output_path(value, root=ROOT):
    path = Path(value).resolve()
    out = (Path(root) / 'out').resolve()
    if not path.is_relative_to(out) or path == out:
        raise ProtocolError('output must be strictly beneath this checkout out/')
    return path


def unique(rows, field='identity'):
    identities = [row[field] for row in rows]
    if len(set(identities)) != len(identities):
        raise ProtocolError(f'duplicate ordered identity: {field}')


def _validate(value, rule, document, location='$'):
    supported = {'$schema', '$id', '$defs', '$ref', 'title', 'description', 'type', 'properties', 'required',
                 'additionalProperties', 'items', 'minItems', 'maxItems', 'uniqueItems', 'enum', 'const',
                 'minimum', 'maximum', 'exclusiveMinimum', 'minLength', 'maxLength', 'pattern', 'format',
                 'oneOf', 'anyOf', 'allOf'}
    if set(rule) - supported:
        raise ProtocolError(f'unsupported schema vocabulary: {set(rule) - supported}')
    if '$ref' in rule:
        ref = rule['$ref']
        if not ref.startswith('#/$defs/'):
            raise ProtocolError('only local schema definitions are permitted')
        return _validate(value, document['$defs'][ref[8:]], document, location)
    for branch in rule.get('allOf', []):
        _validate(value, branch, document, location)
    for keyword in ('oneOf', 'anyOf'):
        if keyword in rule:
            accepted = 0
            for branch in rule[keyword]:
                try:
                    _validate(value, branch, document, location)
                    accepted += 1
                except ProtocolError:
                    pass
            if accepted < 1 or (keyword == 'oneOf' and accepted != 1):
                raise ProtocolError(f'{location}: {keyword} did not match uniquely')
    types = {'object': dict, 'array': list, 'string': str, 'integer': int, 'number': (int, float),
             'boolean': bool, 'null': type(None)}
    if 'type' in rule:
        expected = rule['type']
        if not isinstance(expected, list):
            expected = [expected]
        if not any(type(value) in (types[t] if isinstance(types[t], tuple) else (types[t],)) for t in expected):
            raise ProtocolError(f'{location}: wrong type')
    if 'const' in rule and (type(value) is not type(rule['const']) or value != rule['const']):
        raise ProtocolError(f'{location}: wrong constant')
    if 'enum' in rule and not any(type(value) is type(item) and value == item for item in rule['enum']):
        raise ProtocolError(f'{location}: unknown enum value')
    if isinstance(value, dict):
        missing = set(rule.get('required', [])) - set(value)
        if missing:
            raise ProtocolError(f'{location}: missing fields {sorted(missing)}')
        props = rule.get('properties', {})
        additional = rule.get('additionalProperties', True)
        for key, item in value.items():
            if key in props:
                _validate(item, props[key], document, f'{location}.{key}')
            elif additional is False:
                raise ProtocolError(f'{location}: unknown field {key}')
            elif isinstance(additional, dict):
                _validate(item, additional, document, f'{location}.{key}')
    if isinstance(value, list):
        if not rule.get('minItems', 0) <= len(value) <= rule.get('maxItems', 1000000):
            raise ProtocolError(f'{location}: invalid array size')
        if rule.get('uniqueItems') and len({json.dumps(item, sort_keys=True) for item in value}) != len(value):
            raise ProtocolError(f'{location}: duplicate array member')
        for index, item in enumerate(value):
            _validate(item, rule.get('items', {}), document, f'{location}[{index}]')
    if isinstance(value, str):
        if not rule.get('minLength', 0) <= len(value) <= rule.get('maxLength', MAX_BYTES):
            raise ProtocolError(f'{location}: invalid string length')
        if 'pattern' in rule and not re.search(rule['pattern'], value):
            raise ProtocolError(f'{location}: invalid spelling')
        if rule.get('format') == 'relative-path':
            relative_path(value)
    if type(value) in (int, float):
        if not rule.get('minimum', -math.inf) <= value <= rule.get('maximum', math.inf):
            raise ProtocolError(f'{location}: number out of range')
        if 'exclusiveMinimum' in rule and value <= rule['exclusiveMinimum']:
            raise ProtocolError(f'{location}: number below exclusive minimum')


def validate(kind, value):
    finite(value)
    document = read(FIXTURES / f'{kind}.schema.json')
    _validate(value, document, document)
    if kind == 'case':
        unique(value['actions'], 'step')
        unique(value['checkpoints'], 'step')
        previous = -1
        steps = set()
        for action in value['actions']:
            if action['time_us'] < previous:
                raise ProtocolError('action clock went backwards')
            previous = action['time_us']
            steps.add(action['step'])
        if any(checkpoint['step'] not in steps for checkpoint in value['checkpoints']):
            raise ProtocolError('checkpoint has no action')
        if [x['step'] for x in value['actions'] if x['kind'] == 'checkpoint'] != [x['step'] for x in value['checkpoints']]:
            raise ProtocolError('checkpoint action/declaration order differs')
        if any('native.' in json.dumps(predicate) for predicate in value['oracle']):
            raise ProtocolError('native-derived expected is forbidden')
    if kind == 'inventory':
        for vector in ('catalog', 'routes', 'samples', 'sources', 'targets'):
            unique(value[vector])
    if kind == 'coverage':
        unique(value['rows'])
    return value
