"""Bounded v1 resources.actual_inputs contract and per-side consumption obligations.

This gate never derives expected inputs from the opposite adapter. Initial seeds
and live state are separate: live state is checked by the case action oracle.
"""
from __future__ import annotations

from copy import deepcopy

from .observations import IncompleteRun
from .oracle import exact
from .protocol import ProtocolError, finite

NATIVE_SOURCE = 'mounted hosts, applied editor state, retained slots/ThemeScope/layout'
WEB_SOURCE = 'committed official fibers and connected hosts'


def ordered_targets(case):
    controls = case['fixture']['props']['controls']
    slots = case['fixture']['slots']
    result = []
    def add(target):
        if target in result:
            return
        if target not in controls:
            raise ProtocolError('input identity not declared: ' + target)
        result.append(target)
        for key in ('prefix', 'suffix', 'popup'):
            value = slots.get(target, {}).get(key)
            if isinstance(value, dict) and 'control' in value:
                add(value['control'])
    # Legacy vector remains an intact prefix. Slot recursion follows afterwards.
    for target in case['native']['targets']:
        if target in controls and target not in result:
            result.append(target)
    def visit(node):
        if node['component'] == 'control':
            add(node['target'])
        for child in node.get('children', []):
            visit(child)
    visit(case['fixture']['props']['composition'])
    for target in list(result):
        for key in ('prefix', 'suffix', 'popup'):
            value = slots.get(target, {}).get(key)
            if isinstance(value, dict) and 'control' in value:
                add(value['control'])
    if set(result) != set(controls):
        raise ProtocolError('composition/slots do not consume the full input identity set')
    return result


def validate_popup_plan(case):
    actions = case['actions']
    order = {a['step']: i for i, a in enumerate(actions)}
    for child, (parent, slot) in slot_owners(case['fixture']).items():
        if slot != 'popup':
            continue
        deadlines = [c for c in case['checkpoints'] if child in c['targets'] and parent in c['targets']]
        if not deadlines:
            raise IncompleteRun('popup child has no declared consumption checkpoint: ' + child)
        stop = min(order[c['step']] for c in deadlines)
        before = actions[:stop]
        if not all(any(a['target'] == parent and a['kind'] == kind for a in before)
                   for kind in ('pointer-down', 'pointer-up')):
            raise IncompleteRun('popup deadline has no actual pointer opening actions: ' + child)
        if any(a['target'] == child and a['kind'] not in ('checkpoint', 'wheel') for a in before):
            raise ProtocolError('popup child input precedes its first-applied checkpoint: ' + child)


def slot_owners(fixture):
    result = {}
    for parent, slots in fixture['slots'].items():
        for slot, value in slots.items():
            if isinstance(value, dict) and 'control' in value:
                child = value['control']
                controls = fixture['props']['controls']
                if (set(value) != {'control'} or parent not in controls or child not in controls or slot not in ('prefix', 'suffix', 'popup') or
                        (slot == 'popup' and controls[parent].get('component') != 'Select')):
                    raise ProtocolError('unknown declared popup/slot identity: ' + str(child))
                if child in result:
                    raise ProtocolError('duplicate actual slot obligation: ' + child)
                result[child] = (parent, slot)
    return result


def checkpoint_input_targets(case, legacy):
    declared = set(ordered_targets(case))
    aliases = {parent + '.popup' for parent, slot in slot_owners(case['fixture']).values() if slot == 'popup'}
    for checkpoint in case['checkpoints']:
        for roi in checkpoint['rois']:
            if roi['kind'] == 'popup' and roi['target'] not in aliases:
                raise ProtocolError('unknown actual popup alias/ROI')
            if roi['target'] in aliases and roi['kind'] != 'popup':
                raise ProtocolError('actual popup alias requires its own popup ROI')
    return set(legacy) | declared | aliases | {case['native']['sample_id']}


def _object(value, keys, required=None):
    if type(value) is not dict or set(value) - set(keys) or not set(required or ()) <= set(value):
        raise ProtocolError('actual_inputs unknown/missing map fields')


