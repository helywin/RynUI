"""Actual Chromium font-selection contracts. Parent execution only; no native verdict.

Prepare the locked Web toolchain and the seven-font chain first. Missing tools
fail rather than skip. This suite must not be included in CPU-only unittest runs.
"""
from copy import deepcopy
import json
from pathlib import Path
import sys
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_reference_web import reference, bootstrap
import demo_differential_web_adapter_tests as contract

OUT = ROOT / 'out/demo-differential/web-font-tests' / uuid.uuid4().hex


class WebFontSelectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        bootstrap.verify_prepared(contract.PREPARED)
        OUT.mkdir(parents=True)

    def request(self, *, strong_text='Ab\u4e2d'):
        request = contract.request(controls={'regular': {'component': 'Button'}},
                                   slots={'regular': {'content': 'Ab\u4e2d\U0001f642'}})
        fixture = request['definition']['fixture']
        fixture['theme']['token'] = {'fontWeightStrong': 600}
        fixture['container']['layout'] = 'composition-v1'
        fixture['props']['composition'] = {'component': 'Flex', 'props': {'vertical': True, 'gap': 8}, 'children': [
            {'component': 'control', 'target': 'regular'},
            {'component': 'Typography.Text', 'props': {'strong': True}, 'content': strong_text}]}
        for item in request['definition']['official']['adaptation']:
            item['value'] = deepcopy(fixture[item['field']])
        return request

    def execute(self, request):
        output = OUT / uuid.uuid4().hex
        reference.execute(request, contract.PREPARED, output)
        return reference.validate_observations(output, request)

    def assert_font_failure(self, request):
        output = OUT / uuid.uuid4().hex
        with self.assertRaises(bootstrap.PreparationError):
            reference.execute(request, contract.PREPARED, output)
        error = json.loads((output / 'error.json').read_text())
        self.assertRegex(json.dumps(error), 'font source/fallback|actual weight|selected-font evidence')

    def test_real400_600_and_emoji400_selected_sources_match_explicit_bytes(self):
        request = self.request()
        records = self.execute(request)
        checkpoint = next(record for record in records if record['type'] == 'checkpoint')
        sample = next(target for target in checkpoint['targets'] if target['identity'] == 'sample')
        sources = [source for run in sample['tokens']['font_runs'] for source in run['sources']]
        self.assertTrue({('noto-sans', 400), ('noto-sans-cjk-sc', 400), ('noto-emoji', 400),
                         ('noto-sans', 600), ('noto-sans-cjk-sc', 600)} <=
                        {(source['identity'], source['weight']) for source in sources})
        explicit = {(file['identity'], file['weight']): file for file in request['font_files']}
        for source in sources:
            file = explicit[source['identity'], source['weight']]
            self.assertEqual((source['sha256'], source['face_index']), (file['sha256'], file['face_index']))
            self.assertTrue(source['isCustomFont'])
            self.assertGreater(source['glyphCount'], 0)

    def test_regular_files_do_not_satisfy_actual600_selection(self):
        request = self.request()
        request['font_files'] = [file for file in request['font_files'] if file['weight'] == 400]
        request['definition']['fixture']['fonts'] = [{key: file[key] for key in ('identity', 'sha256', 'face_index', 'weight')}
                                                    for file in request['font_files']]
        self.assert_font_failure(request)

    def medium_request(self):
        request = self.request()
        # Test-only consumption probe; production Theme weights are unchanged.
        fixture = request['definition']['fixture']
        fixture['theme']['token']['fontWeightStrong'] = 500
        for item in request['definition']['official']['adaptation']:
            item['value'] = deepcopy(fixture[item['field']])
        return request

    def test_real500_selected_from_actual_text_nodes(self):
        request = self.medium_request()
        checkpoint = next(record for record in self.execute(request) if record['type'] == 'checkpoint')
        sample = next(target for target in checkpoint['targets'] if target['identity'] == 'sample')
        sources = [source for run in sample['tokens']['font_runs'] for source in run['sources']]
        self.assertTrue({('noto-sans', 500), ('noto-sans-cjk-sc', 500)} <=
                        {(source['identity'], source['weight']) for source in sources})
        explicit = {(file['identity'], file['weight']): file for file in request['font_files']}
        for source in sources:
            file = explicit[source['identity'], source['weight']]
            self.assertEqual((source['sha256'], source['face_index']), (file['sha256'], file['face_index']))
            self.assertTrue(source['isCustomFont'])
            self.assertGreater(source['glyphCount'], 0)

    def test_missing500_cannot_use_regular_semibold_or_system(self):
        request = self.medium_request()
        request['font_files'] = [file for file in request['font_files'] if file['weight'] != 500]
        request['definition']['fixture']['fonts'] = [{key: file[key] for key in ('identity', 'sha256', 'face_index', 'weight')}
                                                    for file in request['font_files']]
        self.assert_font_failure(request)

    def test_missing_emoji_cannot_be_completed_by_system_fallback(self):
        request = self.request()
        request['font_files'] = [file for file in request['font_files'] if file['identity'] != 'noto-emoji']
        request['definition']['fixture']['fonts'] = [{key: file[key] for key in ('identity', 'sha256', 'face_index', 'weight')}
                                                    for file in request['font_files']]
        self.assert_font_failure(request)

    def test_emoji400_does_not_claim_actual600_consumption(self):
        self.assert_font_failure(self.request(strong_text='Ab\u4e2d\U0001f642'))


if __name__ == '__main__':
    unittest.main()
