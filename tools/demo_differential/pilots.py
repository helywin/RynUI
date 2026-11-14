"""Three source-bound pilot definitions; no expected values come from observations."""
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
    {'identity': 'noto-emoji', 'sha256': '1ada997ccb67bf2404828e25528e5257d6db10c80287e77684abf6737a5d198f', 'face_index': 0, 'weight': 400},
    {'identity': 'noto-sans', 'sha256': '87a8b90ece1e89746b544e4e086f85a3710e41485a8078f9be874837dfad45d5', 'face_index': 0, 'weight': 600},
    {'identity': 'noto-sans-cjk-sc', 'sha256': '4137d864b16d2670304ca58d1f884e20a2656d19230b8f497e0ce53584fa3d2a', 'face_index': 0, 'weight': 600},
    {'identity': 'noto-sans', 'sha256': '7bbe267354704c6ad18bde24b1dbc756c8e4380ca1c3f3c25c45ec5c4471510b', 'face_index': 0, 'weight': 500},
    {'identity': 'noto-sans-cjk-sc', 'sha256': 'feef2a772298a640a5d9a68185b12c068b8ce824b6edaa96d0979a290fe5a251', 'face_index': 0, 'weight': 500},
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
        # GalleryPagesHost uses gallery_page_layout.hpp, not the legacy grid.
        # At logical width 1280: 1280 - 2*24 outer - 240 nav - 160 toc
        # - 2*24 column gaps - 2*32 document padding - 2*24 sample padding.
        'container': {'width': 672, 'height': 0, 'padding': 0, 'gap': 12, 'layout': 'composition-v1'},
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
            'adapter_sha256': 'eb92e49212b41b59f29626fd5bea5dd2f1593d0cf256cf4683721609ff25085e',
            'mode': 'official-component-adapted', 'adaptation': [
                {'field': field, 'value': deepcopy(fixture[field]),
                 'reason': f'{function}: declared public {field}; original sample content and default local Theme remain required; no measured native input'}
                for field in ('props', 'slots', 'data', 'container', 'theme', 'parent_policy', 'fonts')]},
        'native': {'route': 'input/input' if component == 'input' else component, 'family': 'input',
            'sample_id': sample, 'targets': list(TARGETS[component]), 'source_function': function,
            'public_header': f'include/ryn/{component}.hpp'},
        'mapping': 'paired', 'fixture': fixture, 'profile': 'pilot-pinned-font', 'actions': actions,
        'checkpoints': checkpoints, 'oracle': oracle,
        'features': ['default-visible-context', 'controlled-contract' if component != 'button' else 'input-eligibility'],
        'dependencies': [f'include/ryn/{component}.hpp', f'src/component/{component}_component.cpp',
            'examples/token_gallery/token_gallery_definition.cpp', 'examples/token_gallery/gallery_i18n.cpp',
            'examples/token_gallery/gallery_page_layout.hpp', 'tools/demo_reference_web/app.tsx',
            'tools/demo_differential/pilots.py', 'tools/demo_differential/oracle.py']}
    return validate('case', case)


