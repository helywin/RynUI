"""Three source-bound pilot definitions; no expected values come from observations.

The locked adapter cannot yet render Select's complete source composition.
That required context is explicit input, so preflight rejects rather than using
a smaller substitute. These definitions are not runtime acceptance results.
"""
from __future__ import annotations

from copy import deepcopy

from .protocol import validate

TARGETS = {
    'button': ['gallery.state.' + key for key in
               ('default', 'primary', 'danger', 'hover', 'active', 'focus-visible', 'disabled', 'loading')],
    'input': ['gallery.input.controlled', 'gallery.input.uncontrolled'],
    'select': ['gallery.select.' + key for key in ('controlled-search', 'numeric', 'disabled', 'loading', 'empty',
        'controlled-open', 'outlined', 'filled', 'borderless', 'underlined', 'small', 'middle', 'large', 'warning',
        'error', 'component-theme', 'focus', 'toggle-open', 'dynamic-options')],
}
SOURCES = {
    'button': ('Button.tsx', '2ce5986af953bf012764a876cbf17c1e3396b1aacbe7bf79aaea227ca468062e',
               'basic.tsx', 'f970528664503dd9141474bb99ea5a19037b6a8a3f55ac7b1a1d5a09c5836f03',
               'basics', 'add_button_base_samples', 'basic-states'),
    'input': ('Input.tsx', '099aa585e212c735968cfc8eb9dba1229aebc2ec4052e81c270feaf1a1fa9d61',
              'allowClear.tsx', 'ce6051c1c7b096a7d2ef823c513ef6c0378f139d1027e1bdbe4c46a8aa960a66',
              'basics', 'add_input_base_samples', 'controlled-edit'),
    'select': ('index.tsx', '10ae24d48080a301d304024e54f7fb307a53d2a84e1c1893d8a143f6428d5fe2',
               'basic.tsx', '925d3e5f18ead0e61b5f8490b677d4cbe6c9ebea4f23acde12a7ad65cd1e45d8',
               'options', 'add_select_samples', 'popup-lifecycle'),
}
FONTS = [
    {'identity': 'noto-sans', 'sha256': 'b85c38ecea8a7cfb39c24e395a4007474fa5a4fc864f6ee33309eb4948d232d5', 'face_index': 0, 'weight': 400},
    {'identity': 'noto-sans-cjk-sc', 'sha256': '2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b', 'face_index': 0, 'weight': 400},
]


def action(step, kind, target, parameters=None):
    return {'step': step, 'kind': kind, 'target': target, 'parameters': parameters or {}, 'time_us': 0,
            'preconditions': {}, 'layer': 'observer' if kind == 'checkpoint' else
            'external-props' if kind == 'external-props' else 'native-core-dispatch'}


def click(target, name):
    return [action(name + '-down', 'pointer-down', target, {'button': 'primary', 'x_fraction': .5, 'y_fraction': .5}),
            action(name + '-up', 'pointer-up', target, {'button': 'primary', 'x_fraction': .5, 'y_fraction': .5})]


def key(target, name, value, modifiers=None):
    return [action(name + '-down', 'key-down', target, {'key': value, 'modifiers': modifiers or []}),
            action(name + '-up', 'key-up', target, {'key': value, 'modifiers': modifiers or []})]


def predicate(op, step, **values):
    return {'op': op, 'step': step, 'side': 'both', 'origin': 'independent-contract', **values}


def text(content):
    return {'component': 'Typography.Text', 'content': content}


def leaf(target):
    return {'component': 'control', 'target': target}