def _text(value, empty=False):
    if type(value) is not str or len(value) > 4096 or (not value and not empty):
        raise ProtocolError('actual_inputs invalid bounded string')


def _integer(value, minimum=0):
    if type(value) is not int or not minimum <= value <= 2**53 - 1:
        raise ProtocolError('actual_inputs invalid bounded integer')


def _tree(node, depth=0):
    if depth > 24:
        raise ProtocolError('actual_inputs composition depth')
    _object(node, ('component', 'target', 'identity', 'content', 'props', 'width', 'children'), ('component',))
    kind = node['component']
    if kind not in ('control', 'origin', 'Flex', 'Space', 'Typography.Text', 'source-feedback', 'retained'):
        raise ProtocolError('actual_inputs unknown composition node')
    if kind == 'control':
        _object(node, ('component', 'target'), ('component', 'target'))
        _text(node['target'])
    elif kind == 'origin':
        _object(node, ('component',), ('component',))
    elif kind == 'Typography.Text':
        _object(node, ('component', 'content'), ('component', 'content'))
        _text(node['content'], True)
    elif kind == 'source-feedback':
        _object(node, ('component', 'identity', 'content'), ('component', 'identity', 'content'))
        _text(node['identity'])
        _text(node['content'], True)
    else:
        _object(node, ('component', 'props', 'width', 'children'), ('component', 'children'))
        props = node.get('props', {})
        _object(props, ('vertical', 'gap', 'cross_gap', 'wrap', 'align'))
        for key, value in props.items():
            if key in ('vertical', 'wrap'):
                if type(value) is not bool:
                    raise ProtocolError('actual_inputs composition boolean')
            elif key == 'align':
                if value not in ('start', 'end', 'center', 'baseline', 'stretch'):
                    raise ProtocolError('actual_inputs composition alignment')
            elif type(value) not in (int, float) or not 0 <= value <= 16384:
                raise ProtocolError('actual_inputs composition gap')
        if 'width' in node and (type(node['width']) not in (int, float) or not 0 < node['width'] <= 16384):
            raise ProtocolError('actual_inputs composition width')
        children = node.get('children')
        if type(children) is not list or len(children) > 512:
            raise ProtocolError('actual_inputs composition children')
        for child in children:
            _tree(child, depth + 1)


def authored_feedbacks(node):
    result = {}
    def visit(current):
        if current.get('component') == 'source-feedback':
            identity = current['identity']
            if identity in result:
                raise ProtocolError('duplicate authored feedback identity')
            result[identity] = current['content']
        for child in current.get('children', current.get('roots', [])):
            visit(child)
    visit(node)
    return result


def expected_feedbacks(case, side, step):
    result = authored_feedbacks(case['fixture']['props']['composition'])
    if step == 'setup':
        return result
    paths = {'/resources/actual_inputs/feedbacks/' + identity.replace('~', '~0').replace('/', '~1'): identity
             for identity in result}
    for action in case['actions']:
        updated = {}
        for predicate in case.get('oracle', []):
            if (predicate.get('op') != 'equals' or predicate.get('step') != action['step'] or
                    predicate.get('side') not in ('both', side) or predicate.get('path') not in paths):
                continue
            if predicate.get('origin') != 'independent-contract':
                raise ProtocolError('live authored feedback requires an independent literal contract')
            identity = paths[predicate['path']]
            _text(predicate['value'], True)
            if identity in updated and updated[identity] != predicate['value']:
                raise ProtocolError('conflicting authored feedback contract')
            updated[identity] = predicate['value']
        result.update(updated)
        if action['step'] == step:
            break
    return result