def button():
    targets = TARGETS['button']
    labels = ('默认', '主色', '危险', '悬停', '按下', '键盘焦点', '禁用', '加载')
    controls = {target: {'component': 'Button', 'type': 'primary' if i in (1, 2, 7) else 'default',
                'size': 'small' if i < 6 else 'middle', 'danger': i == 2, 'disabled': i == 6,
                'loading': i == 7, 'width': 260, 'localTheme': {'algorithm': 'default'}} for i, target in enumerate(targets)}
    slots = {target: {'content': label} for target, label in zip(targets, labels)}
    composition = {'component': 'Space', 'width': 672, 'props': {'wrap': True, 'align': 'center', 'size': 8},
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
    composition = {'component': 'Flex', 'props': {'vertical': True, 'gap': 12, 'align': 'start'}, 'children': [
        text('Input / 输入家族 · 原生桌面功能'), text('支持：四变体、统计/上限、ref/系统提示、清空、Unicode 编辑与 IME'),
        text('Input / Password / Search / TextArea / OTP：双向排版、视觉导航与逻辑编辑'),
        {'component': 'Space', 'width': 672, 'props': {'wrap': True, 'align': 'start', 'size': 8}, 'children': [
            {'component': 'Flex', 'props': {'vertical': True, 'gap': 8, 'align': 'stretch'}, 'children': [
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
    controls[targets[0]].update(placeholder='请选择 / Search')
    data = {targets[0]: {'value': 'desktop', 'options': choices[:2]},
            targets[1]: {'defaultValue': 'desktop', 'options': choices},
            targets[2]: {'options': choices}, targets[3]: {'options': choices}}
    steps = [action('initial', 'checkpoint', targets[0]), *click(targets[0], 'open'),
             action('opened', 'checkpoint', targets[0]), *key(targets[0], 'next', 'ArrowDown'),
             *key(targets[0], 'choose', 'Enter'), action('accepted', 'checkpoint', targets[0]),
             action('reject-policy', 'external-props', targets[0], {'values': {'parent_policy': 'reject'}}),
             *click(targets[0], 'reopen'), *key(targets[0], 'previous', 'ArrowUp'), *key(targets[0], 'reject-choice', 'Enter'),
             *click(targets[1], 'disabled'),
             action('controlled-open-wheel', 'wheel', 'sample', {'delta_x': 0, 'delta_y': 240}),
             *click(targets[3], 'reject-open'), action('rejected', 'checkpoint', targets[0]),
             action('accept-policy', 'external-props', targets[3], {'values': {'parent_policy': 'accept'}}),
             *click(targets[3], 'accept-open'), *key(targets[3], 'escape', 'Escape'),
             action('closed', 'checkpoint', targets[3])]
    oracle = [predicate('count', 'choose-down', stream='callbacks', target=targets[0], event='change', equals=1),
              predicate('count', 'reject-choice-down', stream='callbacks', target=targets[0], event='change', equals=1),
              predicate('count', 'disabled-up', stream='callbacks', target=targets[1], event='change', equals=0)]
    oracle += [predicate('equals', step, path='/targets/1/state/value', value='service') for step in ('accepted', 'rejected')]
    oracle += [predicate('equals', step, path=f'/resources/actual_inputs/current/{targets[0]}/value', value='service')
               for step in ('accepted', 'rejected')]
    oracle += [predicate('equals', 'rejected', path='/targets/4/state/open', value=False),
               predicate('equals', 'closed', path='/targets/4/state/open', value=False)]
    controls[targets[0]]['notFoundContent'] = '没有匹配项'
    slots = {}
    captions = ('受控 / 清空 / 动态 options', 'int64 value', 'Disabled', '保留搜索，阻止提交', 'Empty',
                'Controlled open', 'Outlined', 'Filled', 'Borderless', 'Underlined', 'Small', 'Middle',
                'Large', 'Warning', 'Error')
    # Ordered identically to add_select_samples, including the source's nested
    # add_complete_select_samples. These are authored inputs, never measured geometry.
    for name in TARGETS['select'][:15]:
        controls.setdefault(name, {'component': 'Select', 'width': 260})
        data.setdefault(name, {'defaultValue': 'desktop', 'options': deepcopy(choices)})
    numeric = 'gallery.select.numeric'
    data[numeric] = {'defaultValue': 42, 'options': [
        {'value': 1, 'label': '优先级 1'}, {'value': 42, 'label': '优先级 42'},
        {'value': 99, 'label': '禁用', 'disabled': True}]}
    empty = 'gallery.select.empty'
    controls[empty].update(showSearch=True, notFoundContent='暂无选项')
    data[empty] = {'options': []}
    for variant in ('outlined', 'filled', 'borderless', 'underlined'):
        controls['gallery.select.' + variant].update(variant=variant, allowClear=True)
    for size in ('small', 'middle', 'large'):
        controls['gallery.select.' + size]['size'] = size
    for status in ('warning', 'error'):
        controls['gallery.select.' + status]['status'] = status
    children = [text('Select · 单选 / Unicode 搜索 / 清空 / 动态选项 / 键盘')]
    for name, caption in zip(TARGETS['select'][:15], captions):
        slots[name] = {'caption': caption}
        children.extend([{'component': 'Typography.Text', 'target': name, 'slot': 'caption'}, leaf(name)])
    themed = 'gallery.select.component-theme'
    controls[themed] = {'component': 'Select', 'width': 260, 'showSearch': True,
        'localTheme': {'components': {'Select': {'algorithm': True, 'colorPrimary': '#722ed1', 'optionHeight': 36}}}}
    data[themed] = {'defaultValue': 'service', 'options': deepcopy(choices)}
    slots[themed] = {'prefix': {'icon': 'SearchOutlined'}}
    children.extend([text('独立颜色 / 36 dp 行高'), leaf(themed)])
    for name, label, recipe in [('focus', 'Focus Select', 'select-focus'),
                                ('toggle-open', 'Toggle Select popup', 'select-toggle-open'),
                                ('dynamic-options', 'Update options', 'select-dynamic-options')]:
        identity = 'gallery.select.' + name
        controls[identity] = {'component': 'Button', 'recipe': recipe}
        slots[identity] = {'content': label}
        children.append(leaf(identity))
    groups = [{'label': '平台', 'title': 'platform', 'options': deepcopy(choices[:2])},
              {'label': '优先级', 'title': 'priority', 'options': [
                  {'value': 1, 'label': '高'}, {'value': 2, 'label': '低'}, {'value': 3, 'label': '停用', 'disabled': True}]}]
    multiple, tags, virtual, asynchronous = ['gallery.select.' + key for key in ('multiple', 'tags', 'virtual', 'async')]
    controls[multiple] = {'component': 'Select', 'width': 340, 'marginBlock': 4, 'mode': 'multiple',
        'maxTagCount': 2, 'allowClear': True, 'placeholder': '多选；标签关闭和空搜索 Backspace 可移除', 'recipe': 'select-multiple'}
    data[multiple] = {'value': ['desktop'], 'options': deepcopy(groups)}
    controls[tags] = {'component': 'Select', 'width': 340, 'marginBlock': 4, 'mode': 'tags',
        'tokenSeparators': [',', '；'], 'maxTagCount': 'responsive', 'maxTagTextLength': 8, 'maxCount': 12,
        'placeholder': 'Tags：输入中文；英文, 或 Enter 创建标签', 'recipe': 'select-tags'}
    tag_groups = deepcopy(groups)
    for group in tag_groups:
        for option in group['options']:
            option['value'] = str(option['value'])
    data[tags] = {'options': tag_groups}
    controls[virtual] = {'component': 'Select', 'width': 340, 'marginBlock': 4, 'showSearch': True,
        'listHeight': 160, 'direction': 'rtl', 'placeholder': '合成长列表；Home/End/上下键/滚轮', 'recipe': 'select-virtual'}
    data[virtual] = {'options': [{'value': index, 'label': f'合成选项 {index}'} for index in range(10000)]}
    controls[asynchronous] = {'component': 'Select', 'width': 340, 'marginBlock': 4, 'showSearch': True,
        'placeholder': '输入查询后，在 popup 内点击交付', 'notFoundContent': '合成选项待交付', 'recipe': 'select-async'}
    data[asynchronous] = {'options': []}
    for name, component, content, recipe in [
            ('authority', 'Button', '接受/拒绝多选请求', 'select-authority'),
            ('prefix', 'Button', None, 'select-prefix'),
            ('popup-note', 'Input', None, None),
            ('deliver', 'Button', '交付合成异步选项', 'select-deliver')]:
        identity = 'gallery.select.' + name
        controls[identity] = {'component': component, **({'recipe': recipe} if recipe else {})}
        if content: slots[identity] = {'content': content}
    controls['gallery.select.prefix']['size'] = 'small'
    slots['gallery.select.prefix'] = {'prefix': {'icon': 'SearchOutlined'}}
    controls['gallery.select.popup-note']['placeholder'] = 'popup 内独立草稿'
    slots[virtual] = {'prefix': {'control': 'gallery.select.prefix'}, 'popup': {'control': 'gallery.select.popup-note'}}
    slots[asynchronous] = {'popup': {'control': 'gallery.select.deliver'}}
    children.extend([text('Multiple / Tags / 分组 / typed 多值 / Token 分隔 / 标签溢出'), leaf(multiple),
        leaf('gallery.select.authority'), leaf(tags), {'component': 'source-feedback', 'identity': 'select-multi', 'content': '尚未修改多选'},
        text('10,000 项窗口列表 / RTL / 自定义 option 与 popup / 可操作前缀'), leaf(virtual),
        text('可取消异步选项（确定性合成数据，无外部网络请求）'), leaf(asynchronous)])
    composition = {'component': 'Flex', 'props': {'vertical': True, 'align': 'flex-start', 'gap': 12}, 'children': children}
    note, deliver = 'gallery.select.popup-note', 'gallery.select.deliver'
    steps += [action('virtual-wheel', 'wheel', 'sample', {'delta_x': 0, 'delta_y': 1152}),
        *click(virtual, 'virtual-open'), action('virtual-mounted', 'checkpoint', virtual),
        *click(note, 'popup-note-focus'), action('popup-note-edit', 'text-commit', note, {'text': 'popup draft'}),
        action('virtual-drafted', 'checkpoint', note), *key(virtual, 'virtual-close', 'Escape'),
        action('async-wheel', 'wheel', 'sample', {'delta_x': 0, 'delta_y': 96}),
        *click(asynchronous, 'async-open'), action('async-mounted', 'checkpoint', asynchronous),
        action('async-query', 'text-commit', asynchronous, {'text': 'query'}),
        *click(deliver, 'async-deliver'), action('async-delivered', 'checkpoint', asynchronous),
        *click(asynchronous, 'async-selector-focus'),
        *key(asynchronous, 'async-close', 'Escape'), action('popup-proof-complete', 'checkpoint', asynchronous)]
    for step, target, event in [('popup-note-edit', note, 'change'),
                                ('async-query', asynchronous, 'search'), ('async-deliver-up', deliver, 'activate')]:
        oracle += [predicate('count', step, stream='callbacks', target=target, event=event, equals=1),
                   predicate('equals', step, path='/receipt/dispatched', value=True),
                   predicate('equals', step, path='/receipt/hit', value=target)]
    # Preserve the actual input phases: native Select toggles on pointer-up;
    # the pinned browser selector toggles on mousedown. Neither adapter moves
    # callbacks to another raw step to manufacture a matching trace.
    for name, target in [('virtual-open', virtual), ('async-open', asynchronous)]:
        for side, phase in [('native', 'up'), ('web', 'down')]:
            for observed_phase in ('down', 'up'):
                oracle.append({**predicate('count', name + '-' + observed_phase, stream='callbacks', target=target,
                    event='open-change', equals=int(observed_phase == phase)), 'side': side})
        oracle += [predicate('equals', name + '-up', path='/receipt/dispatched', value=True),
                   predicate('equals', name + '-up', path='/receipt/hit', value=target)]
    for step, parent, child in [('virtual-mounted', virtual, note), ('async-mounted', asynchronous, deliver)]:
        oracle += [predicate('equals', step, path=f'/resources/actual_inputs/instances/{child}/status', value='mounted'),
                   predicate('equals', step, path=f'/resources/actual_inputs/instances/{child}/parent_target', value=parent),
                   predicate('equals', step, path=f'/resources/actual_inputs/current/{parent}/open', value=True)]
    for step in ('virtual-drafted', 'async-delivered', 'popup-proof-complete'):
        oracle += [predicate('equals', step, path=f'/resources/actual_inputs/current/{note}/value', value='popup draft'),
                   predicate('unchanged', step, from_step='virtual-mounted', path=f'/resources/actual_inputs/instances/{note}/instance')]
    oracle += [predicate('equals', 'virtual-drafted', path=f'/resources/actual_inputs/current/{note}/focused', value=True),
        predicate('equals', 'virtual-mounted', path=f'/resources/actual_inputs/current/{note}/value', value=''),
        predicate('equals', 'virtual-drafted', path=f'/resources/actual_inputs/current/{note}/selection/start', value=11),
        predicate('equals', 'virtual-drafted', path=f'/resources/actual_inputs/current/{note}/selection/end', value=11),
        predicate('equals', 'async-delivered', path=f'/resources/actual_inputs/current/{asynchronous}/query', value='query'),
        predicate('equals', 'async-delivered', path=f'/resources/actual_inputs/data/{asynchronous}/options/0/value', value='query · synthetic'),
        predicate('equals', 'async-delivered', path=f'/resources/actual_inputs/data/{asynchronous}/options/0/label', value='query · synthetic'),
        predicate('unchanged', 'async-delivered', from_step='async-mounted', path=f'/resources/actual_inputs/instances/{asynchronous}/instance'),
        predicate('unchanged', 'async-delivered', from_step='async-mounted', path=f'/resources/actual_inputs/instances/{deliver}/instance'),
        predicate('equals', 'popup-proof-complete', path=f'/resources/actual_inputs/current/{asynchronous}/open', value=False)]
    oracle += [predicate('equals', 'async-deliver-up', path='/resources/actual_inputs/feedbacks/select-multi',
                         value='已处理合成选项；已取消的请求不会更新选择器'),
               predicate('equals', 'async-selector-focus-up',
                         path=f'/resources/actual_inputs/current/{asynchronous}/open', value=True),
               predicate('equals', 'async-selector-focus-up',
                         path=f'/resources/actual_inputs/current/{asynchronous}/focused', value=True)]
    for entry in controls.values():
        if entry['component'] == 'Select' and (entry.get('showSearch') or entry.get('mode')):
            entry['optionFilterProp'] = 'label'
    result = _base('select', controls, data, slots, composition, steps, oracle, targets)
    result['fixture']['container']['layout'] = 'composition-v2'
    result['official']['adaptation'][3]['value'] = deepcopy(result['fixture']['container'])
    for checkpoint in result['checkpoints']:
        extra = { 'virtual-mounted': [virtual, virtual + '.popup', note, 'gallery.select.prefix'],
                  'virtual-drafted': [virtual, virtual + '.popup', note, 'gallery.select.prefix'],
                  'async-mounted': [asynchronous, asynchronous + '.popup', deliver],
                  'async-delivered': [asynchronous, asynchronous + '.popup', deliver] }.get(checkpoint['step'], [])
        checkpoint['targets'] += extra
        checkpoint['rois'] += [{'identity': target, 'kind': 'popup' if target.endswith('.popup') else 'control',
            'target': target, 'guard_px': 2} for target in extra]
    # Existing target/sample identities and their order stay authoritative.
    return validate('case', result)


def definitions():
    return [button(), input_case(), select_case()]
