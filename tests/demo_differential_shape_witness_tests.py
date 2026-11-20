"""Controls for source-bound rounded edges outside the analytic diagonal."""
from copy import deepcopy
from pathlib import Path
import struct
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))

from demo_differential import comparison
from demo_differential.adapters import RunError
from demo_differential.comparison import _layer_coverage
from demo_differential.protocol import ProtocolError
from demo_differential.shape_blend_witness import _samples, _stacks, bind_blends, verified_blend
from demo_differential.shape_witness import ShapeWitnesses, edge_samples, locked_edge_value, shape_request


def native_outline():
    return dict(kind='outline', side='native', bounds=dict(x=1, y=133, width=258, height=30),
                width=1, offset_width=0, radii=5, alpha=1, opacity=1, color=(250/255, 173/255, 20/255))


class RoundedWitnessTests(unittest.TestCase):
    def test_border_and_outline_share_the_same_physical_geometry(self):
        native = native_outline()
        web = dict(native, kind='border', side='web', bounds=dict(x=0, y=132, width=260, height=32),
                   widths=[1, 1, 1, 1], radii=(6, 6, 6, 6))
        n, ns = shape_request(native, 1.5)
        w, ws = shape_request(web, 1.5)
        self.assertEqual((n[0], w[0]), ('outline', 'border'))
        self.assertEqual(n[1:], w[1:])
        self.assertEqual(ns, ws)
        web['bounds']['x'] = .1
        self.assertIsNone(shape_request(web, 1.5))
        self.assertIsNone(shape_request(dict(native, opacity=.5), 1.5))

    def test_partial_source_byte_fixes_the_colour_prediction(self):
        row = native_outline()
        _, (dx, dy) = shape_request(row, 1.5)
        point = (1, 160 + 1/3)
        self.assertEqual(_layer_coverage(row, point, 1.5), (1, False))
        row['locked_edge_witness'] = ({(1 - dx, 240 - dy): 248/255}, (dx, dy))
        self.assertEqual(locked_edge_value(row, point, 1.5, 1), 248/255)
        self.assertEqual(locked_edge_value(row, point, 1.5, 0), 0)

    def test_opaque_rounded_fill_accepts_only_matching_uniform_geometry(self):
        native = dict(kind='fill', side='native', bounds=dict(x=268, y=96, width=260, height=32),
                      radii=6, alpha=1, opacity=1, color=(22/255, 119/255, 1.))
        web = dict(native, side='web', radii=(6, 6, 6, 6))
        spec, (dx, dy) = shape_request(native, 1.5)
        self.assertEqual(shape_request(web, 1.5), (spec, (dx, dy)))
        self.assertIsNone(shape_request(dict(web, radii=(6, 6, 6, 7)), 1.5))
        self.assertIsNone(shape_request(dict(web, alpha=.65), 1.5))
        witnesses = ShapeWitnesses({'tools': {'control_binary': 'out/bin/mock'}}, Path.cwd())
        witnesses.samples[spec] = {(403 - dx, 186 - dy): 248/255}
        witnesses.bind((web, native), 1.5)
        point = (269., 124 + 1/3)
        self.assertEqual(_layer_coverage(native, point, 1.5), (1, False))
        for row in (web, native):
            self.assertEqual(locked_edge_value(row, point, 1.5, 1), 248/255)

    def test_receipt_rejects_invalid_and_repeated_samples(self):
        good = dict(coverage_extent=[1, 2, 15, 8], edges=[[2, 3, 180]])
        self.assertEqual(edge_samples(good), {(2, 3): 180/255})
        for bad in ([2, 3, 180], [16, 3, 100], [3, 8, 255], [3, 8, 0], [3, 8, 100.0]):
            mutated = deepcopy(good)
            mutated['edges'].append(bad)
            with self.assertRaises(ProtocolError):
                edge_samples(mutated)

    def test_only_fragment_bytes_shared_by_both_paths_bind_to_actual_paint(self):
        web = dict(native_outline(), kind='border', side='web', bounds=dict(x=0, y=132, width=260, height=32),
                   widths=[1, 1, 1, 1], radii=(6, 6, 6, 6))
        native = native_outline()
        native['color'] = struct.unpack('<3f', struct.pack('<3f', *web['color']))
        witnesses = ShapeWitnesses({'tools': {'control_binary': 'out/bin/mock'}}, Path.cwd())
        witnesses.samples = {shape_request(row, 1.5)[0]: {(4, 7): 248/255, (9, 11): alpha}
                             for row, alpha in ((web, 240/255), (native, 243/255))}
        witnesses.bind((web, native), 1.5)
        self.assertEqual(web['locked_edge_witness'][0], {(4, 7): 248/255})
        self.assertEqual(web['locked_edge_witness'], native['locked_edge_witness'])


