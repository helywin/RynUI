"""Frozen native map controls only; no renderer/fence/presentation execution."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_differential.native_resources import lifecycle, validate
from demo_differential.observations import IncompleteRun


def lifecycle_control(present=True):
    return {'owner_generation': 4,
        'owners': [{'identity': 'sample-owner-0', 'present': present, 'branch_active': True if present else None,
                    'generation': 9, 'page_generation': 4}],
        'animation_scopes': [{'identity': 'sample-scope-0', 'present': present, 'active': 0 if present else None,
                              'deadline_us': None, 'generation': 8, 'page_generation': 4}],
        'auxiliary_deadlines': [], 'live_components': int(present), 'live_scopes': int(present),
        'live_interactions': int(present), 'focus_owned': False, 'capture_owned': False, 'session_owned': False,
        'retired': not present, 'idle_observation': 'not-sampled',
        'deadline_coverage': 'sample-animation-scopes-and-pilot-auxiliary',
        'window_deadline_us': 900000, 'shell_owner_present': True,
        'shared_font_cache_policy': 'window-owned-retained-not-page-leak'}


def stream_control():
    metrics = {'logical_width': 16, 'logical_height': 16, 'pixel_width': 16, 'pixel_height': 16, 'dpr': 1}
    rows = []
    for kind, step, serial in [('hello', '', 0), ('checkpoint', 'first', 1), ('checkpoint', 'second', 2), ('complete', '', 0)]:
        resources = {'lifecycle': lifecycle_control(), 'metrics': deepcopy(metrics), 'compositor_callback': None,
                     'presented': None, 'presentation_evidence': 'not-observed-by-checkpoint-readback', 'readback': None}
        captures = []
        if kind == 'checkpoint':
            resources['readback'] = {'nonce': 'unit-run', 'checkpoint': step, 'readback_id': serial, 'fence_id': serial,
                'scene_revision': 7, 'renderer_owner': 3, 'device_epoch': 2, 'metrics': deepcopy(metrics),
                'source_channel_order': 'BGRA', 'source_hash': 'a' * 64, 'binary_sha256': 'b' * 64}
            captures = [{'scene_revision': 7, 'channel_order': 'RGBA', 'framebuffer_format': 'BGRA8_UNORM'}]
        rows.append({'type': kind, 'step': step, 'nonce': 'unit-run', 'resources': resources, 'captures': captures})
    return {'records': rows}


class NativeResourcesTests(unittest.TestCase):
    def test_same_scene_new_fence_and_normalized_bgra_readback(self):
        validate(stream_control(), {'source_hash': 'a' * 64}, 'b' * 64)

    def test_old_fence_readback_epoch_scene_metrics_source_binary_and_channel_fail(self):
        for fault in ('fence', 'readback', 'epoch', 'scene', 'metrics', 'source', 'binary', 'channel', 'nonce'):
            observed = stream_control()
            readback = observed['records'][2]['resources']['readback']
            if fault == 'fence':
                readback['fence_id'] = 1
            elif fault == 'readback':
                readback['readback_id'] = 1
            elif fault == 'epoch':
                readback['device_epoch'] = 1
            elif fault == 'scene':
                readback['scene_revision'] = 6
            elif fault == 'metrics':
                readback['metrics']['dpr'] = 2
            elif fault == 'source':
                readback['source_hash'] = '0' * 64
            elif fault == 'binary':
                readback['binary_sha256'] = '0' * 64
            elif fault == 'channel':
                readback['source_channel_order'] = 'RGBA'
            else:
                readback['nonce'] = 'old-run'
            with self.subTest(fault=fault), self.assertRaises(IncompleteRun):
                validate(observed, {'source_hash': 'a' * 64}, 'b' * 64)

    def test_flat_retired_sample_allows_shell_window_deadline_and_shared_fonts(self):
        value = lifecycle_control(False)
        self.assertTrue(lifecycle(value)['retired'])
        self.assertEqual(value['window_deadline_us'], 900000)
        self.assertEqual(value['idle_observation'], 'not-sampled')
        value['owner_generation'] = 0
        value['retired'] = None
        self.assertIsNone(lifecycle(value)['retired'])

    def test_scope_active_is_integer_absence_null_and_owner_generation_not_guessed(self):
        for fault in ('bool-active', 'absent-active', 'retired', 'generation', 'window', 'idle', 'nested'):
            value = lifecycle_control(False if fault == 'absent-active' else True)
            if fault == 'bool-active':
                value['animation_scopes'][0]['active'] = False
            elif fault == 'absent-active':
                value['animation_scopes'][0]['active'] = 0
            elif fault == 'retired':
                value['retired'] = True
            elif fault == 'generation':
                value['owners'][0]['generation'] = True
            elif fault == 'window':
                value['window_deadline_us'] = False
            elif fault == 'idle':
                value['idle_observation'] = 'passed'
            else:
                value = {'sample': value, 'shell': {}, 'shared': {}}
            with self.subTest(fault=fault), self.assertRaises(IncompleteRun):
                lifecycle(value)

    def test_readback_outside_checkpoint_and_manufactured_present_evidence_fail(self):
        for fault in ('outside', 'presented', 'generation-reuse'):
            observed = stream_control()
            if fault == 'outside':
                observed['records'][0]['resources']['readback'] = deepcopy(observed['records'][1]['resources']['readback'])
            elif fault == 'presented':
                observed['records'][1]['resources']['presented'] = False
            else:
                observed['records'][2]['resources']['lifecycle']['owners'][0]['generation'] += 1
            with self.subTest(fault=fault), self.assertRaises(IncompleteRun):
                validate(observed, {'source_hash': 'a' * 64}, 'b' * 64)


if __name__ == '__main__':
    unittest.main()