def validate_snapshot(value, expected_targets):
    required = ('version', 'source', 'phase', 'query_seq', 'step', 'page_generation', 'ordered_targets',
                'instances', 'controls', 'data', 'slots', 'themes', 'contexts', 'slot_targets',
                'slot_composition', 'composition', 'current', 'aliases')
    _object(value, (*required, 'feedbacks'), required)
    finite(value)
    if type(value['version']) is not int or value['version'] != 1 or value['phase'] not in ('initial', 'settled'):
        raise ProtocolError('actual_inputs internal version/phase')
    if value['source'] not in (NATIVE_SOURCE, WEB_SOURCE):
        raise ProtocolError('actual_inputs lacks an independent applied source')
    _integer(value['query_seq'], 1)
    _integer(value['page_generation'], 1)
    _text(value['step'])
    if type(value['ordered_targets']) is not list or value['ordered_targets'] != expected_targets or len(expected_targets) > 512:
        raise ProtocolError('actual_inputs full ordered identity vector differs')
    _object(value['instances'], expected_targets, expected_targets)
    mounted = set()
    tokens = set()
    for target, instance in value['instances'].items():
        fields = ('status', 'instance', 'owner_generation', 'parent_target', 'slot', 'first_observed_query', 'reason')
        _object(instance, fields, fields[:-1])
        if instance['status'] not in ('mounted', 'deferred', 'missing', 'retired'):
            raise ProtocolError('actual_inputs unknown instance status')
        for key in ('instance', 'parent_target', 'slot'):
            _text(instance[key], True)
        _integer(instance['owner_generation'], 1)
        _integer(instance['first_observed_query'])
        if instance['status'] == 'mounted':
            if not instance['instance'] or instance['instance'] in tokens or 'reason' in instance:
                raise ProtocolError('actual_inputs duplicated/empty mounted identity')
            if not 1 <= instance['first_observed_query'] <= value['query_seq']:
                raise IncompleteRun('actual_inputs first-observed query is stale/unexecuted')
            tokens.add(instance['instance'])
            mounted.add(target)
        elif instance.get('reason') not in ('closed-bound-popup', 'no-live-host', 'owner-retired'):
            raise ProtocolError('actual_inputs missing finite absence reason')
        elif instance['instance'] or instance['first_observed_query']:
            raise ProtocolError('actual_inputs absence is not an instance witness')
    for group in ('controls', 'data', 'slots', 'themes', 'contexts', 'current'):
        _object(value[group], mounted, mounted)
    for group in ('slot_targets', 'slot_composition'):
        _object(value[group], mounted)
        for slots in value[group].values():
            _object(slots, ('prefix', 'suffix', 'popup'))
    aliases = {target + '.popup' for target in mounted if type(value['controls'][target]) is dict and
               value['controls'][target].get('component') == 'Select'}
    _object(value['aliases'], aliases)
    for name, alias in value['aliases'].items():
        _object(alias, ('instance', 'parent_target', 'owner_generation'), ('instance', 'parent_target', 'owner_generation'))
        _text(alias['instance'])
        _integer(alias['owner_generation'], 1)
        if alias['parent_target'] != name[:-6] or alias['owner_generation'] != value['page_generation']:
            raise ProtocolError('actual_inputs stale/wrong real popup alias owner')
    prop_keys = ('component', 'width', 'marginBlock', 'marginTop', 'marginBottom', 'disabled', 'loading', 'size', 'type', 'danger',
                 'readOnly', 'allowClear', 'placeholder', 'status', 'variant', 'maxLength', 'showSearch',
                 'notFoundContent', 'listHeight', 'direction', 'virtual', 'optionFilterProp', 'mode',
                 'maxTagCount', 'maxTagTextLength', 'maxCount', 'tokenSeparators', 'open', 'defaultOpen',
                 'localTheme', 'recipe', 'applied_renderers')
    bool_keys = ('disabled', 'loading', 'danger', 'readOnly', 'allowClear', 'showSearch', 'virtual', 'open', 'defaultOpen')
    for target in mounted:
        props = value['controls'][target]
        _object(props, prop_keys, ('component',))
        if props['component'] not in ('Button', 'Input', 'Select'):
            raise ProtocolError('actual_inputs unknown mounted public component')
        for key, item in props.items():
            if key in ('placeholder', 'notFoundContent', 'recipe'):
                _text(item, True)
            enums = {'size': ('small', 'middle', 'large'), 'status': ('', 'warning', 'error'),
                     'variant': ('outlined', 'filled', 'borderless', 'underlined'),
                     'type': ('default', 'primary', 'text', 'dashed', 'link'),
                     'direction': ('ltr', 'rtl'), 'mode': ('multiple', 'tags'), 'optionFilterProp': ('label', 'value')}
            if key in enums and item not in enums[key]:
                raise ProtocolError('actual_inputs invalid enumerated prop')
            if key in bool_keys and type(item) is not bool:
                raise ProtocolError('actual_inputs invalid boolean prop')
            if key in ('width', 'marginBlock', 'marginTop', 'marginBottom', 'listHeight', 'maxLength', 'maxTagTextLength', 'maxCount') and (
                    type(item) not in (int, float) or not 0 <= item <= 1000000):
                raise ProtocolError('actual_inputs invalid numeric prop')
            if key == 'applied_renderers':
                _object(item, ('option', 'tag', 'popup', 'async_source'))
                if any(type(v) is not bool for v in item.values()):
                    raise ProtocolError('actual_inputs renderer binding type')
            if key == 'tokenSeparators':
                if type(item) is not list or len(item) > 32:
                    raise ProtocolError('actual_inputs token separator bounds')
                for separator in item: _text(separator)
            if key == 'maxTagCount' and item != 'responsive': _integer(item)
            if key == 'localTheme':
                _object(item, ('algorithm', 'components'))
                if 'algorithm' in item and item['algorithm'] != 'default':
                    raise ProtocolError('actual_inputs local Theme algorithm')
                if 'components' in item:
                    _object(item['components'], ('Select',), ('Select',))
                    select = item['components']['Select']
                    _object(select, ('algorithm', 'colorPrimary', 'optionHeight'), ('algorithm', 'colorPrimary', 'optionHeight'))
                    if select['algorithm'] is not True or type(select['optionHeight']) not in (int, float) or not 0 < select['optionHeight'] <= 4096:
                        raise ProtocolError('actual_inputs local Select Theme types/bounds')
                    _text(select['colorPrimary'])
        data = value['data'][target]
        _object(data, ('value', 'defaultValue', 'options'))
        def selection(item):
            if item is None: return
            if type(item) is str: _text(item, True); return
            if type(item) is int: _integer(item, -(2**53 - 1)); return
            if type(item) is list and len(item) <= 10000:
                for child in item:
                    if type(child) is list: raise ProtocolError('actual_inputs nested selection')
                    selection(child)
                return
            raise ProtocolError('actual_inputs value type/bounds')
        for key in ('value', 'defaultValue'):
            if key in data: selection(data[key])
        def options(rows, depth=0):
            if depth > 4 or type(rows) is not list or len(rows) > 10000:
                raise ProtocolError('actual_inputs options bounds')
            for row in rows:
                _object(row, ('value', 'label', 'disabled', 'key', 'group', 'title', 'options'), ('label',))
                _text(row['label'], True)
                if 'value' in row: selection(row['value'])
                for key in ('key', 'title'):
                    if key in row: _text(row[key], True)
                for key in ('disabled', 'group'):
                    if key in row and type(row[key]) is not bool:
                        raise ProtocolError('actual_inputs option boolean')
                if 'options' in row:
                    options(row['options'], depth + 1)
        if 'options' in data:
            options(data['options'])
        _object(value['slots'][target], ('content', 'caption', 'prefix', 'suffix', 'popup'))
        for slot in value['slots'][target].values():
            items = slot if type(slot) is list else [slot]
            if len(items) > 512: raise ProtocolError('actual_inputs slot content bounds')
            for item in items:
                if type(item) is str: _text(item, True)
                elif type(item) is dict and len(item) == 1 and set(item) <= {'icon', 'control'}:
                    _text(next(iter(item.values())))
                else: raise ProtocolError('actual_inputs unknown slot content/type')
        theme = value['themes'][target]
        _object(theme, ('source', 'algorithms', 'colorBgContainer', 'Select', 'select_option_height'),
                ('source', 'algorithms', 'colorBgContainer'))
        if theme['algorithms'] not in ([], ['default'], ['dark'], ['compact']):
            raise ProtocolError('actual_inputs Theme algorithm')
        if 'Select' in theme:
            _object(theme['Select'], ('algorithm', 'colorPrimary', 'optionHeight'))
            if 'algorithm' in theme['Select'] and type(theme['Select']['algorithm']) is not bool:
                raise ProtocolError('actual_inputs Select Theme algorithm type')
            if 'colorPrimary' in theme['Select']: _text(theme['Select']['colorPrimary'])
            if 'optionHeight' in theme['Select'] and (type(theme['Select']['optionHeight']) not in (int, float) or not 0 < theme['Select']['optionHeight'] <= 4096):
                raise ProtocolError('actual_inputs optionHeight bounds')
        _text(theme['source'])
        _text(theme['colorBgContainer'])
        if 'select_option_height' in theme and (type(theme['select_option_height']) not in (int, float) or not 0 < theme['select_option_height'] <= 4096):
            raise ProtocolError('actual_inputs resolved optionHeight bounds')
        context = value['contexts'][target]
        fields = ('config_scope', 'theme_scope', 'config_generation', 'theme_generation', 'locale', 'format_locale', 'direction')
        _object(context, fields, fields)
        for key in ('config_scope', 'theme_scope', 'locale', 'format_locale'):
            _text(context[key])
        for key in ('config_generation', 'theme_generation'):
            _integer(context[key], 1)
        if context['direction'] not in ('ltr', 'rtl'):
            raise ProtocolError('actual_inputs context direction')
        current = value['current'][target]
        _object(current, ('value', 'query', 'open', 'focused', 'selection'),
                ('focused',) if props['component'] == 'Button' else ('focused', 'value'))
        if type(current['focused']) is not bool:
            raise ProtocolError('actual_inputs focus type')
        if 'open' in current and type(current['open']) is not bool:
            raise ProtocolError('actual_inputs open type')
        if 'query' in current:
            _text(current['query'], True)
        if 'value' in current: selection(current['value'])
        if current.get('selection') is not None:
            selection = current['selection']
            _object(selection, ('start', 'end', 'unit'), ('start', 'end', 'unit'))
            _integer(selection['start'])
            _integer(selection['end'])
            if selection['unit'] not in ('scalar', 'UTF-16', 'UTF-8-bytes'):
                raise ProtocolError('actual_inputs selection unit')
    for slots in value['slot_targets'].values():
        for children in slots.values():
            if type(children) is not list or len(children) > 512:
                raise ProtocolError('actual_inputs slot target bounds')
            for child in children:
                _object(child, ('control',), ('control',))
                if child['control'] not in expected_targets:
                    raise ProtocolError('actual_inputs unknown slot child')
    for slots in value['slot_composition'].values():
        for tree in slots.values():
            _tree(tree)
    composition = value['composition']
    _object(composition, ('source', 'host', 'roots'), ('source', 'host', 'roots'))
    if type(composition['roots']) is not list or len(composition['roots']) > 512:
        raise ProtocolError('actual_inputs root bounds')
    _text(composition['source'])
    if composition['host'] is not None:
        _tree({'component': 'Flex', 'props': composition['host'], 'children': []})
    for node in composition['roots']:
        _tree(node)
    if not exact(value.get('feedbacks', {}), authored_feedbacks(composition)):
        raise ProtocolError('actual authored feedback map differs from retained composition')
    return value