def hard_shadow_stack(side):
    owner = {}
    bounds = dict(x=0, y=132, width=260, height=32)
    web = side == 'web'
    base = dict(groups=[('owner', 1.)] if web else [], alpha=1., opacity=1., source=owner)
    return (
        dict(base, kind='shadow', bounds=bounds, radii=6, alpha=.1, color=(1, 215/255, 5/255),
             blur=0, spread=2, offset=[0, 0], caster_inset=1, knockout=True),
        dict(base, kind='fill', bounds=bounds if web else dict(x=.5, y=132.5, width=259, height=31),
             radii=(6,)*4 if web else 5.5, color=(1., 1., 1.)),
        dict(base, kind='border' if web else 'outline', color=(250/255, 173/255, 20/255),
             bounds=bounds if web else dict(x=1, y=133, width=258, height=30),
             radii=(6,)*4 if web else 5, widths=[1, 1, 1, 1], width=1, offset_width=0),
    )


class RoundedBlendWitnessTests(unittest.TestCase):
    def test_focus_blend_requires_the_observed_order_on_each_side(self):
        def focused(side):
            shadow, fill, line = hard_shadow_stack(side)
            shadow.update(offset=[0, 2], spread=0, color=(5/255, 145/255, 1.), knockout=side == 'web')
            fill.update(bounds=shadow['bounds'], radii=shadow['radii'], color=(22/255, 119/255, 1.))
            line.update(kind='outline', bounds=shadow['bounds'], radii=shadow['radii'], width=3, offset_width=1,
                        color=(145/255, 202/255, 1.))
            return (shadow, fill, line) if side == 'web' else (shadow, line, fill)

        web, native = (focused(side) for side in ('web', 'native'))
        spec, dx, dy = list(_stacks(web, 'web', 1.5))[-1][0]
        self.assertEqual(list(_stacks(native, 'native', 1.5))[-1][0], (spec, dx, dy))
        observed = ((235, 245, 255), (236, 246, 255))
        pair_spec = next(_stacks(web, 'web', 1.5))[0][0]
        cache = {spec: _samples([[0-dx, 198-dy, *observed[0], *observed[1]]]), pair_spec: {}}
        pixels = bind_blends((web, native), 1.5, Path.cwd(), Path('out/bin/mock'), cache)
        self.assertEqual(verified_blend(pixels, 0, 198, 1.5, ({'active':web},{'active':native}), observed), observed[0])
        self.assertIsNone(verified_blend(pixels, 0, 198, 1.5, ({'active':web},{'active':native[::-1]}), observed))

    def test_llvmpipe_integer_shadow_stroke_has_separate_encoding(self):
        web, native = (hard_shadow_stack(side) for side in ('web', 'native'))
        expected, _ = next(_stacks(web, 'web', 1, True))
        actual, _ = next(_stacks(native, 'native', 1, True))
        self.assertEqual(actual, expected)
        self.assertTrue(actual[0][-1])
        self.assertFalse(next(_stacks(native, 'native', 1, False))[0][0][-1])
        self.assertFalse(next(_stacks(native, 'native', 1.5, True))[0][0][-1])

    def test_integer_opaque_stroke_requires_exact_box_and_both_encoded_colors(self):
        web, native = (hard_shadow_stack(side)[1:] for side in ('web', 'native'))
        for rows in (web, native):
            rows[1]['color'] = (217/255,)*3
        spec, dx, dy = next(_stacks(web, 'web', 1))[0]
        self.assertEqual(next(_stacks(native, 'native', 1))[0], (spec, dx, dy))
        colors = ((241,)*3, (242,)*3)
        cache = {spec: _samples([[3-dx, 132-dy, *colors[0], *colors[1]]])}
        pixels = bind_blends((web, native), 1, Path.cwd(), Path('out/bin/mock'), cache)
        sides = tuple({'active': rows} for rows in (web, native))
        self.assertEqual(verified_blend(pixels, 3, 132, 1, sides, colors), colors[0])
        self.assertIsNone(verified_blend(pixels, 3, 132, 1, sides, (colors[0], (243,)*3)))
        self.assertFalse(list(_stacks(web, 'web', 1.5)))
        native[0]['bounds'] = dict(native[0]['bounds'], x=.75)
        self.assertFalse(bind_blends((web, native), 1, Path.cwd(), Path('out/bin/mock'), cache))

    def test_translated_hard_shadow_requires_same_offset_and_opaque_colored_fill(self):
        def translated(side):
            shadow, fill, _ = hard_shadow_stack(side)
            shadow.update(offset=[0, 2], spread=0, color=(5/255, 145/255, 1.), knockout=side == 'web')
            fill.update(bounds=shadow['bounds'], radii=shadow['radii'], color=(22/255, 119/255, 1.))
            return shadow, fill

        web, native = translated('web'), translated('native')
        spec, dx, dy = next(_stacks(web, 'web', 1.5))[0]
        self.assertEqual(next(_stacks(native, 'native', 1.5))[0], (spec, dx, dy))
        colors = ((79, 153, 255), (80, 154, 255))
        cache = {spec: _samples([[0-dx, 229-dy, *colors[0], *colors[1]]])}
        pixels = bind_blends((web, native), 1.5, Path.cwd(), Path('out/bin/mock'), cache)
        sides = tuple({'active': rows} for rows in (web, native))
        self.assertEqual(verified_blend(pixels, 0, 229, 1.5, sides, colors), colors[0])
        self.assertIsNone(verified_blend(pixels, 0, 229, 1.5, sides, (colors[0], (79, 154, 255))))

        native[0]['offset'] = [0, 1]
        self.assertFalse(bind_blends((web, native), 1.5, Path.cwd(), Path('out/bin/mock'), cache))
        native = translated('native')
        native[1]['alpha'] = .9
        self.assertFalse(bind_blends((web, native), 1.5, Path.cwd(), Path('out/bin/mock'), cache))

    def test_only_exact_paired_readback_can_use_the_shared_encoding(self):
        web = hard_shadow_stack('web')
        native = hard_shadow_stack('native')
        (spec, dx, dy), _ = next(_stacks(web, 'web', 1.5))
        self.assertEqual(next(_stacks(native, 'native', 1.5))[0], (spec, dx, dy))
        colors = ((254, 236, 191), (254, 237, 192))
        cache = {spec: _samples([[5-dx, 198-dy, *colors[0], *colors[1]]])}
        pixels = bind_blends((web, native), 1.5, Path.cwd(), Path('out/bin/mock'), cache)
        sides = ({'active': web}, {'active': native[::2]})
        self.assertEqual(verified_blend(pixels, 5, 198, 1.5, sides, colors), colors[0])
        for wrong in (((255, 236, 191), colors[1]), (colors[0], (254, 237, 193)), (colors[0], colors[0])):
            self.assertIsNone(verified_blend(pixels, 5, 198, 1.5, sides, wrong))
        other = dict(native[0], kind='glyph')
        self.assertIsNone(verified_blend(pixels, 5, 198, 1.5, (sides[0], {'active': [other]}), colors))
        native[1]['clips'] = [dict(x=4, y=132, width=255, height=32)]
        self.assertIsNone(verified_blend(pixels, 5, 198, 1.5, sides, colors))

    def test_moved_or_translucent_sources_have_no_shared_encoding(self):
        web = hard_shadow_stack('web')
        native = hard_shadow_stack('native')
        native[1]['bounds'] = dict(native[1]['bounds'], y=133)
        self.assertFalse(bind_blends((web, native), 1.5, Path.cwd(), Path('out/bin/mock'), {}))
        native = hard_shadow_stack('native')
        native[0]['groups'] = [('faded', .65)]
        self.assertFalse(bind_blends((web, native), 1.5, Path.cwd(), Path('out/bin/mock'), {}))
        with self.assertRaises(ProtocolError):
            _samples([[3, 4, 0, 0, 0, 0, 0, 0]])