def _base(component, controls, data, slots, composition, actions, oracle, targets=None):
    source, source_hash, demo, demo_hash, sample, function, scenario = SOURCES[component]
    sample = 'gallery.sample.' + component + '.' + sample
    fixture = {'data': data, 'props': {'controls': controls, 'composition': composition}, 'slots': slots,
        'parent_policy': 'accept', 'clock_epoch_us': 1760000000000000, 'seed': 109, 'timezone': 'UTC',
        'viewport': {'width': 1280, 'height': 900, 'dpr': 'native'},
        # From gallery_layout.hpp and make_gallery_sample_page: 952 card width
        # minus two 24 logical px paddings. This is source input, not measured.
        'container': {'width': 904, 'height': 0, 'padding': 0, 'gap': 12, 'layout': 'composition-v1'},
        'theme': {'algorithm': 'default', 'token': {'colorBgContainer': '#ffffff'}},
        'locale': 'zh-CN', 'format_locale': 'zh-CN', 'direction': 'ltr',
        'fonts': deepcopy(FONTS), 'font_policy': 'pinned', 'motion': 'disabled'}
    checkpoints = []
    observed = targets or list(controls)
    for item in actions:
        if item['kind'] != 'checkpoint':
            continue
        rois = [{'identity': 'context', 'kind': 'sample-context', 'target': sample, 'guard_px': 2},
                {'identity': 'context-guard', 'kind': 'guard-band', 'target': sample, 'guard_px': 2}]
        rois += [{'identity': target, 'kind': 'control', 'target': target, 'guard_px': 2} for target in observed]
        checkpoints.append({'step': item['step'], 'targets': [sample, *observed],
                            'layers': ['geometry', 'token', 'pixels', 'text'], 'rois': rois})
    case = {'schema_version': 1, 'identity': f'antd665.{component}.{scenario}.v1', 'revision': 1,
        'official': {'repository': 'https://github.com/ant-design/ant-design',
            'commit': '4a39f54842eade4e565ab336ef6097cd7e723cdd',
            'demo_path': f'components/{component}/demo/{demo}', 'demo_sha256': demo_hash,
            'source_paths': [{'path': f'components/{component}/{source}', 'sha256': source_hash}],
            'adapter_id': 'antd665.reference-web.v1',
            'adapter_sha256': '192d5a384a08c3a918d7326f5f022370d8b34a2c59a716394fed031fe92cbefc',
            'mode': 'official-component-adapted', 'adaptation': [
                {'field': field, 'value': deepcopy(fixture[field]),
                 'reason': f'{function}: declared public {field}; original sample content and default local Theme remain required; no measured native input'}
                for field in ('props', 'slots', 'data', 'container', 'theme', 'parent_policy')]},
        'native': {'route': 'input/input' if component == 'input' else component, 'family': 'input',
            'sample_id': sample, 'targets': list(TARGETS[component]), 'source_function': function,
            'public_header': f'include/ryn/{component}.hpp'},
        'mapping': 'paired', 'fixture': fixture, 'profile': 'pilot-pinned-font', 'actions': actions,
        'checkpoints': checkpoints, 'oracle': oracle,
        'features': ['default-visible-context', 'controlled-contract' if component != 'button' else 'input-eligibility'],
        'dependencies': [f'include/ryn/{component}.hpp', f'src/component/{component}_component.cpp',
            'examples/token_gallery/token_gallery_definition.cpp', 'examples/token_gallery/gallery_i18n.cpp',
            'examples/token_gallery/gallery_layout.hpp', 'tools/demo_reference_web/app.tsx',
            'tools/demo_differential/pilots.py', 'tools/demo_differential/oracle.py']}
    return validate('case', case)


def button():
    targets = TARGETS['button']
    labels = ('默认', '主色', '危险', '悬停', '按下', '键盘焦点', '禁用', '加载')
    controls = {target: {'component': 'Button', 'type': 'primary' if i in (1, 2, 7) else 'default',
                'size': 'small' if i < 6 else 'middle', 'danger': i == 2, 'disabled': i == 6,
                'loading': i == 7, 'width': 260} for i, target in enumerate(targets)}
    slots = {target: {'content': label} for target, label in zip(targets, labels)}
    composition = {'component': 'Space', 'props': {'wrap': True, 'align': 'center', 'size': 8},
                   'children': [leaf(t) for t in targets]}
    steps = [action('initial', 'checkpoint', targets[0]),
             action('hover', 'pointer-move', targets[3], {'x_fraction': .5, 'y_fraction': .5}),
             action('hovered', 'checkpoint', targets[3]), *click(targets[0], 'activate'),
             *key(targets[0], 'tab', 'Tab'), action('tabbed', 'checkpoint', targets[1]),
             *click(targets[6], 'disabled'), *click(targets[7], 'loading'), action('final', 'checkpoint', targets[0])]
    oracle = [predicate('count', name + '-up', stream='callbacks', target=target, event='activate', equals=count)
              for name, target, count in [('activate', targets[0], 1), ('disabled', targets[6], 0), ('loading', targets[7], 0)]]
    oracle += [predicate('equals', step, path='/receipt/dispatched', value=True) for step in ('activate-up', 'disabled-up', 'loading-up')]
    oracle.append(predicate('focused', 'tabbed', target=targets[1]))
    return _base('button', controls, {}, slots, composition, steps, oracle)