def _slot_tree(node):
    # Only a passive single-child retained wrapper may disappear.
    if node['component'] == 'retained':
        if len(node['children']) != 1:
            raise ProtocolError('unobserved/multiple popup origin structure')
        return _slot_tree(node['children'][0])
    if node['component'] in ('control', 'origin'):
        return node
    props = {k: v for k, v in node.get('props', {}).items() if k != 'cross_gap'}
    return {'component': node['component'], 'props': props,
            'children': [_slot_tree(child) for child in node.get('children', [])]}


def verify_relationship(fixture, query, target, side):
    instance = query['instances'][target]
    owner = slot_owners(fixture).get(target)
    if not owner:
        if instance['parent_target'] or instance['slot']:
            raise ProtocolError(side + ' unexpected actual slot ownership: ' + target)
        return
    parent, slot = owner
    if instance['parent_target'] != parent or instance['slot'] != slot or query['instances'][parent]['status'] != 'mounted':
        raise ProtocolError(side + ' wrong actual slot owner: ' + target)
    if query['slot_targets'].get(parent, {}).get(slot) != [{'control': target}]:
        raise ProtocolError(side + ' wrong actual slot membership: ' + target)
    if slot == 'popup':
        expected = {'component': 'Flex', 'props': {'vertical': True, 'gap': 4, 'wrap': False, 'align': 'stretch'},
                    'children': [{'component': 'origin'}, {'component': 'control', 'target': target}]}
        tree = query['slot_composition'].get(parent, {}).get(slot)
        if tree is None or not exact(_slot_tree(tree), expected):
            raise ProtocolError(side + ' wrong actual popup wrapper/origin/order: ' + target)


