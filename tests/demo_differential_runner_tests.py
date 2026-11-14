"""Injectable adapter fault controls. Never native/browser/GPU pilot evidence."""
from copy import deepcopy
import hashlib
import json
import platform
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_differential import pilots, runner
from demo_differential.adapters import RunError, native_command, process
from demo_differential.protocol import ProtocolError, read, validate
from demo_differential.reporting import write_json
from demo_differential.source_closure import source_manifest
from demo_differential.inventory import source_inventory, obligations
from demo_differential.comparison import digest
from demo_differential.adapters import web_definition

CONTROL_INVENTORY = None
CONTROL_MANIFEST = None


def bmp(size=16):
    pixels = bytes([255, 255, 255, 255]) * size * size
    return b'BM' + struct.pack('<IHHI', 54 + len(pixels), 0, 0, 54) + struct.pack(
        '<IiiHHIIiiII', 40, size, size, 1, 32, 0, len(pixels), 0, 0, 0, 0) + pixels


def control_case():
    case = pilots.button()
    case['fixture']['viewport'] = {'width': 16, 'height': 16, 'dpr': 'native'}
    case['fixture']['slots'] = {}
    case['fixture']['theme'] = {}
    case['actions'] = [pilots.action('send', 'pointer-down', case['native']['targets'][0],
                       {'button': 'primary', 'x_fraction': .5, 'y_fraction': .5}),
                       pilots.action('capture', 'checkpoint', case['native']['sample_id'])]
    case['checkpoints'] = [{**case['checkpoints'][0], 'step': 'capture'}]
    case['oracle'] = [pilots.predicate('count', 'send', stream='callbacks', target=case['native']['targets'][0],
                                      event='activate', equals=0)]
    return validate('case', case)