class RoundedWitnessEntryTests(unittest.TestCase):
    def test_pinned_comparison_initializes_witnesses_at_the_repository_root(self):
        manifest = dict(environment=dict(native_capture=dict(shape_rasterizer='skia-rounded-coverage')),
                        tools=dict(control_binary='out/bin/control'))
        case = dict(identity='test', profile={}, fixture={})
        native = dict(hello=dict(provenance=dict(binary_sha256='test')))
        profile = dict(scope=dict(cases=[]))
        proof = SimpleNamespace(manifest=lambda: manifest)
        with (patch.object(comparison, 'binding', return_value={}),
              patch.object(comparison, 'profile_thresholds', return_value={}),
              patch.object(comparison, '_glyph_priors_for_run', return_value={}),
              self.assertRaisesRegex(RunError, 'outside the finite independent pilot scope')):
            comparison.compare(case, native, {}, {}, profile, None, profile_proof=proof)

    def test_profile_hash_reads_both_rounded_witness_modules(self):
        current = comparison.algorithm_hash()
        read = Path.read_bytes
        for name in ('shape_witness.py', 'shape_blend_witness.py'):
            with patch.object(Path, 'read_bytes', lambda path: read(path) + (b'x' if path.name == name else b'')):
                self.assertNotEqual(comparison.algorithm_hash(), current)


if __name__ == '__main__':
    unittest.main()
