"""Independent-input mechanism controls. These are not native/browser run evidence."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_differential.protocol import ProtocolError
from demo_differential.runner_evidence import native_actual_inputs, native_options, web_options, verify_theme


def declarations():
    return {'props': {'controls': {'editor': {'component': 'Input', 'width': 260, 'status': 'warning',
            'variant': 'outlined', 'maxLength': 32}}, 'composition': {'component': 'Flex',
            'props': {'vertical': True, 'gap': 12, 'align': 'start'}, 'children': [
                {'component': 'Typography.Text', 'target': 'editor', 'slot': 'caption'},
                {'component': 'control', 'target': 'editor'}]}},
            'data': {'editor': {'value': '中文 seed'}},
            'slots': {'editor': {'caption': '受控输入', 'prefix': '前', 'suffix': '32'}},
            'theme': {'algorithm': 'default', 'token': {'colorBgContainer': '#ffffff'}}}


def observations():
    # Authored separately to exercise semantic mapping: native splits the suffix
    # into text runs and retains a passive slot wrapper absent from React.
    return {'source': 'mounted hosts, applied editor state, retained slots/ThemeScope/layout',
            'controls': {'editor': {'component': 'Input', 'width': 260, 'status': 'warning', 'variant': 'outlined',
                'maxLength': 32, 'disabled': False}}, 'data': {'editor': {'value': '中文 seed'}},
            'slots': {'editor': {'prefix': ['前'], 'suffix': ['3', '2']}},
            'themes': {'editor': {'source': 'retained ThemeScope applied config and resolved snapshot',
                'algorithms': ['default'], 'colorBgContainer': '#ffffff'}},
            'composition': {'source': 'retained component tree and applied layout models',
                'host': {'vertical': True, 'gap': 12, 'align': 'start'}, 'roots': [
                    {'component': 'Typography.Text', 'content': '受控输入'},
                    {'component': 'retained', 'children': [{'component': 'control', 'target': 'editor'}]}]}}


class ActualInputTests(unittest.TestCase):
    def test_semantically_equivalent_retained_slots_and_layout_are_accepted_without_mutating_raw(self):
        fixture, actual = declarations(), observations()
        before = deepcopy(actual)
        native_actual_inputs(fixture, actual)
        self.assertEqual(actual, before)

    def test_flex_default_stretch_and_explicit_start_remain_distinct(self):
        fixture, actual = declarations(), observations()
        fixture['props']['composition']['props'].pop('align')
        with self.assertRaisesRegex(ProtocolError, 'composition'):
            native_actual_inputs(fixture, actual)
        actual['composition']['host']['align'] = 'stretch'
        native_actual_inputs(fixture, actual)

    def test_wrong_props_data_slots_theme_and_composition_each_fail(self):
        fixture = declarations()
        mutations = [lambda a: a['controls']['editor'].update(status='error'),
            lambda a: a['controls']['editor'].update(variant='filled'),
            lambda a: a['controls']['editor'].update(width=261),
            lambda a: a['controls']['editor'].update(maxLength=31),
            lambda a: a['data']['editor'].update(value='wrong'),
            lambda a: a['slots']['editor'].update(prefix=['后']),
            lambda a: a['slots']['editor'].update(suffix=['3']),
            lambda a: a['themes']['editor'].update(algorithms=['dark']),
            lambda a: a['themes']['editor'].update(colorBgContainer='#000000'),
            lambda a: a['composition']['roots'].reverse(),
            lambda a: a['composition']['host'].update(gap=13)]
        for index, mutate in enumerate(mutations):
            actual = observations()
            mutate(actual)
            with self.subTest(fault=index), self.assertRaises(ProtocolError):
                native_actual_inputs(fixture, actual)

    def test_fixture_echo_and_missing_raw_query_are_not_consumption_evidence(self):
        with self.assertRaises(ProtocolError):
            native_actual_inputs(declarations(), declarations())
        actual = observations()
        actual.pop('composition')
        with self.assertRaises(ProtocolError):
            native_actual_inputs(declarations(), actual)

    def test_local_theme_algorithm_color_and_actual_resolved_height_are_required(self):
        local = {'components': {'Select': {'algorithm': True, 'colorPrimary': '#722ed1', 'optionHeight': 36}}}
        actual = {'source': 'retained ThemeScope applied config and resolved snapshot', 'algorithms': ['default'],
                  'colorBgContainer': '#ffffff', 'Select': {'algorithm': True, 'colorPrimary': '#722ed1', 'optionHeight': 36},
                  'select_option_height': 36}
        verify_theme(declarations(), 'select', local, actual)
        for key, wrong in [('algorithm', False), ('colorPrimary', '#1677ff'), ('optionHeight', 32)]:
            broken = deepcopy(actual)
            broken['Select'][key] = wrong
            with self.assertRaises(ProtocolError):
                verify_theme(declarations(), 'select', local, broken)
        actual['select_option_height'] = 32
        with self.assertRaises(ProtocolError):
            verify_theme(declarations(), 'select', local, actual)

    def test_chinese_numeric_group_keys_order_and_disabled_are_preserved(self):
        web = [{'title': 'priority', 'label': '优先级', 'options': [
            {'value': 42, 'label': '优先级 42'}, {'value': 99, 'label': '停用', 'disabled': True}]}]
        native = [{'group': True, 'key': 'priority', 'label': '优先级', 'value': '', 'disabled': True},
            {'group': False, 'key': '', 'value': 42, 'label': '优先级 42', 'disabled': False},
            {'group': False, 'key': '', 'value': 99, 'label': '停用', 'disabled': True}]
        self.assertEqual(web_options(web), native_options(native))
        for field, value in [('value', '42'), ('label', 'Priority 42'), ('disabled', True)]:
            broken = deepcopy(native)
            broken[1][field] = value
            self.assertNotEqual(web_options(web), native_options(broken))
        self.assertNotEqual(web_options(web), native_options(native[::-1]))
        broken = deepcopy(native)
        broken[0]['disabled'] = False
        with self.assertRaisesRegex(ProtocolError, 'heading'):
            native_options(broken)
        unsupported = deepcopy(web)
        unsupported[0]['disabled'] = True
        with self.assertRaisesRegex(ProtocolError, 'disabled-group'):
            web_options(unsupported)

    def test_unobserved_deferred_child_fails_with_specific_gap(self):
        fixture, actual = declarations(), observations()
        fixture['props']['controls']['popup-note'] = {'component': 'Input', 'placeholder': 'popup 内独立草稿'}
        actual['unobserved'] = [{'target': 'popup-note', 'reason': 'deferred slot has not mounted'}]
        with self.assertRaisesRegex(ProtocolError, 'popup-note: deferred slot has not mounted'):
            native_actual_inputs(fixture, actual)


if __name__ == '__main__':
    unittest.main()