class AdapterControl:
    """Writes protocol controls, not real rendered components or valid references."""
    def __init__(self, faults=None):
        self.faults = faults or {}
        self.cleaned = False
        self.calls = []

    def inventory(self, binary, output):
        value = deepcopy(CONTROL_INVENTORY)
        write_json(output, value)
        return value

    def prepare(self, directory, options):
        self.calls.append('prepare')
        if self.faults.get('prepare'):
            raise RunError(3, 'environment', 'unit prepare unavailable')
        return {'fonts': deepcopy(pilots.FONTS), 'prepared': str(directory / 'unit-only')}

    def web_preflight(self, definition, nonce, prepared):
        self.calls.append('preflight')

    def font_preflight(self, definition, prepared):
        # Explicit mechanism substitute; no real pinned font/GPU claims.
        pass

    def native(self, definition, nonce, directory, prepared, options):
        return self.emit('native', definition, nonce, directory, options)

    def web(self, definition, nonce, directory, prepared, options):
        return self.emit('web', definition, nonce, directory, options)

    def emit(self, side, case, nonce, directory, options):
        is_probe = directory.name == 'preflight-native'
        fault = '' if is_probe else self.faults.get(case['identity'], self.faults.get(side, ''))
        self.calls.append(side + ('-probe' if is_probe else ''))
        if fault in ('crash', 'timeout'):
            raise RunError(4, 'protocol', 'unit injected ' + fault)
        if fault == 'environment':
            raise RunError(3, 'environment', 'unit environment unavailable')
        directory.mkdir(parents=True)
        metrics = {'dpr': 1, 'logical_width': 16, 'logical_height': 16, 'pixel_width': 16, 'pixel_height': 16}
        provenance = {'source': deepcopy(CONTROL_MANIFEST), 'binary_sha256': hashlib.sha256(Path(options['binary']).read_bytes()).hexdigest(),
            'preset': 'windows-msvc' if platform.system() == 'Windows' else 'linux-gcc', 'configuration': 'Release',
            'toolchain': 'MSVC', 'backend': 'd3d12', 'shader': 'unit-only', 'fault': 'none', 'desktop_fallback': False}
        if side == 'web':
            from demo_reference_web import bootstrap, reference
            resolved = deepcopy(case)
            resolved['fixture']['viewport']['dpr'] = 1
            provenance.update(source_commit=case['official']['commit'], source_tree_sha256=bootstrap.load_lock()['source']['components_tree_sha256'],
                adapter_sha256=case['official']['adapter_sha256'], adapter_closure_sha256=reference.code_hash(),
                fixture_sha256=digest(resolved['fixture']), case_sha256=digest(web_definition(resolved)),
                source_closure={r['path']: r['sha256'] for r in case['official']['source_paths']})
        if fault in ('source', 'source+complete'):
            if side == 'web':
                provenance['source_commit'] = '0' * 40
            else:
                provenance['source']['source_hash'] = '0' * 64
        if fault == 'binary':
            provenance['binary_sha256'] = '0' * 64
        if fault == 'font':
            case = deepcopy(case)
            case['fixture']['fonts'][0]['sha256'] = '0' * 64
        fixture = deepcopy(case['fixture'])
        fixture['viewport']['dpr'] = 1
        effective = {**fixture, 'actual_environment': {'ratio': 1, 'width': 16, 'height': 16}}
        # Mechanism substitute only: the native CORE controls separately prove
        # retained reads rather than fixture echo on the production path.
        from demo_differential_input_controls import actual_input_control
        actual = actual_input_control(case, side)
        if side == 'web':
            effective['actual_initial_inputs'] = deepcopy(actual)
        if fault == 'props':
            actual['controls'][case['native']['targets'][0]]['size'] = 'large'
        records, completed, checked = [], [], []
        def record(kind, step=''):
            from demo_differential_native_resources_tests import lifecycle_control
            current = deepcopy(actual)
            if step:
                current.update(phase='settled', step=step,
                    query_seq=2 + next(i for i, a in enumerate(case['actions']) if a['step'] == step))
            elif kind == 'complete':
                current.update(phase='settled', step=case['actions'][-1]['step'], query_seq=1 + len(case['actions']))
            return {'schema_version': 1, 'type': kind, 'nonce': nonce, 'case': case['identity'], 'step': step,
                'adapter': 'rynui-gallery-native-v1' if side == 'native' else case['official']['adapter_id'],
                'provenance': provenance, 'effective_inputs': effective,
                'receipt': {'dispatched': kind == 'receipt', 'hit': '', 'consumed': False, 'layer': 'observer'},
                'state': {}, 'callbacks': [], 'targets': [], 'captures': [],
                'resources': {'metrics': metrics, 'actual_initial_inputs': deepcopy(actual), 'actual_inputs': current, 'readback': None,
                              'lifecycle': lifecycle_control(), 'compositor_callback': None, 'presented': None,
                              'presentation_evidence': 'not-observed-by-checkpoint-readback'},
                'completed_steps': list(completed), 'completed_checkpoints': list(checked)}
        records.append(record('hello'))
        for action in case['actions']:
            step = action['step']
            receipt = record('receipt', step)
            completed.append(step)
            receipt['completed_steps'] = list(completed)
            if side == 'native':
                records.append(receipt)
            if action['kind'] == 'checkpoint':
                checked.append(step)
                row = record('checkpoint', step)
                names = next(c['targets'] for c in case['checkpoints'] if c['step'] == step)
                row['targets'] = [{'identity': name, 'key': '', 'bounds': {'x': 2, 'y': 2, 'width': 12, 'height': 12},
                    'clip': {'x': 0, 'y': 0, 'width': 16, 'height': 16}, 'scroll': {}, 'state': {},
                    'text': [], 'icons': [], 'tokens': {}, 'owner_generation': 1, 'scene_revision': 1} for name in names]
                filename = f'capture-{len(checked)}.bmp'
                image = directory / filename
                image.write_bytes(bmp())
                capture = {'path': filename, 'sha256': hashlib.sha256(image.read_bytes()).hexdigest(),
                    'nonce': nonce + '-capture-' + str(len(checked)) if side == 'native' else nonce,
                    'checkpoint': step, 'scene_revision': 1, 'width': 16, 'height': 16, 'dpr': 1,
                    'fence_completed': True, 'framebuffer_format': 'unit-only', 'channel_order': 'RGBA',
                    'alpha': 'opaque', 'color_profile': 'unit-only'}
                row['captures'] = [capture]
                if side == 'native':
                    capture['framebuffer_format'] = 'RGBA8_UNORM'
                    row['resources']['readback'] = {'run_nonce': nonce, 'nonce': capture['nonce'], 'checkpoint': step,
                        'readback_id': len(checked), 'fence_id': len(checked), 'scene_revision': 1,
                        'renderer_owner': 1, 'device_epoch': 1, 'metrics': deepcopy(metrics),
                        'source_channel_order': 'RGBA', 'source_hash': provenance['source']['source_hash'],
                        'binary_sha256': provenance['binary_sha256']}
                row['resources']['paint_receipt'] = {'nonce': nonce, 'step': step, 'sequence': len(checked),
                    'sha256': capture['sha256'], 'fresh_readback': True}
                if fault == 'image':
                    image.unlink()
                elif fault == 'frame':
                    capture['scene_revision'] = 0
                elif fault == 'dpr':
                    capture['dpr'] = 2
                elif fault == 'path':
                    capture['path'] = '../escape.bmp'
                elif fault == 'nonce':
                    row['nonce'] = 'old'
                elif fault == 'capture':
                    row['captures'] = []
                records.append(row)
            if side == 'web':
                records.append(receipt)
        records.append(record('complete'))
        if fault in ('complete', 'source+complete'):
            records.pop()
        elif fault == 'receipt':
            records = [r for r in records if r['type'] != 'receipt']
        elif fault == 'dispatched':
            next(r for r in records if r['type'] == 'receipt')['receipt']['dispatched'] = False
        path = directory / 'observations.jsonl'
        path.write_text(''.join(json.dumps(r) + '\n' for r in records), encoding='utf-8')
        return path

    def cleanup(self):
        self.cleaned = True


class RunnerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        global CONTROL_INVENTORY, CONTROL_MANIFEST
        CONTROL_INVENTORY = source_inventory()
        CONTROL_MANIFEST = source_manifest(ROOT)

    def setUp(self):
        (ROOT / 'out').mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / 'out', prefix='runner-unit-')
        self.addCleanup(self.temp.cleanup)
        for name, value in (('source_inventory', CONTROL_INVENTORY), ('source_manifest', CONTROL_MANIFEST)):
            mock = patch.object(runner, name, return_value=value)
            mock.start()
            self.addCleanup(mock.stop)
        self.directory = Path(self.temp.name)
        self.binary = self.directory / 'unit-binary'
        self.binary.write_bytes(b'unit-only; not a native executable')
        self.profile = self.directory / 'profile.json'
        write_json(self.profile, {'schema_version': 1, 'identity': 'pilot-pinned-font', 'environment_hash': 'a' * 64,
            'algorithm_hash': 'a' * 64, 'corpus_hash': 'b' * 64, 'source_hash': 'c' * 64,
            'font_hashes': [f['sha256'] for f in pilots.FONTS], 'repetitions': 5, 'calibrated': True,
            'metrics': [{'identity': 'unit-only', 'threshold': 0, 'hard_cap': 0, 'quantization_floor': 0,
                         'positive_max': 0, 'margin': 0, 'negative_min': 1}], 'masks': [],
            'color': {'profile': 'sRGB-SDR', 'channel_order': 'RGBA', 'alpha': 'opaque'}, 'font_policy': 'pinned'})
        self.options = {'binary': self.binary, 'preset': 'windows-msvc-release' if platform.system() == 'Windows' else 'linux-gcc-release',
            'timeout': 1, 'reference': 'locked', 'profile_file': self.profile, 'output': self.directory / 'run',
            'suite': 'pilot', 'repro_argv': ['unit-mechanism-only']}

    def execute(self, faults=None, cases=None, comparator=None, planned=False):
        adapter = AdapterControl(faults)
        cases = cases or [control_case()]
        ledger = {'schema_version': 1, 'baseline_commit': '94ad9bf0cd0a383265d37e253d83625ea611e692',
            'rows': [{**r, 'mapping': 'paired', 'implementation': 'planned' if planned else 'ready', 'official_demo_ids': [],
                      'cases': [c['identity'] for c in cases], 'reason': 'unit mechanism control, not actual coverage'}
                     for r in obligations(CONTROL_INVENTORY) if r['sample_id'] == cases[0]['native']['sample_id']]}
        code = runner.run(self.options, cases, adapters=adapter, coverage_document=ledger,
            comparator=comparator or (lambda *args: ([], {'unit_mechanism_control': True})))
        report = validate('report', read(self.options['output'] / 'report.json'))
        self.assertTrue(adapter.cleaned)
        return code, report, adapter

    def test_complete_mechanism_zero_and_strict_artifact_reports(self):
        code, report, adapter = self.execute()
        self.assertEqual(code, 0)
        self.assertEqual(report['selected'], report['completed'])
        self.assertEqual(report['selected'], report['passed'])
        self.assertEqual(adapter.calls, ['prepare', 'native-probe', 'preflight', 'native', 'web'])
        for name in ('report.json', 'junit.xml', 'index.html'):
            self.assertTrue((self.options['output'] / name).is_file())

    def test_empty_selection_and_missing_profile_fail_with_report(self):
        self.options['component'] = 'unknown'
        code, report, _ = self.execute()
        self.assertEqual(code, 2)
        self.assertEqual(report['passed'], [])

    def test_planned_and_uncalibrated_inputs_cannot_become_pass(self):
        code, report, adapter = self.execute(planned=True)
        self.assertEqual(code, 2)
        self.assertEqual(report['cases'][0]['categories'], ['coverage'])
        self.assertNotIn('native-probe', adapter.calls)
        self.options['output'] = self.directory / 'uncalibrated'
        profile = read(self.profile)
        profile['calibrated'] = False
        write_json(self.profile, profile)
        code, report, adapter = self.execute()
        self.assertEqual(code, 3)
        self.assertNotIn('native-probe', adapter.calls)

    def test_requested_dpr_mismatch_is_environment_error_before_web(self):
        self.options['dpr'] = '2'
        code, report, adapter = self.execute()
        self.assertEqual(code, 3)
        self.assertNotIn('web', adapter.calls)

    def test_exit_zero_missing_action_complete_capture_old_nonce_frame_and_escape_are_four(self):
        for fault in ('receipt', 'complete', 'source+complete', 'capture', 'image', 'nonce', 'frame', 'dpr', 'dispatched', 'path', 'crash', 'timeout'):
            for side in ('native', 'web'):
                with self.subTest(fault=fault, side=side):
                    self.options['output'] = self.directory / (side + '-' + fault)
                    code, report, _ = self.execute({side: fault})
                    self.assertEqual(code, 4 if fault != 'path' else 2)
                    self.assertEqual(report['passed'], [])

    def test_source_binary_font_and_actual_props_do_not_pass(self):
        for fault in ('source', 'binary', 'font', 'props'):
            with self.subTest(fault=fault):
                self.options['output'] = self.directory / fault
                code, _, _ = self.execute({'native': fault})
                self.assertEqual(code, 3 if fault == 'font' else 2)
        self.options['output'] = self.directory / 'web-source'
        self.assertEqual(self.execute({'web': 'source'})[0], 2)

    def test_diff_one_environment_three_and_cleanup_four(self):
        code, report, _ = self.execute(comparator=lambda *args: ([{'category': 'layout', 'step': 'capture',
                'message': 'unit geometry difference'}], {}))
        self.assertEqual(code, 1)
        self.assertEqual(report['cases'][0]['first_step'], 'capture')
        self.options['output'] = self.directory / 'environment'
        self.assertEqual(self.execute({'prepare': True})[0], 3)

    def test_priority_preserves_all_cases(self):
        cases = [control_case() for _ in range(4)]
        for index, case in enumerate(cases):
            case['identity'] = f'antd665.button.unit-{index}.v1'
        faults = {cases[1]['identity']: 'source', cases[2]['identity']: 'environment', cases[3]['identity']: 'crash'}
        code, report, _ = self.execute(faults, cases, lambda *args: ([{'category': 'visual', 'step': 'capture'}], {}))
        self.assertEqual(code, 4)
        self.assertEqual([r['verdict'] for r in report['cases']], ['fail', 'error', 'error', 'error'])

    def test_old_output_and_output_escape_refused_without_rewrite(self):
        self.execute()
        before = (self.options['output'] / 'report.json').read_bytes()
        with self.assertRaises(ProtocolError):
            self.execute()
        self.assertEqual(before, (self.options['output'] / 'report.json').read_bytes())
        self.options['output'] = ROOT / 'tests/not-owned'
        with self.assertRaises(ProtocolError):
            self.execute()

    def test_private_wayland_and_hidden_windows_commands(self):
        case = control_case()
        command = native_command(self.binary, case, self.directory / 'request.json', self.directory, 1, 'Linux')
        self.assertIn(str(ROOT / 'tools/run_background_wayland.py'), command)
        self.assertIn('--page-visual', command)
        self.assertIn('--locale=zh-CN', command)
        self.assertNotIn('--page-locale=zh-CN', command)
        with self.assertRaises(RunError):
            native_command(self.binary, case, self.profile, self.directory, 1, 'Other')

    def test_real_child_timeout_is_stopped(self):
        with self.assertRaises(RunError) as error:
            process([sys.executable, '-B', '-c', 'import time;time.sleep(30)'], self.directory / 'process', .1)
        self.assertEqual(error.exception.code, 4)


if __name__ == '__main__':
    unittest.main()
