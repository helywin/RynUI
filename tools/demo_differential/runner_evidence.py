"""Validate both existing adapter contracts without rewriting their raw records."""
from __future__ import annotations

import hashlib
from pathlib import Path

from .observations import IncompleteRun, image_extent, read_observations
from .oracle import exact
from .protocol import ProtocolError, loads, relative_path, validate
from .adapters import RunError


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def artifact(directory, name):
    relative_path(name)
    path = (Path(directory) / name).resolve()
    if not path.is_relative_to(Path(directory).resolve()) or not path.is_file():
        raise IncompleteRun('artifact missing or escaping case output: ' + name)
    return path


def native(path, case, nonce, manifest, binary_hash, preset, configuration, *, expected_fault='none'):
    # Incomplete execution has priority over source/configuration drift.
    result = read_observations(path, case, nonce, adapter='rynui-gallery-native-v1')
    provenance = result['hello']['provenance']
    if (provenance.get('source') != manifest or provenance.get('binary_sha256') != binary_hash or
            provenance.get('preset') != preset or provenance.get('configuration') != configuration):
        raise RunError(2, 'reference-drift', 'native full build closure/binary SHA differs; rebuild required')
    from .meta_test import reference_cache_publish_allowed
    reference_cache_publish_allowed(expected_fault)  # Reject unknown hook identities.
    if provenance.get('desktop_fallback') is not False or any(
            row['provenance'].get('fault') != expected_fault for row in result['records']):
        raise ProtocolError('desktop fallback or unexpected acceptance fault fingerprint')
    backend = provenance.get('backend', '').lower()
    if backend not in ('vulkan', 'd3d12', 'direct3d12') or not provenance.get('shader'):
        raise ProtocolError('native needs a real GPU backend and shader provenance')
    if preset.startswith('windows') and provenance.get('toolchain') != 'MSVC':
        raise ProtocolError('Windows native evidence requires MSVC')
    from .native_resources import validate as validate_resources
    validate_resources(result, manifest, binary_hash)
    return result


def web(path, case, nonce):
    # Web v1 emits checkpoint before its checkpoint action receipt, and binds
    # fresh readbacks to (run nonce, step, paint sequence). Native v1 instead
    # emits receipt first and a separate capture nonce. Preserve both contracts.
    path = Path(path)
    if not path.is_file() or not path.stat().st_size:
        raise IncompleteRun('Web returned no observation stream')
    text = path.read_text(encoding='utf-8')
    if not text.endswith('\n'):
        raise IncompleteRun('Web stream truncated')
    records = [validate('observation', loads(line)) for line in text.splitlines()]
    if not records:
        raise IncompleteRun('Web returned an empty observation stream')
    from demo_reference_web import bootstrap, reference
    from .adapters import web_definition
    from .comparison import digest
    provenance = records[0]['provenance'] if records else {}
    lock = bootstrap.load_lock()
    source_differs = (provenance.get('source_commit') != case['official']['commit'] or
            provenance.get('source_tree_sha256') != lock['source']['components_tree_sha256'] or
            provenance.get('adapter_sha256') != case['official']['adapter_sha256'] or
            provenance.get('adapter_closure_sha256') != reference.code_hash() or
            provenance.get('fixture_sha256') != digest(case['fixture']) or
            provenance.get('case_sha256') != digest(web_definition(case)))
    closure = provenance.get('source_closure', {})
    source_differs = source_differs or any(closure.get(item['path']) != item['sha256'] for item in case['official']['source_paths'])
    expected = [('hello', '')]
    for action in case['actions']:
        if action['kind'] == 'checkpoint':
            expected.append(('checkpoint', action['step']))
        expected.append(('receipt', action['step']))
    expected.append(('complete', ''))
    if [(r['type'], r['step']) for r in records] != expected:
        raise IncompleteRun('Web hello/actions/checkpoints/complete identities or ordering differ')
    previous = []
    capture_paths = set()
    sequence = 0
    for row in records:
        if row['nonce'] != nonce or row['case'] != case['identity'] or row['adapter'] != case['official']['adapter_id']:
            raise IncompleteRun('Web stale nonce/case or adapter')
        if row['provenance'] != provenance:
            raise IncompleteRun('Web provenance changed within one stream')
        callbacks = row['callbacks']
        if (callbacks[:len(previous)] != previous or [c['seq'] for c in callbacks] != list(range(len(callbacks))) or
                any(c['step'] not in {a['step'] for a in case['actions']} for c in callbacks)):
            raise IncompleteRun('Web callback history changed, reordered or lost')
        previous = callbacks
        if row['type'] == 'receipt' and not row['receipt']['dispatched']:
            raise IncompleteRun('Web action was not dispatched')
        if row['type'] != 'checkpoint':
            if row['captures']:
                raise IncompleteRun('Web capture outside checkpoint')
            continue
        declaration = next(c for c in case['checkpoints'] if c['step'] == row['step'])
        if [t['identity'] for t in row['targets']] != declaration['targets'] or len(row['captures']) != 1:
            raise IncompleteRun('Web omitted checkpoint target/current image')
        capture = row['captures'][0]
        sequence += 1
        paint = row['resources'].get('paint_receipt', {})
        if (capture['nonce'] != nonce or capture['checkpoint'] != row['step'] or not capture['fence_completed'] or
                paint.get('nonce') != nonce or paint.get('step') != row['step'] or
                paint.get('sequence') != sequence or paint.get('fresh_readback') is not True or
                paint.get('sha256') != capture['sha256'] or capture['path'] in capture_paths):
            raise IncompleteRun('Web stale/missing current paint receipt')
        capture_paths.add(capture['path'])
        if any(t['scene_revision'] != capture['scene_revision'] for t in row['targets']):
            raise IncompleteRun('Web capture scene revision is stale')
        image = artifact(path.parent, capture['path'])
        if sha(image) != capture['sha256'] or image_extent(image) != (capture['width'], capture['height']):
            raise IncompleteRun('Web image content/extent differs')
    last = records[-1]
    if (last['completed_steps'] != [a['step'] for a in case['actions']] or
            last['completed_checkpoints'] != [c['step'] for c in case['checkpoints']]):
        raise IncompleteRun('Web complete identity sets differ')
    if source_differs:
        raise RunError(2, 'reference-drift', 'Web actual source/imported component/adapter/fixture identity differs')
    return {'records': records, 'hello': records[0], 'receipts': {r['step']: r for r in records if r['type'] == 'receipt'},
            'checkpoints': {r['step']: r for r in records if r['type'] == 'checkpoint'},
            'complete': last, 'callbacks': previous}


