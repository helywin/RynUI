"""Explicit protocol substitutes for runner mechanism tests, never run evidence."""
from copy import deepcopy

from demo_differential.actual_inputs import NATIVE_SOURCE, WEB_SOURCE, authored_feedbacks, ordered_targets


def actual_input_control(case, side):
    fixture = case['fixture']
    targets = ordered_targets(case)
    composition = deepcopy(fixture['props']['composition'])
    def literal(node):
        if node.get('slot'):
            return {'component': 'Typography.Text', 'content': fixture['slots'][node['target']][node['slot']]}
        if node['component'] == 'control': return node
        if node['component'] in ('Typography.Text', 'source-feedback'): return node
        props = node.get('props', {})
        return {'component': node['component'], **({'width': node['width']} if 'width' in node else {}),
                'props': {'vertical': props.get('vertical', props.get('direction') == 'vertical'),
                          'gap': props.get('gap', props.get('size', 8 if node['component'] == 'Space' else 0)),
                          'wrap': props.get('wrap', False), 'align': props.get('align', 'center' if node['component'] == 'Space' else 'stretch')},
                'children': [literal(n) for n in node.get('children', [])]}
    composition = literal(composition)
    return {'version': 1, 'phase': 'initial', 'source': NATIVE_SOURCE if side == 'native' else WEB_SOURCE,
        'step': 'setup', 'query_seq': 1, 'page_generation': 1, 'ordered_targets': targets,
        'instances': {t: {'status': 'mounted', 'instance': t + ':unit-1', 'owner_generation': 1,
            'parent_target': '', 'slot': '', 'first_observed_query': 1} for t in targets},
        'controls': deepcopy(fixture['props']['controls']),
        'data': {t: deepcopy(fixture['data'].get(t, {})) for t in targets},
        'slots': {t: {k: [deepcopy(v)] if side == 'native' else deepcopy(v)
                       for k, v in fixture['slots'].get(t, {}).items()} for t in targets},
        'contexts': {t: {'config_scope': 'unit-config', 'theme_scope': t + ':unit-theme',
            'config_generation': 1, 'theme_generation': 1, **{k: fixture[k] for k in ('locale', 'format_locale', 'direction')}} for t in targets},
        'themes': {t: {'source': 'retained ThemeScope applied config and resolved snapshot',
            'algorithms': ['default'], 'colorBgContainer': '#ffffff'} for t in targets},
        'current': {t: {'focused': False, **({'value': fixture['data'].get(t, {}).get('value', fixture['data'].get(t, {}).get('defaultValue', ''))}
                    if fixture['props']['controls'][t]['component'] == 'Input' else {})} for t in targets},
        'slot_targets': {}, 'slot_composition': {}, 'aliases': {}, 'feedbacks': authored_feedbacks(composition),
        'composition': {'source': 'retained component tree and applied layout models',
            'host': deepcopy(composition.get('props', {})) if composition['component'] == 'Flex' else None,
            'roots': deepcopy(composition['children']) if composition['component'] == 'Flex' else [composition]}}
