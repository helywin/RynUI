"""Independent analytic mechanisms; no GPU, DPI, codec or platform calibration."""
from pathlib import Path
import math
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from demo_differential.color_metrics import (srgb_to_linear, srgb_to_lab, delta_e00, rgb8_delta_e00,
                                            source_over, dual_background, unmix_coverage)
from demo_differential.protocol import ProtocolError


class ColorTests(unittest.TestCase):
    def test_primary_standard_transfer_and_d65_white(self):
        self.assertEqual(srgb_to_linear((0,0,0)), (0,0,0))
        self.assertAlmostEqual(srgb_to_linear((.04045,1,.5))[0], .00313080495356)
        self.assertAlmostEqual(srgb_to_linear((.04045,1,.5))[2], .21404114048223)
        for a,b in zip(srgb_to_lab((1,1,1)), (100,0,0)):
            self.assertAlmostEqual(a,b, places=10)
        # Independent published sRGB red XYZ/D65 Lab, not CSS Lab/D50.
        for a,b in zip(srgb_to_lab((1,0,0)), (53.2371156,80.0901135,67.2032635)):
            self.assertAlmostEqual(a,b, places=5)

    def test_author_test_data_zero_chroma_and_hue_wrap(self):
        # Sharma/Wu/Dalal supplementary numeric test data, selected rows 1,7,10,11,17.
        # https://hajim.rochester.edu/ece/sites/gsharma/ciede2000/dataNprograms/ciede2000testdata.txt
        rows = [((50,2.6772,-79.7751),(50,0,-82.7485),2.0425),
                ((50,0,0),(50,-1,2),2.3669),
                ((50,2.49,-.001),(50,-2.49,.001),7.1792),
                ((50,2.49,-.001),(50,-2.49,.0011),7.2195),
                ((50,2.5,0),(73,25,-18),27.1492)]
        for a,b,expected in rows:
            with self.subTest(a=a,b=b):
                self.assertAlmostEqual(delta_e00(a,b),expected,delta=.00005)
                self.assertAlmostEqual(delta_e00(b,a),expected,delta=.00005)
                self.assertEqual(delta_e00(a,a),0)

    def test_dual_background_alpha_zero_and_source_over(self):
        black,white,f = (0,0,0),(1,1,1),(17/255,83/255,201/255)
        for alpha in (0,.25,.5,.75,1):
            found = dual_background(source_over(f,black,alpha),source_over(f,white,alpha),black,white)
            self.assertAlmostEqual(found['alpha'],alpha)
            if alpha:
                for a,b in zip(found['foreground'],f):
                    self.assertAlmostEqual(a,b)
            else:
                self.assertIsNone(found['foreground'])
        with self.assertRaises(ProtocolError):
            dual_background(black,white,black,black)
        with self.assertRaises(ProtocolError):
            dual_background((.25,0,0),(.75,.75,.75),black,white)

    def test_material_coverage_has_no_gray_color_bias_or_silent_clamp(self):
        for foreground in ((0,0,0),(.5,.5,.5),(17/255,83/255,201/255)):
            for alpha in (.25,.5,1):
                for coverage in (0,.25,.5,1):
                    observed=source_over(foreground,(1,1,1),alpha*coverage)
                    result=unmix_coverage(observed,foreground,(1,1,1),alpha)
                    self.assertAlmostEqual(result['coverage'],coverage)
        for args in (((0,0,0),(1,1,1),(1,1,1),1),
                     ((0,0,0),(0,0,0),(1,1,1),0),
                     ((0,0,0),(.9,.9,.9),(1,1,1),1),
                     ((.5,.6,.5),(0,0,0),(1,1,1),1)):
            with self.subTest(args=args),self.assertRaises(ProtocolError):
                unmix_coverage(*args)

    def test_fixed_wrong_color_and_channels_are_nonzero(self):
        self.assertGreater(rgb8_delta_e00((17,83,201),(201,83,17)),3)
        for value in ((math.nan,0,0),(-.1,0,0),(1.1,0,0)):
            with self.assertRaises(ProtocolError):
                srgb_to_lab(value)


if __name__ == '__main__':
    unittest.main()