def input_case():
    controlled, uncontrolled = TARGETS['input']
    controls = {controlled: {'component': 'Input', 'width': 260, 'maxLength': 32, 'status': 'warning',
                            'placeholder': '受控：输入中文 / Latin / emoji'},
                uncontrolled: {'component': 'Input', 'width': 260, 'allowClear': True,
                               'placeholder': '清空后显示 placeholder'}}
    data = {controlled: {'value': 'seed'}, uncontrolled: {'defaultValue': '非受控 / Input'}}
    slots = {controlled: {'prefix': '前', 'suffix': '32', 'caption': '受控输入'},
             uncontrolled: {'caption': '非受控输入'}}
    composition = {'component': 'Flex', 'props': {'vertical': True, 'gap': 12}, 'children': [
        text('Input / 输入家族 · 原生桌面功能'), text('支持：四变体、统计/上限、ref/系统提示、清空、Unicode 编辑与 IME'),
        text('Input / Password / Search / TextArea / OTP：双向排版、视觉导航与逻辑编辑'),
        {'component': 'Space', 'props': {'wrap': True, 'align': 'start', 'size': 8}, 'children': [
            {'component': 'Flex', 'props': {'vertical': True, 'gap': 8}, 'children': [
                {'component': 'Typography.Text', 'target': t, 'slot': 'caption'}, leaf(t)]} for t in (controlled, uncontrolled)]},
        text('Enter 提交；支持选择、剪贴板、撤销/重做')]}
    steps = [action('initial', 'checkpoint', controlled), *click(controlled, 'focus'),
             *key(controlled, 'select-all', 'a', ['control']),
             action('accepted-edit', 'text-commit', controlled, {'text': 'accepted'}), action('accepted', 'checkpoint', controlled),
             action('reject-policy', 'external-props', controlled, {'values': {'parent_policy': 'reject'}}),
             action('rejected-edit', 'text-commit', controlled, {'text': 'x'}), action('rejected', 'checkpoint', controlled)]
    oracle = [predicate('count', step, stream='callbacks', target=controlled, event='change', equals=1)
              for step in ('accepted-edit', 'rejected-edit')]
    for index, step, proposed, accepted in ((0, 'accepted', 'accepted', True), (1, 'rejected', 'acceptedx', False)):
        oracle.append(predicate('equals', step, path=f'/callbacks/{index}/payload',
                      value={'proposed': proposed, 'committed': 'accepted', 'accepted': accepted}))
        oracle.append(predicate('equals', step, path='/targets/1/state/value', value='accepted'))
    return _base('input', controls, data, slots, composition, steps, oracle)


def select_case():
    names = ('controlled-search', 'disabled', 'loading', 'controlled-open')
    targets = ['gallery.select.' + key for key in names]
    choices = [{'value': 'desktop', 'label': '桌面'}, {'value': 'service', 'label': '服务'},
               {'value': 'unavailable', 'label': '禁用选项', 'disabled': True}]
    controls = {targets[0]: {'component': 'Select', 'showSearch': True, 'allowClear': True, 'width': 260},
                targets[1]: {'component': 'Select', 'disabled': True, 'width': 260},
                targets[2]: {'component': 'Select', 'loading': True, 'showSearch': True, 'width': 260},
                targets[3]: {'component': 'Select', 'open': False, 'width': 260}}
    data = {targets[0]: {'value': 'desktop', 'options': choices[:2]},
            targets[1]: {'defaultValue': 'desktop', 'options': choices},
            targets[2]: {'options': choices}, targets[3]: {'options': choices}}
    steps = [action('initial', 'checkpoint', targets[0]), *click(targets[0], 'open'),
             action('opened', 'checkpoint', targets[0]), *key(targets[0], 'next', 'ArrowDown'),
             *key(targets[0], 'choose', 'Enter'), action('accepted', 'checkpoint', targets[0]),
             action('reject-policy', 'external-props', targets[0], {'values': {'parent_policy': 'reject'}}),
             *click(targets[0], 'reopen'), *key(targets[0], 'previous', 'ArrowUp'), *key(targets[0], 'reject-choice', 'Enter'),
             *click(targets[1], 'disabled'), *click(targets[3], 'reject-open'), action('rejected', 'checkpoint', targets[0]),
             action('accept-policy', 'external-props', targets[3], {'values': {'parent_policy': 'accept'}}),
             *click(targets[3], 'accept-open'), *key(targets[3], 'escape', 'Escape'),
             action('closed', 'checkpoint', targets[3])]
    oracle = [predicate('count', 'choose-down', stream='callbacks', target=targets[0], event='change', equals=1),
              predicate('count', 'reject-choice-down', stream='callbacks', target=targets[0], event='change', equals=1),
              predicate('count', 'disabled-up', stream='callbacks', target=targets[1], event='change', equals=0)]
    oracle += [predicate('equals', step, path='/targets/1/state/value', value='service') for step in ('accepted', 'rejected')]
    oracle += [predicate('equals', 'rejected', path='/targets/4/state/open', value=False),
               predicate('equals', 'closed', path='/targets/4/state/open', value=False)]
    # No smaller reference is substituted for the existing factory's complete
    # numeric/variants/local-theme/prefix/multiple/tags/virtual/async context.
    # This source binding is intentionally rejected by today's limited Web
    # composition grammar, until its owner supplies those real public adapters.
    composition = {'component': 'source-factory', 'source_function': 'add_select_samples',
                   'required_nested_function': 'add_complete_select_samples',
                   'required_targets': TARGETS['select'], 'pilot_controls': targets,
                   'local_theme': {'component': 'Select', 'colorPrimary': '#722ed1', 'optionHeight': 36,
                                   'prefix': {'icon': 'SearchOutlined'}},
                   'reason': 'Complete authored sample is required; no truncated Flex substitute'}
    return _base('select', controls, data, {}, composition, steps, oracle, targets)


def definitions():
    return [button(), input_case(), select_case()]
