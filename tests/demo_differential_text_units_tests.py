"""Independent UTF8-byte/UTF16 vectors; no native observation becomes expected."""
import sys
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from demo_differential.text_units import convert,selection,boundaries
from demo_differential.protocol import ProtocolError


class UnitTests(unittest.TestCase):
    def test_declared_pilot_vector(self):
        text='中文A🙂'
        self.assertEqual(boundaries(text),[(0,0),(3,1),(6,2),(7,3),(11,5)])
        self.assertEqual(selection(text,0,11,'UTF-8-bytes'),[0,5])
        self.assertEqual(selection(text,5,0,'UTF-16','UTF-8-bytes'),[11,0])
        self.assertEqual(len(text),4) # Four scalars are not byte or DOM indices.

    def test_combining_bidi_zwj_and_empty(self):
        self.assertEqual(boundaries('e\u0301'),[(0,0),(1,1),(3,2)])
        self.assertEqual(selection('אב',4,0,'UTF-8-bytes'),[2,0])
        self.assertEqual(selection('👩\u200d💻',0,11,'UTF-8-bytes'),[0,5])
        self.assertEqual(selection('',0,0,'UTF-8-bytes'),[0,0])

    def test_split_multibyte_surrogate_and_invalid_units_fail(self):
        for offset,unit in [(1,'UTF-8-bytes'),(2,'UTF-8-bytes'),(4,'UTF-8-bytes'),(8,'UTF-8-bytes'),(4,'UTF-16'),(12,'UTF-8-bytes'),(-1,'UTF-16'),(True,'UTF-16')]:
            with self.subTest(offset=offset,unit=unit),self.assertRaises(ProtocolError):
                convert('中文A🙂',offset,unit,'UTF-16')
        with self.assertRaises(ProtocolError):convert('x',0,'graphemes','UTF-16')
        with self.assertRaises(ProtocolError):boundaries('\ud800')


if __name__=='__main__':unittest.main()