def metrics(observed, side):
    if side == 'native':
        value = next(iter(observed['checkpoints'].values()))['resources'].get('metrics', {})
    else:
        value = observed['hello']['effective_inputs'].get('actual_environment', {})
        value = {'dpr': value.get('ratio'), 'logical_width': value.get('width'), 'logical_height': value.get('height')}
    if any(type(value.get(k)) not in (int, float) or value[k] <= 0 for k in ('dpr', 'logical_width', 'logical_height')):
        raise IncompleteRun(side + ' has no actual positive window metrics')
    return value


def effective_inputs(case, native_observed, web_observed):
    fixture = case['fixture']
    for side, observed in (('native', native_observed), ('web', web_observed)):
        for row in observed['records']:
            if not row['effective_inputs']:
                continue
            for key, value in fixture.items():
                if not exact(row['effective_inputs'].get(key), value):
                    if key in ('fonts', 'font_policy', 'viewport'):
                        raise RunError(3, 'environment', side + ' effective environment differs: ' + key)
                    raise ProtocolError(side + ' effective input differs: ' + key)
    # Native effective_inputs is an echo; require the separate actual production
    # input query and compare every declared prop/data field. Unknown defaults
    # remain in raw evidence, rather than deleting them from the observations.
    actual = next((r['resources'].get('actual_initial_inputs') for r in native_observed['records']
                   if r['resources'].get('actual_initial_inputs')), None)
    if not actual:
        raise ProtocolError('native lacks independently observed actual_initial_inputs')
    for target, declaration in fixture['props']['controls'].items():
        current = actual.get('controls', {}).get(target, {})
        for key, expected in declaration.items():
            if key not in current or not exact(current[key], expected):
                raise ProtocolError('native actual public input differs/missing: ' + target + '.' + key)
        for key, expected in fixture['data'].get(target, {}).items():
            value = actual.get('data', {}).get(target, {}).get(key)
            if key == 'options' and isinstance(value, list):
                # Native explicit defaults are equivalent only for these pinned
                # public option fields. Preserve order, keys, labels, disabled.
                value = [{k: v for k, v in item.items() if k != 'group' or v} for item in value]
                expected = [{'disabled': False, **item} for item in expected]
            if not exact(value, expected):
                raise ProtocolError('native actual initial data differs: ' + target + '.' + key)
    # Slots, local Theme and composition must be observed independently too.
    # Baseline adapter does not yet export these; fail closed, never bless echoes.
    for key in ('slots', 'theme', 'composition'):
        expected = fixture['props'].get(key) if key == 'composition' else fixture[key]
        if expected and not exact(actual.get(key), expected):
            raise ProtocolError('native actual input query lacks/mismatches ' + key)
    for side, observed in (('native', native_observed), ('web', web_observed)):
        current = metrics(observed, side)
        for key, expected in (('dpr', fixture['viewport']['dpr']), ('logical_width', fixture['viewport']['width']),
                              ('logical_height', fixture['viewport']['height'])):
            if not exact(current[key], expected):
                raise ProtocolError(side + ' actual DPR/viewport differs: ' + key)
        for row in observed['checkpoints'].values():
            for capture in row['captures']:
                if (capture['dpr'] != current['dpr'] or abs(capture['width'] - current['logical_width'] * current['dpr']) > .01 or
                        abs(capture['height'] - current['logical_height'] * current['dpr']) > .01):
                    raise IncompleteRun(side + ' checkpoint actual DPR/extent changed')
