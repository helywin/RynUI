"""Low alpha profiles/tails: pure CPU mechanism, no capture or calibration."""
import pathlib
import sys
import unittest

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]))
from tools.demo_differential.regions import shadow_profile_metrics
from tools.demo_differential.protocol import ProtocolError


class ShadowProfiles(unittest.TestCase):
    def test_low_alpha_shadow_does_not_require_half_contour(self):
        plane=[0,.01,.05,.01,0]*3
        value=shadow_profile_metrics(plane,plane,(5,3),tail_indices=(0,4,10,14),profile_indices=range(15))
        self.assertTrue(value['required_ink_present'])
        for key in ('coverage_mass_relative_error','projection_relative_error','profile_max_error','tail_max_error'):
            self.assertEqual(value[key],0)

    def test_missing_shadow_profile_and_tail_have_separate_metrics(self):
        reference=[0,.01,.05,.01,0];native=[0,0,0,0,.02]
        value=shadow_profile_metrics(reference,native,(5,1),tail_indices=(0,4),profile_indices=(1,2,3))
        self.assertGreater(value['coverage_mass_relative_error'],.05)
        self.assertEqual(value['profile_max_error'],.05)
        self.assertEqual(value['tail_max_error'],.02)

    def test_one_pixel_shift_preserves_mass_but_fails_profiles(self):
        value=shadow_profile_metrics([0,.05,.1,.05,0],[0,0,.05,.1,.05],(5,1),tail_indices=(0,4))
        self.assertAlmostEqual(value['coverage_mass_relative_error'],0)
        self.assertGreater(value['projection_relative_error'],.05)
        self.assertGreater(value['tail_max_error'],2/255)

    def test_invalid_planes_and_geometry_are_rejected(self):
        for native in ([0,0],[0,float('nan'),0],[0,1.01,0]):
            with self.assertRaises(ProtocolError):shadow_profile_metrics([0,.1,0],native,(3,1))
        with self.assertRaises(ProtocolError):shadow_profile_metrics([0,.1,0],[0,.1,0],(3,1),tail_indices=(3,))


if __name__=='__main__':unittest.main()