def verify_absence(fixture, query, target, side):
    instance = query['instances'][target]
    owner = slot_owners(fixture).get(target)
    if side != 'web' or instance['status'] != 'deferred' or not owner or owner[1] != 'popup':
        raise IncompleteRun(side + ' missing/retired actual host: ' + target)
    parent = owner[0]
    if (query['instances'][parent]['status'] != 'mounted' or query['current'][parent].get('open') is not False or
            query['slots'][parent].get('popup') != {'control': target} or instance.get('reason') != 'closed-bound-popup'):
        raise IncompleteRun('Web absent child has no live closed popup binding: ' + target)


def verify_control(fixture, actual, target, side, *, first=False, initial=False):
    from .runner_evidence import native_options, web_options, verify_recipe, verify_slots, verify_theme
    declaration = fixture['props']['controls'][target]
    current = actual['controls'][target]
    for key, expected in declaration.items():
        if key in ('localTheme', 'recipe') or (not initial and key in ('open', 'defaultOpen')):
            continue
        if not exact(current.get(key), expected):
            raise ProtocolError(side + ' actual public input differs: ' + target + '.' + key)
    for key, expected in fixture['data'].get(target, {}).items():
        # Controlled values and async delivery have their own action oracle.
        # Virtualization does not permit changing the authored options array.
        if not first and (key != 'options' or declaration.get('recipe') == 'select-async'):
            continue
        observed = actual['data'][target].get(key)
        if key == 'options':
            observed = native_options(observed) if side == 'native' and isinstance(observed, list) else web_options(observed or [])
            expected = web_options(expected)
        if not exact(observed, expected):
            phase = 'first-applied seed/data' if first else 'actual static data'
            raise ProtocolError(side + ' ' + phase + ' differs: ' + target + '.' + key)
        if first and key in ('value', 'defaultValue') and not exact(actual['current'][target].get('value'), expected):
            raise ProtocolError(side + ' first-applied actual editor/selection differs from seed: ' + target)
    verify_theme(fixture, target, declaration.get('localTheme'), actual['themes'][target])
    if side == 'native':
        verify_recipe(target, declaration.get('recipe'), current)
        subset = {**fixture, 'slots': {target: fixture['slots'].get(target, {})}}
        # Popup membership has an explicit deferred lifecycle below.
        subset['slots'] = {target: {k: v for k, v in subset['slots'][target].items()
                                   if not (isinstance(v, dict) and 'control' in v)}}
        verify_slots(subset, actual)
    else:
        for key, expected in fixture['slots'].get(target, {}).items():
            if isinstance(expected, dict) and 'control' in expected:
                continue
            if not exact(actual['slots'][target].get(key), expected):
                raise ProtocolError('web actual slot differs: ' + target + '.' + key)
    context = actual['contexts'][target]
    for key in ('locale', 'format_locale', 'direction'):
        if context[key] != fixture[key]:
            raise ProtocolError(side + ' actual applied context differs: ' + target + '.' + key)


