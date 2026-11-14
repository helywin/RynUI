"""Validate both existing adapter contracts without rewriting their raw records."""
from __future__ import annotations

import hashlib
from pathlib import Path

from .observations import IncompleteRun, image_extent, read_observations
from .oracle import exact
from .protocol import ProtocolError, loads, relative_path, validate
from .adapters import RunError
from .device_metrics import stable_device_metrics


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
        try:
            return stable_device_metrics(value)
        except ProtocolError as error:
            raise IncompleteRun('native has invalid actual device metrics: ' + str(error)) from error
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
    from .actual_inputs import validate_timeline
    validate_timeline(case, native_observed, 'native')
    validate_timeline(case, web_observed, 'web')
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


def native_actual_inputs(fixture, actual):
    if actual.get('source') != 'mounted hosts, applied editor state, retained slots/ThemeScope/layout':
        raise ProtocolError('native actual input query is missing its independent state source')
    for target, declaration in fixture['props']['controls'].items():
        current = actual.get('controls', {}).get(target, {})
        if not current:
            gap = next((row.get('reason') for row in actual.get('unobserved', []) if row.get('target') == target), 'no mounted host query')
            raise ProtocolError('native actual input unobserved: ' + target + ': ' + gap)
        for key, expected in declaration.items():
            if key in ('localTheme', 'recipe'):
                continue
            if key not in current or not exact(current[key], expected):
                raise ProtocolError('native actual public input differs/missing: ' + target + '.' + key)
        for key, expected in fixture['data'].get(target, {}).items():
            value = actual.get('data', {}).get(target, {}).get(key)
            if key == 'options' and isinstance(value, list):
                value = native_options(value)
                expected = web_options(expected)
            if not exact(value, expected):
                raise ProtocolError('native actual initial data differs: ' + target + '.' + key)
        verify_theme(fixture, target, declaration.get('localTheme'), actual.get('themes', {}).get(target))
        verify_recipe(target, declaration.get('recipe'), current)
    verify_slots(fixture, actual)
    verify_composition(fixture, actual.get('composition'))


def web_options(rows):
    result = []
    for row in rows:
        if 'options' in row:
            if row.get('disabled', False):
                raise ProtocolError('native SelectOptionGroup has no public disabled-group input')
            result.append({'group': True, 'key': row['title'], 'label': row['label']})
            result.extend(web_options(row['options']))
        else:
            result.append({'value': row['value'], 'label': row['label'], 'disabled': row.get('disabled', False), 'key': ''})
    return result


def native_options(rows):
    result = []
    for row in rows:
        if row.get('group'):
            if row['disabled'] is not True:
                raise ProtocolError('native Select group heading must remain nonselectable')
            result.append({'group': True, 'key': row['key'], 'label': row['label']})
        else:
            result.append({'value': row['value'], 'label': row['label'], 'disabled': row['disabled'], 'key': row.get('key', '')})
    return result


def verify_theme(fixture, target, local, observed):
    if not observed or observed.get('source') not in ('retained ThemeScope applied config and resolved snapshot',
            'committed ConfigProvider ancestry and applied public Theme'):
        raise ProtocolError('native lacks actual ThemeScope: ' + target)
    expected_algorithm = (local or {}).get('algorithm', fixture['theme'].get('algorithm', 'default'))
    algorithms = observed.get('algorithms') or ['default']
    if algorithms != [expected_algorithm] or observed.get('colorBgContainer') != fixture['theme'].get('token', {}).get('colorBgContainer', '#ffffff'):
        raise ProtocolError('native actual local/global Theme differs: ' + target)
    if local and 'components' in local:
        expected = local['components']['Select']
        if not exact(observed.get('Select'), expected) or observed.get('select_option_height') != expected['optionHeight']:
            raise ProtocolError('native applied Select local Theme differs: ' + target)


def verify_recipe(target, recipe, observed):
    if not recipe:
        return
    from demo_reference_web.reference import SELECT_RECIPES
    if SELECT_RECIPES.get(target) != recipe:
        raise ProtocolError('unknown same-source wrapper: ' + target)
    fields = {'select-virtual': {'option': True, 'popup': True}, 'select-tags': {'tag': True},
              'select-async': {'popup': True, 'async_source': True}}.get(recipe, {})
    for key, value in fields.items():
        if observed.get('applied_renderers', {}).get(key) is not value:
            raise ProtocolError('native applied source renderer missing: ' + target + '.' + key)
    # Callback behavior is checked by the independent action oracle. The recipe
    # name itself is source binding, not evidence that a callback was executed.


def verify_slots(fixture, actual):
    for target, slots in fixture['slots'].items():
        for key, value in slots.items():
            if key == 'caption':
                continue # Checked in its real retained composition position.
            observed = actual.get('slots', {}).get(target, {}).get(key)
            if isinstance(value, str):
                content = ''.join(item for item in (observed or []) if isinstance(item, str))
                if content != value:
                    raise ProtocolError('native actual slot text differs: ' + target + '.' + key)
            elif isinstance(value, dict) and 'icon' in value:
                source = actual.get('slots', {}).get(target, {}).get('content') if fixture['props']['controls'][target]['component'] == 'Button' else observed
                if [item for item in (source or []) if isinstance(item, dict) and 'icon' in item] != [value]:
                    raise ProtocolError('native actual slot icon differs: ' + target + '.' + key)
            else:
                if actual.get('slot_targets', {}).get(target, {}).get(key) != [value]:
                    # A dormant popup has no applied children to observe. Keep
                    # this gap nonzero rather than assert fixture equivalence.
                    raise ProtocolError('native nested slot needs applied child identity evidence: ' + target + '.' + key)


def verify_composition(fixture, observed, feedback_values=None):
    if not observed or observed.get('source') != 'retained component tree and applied layout models':
        raise ProtocolError('native lacks independently retained composition')
    def projected(node, native=False):
        component = node['component']
        if component == 'retained':
            children = [projected(n, True) for n in node['children']]
            if len(children) != 1:
                raise ProtocolError('unknown retained authoring structure; cannot prove equivalence')
            return children[0]
        if component == 'control':
            return {'component': component, 'target': node['target']}
        if component == 'source-feedback':
            value = node['content'] if native or feedback_values is None else feedback_values[node['identity']]
            return {'component': component, 'identity': node['identity'], 'content': value}
        if component == 'Typography.Text':
            value = fixture['slots'][node['target']][node['slot']] if 'slot' in node else node['content']
            return {'component': 'Typography.Text', 'content': value}
        props = node.get('props', {})
        align = props.get('align', 'center' if component == 'Space' else 'stretch')
        align = {'flex-start': 'start', 'flex-end': 'end'}.get(align, align)
        gap = props.get('gap', props.get('size', 8 if component == 'Space' else 0))
        return {'component': component, **({'width': node['width']} if 'width' in node else {}), 'props': {'vertical': props.get('vertical', props.get('direction') == 'vertical'),
                'gap': gap, 'wrap': props.get('wrap', False), 'align': align},
                'children': [projected(n, native) for n in node.get('children', [])]}
    expected = projected(fixture['props']['composition'])
    roots = observed.get('roots', [])
    if len(roots) == 1 and expected['component'] != 'Flex':
        actual = projected(roots[0], True)
    else:
        actual = projected({'component': 'Flex', 'props': observed.get('host') or {}, 'children': roots}, True)
    if not exact(actual, expected):
        raise ProtocolError('native actual authored composition/text/order differs')
