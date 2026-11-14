"""Device compatibility retains actual receipt counters outside the stable key."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from demo_differential.observations import IncompleteRun
from demo_differential.runner_evidence import metrics


class DeviceMetricsTests(unittest.TestCase):
    def native(self, **extra):
        value = dict(logical_width=1280, logical_height=900, pixel_width=1920, pixel_height=1350, dpr=1.5)
        value.update(extra)
        return {'checkpoints': {'capture': {'resources': {'metrics': value}}}}

    def test_fresh_frame_counters_do_not_change_device_identity(self):
        first = self.native(frames=1, readbacks=1, coordinate_origin=[0, 0])
        fresh = self.native(frames=23, readbacks=7, coordinate_origin=[-32, 80])
        self.assertEqual(metrics(first, 'native'), metrics(fresh, 'native'))
        self.assertEqual(set(metrics(fresh, 'native')),
                         {'logical_width', 'logical_height', 'pixel_width', 'pixel_height', 'dpr'})

    def test_raw_receipts_are_not_modified(self):
        observed = self.native(frames=23, readbacks=7, coordinate_origin=[-32, 80])
        before = deepcopy(observed)
        metrics(observed, 'native')
        self.assertEqual(observed, before)

    def test_missing_nonfinite_boolean_or_inconsistent_device_fails(self):
        for field, value in [('dpr', float('nan')), ('dpr', True), ('pixel_width', 1919),
                             ('logical_height', 0), ('pixel_height', None)]:
            with self.subTest(field=field, value=value), self.assertRaises(IncompleteRun):
                metrics(self.native(**{field: value}), 'native')
        observed = self.native()
        del observed['checkpoints']['capture']['resources']['metrics']['pixel_width']
        with self.assertRaises(IncompleteRun):
            metrics(observed, 'native')

    def test_web_environment_keeps_its_actual_adapter_contract(self):
        observed = {'hello': {'effective_inputs': {'actual_environment':
                    {'ratio': 1.5, 'width': 1280, 'height': 900}}}}
        self.assertEqual(metrics(observed, 'web'), {'dpr': 1.5, 'logical_width': 1280, 'logical_height': 900})


if __name__ == '__main__':
    unittest.main()
