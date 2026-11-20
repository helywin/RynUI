"""Controls for source-bound rounded edges outside the analytic diagonal."""
from copy import deepcopy
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))

from demo_differential.comparison import _layer_coverage
from demo_differential.protocol import ProtocolError
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


if __name__ == '__main__':
    unittest.main()
