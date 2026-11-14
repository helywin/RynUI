"""Actual Chromium contracts. No paired native verdicts or synthetic browser substitutes.

Run prepare and fonts from docs/development/demo-reference-web.md first. Missing tools fail,
not skip. Generated requests, screenshots, reports and negative artifacts stay under out/.
"""
from copy import deepcopy
import json
import os
import re
from pathlib import Path
import shutil
import sys
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_reference_web import reference as web
from demo_reference_web import bootstrap
from demo_differential.protocol import validate

PREPARED = ROOT / os.environ.get('RYNUI_WEB_PREPARED', 'out/demo-differential/prepared')
FONT_FILE = ROOT / os.environ.get('RYNUI_WEB_FONTS', 'out/demo-differential/fonts/font-files.json')
OUT = ROOT / 'out/demo-differential/web-tests' / uuid.uuid4().hex


def action(step, kind, target='sample', parameters=None, **extra):
    return {'step': step, 'kind': kind, 'target': target, 'time_us': 0,
            'layer': 'observer' if kind == 'checkpoint' else 'external-props' if kind == 'external-props' else 'browser-input',
            'parameters': parameters or {}, 'preconditions': {}, **extra}


def request(demo='components/button/demo/basic.tsx', controls=None, data=None, slots=None, actions=None):
    lock = bootstrap.load_lock()
    fonts = json.loads(FONT_FILE.read_text(encoding='utf-8'))
    fixture = {'props': {'controls': controls} if controls else {}, 'data': data or {}, 'slots': slots or {},
               'parent_policy': 'accept', 'clock_epoch_us': 1760000000000000, 'seed': 1, 'timezone': 'UTC',
               'viewport': {'width': 640, 'height': 420, 'dpr': 1},
               'container': {'width': 580, 'height': 360, 'padding': 12, 'gap': 8, 'layout': 'row'},
               'theme': {'algorithm': 'default'}, 'locale': 'zh-CN', 'format_locale': 'zh-CN',
               'direction': 'ltr', 'fonts': [{k: f[k] for k in ('identity', 'sha256', 'face_index', 'weight')} for f in fonts],
               'font_policy': 'pinned', 'motion': 'disabled'}
    actions = actions or [action('capture', 'checkpoint')]
    checkpoints = [{'step': a['step'], 'targets': ['sample'] + list(controls or {}), 'layers': ['geometry', 'text', 'pixels'],
                    'rois': [{'identity': 'context', 'kind': 'sample-context', 'target': 'sample', 'guard_px': 2}]}
                   for a in actions if a['kind'] == 'checkpoint']
    source_path = 'components/' + demo.split('/')[1] + '/' + {
        'button': 'Button.tsx', 'input': 'Input.tsx', 'select': 'index.tsx'}[demo.split('/')[1]]
    source = Path(json.loads((PREPARED / 'prepared.json').read_text(encoding='utf-8'))['source'])
    definition = {'schema_version': 1, 'identity': 'antd665.' + demo.split('/')[1] + '.web-contract.v1',
                  'revision': 1, 'official': {'repository': lock['source']['repository'], 'commit': lock['source']['commit'],
                    'demo_path': demo, 'demo_sha256': lock['source']['demos'][demo],
                    'source_paths': [{'path': source_path, 'sha256': bootstrap.sha(source / source_path)}],
                    'adapter_id': web.ADAPTER_ID, 'adapter_sha256': web.adapter_sha256(),
                    'mode': 'official-component-adapted' if controls else 'official-demo',
                    'adaptation': [{'field': key, 'value': fixture[key], 'reason': 'Explicit Web contract input from public component API'}
                                   for key in ('props', 'slots', 'data', 'container', 'theme', 'parent_policy')] if controls else []},
                  'native': {'route': demo.split('/')[1], 'family': 'input', 'sample_id': 'sample',
                             'targets': list(controls or {'sample': {}}), 'source_function': 'web_contract',
                             'public_header': 'include/ryn/button.hpp'}, 'mapping': 'web-only', 'fixture': fixture,
                  'profile': 'linux-web-contract-pinned', 'actions': actions, 'checkpoints': checkpoints,
                  'oracle': [{'op': 'equals', 'step': checkpoints[-1]['step'], 'side': 'web',
                              'origin': 'independent-contract', 'path': 'resources.isolated_context', 'value': True}],
                  'features': ['web-contract'], 'dependencies': ['tools/demo_reference_web/app.tsx']}
    validate('case', definition)
    return {'schema_version': 1, 'nonce': uuid.uuid4().hex, 'definition': definition, 'font_files': fonts}


