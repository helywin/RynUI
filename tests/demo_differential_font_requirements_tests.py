"""Synthetic SFNT metadata controls, not actual font loading or glyph coverage."""
from copy import deepcopy
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_differential.font_requirements import preflight, sfnt_weight, required_faces
from demo_differential.adapters import RunError


def sfnt(weight, italic=False):
    table = bytearray(64)
    struct.pack_into('>H', table, 4, weight)
    struct.pack_into('>H', table, 62, int(italic))
    return struct.pack('>IHHHH', 0x00010000, 1, 0, 0, 0) + struct.pack('>4sIII', b'OS/2', 0, 28, 64) + table


class FontRequirementsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / 'out', prefix='font-weight-unit-')
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def file(self, identity, weight, actual_weight=None, italic=False):
        path = self.directory / (identity + '-' + str(weight) + '.bin')
        path.write_bytes(sfnt(weight if actual_weight is None else actual_weight, italic))
        return {'identity': identity, 'weight': weight, 'face_index': 0, 'path': str(path),
                'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

    def case(self, files):
        return {'fixture': {'font_policy': 'pinned', 'fonts': [{k: f[k] for k in ('identity', 'weight', 'face_index', 'sha256')}
                                                            for f in files]}}

    def test_missing_real_600_lists_both_exact_family_weight_requirements(self):
        files = [self.file(identity, 400) for identity in ('noto-sans', 'noto-sans-cjk-sc')]
        with self.assertRaises(RunError) as error:
            preflight(self.case(files), files)
        self.assertEqual(error.exception.code, 3)
        missing = error.exception.details['missing_font_faces']
        self.assertEqual([(r['identity'], r['weight']) for r in missing], [('noto-sans', 600), ('noto-sans-cjk-sc', 600)])
        self.assertFalse(error.exception.details['implicit_fallback'])

    def test_regular_relabelled_600_and_unlocked_slant_are_rejected(self):
        for files in ([self.file('noto-sans', 600, 400)], [self.file('noto-sans', 600, italic=True)]):
            with self.subTest(files=files), self.assertRaises(RunError):
                preflight(self.case(files), files)

    def test_actual_metadata_checked_no_missing_table_or_wrong_hash_is_accepted(self):
        file = self.file('noto-sans', 600)
        self.assertEqual(sfnt_weight(file['path'], 0), (600, False))
        with self.assertRaises(ValueError):
            sfnt_weight(file['path'], 1)
        bad = deepcopy(file)
        bad['sha256'] = '0' * 64
        with self.assertRaises(RunError):
            preflight(self.case([bad]), [bad])
        Path(file['path']).write_bytes(b'not-a-font')
        with self.assertRaises(ValueError):
            sfnt_weight(file['path'], 0)


if __name__ == '__main__':
    unittest.main()
