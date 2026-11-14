"""Analytic local pixel/geometry/profile controls, not platform calibration."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_differential.comparison import LEGACY_DIAGNOSTIC_CAPS as CAPS, compare_checkpoint, profile_thresholds
from demo_differential.adapters import RunError
from demo_differential.protocol import ProtocolError


def area(x=10, y=10, width=40, height=30):
    return dict(x=x, y=y, width=width, height=height)


def pixels(ink):
    return b''.join(bytes((0, 0, 0, 255) if (x, y) in ink else (255, 255, 255, 255))
                    for y in range(100) for x in range(200))


def synthetic_glyph():
    # Explicit CPU mechanism metadata, never a real native/DOM observation.
    return {'semantic_groups': [{'byte_begin': 0, 'byte_end': 1, 'line': 0, 'advance': 3,
                                'boxes': [area(20,20,3,5)]}],
            'ink_instances': [{'byte_begin': 0, 'byte_end': 1, 'line': 0,
                              'color': [0,0,0,1], 'opacity': 1, 'rotation_degrees': 0,
                              'translation': [0,0], 'clip': [20,20,23,25], 'bounds': area(20,20,3,5)}]}


def controls():
    font = {'identity': 'control-font', 'sha256': 'a' * 64, 'face_index': 0, 'weight': 400}
    case = {'native': {'sample_id': 'sample'}, 'fixture': {'viewport': {'dpr': 1}, 'fonts': [font],
            'theme': {'token': {'colorBgContainer': '#ffffff'}}, 'container': {'width': 40, 'padding': 0}}}
    declaration = {'step': 'initial', 'targets': ['sample'], 'rois': [
        {'identity': 'context', 'kind': 'sample-context', 'target': 'sample', 'guard_px': 2},
        {'identity': 'guard', 'kind': 'guard-band', 'target': 'sample', 'guard_px': 2}]}
    target = {'identity': 'sample', 'bounds': area(), 'clip': area(0, 0, 200, 100), 'state': {},
              'tokens': {'backgroundColor': 'rgb(255, 255, 255)'}, 'icons': [], 'text': []}
    native = {'targets': [deepcopy(target)], 'provenance': {}}
    web = {'targets': [deepcopy(target)], 'provenance': {'actual_font_sources': [
        {'sha256': 'a' * 64, 'face_index': 0, 'names': {'6': ['ControlFont']}}]}}
    for observed,source in ((native,'retained-host-layout'),(web,'fixed-web-wrapper')):
        observed['targets'][0]['tokens']['sample_anchor'] = {'source': source, 'container': area(), 'content': area(),
                                                            'padding': dict(left=0,top=0,right=0,bottom=0)}
    ink = {(x, y) for x in range(20, 23) for y in range(20, 25)}
    return case, declaration, native, web, ink


class ComparisonTests(unittest.TestCase):
    def compare(self, values, ink_native=None):
        case, declaration, native, web, ink = values
        images = [(pixels(ink), (200, 100)), (pixels(ink if ink_native is None else ink_native), (200, 100))]
        return compare_checkpoint(case, declaration, native, web, images, {key: 0 for key in CAPS},
                                  {'profile': 'sRGB-SDR', 'channel_order': 'RGBA', 'alpha': 'opaque'})

    def test_correct_analytic_context_and_one_logical_pixel_offset(self):
        values = controls()
        self.assertEqual(self.compare(values)[0], [])
        values[2]['targets'][0]['bounds']['width'] += 1
        self.assertIn('layout', {f['category'] for f in self.compare(values)[0]})

    def test_fixed_anchor_rejects_all_content_shift_and_accepts_shell_origin(self):
        values=controls()
        values[2]['targets'][0]['bounds']['x']+=1
        self.assertIn('layout',{f['category'] for f in self.compare(values)[0]})
        values=controls()
        target=values[2]['targets'][0]
        target['bounds']['x']+=50
        for key in ('container','content'):
            target['tokens']['sample_anchor'][key]['x']+=50
        moved={(x+50,y) for x,y in values[4]}
        self.assertEqual(self.compare(values,moved)[0],[])

    def test_anchor_is_required_not_derived_from_content(self):
        values=controls()
        values[2]['targets'][0]['tokens'].pop('sample_anchor')
        with self.assertRaises(ProtocolError):
            self.compare(values)

    def test_both_glyphs_absent_with_full_frame_border_is_red(self):
        values=controls()
        n,w=values[2]['targets'][0],values[3]['targets'][0]
        n['text']=[dict(content='X',bounds=area(),**synthetic_glyph(),font_coverage_available=True,
            missing_font_scalars=[],replacement_count=0,font_runs=[{'font':{
                'sha256':'a'*64,'face_index':0,'logical_pixel_size':14,'bold':False,'italic':False,'source':'pinned-file'}}])]
        w['text']=[dict(content='X',boxes=[area(20,20,3,5)],**synthetic_glyph())]
        w['tokens']['platform_fonts']=[{'postScriptName':'ControlFont','isCustomFont':True}]
        values[4].clear()
        values[4].update({(x,y) for x in range(10,50) for y in (10,39)} |
                         {(x,y) for y in range(10,40) for x in (10,49)})
        self.assertTrue(any(f['category']=='typography' and 'missing' in f['message'] for f in self.compare(values)[0]))

    def test_button_input_token_units_keep_both_padding_sides(self):
        for component in ('Button', 'Input'):
            values = controls()
            case, declaration, native, web = values[:4]
            case['fixture']['props'] = {'controls': {'control': {'component': component}}}
            declaration['targets'].append('control')
            for observed in (native, web):
                row = deepcopy(observed['targets'][0])
                row['identity'] = 'control'
                row['state'] = {'disabled': False, 'loading': False, 'value': ''}
                observed['targets'].append(row)
            native['targets'][1]['tokens'].update(border_width=1, border_radius=6, font_size=14, padding_inline=11)
            web['targets'][1]['tokens'].update(borderWidth='1px', borderRadius='6px', fontSize='14px',
                                              paddingInlineStart='11px', paddingInlineEnd='11px')
            self.assertEqual(self.compare(values)[0], [])
            web['targets'][1]['tokens']['paddingInlineEnd'] = '12px'
            self.assertTrue(any(row['category'] == 'token' and row.get('token') == 'padding_inline'
                                for row in self.compare(values)[0]))

    def test_local_missing_icon_and_double_background_cannot_pass(self):
        values = controls()
        icon = area(20, 20, 3, 5)
        values[2]['targets'][0]['icons'] = [{'identity': 'SearchOutlined', 'bounds': icon}]
        values[3]['targets'][0]['icons'] = [{'identity': 'search', 'bounds': icon}]
        self.assertEqual(self.compare(values)[0], [])
        failures, _ = self.compare(values, set())
        self.assertTrue(any(f['target'] == 'sample.icon' and 'missing' in f['message'] for f in failures))
        values[4].clear()
        self.assertTrue(self.compare(values)[0])

    def test_actual_text_local_ink_font_and_required_bounds(self):
        values = controls()
        native, web = values[2]['targets'][0], values[3]['targets'][0]
        native['text'] = [{'content': 'X', **synthetic_glyph(), 'bounds': area(20, 20, 3, 5), 'font_coverage_available': True,
            'missing_font_scalars': [], 'replacement_count': 0, 'font_runs': [{'font': {
            'sha256': 'a' * 64, 'face_index': 0, 'logical_pixel_size': 14, 'bold': False, 'italic': False, 'source': 'pinned-file'}}]}]
        web['text'] = [{'content': 'X', **synthetic_glyph(), 'boxes': [area(20, 20, 3, 5)]}]
        web['tokens']['platform_fonts'] = [{'postScriptName': 'ControlFont', 'isCustomFont': True}]
        self.assertEqual(self.compare(values)[0], [])
        self.assertTrue(any(f['category'] == 'typography' for f in self.compare(values, set())[0]))
        native['text'][0]['font_runs'][0]['font']['sha256'] = 'b' * 64
        with self.assertRaises(RunError) as error:
            self.compare(values)
        self.assertEqual(error.exception.code, 3)
        native['text'][0]['font_runs'][0]['font']['sha256'] = 'a' * 64
        web['text'][0].pop('semantic_groups')
        with self.assertRaises(ProtocolError):
            self.compare(values)

    def test_pinned_normal_text_rejects_system_unknown_and_bundled_fallback(self):
        values = controls()
        native, web = values[2]['targets'][0], values[3]['targets'][0]
        font = {'sha256': 'a' * 64, 'face_index': 0, 'logical_pixel_size': 14, 'bold': False, 'italic': False}
        native['text'] = [{'content': 'X', **synthetic_glyph(), 'bounds': area(20, 20, 3, 5), 'font_coverage_available': True,
            'missing_font_scalars': [], 'replacement_count': 0, 'font_runs': [{'font': font}]}]
        web['text'] = [{'content': 'X', **synthetic_glyph(), 'boxes': [area(20, 20, 3, 5)]}]
        web['tokens']['platform_fonts'] = [{'postScriptName': 'ControlFont', 'isCustomFont': True}]
        for source in ('system-file', 'bundled-fallback', 'unknown', 'bundled-icon', None):
            font['source'] = source
            with self.subTest(source=source), self.assertRaises(RunError):
                self.compare(values)
        font['source'] = 'pinned-file'
        self.assertEqual(self.compare(values)[0], [])

    def test_guard_leakage_and_uncaptured_guard_are_red(self):
        values = controls()
        leaked = values[4] | {(9, 20)}
        self.assertTrue(self.compare(values, leaked)[0])
        values[2]['targets'][0]['bounds']['x'] = 0
        with self.assertRaises(ProtocolError):
            self.compare(values)

    def test_both_sides_missing_declared_text_still_fail(self):
        values = controls()
        values[0]['fixture']['props'] = {'composition': {'component': 'Typography.Text', 'content': 'Required text'}}
        failures, _ = self.compare(values)
        self.assertTrue(any(f['category'] == 'typography' and f.get('expected') == 'Required text' for f in failures))

    def test_colored_material_and_white_guard_do_not_count_as_glyph_ink(self):
        values = controls()
        native, web = values[2]['targets'][0], values[3]['targets'][0]
        native['text'] = [{'content': 'X', **synthetic_glyph(), 'bounds': area(20, 20, 3, 5), 'font_coverage_available': True,
            'missing_font_scalars': [], 'replacement_count': 0, 'font_runs': [{'font': {
            'sha256': 'a' * 64, 'face_index': 0, 'logical_pixel_size': 14, 'bold': False, 'italic': False, 'source': 'pinned-file'}}]}]
        web['text'] = [{'content': 'X', **synthetic_glyph(), 'boxes': [area(20, 20, 3, 5)]}]
        web['tokens'].update(backgroundColor='rgb(22, 119, 255)', platform_fonts=[{'postScriptName': 'ControlFont', 'isCustomFont': True}])
        # A material-colored run rectangle surrounded by white guards has no
        # glyph ink, even though both images and all font/content metadata agree.
        data = bytearray(pixels(set()))
        for y in range(20, 25):
            for x in range(20, 23):
                data[(y * 200 + x) * 4:(y * 200 + x) * 4 + 4] = bytes((22, 119, 255, 255))
        failures, _ = compare_checkpoint(*values[:4], [(bytes(data), (200, 100))] * 2,
                                         {key: 0 for key in CAPS}, {})
        self.assertTrue(any(f['category'] == 'typography' and 'missing' in f['message'] for f in failures))

    def test_profile_requires_real_binding_calibration_empty_masks_and_negative_margin(self):
        binding = {'algorithm_hash': 'a' * 64, 'environment_hash': 'b' * 64, 'source_hash': 'c' * 64,
                   'font_hashes': ['d' * 64], 'font_policy': 'pinned'}
        profile = {'schema_version': 1, 'identity': 'control', **binding, 'corpus_hash': 'e' * 64,
            'repetitions': 5, 'calibrated': True, 'masks': [],
            'color': {'profile': 'sRGB-SDR', 'channel_order': 'RGBA', 'alpha': 'opaque'},
            'metrics': [{'identity': name, 'threshold': 0, 'hard_cap': cap, 'positive_max': 0,
                         'quantization_floor': 0, 'margin': 0, 'negative_min': 1} for name, cap in CAPS.items()]}
        with self.assertRaises(RunError) as rejected:
            profile_thresholds(profile, binding, 'control')
        self.assertEqual(rejected.exception.code,3)
        for fault in ('uncalibrated', 'source', 'threshold', 'negative', 'missing', 'mask'):
            candidate = deepcopy(profile)
            if fault == 'uncalibrated':
                candidate['calibrated'] = False
            elif fault == 'source':
                candidate['source_hash'] = '0' * 64
            elif fault == 'threshold':
                candidate['metrics'][0]['threshold'] = 1
            elif fault == 'negative':
                candidate['metrics'][0]['negative_min'] = 0
            elif fault == 'missing':
                candidate['metrics'].pop()
            else:
                candidate['masks'] = [{'identity': 'hides-ink', 'reason': 'unit negative', 'area_fraction': .01,
                    'rect': area(), 'protected_intersection': False}]
            with self.subTest(fault=fault), self.assertRaises(ProtocolError):
                profile_thresholds(candidate, binding, 'control')


if __name__ == '__main__':
    unittest.main()
