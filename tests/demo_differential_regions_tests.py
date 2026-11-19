"""Analytic ROI/raster controls; no browser/native or environment calibration claim."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_differential.regions import (geometry, union, physical_box, crop_rgba, raster_metrics, contour_distance,
                                       coverage_metrics,reference_aa_is_bounded)
from demo_differential.protocol import ProtocolError


def rectangle(x=0, y=0, width=10, height=10):
    return dict(x=x, y=y, width=width, height=height)


def pixels(width, height, points):
    return b''.join(bytes((0, 0, 0, 255) if (x, y) in points else (255, 255, 255, 255))
                    for y in range(height) for x in range(width))


class RegionTests(unittest.TestCase):
    def test_reference_aa_band_cannot_expand_with_native_residual(self):
        self.assertTrue(reference_aa_is_bounded([0,.25,1,1,.25,0],(6,1)))
        self.assertFalse(reference_aa_is_bounded([.1,.25,.4,1,1,.4,.25,.1],(8,1)))
        def square(ink):
            return [ink if 1<=x<=7 and 1<=y<=7 else 0 for y in range(9) for x in range(9)]
        self.assertTrue(reference_aa_is_bounded(square(.994),(9,9),uncertainty=.008))
        self.assertFalse(reference_aa_is_bounded(square(.99),(9,9),uncertainty=.008))
        bounds=[.008]*81
        self.assertTrue(reference_aa_is_bounded(square(.994),(9,9),uncertainty=bounds))
        bounds[40]=.004
        self.assertFalse(reference_aa_is_bounded(square(.994),(9,9),uncertainty=bounds))
        with self.assertRaises(ProtocolError):
            reference_aa_is_bounded([.5],(1,1),uncertainty=.5)
        with self.assertRaises(ProtocolError):
            reference_aa_is_bounded(square(.994),(9,9),uncertainty=bounds[:-1])

    def test_subpixel_contour_distinguishes_a_code_crossing_from_a_pixel_shift(self):
        reference=[0,.49,1,1,0]
        nearby=[0,.51,1,1,0]
        moved=[0,0,.49,1,1]
        self.assertLess(coverage_metrics(reference,nearby,(5,1))['contour_distance_px'],.1)
        self.assertGreaterEqual(coverage_metrics(reference,moved,(5,1))['contour_distance_px'],1)
        self.assertIsNone(coverage_metrics(reference,[0]*5,(5,1))['contour_distance_px'])

    def test_opaque_rgb8_midpoint_codes_keep_the_same_frame_contour(self):
        reference=[0,128/255,128/255,0]
        native=[0,127/255,127/255,0]
        self.assertIsNone(coverage_metrics(reference,native,(4,1))['contour_distance_px'])
        result=coverage_metrics(reference,native,(4,1),full_contrast_rgb8=True)
        self.assertEqual(result['contour_distance_px'],0)
        self.assertGreater(result['coverage_mass_relative_error'],0)
        shifted=[0,0,127/255,127/255]
        distance=coverage_metrics(reference,shifted,(4,1),full_contrast_rgb8=True)['contour_distance_px']
        self.assertGreaterEqual(distance,1)
        self.assertLessEqual(distance,1+1/32)
        missing=[0,126/255,126/255,0]
        self.assertIsNone(coverage_metrics(reference,missing,(4,1),full_contrast_rgb8=True)['contour_distance_px'])

    def test_local_coverage_and_projection_catch_last_glyph_pixel(self):
        reference=[1.]*100
        native=reference[:]
        native[-1]=0
        result=coverage_metrics(reference,native,(10,10))
        self.assertLess(result['coverage_mass_relative_error'],.05)
        self.assertGreater(result['coverage_local_relative_error'],.05)
        self.assertGreater(result['projection_relative_error'],.05)
        self.assertGreater(result['contour_distance_px'],.5)
        blank=coverage_metrics([0]*4,[0]*4,(2,2))
        self.assertFalse(blank['required_ink_present'])
        self.assertIsNone(blank['contour_distance_px'])

    def test_local_error_uses_reference_ink_even_when_window_is_nearly_empty(self):
        reference = [1, 1, 0, 0] * 2 + [0] * 8
        native = reference[:]
        native[-1] = 1/255
        result = coverage_metrics(reference, native, (4, 4))
        self.assertAlmostEqual(result['coverage_local_relative_error'], 1/(4*255))
        self.assertAlmostEqual(result['projection_relative_error'], 1/(2*255))
        self.assertEqual(result['contour_distance_px'], 0)

    def test_only_declared_sample_origin_can_translate(self):
        a, b = rectangle(12, 24), rectangle(112, 224)
        self.assertEqual(geometry(a, b, rectangle(10, 20), rectangle(110, 220)), dict(x=0, y=0, width=0, height=0))
        b['x'] += 1
        self.assertEqual(geometry(a, b, rectangle(10, 20), rectangle(110, 220))['x'], 1)
        b['width'] -= 1
        self.assertEqual(union(a, rectangle(13, 24, 8, 10)), a)

    def test_fractional_dpr_floor_ceil_and_guard(self):
        for dpr in (1, 1.25, 1.5, 2):
            box = physical_box(rectangle(.25, .5, 1, 2), dpr)
            self.assertLessEqual(box[0], .25 * dpr - 2)
            self.assertGreaterEqual(box[2], 1.25 * dpr + 2)
        for dpr in (0, True, float('nan')):
            with self.assertRaises(ProtocolError):
                physical_box(rectangle(), dpr)
        with self.assertRaises(ProtocolError):
            physical_box(rectangle(), 1, guard=1)

    def test_no_empty_roi_silent_clip_or_resize(self):
        data = pixels(4, 4, {(1, 1)})
        self.assertEqual(len(crop_rgba(data, (4, 4), (1, 1, 3, 3))), 16)
        for box in ((0, 0, 0, 1), (-1, 0, 3, 4), (0, 0, 5, 4)):
            with self.subTest(box=box), self.assertRaises(ProtocolError):
                crop_rgba(data, (4, 4), box)
        with self.assertRaises(ProtocolError):
            union(rectangle(width=0), rectangle())
        with self.assertRaises(ProtocolError):
            raster_metrics(data, data, (5, 4), (255, 255, 255, 255))

    def test_small_missing_icon_cannot_be_diluted_by_large_background(self):
        icon = {(10, 10), (10, 11), (11, 10), (11, 11)}
        ref, missing = pixels(400, 300, icon), pixels(400, 300, set())
        result = raster_metrics(ref, missing, (400, 300), (255, 255, 255, 255))
        self.assertEqual(result['changed_pixels'], 4)
        self.assertEqual(result['changed_components'], [4])
        self.assertEqual(result['max_channel_residual'], 255)
        self.assertFalse(result['required_ink_present'])
        self.assertIsNone(result['contour_distance_px'])

    def test_double_blank_cropped_character_and_one_pixel_shift_are_red_metrics(self):
        ink = {(x, y) for x in range(2, 5) for y in range(2, 6)}
        ref = pixels(10, 10, ink)
        for actual in (set(), {(x + 1, y) for x, y in ink}, {(x, y) for x, y in ink if x != 4}):
            metrics = raster_metrics(ref, pixels(10, 10, actual), (10, 10), (255, 255, 255, 255))
            self.assertGreater(metrics['max_channel_residual'], 0)
        blank = pixels(10, 10, set())
        self.assertFalse(raster_metrics(blank, blank, (10, 10), (255, 255, 255, 255))['required_ink_present'])
        correct = raster_metrics(ref, ref, (10, 10), (255, 255, 255, 255))
        self.assertTrue(correct['required_ink_present'])
        self.assertEqual(correct['contour_distance_px'], 0)
        self.assertEqual(correct['coverage_mass_relative_error'], 0)
        self.assertEqual(contour_distance({(0, 0)}, {(1, 1)}), 2 ** .5)
        self.assertIsNone(contour_distance({(0, 0)}, {(3, 0)}))


if __name__ == '__main__':
    unittest.main()