def click(target, prefix='click'):
    return [action(prefix + '-down', 'pointer-down', target, {'button': 'primary', 'x_fraction': .5, 'y_fraction': .5}),
            action(prefix + '-up', 'pointer-up', target, {'button': 'primary', 'x_fraction': .5, 'y_fraction': .5})]


class WebBrowserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        bootstrap.verify_prepared(PREPARED)
        OUT.mkdir(parents=True)

    def execute(self, req, name=None):
        output = OUT / (name or uuid.uuid4().hex)
        result = web.execute(req, PREPARED, output)
        records = web.validate_observations(output, req)
        return result, records, output

    def test_five_official_exports_really_execute(self):
        for demo in bootstrap.load_lock()['source']['demos']:
            with self.subTest(demo=demo):
                _, records, _ = self.execute(request(demo))
                hello = records[0]
                self.assertTrue(any(f['component'] == 'OfficialDemo' for f in hello['provenance']['executed_component_fibers']))
                self.assertTrue(hello['provenance']['clock_canary']['js_timer'])
                self.assertGreater(len(hello['provenance']['source_closure']), 100)
                self.assertEqual(records[-1]['type'], 'complete')

    def test_button_actual_input_disabled_loading_and_fibers(self):
        controls = {'normal': {'component': 'Button', 'size': 'small'},
                    'disabled': {'component': 'Button', 'disabled': True, 'size': 'middle'},
                    'loading': {'component': 'Button', 'loading': True}}
        steps = click('normal') + click('disabled', 'disabled') + click('loading', 'loading') + [action('capture', 'checkpoint')]
        req = request(controls=controls, slots={x: {'content': x} for x in controls}, actions=steps)
        _, records, _ = self.execute(req)
        stream = records[-1]['callbacks']
        self.assertEqual([(x['target'], x['event']) for x in stream], [('normal', 'activate')])
        self.assertTrue(all(x['receipt']['dispatched'] for x in records if x['type'] == 'receipt'))
        self.assertTrue(all(e['trusted'] for x in records if x['type'] == 'receipt'
                            for e in x['resources'].get('input_events', [])))

    def test_input_accept_reject_real_commit_selection_and_affixes(self):
        target = 'editor'
        steps = click(target) + [action('type-accepted', 'text-commit', target, {'text': 'ab'}),
                action('accepted', 'checkpoint'), action('reject-policy', 'external-props', target,
                    {'values': {'parent_policy': 'reject'}}),
                action('type-rejected', 'text-commit', target, {'text': 'x'}), action('rejected', 'checkpoint')]
        req = request('components/input/demo/allowClear.tsx', {target: {'component': 'Input', 'allowClear': True, 'width': 220}},
                      {target: {'value': ''}}, {target: {'prefix': '￥', 'suffix': 'RMB'}}, steps)
        _, records, _ = self.execute(req)
        checkpoints = [r for r in records if r['type'] == 'checkpoint']
        values = [next(t['state']['value'] for t in r['targets'] if t['identity'] == target) for r in checkpoints]
        self.assertEqual(values, ['ab', 'ab'])
        callbacks = [r for r in records[-1]['callbacks'] if r['event'] == 'change']
        self.assertEqual([r['payload'] for r in callbacks], [
            {'proposed': 'ab', 'committed': 'ab', 'accepted': True},
            {'proposed': 'abx', 'committed': 'ab', 'accepted': False}])
        self.assertEqual(records[0]['effective_inputs'], checkpoints[-1]['effective_inputs'])
        state = next(t['state'] for t in checkpoints[-1]['targets'] if t['identity'] == target)
        self.assertEqual(state['selection']['unit'], 'UTF-16')
        repeated = deepcopy(req)
        repeated['nonce'] = uuid.uuid4().hex
        _, replayed, _ = self.execute(repeated)
        self.assertNotEqual(records[0]['nonce'], replayed[0]['nonce'])
        self.assertEqual([[c['sha256'] for c in r['captures']] for r in checkpoints],
                         [[c['sha256'] for c in r['captures']] for r in replayed if r['type'] == 'checkpoint'])
        self.assertEqual(replayed[0]['provenance']['clock_canary']['caret_control'],
                         {'mode': 'manual', 'screenshot': 'initial', 'hidden': False})

    def test_clear_uses_real_affix_target_and_keeps_outer_geometry(self):
        steps = click('editor') + [action('type', 'text-commit', 'editor', {'text': 'abc'})]
        steps += click('editor.clear', 'clear') + [action('capture', 'checkpoint')]
        req = request('components/input/demo/allowClear.tsx', {'editor': {'component': 'Input', 'allowClear': True, 'width': 240}},
                      {'editor': {'value': ''}}, {'editor': {'prefix': 'Prefix', 'suffix': 'Suffix'}}, steps)
        _, records, _ = self.execute(req)
        target = next(t for r in records if r['type'] == 'checkpoint' for t in r['targets'] if t['identity'] == 'editor')
        self.assertEqual(target['state']['value'], '')
        self.assertEqual(target['bounds']['width'], 240)
        self.assertIn('Prefix', ''.join(t['content'] for t in target['text']))
        self.assertTrue(any(c['event'] == 'clear' for c in records[-1]['callbacks']))
        self.assertEqual(next(r['receipt']['hit'] for r in records if r['type'] == 'receipt' and r['step'] == 'clear-down'), 'editor.clear')

    def test_select_real_keyboard_options_open_and_escape(self):
        target = 'selector'
        steps = click(target) + [action('open', 'checkpoint'),
            action('arrow-down', 'key-down', target, {'key': 'ArrowDown', 'modifiers': []}),
            action('arrow-up', 'key-up', target, {'key': 'ArrowDown', 'modifiers': []}),
            action('enter-down', 'key-down', target, {'key': 'Enter', 'modifiers': []}),
            action('enter-up', 'key-up', target, {'key': 'Enter', 'modifiers': []}), action('selected', 'checkpoint')]
        steps += [action('reject-policy', 'external-props', target, {'values': {'parent_policy': 'reject'}})]
        steps += click(target, 'reopen') + [
            action('reject-arrow-down', 'key-down', target, {'key': 'ArrowUp', 'modifiers': []}),
            action('reject-arrow-up', 'key-up', target, {'key': 'ArrowUp', 'modifiers': []}),
            action('reject-enter-down', 'key-down', target, {'key': 'Enter', 'modifiers': []}),
            action('reject-enter-up', 'key-up', target, {'key': 'Enter', 'modifiers': []}), action('rejected', 'checkpoint')]
        req = request('components/select/demo/basic.tsx', {target: {'component': 'Select', 'width': 220, 'virtual': False}},
                      {target: {'value': 'a', 'options': [{'value': 'a', 'label': 'Alpha'}, {'value': 'b', 'label': 'Beta'}]}}, actions=steps)
        _, records, _ = self.execute(req)
        self.assertTrue(any(r['event'] == 'change' and r['payload']['committed'] == 'b' for r in records[-1]['callbacks']))
        self.assertTrue(any(r['event'] == 'open-change' for r in records[-1]['callbacks']))
        self.assertTrue(any(r['event'] == 'change' and r['payload'] == {'proposed': 'a', 'committed': 'b', 'accepted': False}
                            for r in records[-1]['callbacks']))
        final = [r for r in records if r['type'] == 'checkpoint'][-1]
        state = next(t['state'] for t in final['targets'] if t['identity'] == target)
        self.assertEqual(state['value'], 'b')
        self.assertFalse(state['open'])

    def test_cache_hit_reexecutes_and_environment_key_changes(self):
        req = request(controls={'button': {'component': 'Button'}}, slots={'button': {'content': 'Fixed'}})
        first, _, _ = self.execute(req)
        req['nonce'] = uuid.uuid4().hex
        second, records, _ = self.execute(req)
        self.assertEqual(first['cache_key'], second['cache_key'])
        self.assertTrue(second['cache_hit'])
        self.assertEqual(records[-1]['nonce'], req['nonce'])
        cache_manifest = ROOT / 'out/demo-differential/reference-cache' / first['cache_key'] / 'manifest.json'
        original = cache_manifest.read_bytes()
        try:
            corrupt = json.loads(original)
            corrupt['browser_environment']['actual_gpu']['devices'] = []
            cache_manifest.chmod(0o644)
            cache_manifest.write_text(json.dumps(corrupt))
            with self.assertRaisesRegex(bootstrap.PreparationError, 'browser/GPU environment mismatch'):
                self.execute(req)
        finally:
            cache_manifest.write_bytes(original)
            cache_manifest.chmod(0o444)
        changed = deepcopy(req)
        changed['definition']['fixture']['viewport']['width'] = 641
        third, _, _ = self.execute(changed)
        self.assertNotEqual(first['cache_key'], third['cache_key'])

    def test_official_composition_preserves_context_geometry_and_text(self):
        controls = {'first': {'component': 'Input', 'width': 200}, 'second': {'component': 'Input', 'width': 200}}
        req = request('components/input/demo/allowClear.tsx', controls, {'first': {'value': ''}, 'second': {'defaultValue': ''}},
                      {'first': {'caption': 'Controlled caption'}, 'second': {'caption': 'Uncontrolled caption'}})
        fixture = req['definition']['fixture']
        fixture['container']['layout'] = 'composition-v1'
        fixture['props']['composition'] = {'component': 'Flex', 'props': {'vertical': True, 'gap': 8}, 'children': [
            {'component': 'Typography.Text', 'content': 'Full sample context'},
            {'component': 'Space', 'props': {'wrap': True, 'align': 'start', 'size': 8}, 'children': [
                {'component': 'Flex', 'props': {'vertical': True, 'gap': 8}, 'children': [
                    {'component': 'Typography.Text', 'target': key, 'slot': 'caption'}, {'component': 'control', 'target': key}]}
                for key in controls]}, {'component': 'Typography.Text', 'content': 'Feedback remains in context'}]}
        for item in req['definition']['official']['adaptation']:
            item['value'] = fixture[item['field']]
        _, records, _ = self.execute(req)
        actual = records[0]['effective_inputs']['actual_composition']
        self.assertEqual(len(actual), 8)
        targets = next(r['targets'] for r in records if r['type'] == 'checkpoint')
        first = next(t for t in targets if t['identity'] == 'first')
        second = next(t for t in targets if t['identity'] == 'second')
        self.assertGreater(second['bounds']['x'], first['bounds']['x'] + first['bounds']['width'])
        self.assertGreater(first['bounds']['y'], 12)
        context_text = ''.join(t['content'] for t in targets[0]['text'])
        self.assertIn('Full sample context', context_text)
        self.assertIn('Feedback remains in context', context_text)
        for mutation in [lambda x: x.update(component='div'), lambda x: x['props'].update(style={'padding': 1}),
                         lambda x: x['children'].append({'component': 'control', 'target': 'first'})]:
            tree = deepcopy(fixture['props']['composition'])
            mutation(tree)
            with self.assertRaises(bootstrap.PreparationError):
                web.validate_composition(tree, controls, fixture['slots'])
        deep = {'component': 'control', 'target': 'first'}
        for _ in range(26):
            deep = {'component': 'Flex', 'children': [deep]}
        with self.assertRaisesRegex(bootstrap.PreparationError, 'limit'):
            web.validate_composition(deep, controls, fixture['slots'])

    def test_unexpected_font_fallback_fails_actual_browser(self):
        req = request(controls={'button': {'component': 'Button'}}, slots={'button': {'content': '中文测试'}})
        _, records, _ = self.execute(req)
        target = next(t for r in records if r['type'] == 'checkpoint' for t in r['targets'] if t['identity'] == 'button')
        self.assertTrue(any(font['postScriptName'] == 'NotoSansCJKsc-Regular' and font['isCustomFont']
                            for font in target['tokens']['platform_fonts']))
        req['font_files'] = req['font_files'][:1]
        req['definition']['fixture']['fonts'] = req['definition']['fixture']['fonts'][:1]
        with self.assertRaisesRegex(bootstrap.PreparationError, 'execution failed'):
            self.execute(req)

    def test_natural_context_height_and_actual_wheel(self):
        req = request(controls={'button': {'component': 'Button'}}, slots={'button': {'content': 'Last control'}},
                      actions=[action('before', 'checkpoint'), action('scroll', 'wheel', 'sample', {'delta_x': 0, 'delta_y': 220}),
                               action('after', 'checkpoint')])
        fixture = req['definition']['fixture']
        fixture['viewport']['height'] = 200
        fixture['container'].update(height=0, layout='composition-v1')
        fixture['props']['composition'] = {'component': 'Flex', 'props': {'vertical': True, 'gap': 8}, 'children': [
            *[{'component': 'Typography.Text', 'content': 'Full context line ' + str(i)} for i in range(16)],
            {'component': 'control', 'target': 'button'}]}
        for item in req['definition']['official']['adaptation']:
            item['value'] = fixture[item['field']]
        _, records, _ = self.execute(req)
        context = records[0]['effective_inputs']['actual_container']
        self.assertEqual(context['height_policy'], 'natural')
        self.assertEqual(context['declared_height_style'], '')
        self.assertGreater(context['bounds']['height'], 200)
        checkpoints = [r for r in records if r['type'] == 'checkpoint']
        before, after = [r['targets'][0]['bounds'] for r in checkpoints]
        self.assertLess(after['y'], before['y'])
        self.assertEqual(after['height'], before['height'])
        receipt = next(r for r in records if r['type'] == 'receipt' and r['step'] == 'scroll')
        self.assertTrue(receipt['receipt']['dispatched'])
        self.assertTrue(any(e['type'] == 'wheel' and e['trusted'] for e in receipt['resources']['input_events']))

    def test_consumed_inputs_are_read_from_committed_fibers(self):
        req = request('components/input/demo/presuffix.tsx',
                      {'editor': {'component': 'Input', 'status': 'warning', 'width': 210},
                       'button': {'component': 'Button', 'size': 'small'}}, {'editor': {'value': 'initial'}},
                      {'editor': {'prefix': {'icon': 'LockOutlined'}, 'suffix': 'RMB', 'caption': 'Actual caption'},
                       'button': {'content': 'Find', 'prefix': {'icon': 'SearchOutlined'}}})
        _, records, output = self.execute(req)
        controls = records[0]['effective_inputs']['actual_controls']
        self.assertEqual(controls['editor']['data'], {'value': 'initial'})
        self.assertEqual(controls['editor']['slots'], req['definition']['fixture']['slots']['editor'])
        self.assertEqual(controls['editor']['source'], 'committed React component memoizedProps')
        code = """
const declarations = window.__reference.controls();
declarations.editor.publicProps.disabled = true;
declarations.editor.slots = { prefix: 'FAKE wrapper declaration' };
declarations.editor.data = { value: 'FAKE wrapper declaration' };
"""
        result, independent = self.worker_control(output, lambda text: text + code)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        observed = web.validate_observations(independent, req)[0]['effective_inputs']
        self.assertEqual(observed['data'], req['definition']['fixture']['data'])
        self.assertEqual(observed['slots'], req['definition']['fixture']['slots'])
        self.assertNotIn('disabled', observed['props']['controls']['editor'])

    def test_full_select_deferred_children_have_real_portal_instances_and_input_receipts(self):
        from demo_differential import pilots
        from demo_differential.adapters import web_definition
        from demo_differential.actual_inputs import validate_timeline
        from demo_differential.oracle import evaluate
        definition = web_definition(pilots.select_case())
        definition['fixture']['viewport']['dpr'] = 1
        req = {'schema_version': 1, 'nonce': uuid.uuid4().hex, 'definition': definition,
               'font_files': json.loads(FONT_FILE.read_text(encoding='utf-8'))}
        _, rows, output = self.execute(req)
        observed = {'records': rows, 'callbacks': rows[-1]['callbacks'],
            'receipts': {r['step']: r for r in rows if r['type'] == 'receipt'},
            'checkpoints': {r['step']: r for r in rows if r['type'] == 'checkpoint'}}
        validate_timeline(definition, observed, 'web')
        self.assertFalse(evaluate(definition, observed, 'web'))
        self.assertEqual(rows[0]['provenance']['clock_canary']['caret_control'],
                         {'mode': 'manual', 'screenshot': 'initial', 'hidden': False})
        initial = deepcopy(rows[0]['effective_inputs']['actual_initial_inputs'])
        self.assertEqual(initial['instances']['gallery.select.popup-note']['status'], 'deferred')
        self.assertEqual(initial['instances']['gallery.select.deliver']['status'], 'deferred')
        self.assertEqual(rows[-1]['effective_inputs']['actual_initial_inputs'], initial)
        drafted = observed['checkpoints']['virtual-drafted']['resources']['actual_inputs']
        closed = observed['checkpoints']['async-mounted']['resources']['actual_inputs']
        retained = closed['instances']['gallery.select.popup-note']
        self.assertEqual(retained['instance'], drafted['instances']['gallery.select.popup-note']['instance'])
        self.assertEqual((retained['parent_target'], retained['slot']), ('gallery.select.virtual', 'popup'))
        self.assertFalse(closed['current']['gallery.select.virtual']['open'])
        insertion = 'const props = { ...publicProps, "data-testid": identity };'
        def wrong_placeholder(text):
            self.assertIn(insertion, text)
            return text.replace(insertion, insertion + '\nif (identity === "gallery.select.popup-note") props.placeholder = "wrong actual placeholder";')
        result, negative = self.worker_control(output, wrong_placeholder)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        with self.assertRaisesRegex(ValueError, 'placeholder'):
            web.validate_observations(negative, req)
        def extra_origin_wrapper(text):
            pattern = r'(\w+\.default\.createElement)\((Flex\w*), \{ vertical: true, gap: 4 \}, origin, slot\(slots\.popup\)\)'
            changed, count = re.subn(pattern, lambda match: (
                f'{match[1]}({match[2]}, {{ vertical: true, gap: 4 }}, '
                f'{match[1]}({match[2]}, {{ vertical: true, gap: 40 }}, origin), slot(slots.popup))'), text)
            self.assertEqual(count, 1, 'actual popup renderer source was not changed')
            return changed
        result, negative = self.worker_control(output, extra_origin_wrapper)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        with self.assertRaisesRegex(ValueError, 'popup wrapper/origin/order'):
            web.validate_observations(negative, req)
        binding = 'props.popupRender.sourceControl = slots.popup.control;'
        def bound_but_never_consumed(text):
            self.assertIn(binding, text)
            return text.replace(binding, binding + '\nif (identity === "gallery.select.virtual") { const empty = () => null; empty.sourceControl = slots.popup.control; props.popupRender = empty; }')
        result, negative = self.worker_control(output, bound_but_never_consumed)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('missing checkpoint target', (negative / 'error.json').read_text(encoding='utf-8'))

    def test_missing_target_fails_real_browser(self):
        req = request()
        req['definition']['checkpoints'][0]['targets'] = ['absent']
        with self.assertRaisesRegex(bootstrap.PreparationError, 'execution failed'):
            self.execute(req)

    def test_external_network_is_rejected_in_execute(self):
        req = request(controls={'button': {'component': 'Button'}}, slots={'button': {'content': 'Network'}})
        # Use a font resource URL via public theme font-family? That cannot issue a network request.
        # Instead append an explicit resource request to a test worker's bundle after its real official build.
        # This validates the worker's network boundary; it is never a product/reference code path.
        _, _, output = self.execute(req)
        config = json.loads((output / 'worker.json').read_text(encoding='utf-8'))
        negative = OUT / uuid.uuid4().hex
        negative.mkdir()
        for name in ('tmp', 'cache', 'config'):
            (negative / name).mkdir()
        bundle = negative / 'bundle'
        shutil.copytree(output / 'bundle', bundle)
        with (bundle / 'app.js').open('a') as stream:
            stream.write("\nfetch('https://example.invalid/undeclared').catch(()=>{});\n")
        config.update(output=str(negative), build=str(bundle))
        (negative / 'worker.json').write_text(json.dumps(config))
        result = bootstrap.subprocess.run([config['prepared']['node'], str(web.HERE / 'execute.mjs'), str(negative / 'worker.json')],
                                          capture_output=True, timeout=40)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('execute network denied', (negative / 'error.json').read_text(encoding='utf-8'))

    def test_protocol_rejects_skipped_input_missing_frame_complete_and_wrong_nonce(self):
        req = request()
        _, records, output = self.execute(req)
        changes = [lambda rows: rows[:-1],
                   lambda rows: [r for r in rows if r['type'] != 'receipt'],
                   lambda rows: [dict(r, captures=[]) if r['type'] == 'checkpoint' else r for r in rows],
                   lambda rows: [dict(r, nonce='old') if r['type'] == 'checkpoint' else r for r in rows],
                   lambda rows: [dict(r, receipt={**r['receipt'], 'dispatched': False}) if r['type'] == 'receipt' else r for r in rows],
                   lambda rows: [dict(r, resources={}) if r['type'] == 'checkpoint' else r for r in rows]]
        for change in changes:
            modified = OUT / uuid.uuid4().hex
            shutil.copytree(output, modified)
            (modified / 'observations.jsonl').write_text(''.join(json.dumps(r) + '\n' for r in change(deepcopy(records))))
            with self.assertRaises((bootstrap.PreparationError, ValueError)):
                web.validate_observations(modified, req)

    def worker_control(self, output, transform, driver_transform=None):
        config = json.loads((output / 'worker.json').read_text(encoding='utf-8'))
        negative = OUT / uuid.uuid4().hex
        negative.mkdir()
        for name in ('tmp', 'cache', 'config'):
            (negative / name).mkdir()
        bundle = negative / 'bundle'
        shutil.copytree(output / 'bundle', bundle)
        app = bundle / 'app.js'
        app.write_text(transform(app.read_text(encoding='utf-8')), encoding='utf-8')
        config.update(output=str(negative), build=str(bundle))
        (negative / 'worker.json').write_text(json.dumps(config))
        driver = web.HERE / 'execute.mjs'
        if driver_transform:
            transformed = negative / 'execute.mjs'
            transformed.write_text(driver_transform(driver.read_text(encoding='utf-8')), encoding='utf-8')
            driver = transformed
        result = bootstrap.subprocess.run([config['prepared']['node'], str(driver), str(negative / 'worker.json')],
                                          capture_output=True, timeout=40)
        return result, negative

    def test_actual_component_execution_and_public_prop_negative_controls(self):
        req = request(controls={'button': {'component': 'Button', 'disabled': False}},
                      slots={'button': {'content': 'Identity control'}})
        _, _, output = self.execute(req)
        insertion = 'const props = { ...publicProps, "data-testid": identity };'
        def wrong_props(text):
            self.assertIn(insertion, text)
            return text.replace(insertion, insertion + '\nprops.disabled = true;')
        result, negative = self.worker_control(output, wrong_props)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('effective public prop mismatch', (negative / 'error.json').read_text(encoding='utf-8'))
        result, negative = self.worker_control(output, lambda text: text + '\nwindow.__reference.fibers = () => [];\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('actual official component did not execute', (negative / 'error.json').read_text(encoding='utf-8'))

    def test_real_skipped_mouse_dispatch_is_detected(self):
        req = request(controls={'button': {'component': 'Button'}}, slots={'button': {'content': 'Dispatch negative'}},
                      actions=click('button') + [action('capture', 'checkpoint')])
        _, _, output = self.execute(req)
        def skip_down(text):
            original = "await page.mouse.down({ button: parameters.button === 'primary' ? 'left' : 'right' });"
            self.assertEqual(text.count(original), 1)
            return text.replace(original, 'void 0; // test deliberately omitted actual dispatch')
        result, negative = self.worker_control(output, lambda text: text, skip_down)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('input not executed/trusted', (negative / 'error.json').read_text(encoding='utf-8'))

    def test_actual_options_mismatch_is_detected(self):
        req = request('components/select/demo/basic.tsx', {'selector': {'component': 'Select', 'width': 200}},
                      {'selector': {'value': 'a', 'options': [{'value': 'a', 'label': 'Alpha'}, {'value': 'b', 'label': 'Beta'}]}})
        _, _, output = self.execute(req)
        def wrong_options(text):
            original = 'props.options = data.options;'
            self.assertEqual(text.count(original), 1)
            return text.replace(original, 'props.options = [...data.options].reverse();')
        result, negative = self.worker_control(output, wrong_options)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('effective options mismatch', (negative / 'error.json').read_text(encoding='utf-8'))

    def test_hidden_saved_position_still_sends_real_input(self):
        steps = [action('save-position', 'pointer-move', 'button', {'x_fraction': .5, 'y_fraction': .5})]
        steps += click('button', 'hidden') + [action('capture', 'checkpoint')]
        for step in steps:
            if step['step'].startswith('hidden'):
                step['preconditions'] = {'allow_hidden_saved': True}
        req = request(controls={'button': {'component': 'Button'}}, slots={'button': {'content': 'Hide input contract'}}, actions=steps)
        _, _, output = self.execute(req)
        code = """
const originalStep = window.__reference.step;
window.__reference.step = (id, timestamp) => {
  originalStep(id, timestamp);
  if (id === 'hidden-down') window.__reference.element('button').style.visibility = 'hidden';
};
"""
        result, negative = self.worker_control(output, lambda text: text + code)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        records = web.validate_observations(negative, req)
        self.assertFalse(records[-1]['callbacks'])
        sent = [r for r in records if r['type'] == 'receipt' and r['step'].startswith('hidden')]
        self.assertEqual(len(sent), 2)
        self.assertTrue(all(r['receipt']['dispatched'] for r in sent))
        self.assertTrue(all(any(e['trusted'] for e in r['resources']['input_events']) for r in sent))

    def test_wrong_props_data_adapter_font_tag_are_rejected(self):
        req = request(controls={'button': {'component': 'Button'}}, slots={'button': {'content': 'Contract'}})
        prepared = bootstrap.verify_prepared(PREPARED)
        web.validate_request(req, prepared)
        mutations = [lambda x: x['definition']['fixture']['props']['controls']['button'].update(style={'padding': 99}),
                     lambda x: x['definition']['official'].update(adapter_sha256='a' * 64),
                     lambda x: x['font_files'][0].update(sha256='a' * 64),
                     lambda x: x['definition']['official'].update(demo_sha256='a' * 64),
                     lambda x: x['definition']['fixture']['data'].update(unconsumed={'value': 'x'}),
                     lambda x: x['definition']['fixture']['props']['controls']['button'].update(width='calc(100%)'),
                     lambda x: x['definition']['fixture']['slots']['button'].update(content={'dangerouslySetInnerHTML': 'x'}),
                     lambda x: x['definition']['official']['adaptation'][0].update(reason='   ')]
        for mutate in mutations:
            changed = deepcopy(req)
            mutate(changed)
            with self.assertRaises((bootstrap.PreparationError, ValueError)):
                web.validate_request(changed, prepared)


if __name__ == '__main__':
    unittest.main()
