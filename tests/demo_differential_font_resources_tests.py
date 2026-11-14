"""Real font-file/outline admission and worker-helper controls; no browser/native."""
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_reference_web import reference
from demo_reference_web import font_resources as resources
from demo_reference_web.bootstrap import PreparationError, fetch, sha
from demo_reference_web.font_metadata import font_metadata
from demo_differential.font_requirements import preflight, required_faces
from demo_differential.adapters import RunError

FONT_FILE = ROOT / os.environ.get('RYNUI_WEB_FONTS', 'out/demo-differential/fonts/font-files.json')
CACHE = ROOT / 'out/demo-differential/archives'


def expected(files):
    return [{key: row[key] for key in ('identity', 'sha256', 'face_index', 'weight')} for row in files]


class FontResourcesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.files = json.loads(FONT_FILE.read_text())
        cls.rows = resources.central_fonts()
        cls.library = resources.fonttools_library(CACHE, 'BUNDLED', offline=True)
        sys.path.insert(0, str(cls.library))
        from fontTools.ttLib import TTFont
        from fontTools.pens.recordingPen import RecordingPen
        cls.TTFont = TTFont
        cls.RecordingPen = RecordingPen

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / 'out', prefix='actual-font-unit-')
        self.addCleanup(self.temp.cleanup)
        self.out = Path(self.temp.name)

    def case(self, files=None, text=''):
        files = self.files if files is None else files
        return {'fixture': {'font_policy': 'pinned', 'fonts': expected(files), 'data': {'value': text}}}

    def test_real_files_all_match_central_face_weight_slant_hash(self):
        self.assertEqual(expected(self.files), expected(self.rows))
        for row, file in zip(self.rows, self.files):
            actual = resources.admit_font(expected([file])[0], file, 'pinned')
            self.assertEqual(actual['names']['6'], [row['postscript_name']])
            self.assertEqual(actual['weight'], row['weight'])
            self.assertEqual(actual['slant'], 'upright')
            self.assertFalse(actual['variable'])
        preflight(self.case(text='abc\U0001f642'), self.files)

    def test_regular_is_not_500_or_600_even_with_unchanged_real_file_and_hash(self):
        for weight in (500, 600):
            for file in self.files[:2]:
                file = file | {'weight': weight}
                with self.subTest(weight=weight), self.assertRaisesRegex(PreparationError, 'weight/slant'):
                    resources.admit_font(expected([file])[0], file, 'pinned')

    def test_medium_is_not_400_or_600_and_semibold_is_not_500(self):
        for file in self.files:
            for weight in ((400, 600) if file['weight'] == 500 else (500,) if file['weight'] == 600 else ()):
                changed = file | {'weight': weight}
                with self.subTest(identity=file['identity'], weight=weight), self.assertRaisesRegex(PreparationError, 'weight/slant'):
                    resources.admit_font(expected([changed])[0], changed, 'pinned')

    def test_wrong_hash_face_and_identity_are_rejected(self):
        for change in ({'sha256': '0' * 64}, {'face_index': 1}, {'identity': 'other-font'}):
            file = self.files[0] | change
            with self.subTest(change=change), self.assertRaises(ValueError):
                resources.admit_font(expected([file])[0], file, 'pinned')

    def test_actual_slant_rejected_after_rehash(self):
        data = bytearray(Path(self.files[0]['path']).read_bytes())
        count, = struct.unpack_from('>H', data, 4)
        for index in range(count):
            tag, _, start, _ = struct.unpack_from('>4sIII', data, 12 + 16 * index)
            if tag == b'OS/2':
                flags, = struct.unpack_from('>H', data, start + 62)
                struct.pack_into('>H', data, start + 62, flags | 1)
        path = self.out / 'italic.ttf'
        path.write_bytes(data)
        file = self.files[0] | {'path': str(path), 'sha256': sha(path)}
        with self.assertRaisesRegex(PreparationError, 'weight/slant'):
            resources.admit_font(expected([file])[0], file, 'system')

    def test_actual_postscript_name_checked_independently_of_hash_lock(self):
        path = self.out / 'other-name.ttf'
        data = Path(self.files[0]['path']).read_bytes()
        for encoding in ('ascii', 'utf-16-be'):
            data = data.replace('NotoSans-Regular'.encode(encoding), 'FakeSans-Regular'.encode(encoding))
        path.write_bytes(data)
        file = self.files[0] | {'path': str(path), 'sha256': sha(path)}
        row = self.rows[0] | {'sha256': file['sha256']}
        with patch.object(resources, 'central_fonts', return_value=[row]):
            with self.assertRaisesRegex(PreparationError, 'PostScript face'):
                resources.admit_font(expected([file])[0], file, 'pinned')

    def test_missing_500_600_and_emoji_are_precise_environment_errors(self):
        for files, missing in (([f for f in self.files if f['weight'] != 500], [('noto-sans', 500), ('noto-sans-cjk-sc', 500)]),
                               ([f for f in self.files if f['weight'] != 600], [('noto-sans', 600), ('noto-sans-cjk-sc', 600)]),
                               ([f for f in self.files if f['identity'] != 'noto-emoji'], [('noto-emoji', 400)])):
            with self.assertRaises(RunError) as error:
                preflight(self.case(files, 'abc\U0001f642'), files)
            self.assertEqual(error.exception.code, 3)
            self.assertEqual([(f['identity'], f['weight']) for f in error.exception.details['missing_font_faces']], missing)
            self.assertFalse(error.exception.details['implicit_fallback'])

    def test_duplicate_face_weight_is_rejected(self):
        files = self.files + [self.files[-1]]
        with self.assertRaisesRegex(RunError, 'duplicate explicit font'):
            preflight(self.case(files), files)

    def test_only_finite_declared_weights_can_be_prepared(self):
        for weights in ((), (700,), (400, 550, 600), (500, 500), (500.0,), (True,)):
            with self.subTest(weights=weights), self.assertRaises(PreparationError):
                resources.prepare_fonts(self.out / 'invalid', weights=weights, offline=True)

    def test_seven_files_preserve_the_original_five_bytes(self):
        self.assertEqual([f['weight'] for f in self.files], [400, 400, 400, 600, 600, 500, 500])
        self.assertEqual([f['sha256'] for f in self.files[:5]], [
            'b85c38ecea8a7cfb39c24e395a4007474fa5a4fc864f6ee33309eb4948d232d5',
            '2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b',
            '1ada997ccb67bf2404828e25528e5257d6db10c80287e77684abf6737a5d198f',
            '87a8b90ece1e89746b544e4e086f85a3710e41485a8078f9be874837dfad45d5',
            '4137d864b16d2670304ca58d1f884e20a2656d19230b8f497e0ce53584fa3d2a'])

    def test_explicit_source_cannot_be_completed_by_system(self):
        missing = self.files[0] | {'path': str(self.out / 'absent.ttf')}
        with self.assertRaisesRegex(PreparationError, 'explicit font bytes'):
            resources.admit_font(expected([missing])[0], missing, 'system')
        case = self.case()
        with self.assertRaises(RunError):
            preflight(case, self.files[1:])

    def test_emoji400_is_not_claimed_as_a600_face(self):
        requirements = required_faces(self.case(text='abc\U0001f642'))
        self.assertEqual([r['weight'] for r in requirements if r['identity'] == 'noto-emoji'], [400])
        self.assertFalse(any(f['identity'] == 'noto-emoji' and f['weight'] == 600 for f in self.files))

    def test_real_unicode_cmap_and_monochrome_nominal_emoji(self):
        for file in self.files[:2]:
            self.assertEqual(font_metadata(file['path'], 0, [0x1f642])['nominal_glyphs'][str(0x1f642)], 0)
        file = next(f for f in self.files if f['identity'] == 'noto-emoji')
        metadata = font_metadata(file['path'], 0, [0x1f642])
        self.assertEqual(metadata['nominal_glyphs'][str(0x1f642)], 961)
        with self.TTFont(file['path']) as font:
            glyph = font.getBestCmap()[0x1f642]
            self.assertEqual(font.getGlyphID(glyph), 961)
            self.assertEqual(font['glyf'][glyph].numberOfContours, 5)
            self.assertNotEqual(glyph, font.getBestCmap().get(0xfffd))
            self.assertTrue(set(font.keys()).isdisjoint({'COLR', 'CPAL', 'CBDT', 'CBLC', 'sbix', 'SVG '}))
            self.assertTrue(set(font.keys()).isdisjoint({'fvar', 'gvar', 'HVAR', 'VVAR'}))

    def test_cjk500_600_outlines_match_variable_and_differ_from_other_weights(self):
        for weight in (500, 600):
            with self.subTest(weight=weight):
                self.check_cjk_outlines(weight)

    def check_cjk_outlines(self, weight):
        row = next(r for r in self.rows if r['identity'] == 'noto-sans-cjk-sc' and r['weight'] == weight)
        source = fetch({'url': row['url'], 'sha256': row['source_sha256']}, CACHE, True)
        file = next(f for f in self.files if f['identity'] == row['identity'] and f['weight'] == weight)
        with self.TTFont(source) as variable, self.TTFont(file['path']) as static:
            for scalar in (ord('A'), 0x4e2d, 0x6587):
                glyph = variable.getBestCmap()[scalar]
                def outline(font, weight=None):
                    pen = self.RecordingPen()
                    font.getGlyphSet(location={'wght': weight} if weight else None)[glyph].draw(pen)
                    return pen.value
                regular = outline(variable, 400)
                desired_outline = outline(variable, weight)
                actual = outline(static)
                self.assertNotEqual(regular, desired_outline)
                self.assertNotEqual(outline(variable, 600 if weight == 500 else 500), desired_outline)
                self.assertEqual([op for op, _ in desired_outline], [op for op, _ in actual])
                for (_, desired), (_, observed) in zip(desired_outline, actual):
                    self.assertEqual(len(desired), len(observed))
                    for a, b in zip(desired, observed):
                        if a is not None:
                            self.assertTrue(all(abs(x - y) <= 1 for x, y in zip(a, b)))

    def test_latin500_is_exact_official_source_with_distinct_outlines(self):
        rows = {f['weight']: f for f in self.files if f['identity'] == 'noto-sans'}
        source = next(r for r in self.rows if r['identity'] == 'noto-sans' and r['weight'] == 500)
        archive = fetch({'url': source['url'], 'sha256': source['source_sha256']}, CACHE, True)
        self.assertEqual(archive.read_bytes(), Path(rows[500]['path']).read_bytes())
        for scalar in (ord('A'), ord('m')):
            outlines = []
            for weight in (400, 500, 600):
                with self.TTFont(rows[weight]['path']) as font:
                    pen = self.RecordingPen()
                    font.getGlyphSet()[font.getBestCmap()[scalar]].draw(pen)
                    outlines.append(pen.value)
            self.assertNotEqual(outlines[1], outlines[0])
            self.assertNotEqual(outlines[1], outlines[2])

    def test_tool_modes_are_explicit_and_source_verified(self):
        self.assertEqual(resources.fonttools_library(CACHE, 'SYSTEM', self.library, True), self.library)
        for mode, library in (('AUTO', None), ('SYSTEM', None), ('BUNDLED', self.library)):
            with self.assertRaises(PreparationError):
                resources.fonttools_library(CACHE, mode, library, True)
        wrong = self.out / 'tool/Lib'
        wrong.mkdir(parents=True)
        with self.assertRaises(PreparationError):
            resources.fonttools_library(CACHE, 'SYSTEM', wrong, True)

    def test_only_requested_weights_downloaded_and_no_product_dependency(self):
        # Static 400 preparation requires no instance tool. An unavailable tool
        # must not affect normal products or this explicitly limited request.
        with patch.object(resources, 'fonttools_library', side_effect=AssertionError('unrequested tool')):
            files = resources.prepare_fonts(self.out / 'regular-only', weights=(400,), emoji=False, offline=True)
        self.assertEqual([(f['identity'], f['weight']) for f in files], [('noto-sans', 400), ('noto-sans-cjk-sc', 400)])
        for path in (ROOT / 'cmake').rglob('*.cmake'):
            if path.name != 'RynUIDependencyLock.cmake':
                self.assertNotIn('RYNUI_NOTO_EMOJI', path.read_text())
                self.assertNotIn('RYNUI_NOTO_SANS_CJK_SC_VARIABLE', path.read_text())
        self.assertEqual(reference.central_fonts(), self.rows)

    def test_worker_font_helpers_enforce_selected_weight_and_explicit_source(self):
        module = (ROOT / 'tools/demo_reference_web/font_faces.mjs').as_uri()
        script = '''
import { fontStyle, selectedSources, platformFontRuns } from MODULE;
const sources = SOURCES;
const files = FILES;
const css = fontStyle(files);
if (!css.includes('font-synthesis:none') || !css.includes("ReferenceFont0;src:url('/font/3');font-weight:600")) throw Error('weight-family CSS');
const face = { postScriptName:'NotoSans-SemiBold', isCustomFont:true, glyphCount:2 };
if (selectedSources([face], sources, 600, 'normal')[0].sha256 !== files[3].sha256) throw Error('source association');
for (const [font, weight, slant] of [[face,400,'normal'],[{...face,isCustomFont:false},600,'normal'],[face,600,'italic']]) {
  let failed=false; try { selectedSources([font],sources,weight,slant); } catch { failed=true; }
  if (!failed) throw Error('accepted wrong weight/source/slant');
}
const cdp = { send: async (method, args) => {
  if (method === 'DOM.describeNode') return { node:{ nodeId:1, nodeType:1, children:[
    {nodeId:2,nodeType:1,children:[{nodeId:5,nodeType:3,nodeValue:'Ab'}]},
    {nodeId:8,nodeType:1,children:[{nodeId:9,nodeType:3,nodeValue:'Cd'}]}],
    shadowRoots:[{nodeId:3,nodeType:11,children:[{nodeId:4,nodeType:1,children:[{nodeId:6,nodeType:3,nodeValue:'🙂'}]}]}] } };
  if (method === 'CSS.getComputedStyleForNode') {
    if (![2,4,8].includes(args.nodeId)) throw Error('style was not read from actual text owner');
    return {computedStyle:[{name:'font-weight',value:args.nodeId === 8 ? '600' : '400'},{name:'font-style',value:'normal'}]};
  }
  if (method === 'CSS.getPlatformFontsForNode') {
    if (![5,6,9].includes(args.nodeId)) throw Error('aggregate element fonts mix descendant weights');
    return {fonts:[{...face,postScriptName:args.nodeId === 5 ? 'NotoSans-Regular' : args.nodeId === 6 ? 'NotoEmoji-Regular' : 'NotoSans-SemiBold'}]};
  }
  throw Error('unexpected protocol query');
} };
const runs = await platformFontRuns(cdp, 1, sources, true);
if (runs.length !== 3 || runs[1].sources[0].sha256 !== files[3].sha256 || runs[2].sources[0].sha256 !== files[2].sha256 ||
    runs[1].node_id !== 9 || runs[1].style_node_id !== 8) throw Error('mixed weight/nested/input shadow source mapping');
let failed = false;
try { await platformFontRuns({send:async(method)=>method === 'DOM.describeNode' ? {node:{nodeId:1,nodeType:1}} : {fonts:[]}},1,sources,true); }
catch { failed = true; }
if (!failed) throw Error('accepted empty actual-font evidence');
console.log('font helper controls passed; browser not run');
'''.replace('MODULE', json.dumps(module)).replace('SOURCES', json.dumps([
            resources.admit_font(e, f, 'pinned') for e, f in zip(expected(self.files), self.files)])).replace('FILES', json.dumps(self.files))
        result = subprocess.run(['node', '--input-type=module', '-e', script], capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