def validate_timeline(case, observed, side):
    from .runner_evidence import verify_composition
    fixture = case['fixture']
    targets = ordered_targets(case)
    owners = slot_owners(fixture)
    records = observed['records']
    actions = {a['step']: a for a in case['actions']}
    checkpoints = {c['step']: c for c in case['checkpoints']}
    initial = None
    previous = None
    seen = {}
    retired = set()
    deadline_seen = {}
    action_queries = set()
    scope_history = {}
    validate_popup_plan(case)
    for row in records:
        raw_initial = row.get('resources', {}).get('actual_initial_inputs')
        if raw_initial is None:
            raw_initial = row.get('effective_inputs', {}).get('actual_initial_inputs')
        if raw_initial is not None:
            validate_snapshot(raw_initial, targets)
            if raw_initial['phase'] != 'initial' or raw_initial['step'] != 'setup' or raw_initial['query_seq'] != 1:
                raise ProtocolError(side + ' initial is not the immutable postfactory snapshot')
            if initial is not None and not exact(raw_initial, initial):
                raise ProtocolError(side + ' actual_initial_inputs changed after setup')
            initial = deepcopy(raw_initial)
        query = row.get('resources', {}).get('actual_inputs')
        if row['type'] == 'hello' and side == 'native':
            continue  # Native hello precedes the real factory.
        if initial is None or query is None:
            raise IncompleteRun(side + ' lacks independent initial/current input query')
        validate_snapshot(query, targets)
        verify_composition(fixture, query['composition'], expected_feedbacks(case, side, query['step']))
        permitted_aliases = {parent + '.popup' for parent, slot in owners.values() if slot == 'popup'}
        if set(query['aliases']) - permitted_aliases or set(initial['aliases']) - permitted_aliases:
            raise ProtocolError(side + ' unknown popup alias outside the actual slot obligations')
        if query['source'] != (NATIVE_SOURCE if side == 'native' else WEB_SOURCE):
            raise ProtocolError(side + ' actual input source differs')
        if previous is None:
            # Validate every actually mounted initial child immediately.
            for target in targets:
                if initial['instances'][target]['status'] == 'mounted':
                    verify_control(fixture, initial, target, side, first=True, initial=True)
                    verify_relationship(fixture, initial, target, side)
                else:
                    verify_absence(fixture, initial, target, side)
            previous = deepcopy(initial)
        if row['type'] == 'hello':
            if not exact(query, initial):
                raise ProtocolError('Web hello initial query differs')
        elif row['type'] != 'complete':
            if query['step'] != row['step'] or query['phase'] != 'settled' or row['step'] not in actions:
                raise IncompleteRun(side + ' query has no current action/settle')
            if row['type'] == 'receipt':
                if row.get('receipt', {}).get('dispatched') is not True:
                    raise IncompleteRun(side + ' action not dispatched')
                action_queries.add(row['step'])
        if previous:
            if query['page_generation'] != initial['page_generation'] and not any(
                    a['kind'] == 'navigate' for a in case['actions'][:next((i + 1 for i, a in enumerate(case['actions']) if a['step'] == query['step']), 0)]):
                raise IncompleteRun(side + ' page generation changed without navigation')
            if row['type'] == 'complete' and not exact(query, previous):
                raise IncompleteRun(side + ' complete manufactured another query')
            if query['page_generation'] < previous['page_generation'] or query['query_seq'] < previous['query_seq']:
                raise IncompleteRun(side + ' query/page generation regressed')
            if query['query_seq'] == previous['query_seq']:
                if not exact(query, previous):
                    raise IncompleteRun(side + ' same query_seq has different snapshot')
            elif query['query_seq'] != previous['query_seq'] + 1:
                raise IncompleteRun(side + ' query sequence has gaps')
            if row['step'] and query['step'] != previous['step'] and query['query_seq'] <= previous['query_seq']:
                raise IncompleteRun(side + ' next action reused an old query')
        for target, instance in query['instances'].items():
            status = instance['status']
            if instance['owner_generation'] != query['page_generation']:
                raise IncompleteRun(side + ' stale owner generation: ' + target)
            if status != 'mounted':
                if target in seen:
                    retired.add(seen[target][0])
                verify_absence(fixture, query, target, side)
                continue
            token = instance['instance']
            first = target not in seen or seen[target][0] != token
            if first and target in seen: retired.add(seen[target][0])
            if token in retired or (not first and instance['first_observed_query'] != seen[target][1]):
                raise IncompleteRun(side + ' reused retired/stale generation witness: ' + target)
            if first and instance['first_observed_query'] not in (query['query_seq'], 1 if initial['instances'][target]['instance'] == token else -1):
                raise IncompleteRun(side + ' invalid first observed query: ' + target)
            initial_instance = initial['instances'][target]
            validated_at_setup = initial_instance['status'] == 'mounted' and initial_instance['instance'] == token
            verify_control(fixture, query, target, side, first=first and not validated_at_setup)
            seen[target] = (token, instance['first_observed_query'])
            verify_relationship(fixture, query, target, side)
            context = query['contexts'][target]
            for kind in ('config', 'theme'):
                scope = (kind, context[kind + '_scope'])
                generation = context[kind + '_generation']
                applied = {k: context[k] for k in ('locale', 'format_locale', 'direction')} if kind == 'config' else query['themes'][target]
                last = scope_history.get(scope)
                if last and (generation < last[0] or (generation == last[0] and not exact(applied, last[1]))):
                    raise IncompleteRun(side + ' stale/inconsistent applied scope generation')
                scope_history[scope] = (generation, deepcopy(applied))
        if row['type'] == 'checkpoint':
            for target in checkpoints[row['step']]['targets']:
                if target.endswith('.popup'):
                    parent = target[:-6]
                    alias = query['aliases'].get(target)
                    snapshots = [t for t in row.get('targets', []) if t['identity'] == target]
                    if (not alias or parent not in query['current'] or query['current'][parent].get('open') is not True or
                            len(snapshots) != 1 or snapshots[0]['state'].get('instance') != alias['instance'] or
                            snapshots[0]['owner_generation'] != query['page_generation']):
                        raise IncompleteRun(side + ' popup ROI has no real live instance/owner proof: ' + target)
                if target in owners and owners[target][1] == 'popup':
                    parent = owners[target][0]
                    if query['instances'][target]['status'] != 'mounted' or query['current'][parent].get('open') is not True:
                        raise IncompleteRun(side + ' popup consumption deadline incomplete: ' + target)
                    deadline_seen[target] = (query['instances'][target]['instance'], query['instances'][parent]['instance'])
        if row['type'] == 'complete' and any(i['status'] != 'mounted' for i in query['instances'].values()):
            raise IncompleteRun(side + ' complete still has pending/missing/retired inputs')
        previous = deepcopy(query)
    required_popups = {t for t, (_, slot) in owners.items() if slot == 'popup'}
    if set(seen) != set(targets) or set(deadline_seen) != required_popups or action_queries != set(actions):
        raise IncompleteRun(side + ' unresolved inputs/actions/popup checkpoint obligations')
    for target in required_popups:
        if deadline_seen[target] != (previous['instances'][target]['instance'], previous['instances'][owners[target][0]]['instance']):
            raise IncompleteRun(side + ' popup deadline belongs to an old child/parent instance')
    # External composition remains independent from callback/data changes.
    verify_composition(fixture, initial['composition'])
    return seen
