"""Actual locked-library install/import; compatibility does not pin Python patch version."""
import os
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from demo_differential.comparison_dependencies import prepare, verify, wheel_profile, image_module
from demo_differential.protocol import ProtocolError


class LibraryTests(unittest.TestCase):
    def test_actual_archive_files_and_import(self):
        package=wheel_profile()
        archive=Path(os.environ.get('RYNUI_COMPARATOR_WHEEL',
            ROOT/'out/demo-differential/comparator-downloads'/package['filename']))
        self.assertTrue(archive.is_file(),'provide actual locked Pillow archive; no mock installation')
        with tempfile.TemporaryDirectory(prefix='comparison-library-',dir=ROOT/'out') as temp:
            output=Path(temp)/'runtime'
            site=prepare(output,archive=archive)
            verify(site,package)
            with patch.dict(sys.modules,{'PIL':SimpleNamespace(__file__=str(Path(temp)/'global/PIL/__init__.py'))}):
                with self.assertRaisesRegex(ProtocolError,'another Pillow'):
                    image_module(site)
            # Windows cannot delete a mapped .pyd. Keep real import/codec work in
            # an owned CPU child that exits before TemporaryDirectory cleanup.
            script=("import sys,io; sys.path.insert(0,"+repr(str(ROOT/'tools'))+"); "
                    "from demo_differential.comparison_dependencies import image_module; "
                    "library=image_module("+repr(str(site))+"); "
                    "assert library.__version__=="+repr(package['version'])+"; "
                    "image=library.new('RGBA',(3,2),(12,34,56,255)); "
                    "buffer=io.BytesIO(); image.save(buffer,format='PNG'); buffer.seek(0); "
                    "assert library.open(buffer).convert('RGBA').tobytes()==image.tobytes()")
            subprocess.run([sys.executable,'-I','-S','-B','-c',script],check=True,timeout=30)
            target=site/'PIL/Image.py';target.write_bytes(target.read_bytes()+b'\n# corrupted\n')
            with self.assertRaises(ProtocolError): verify(site,package)
            with self.assertRaises(ProtocolError): prepare(output,archive=archive)

    def test_wrong_abi_is_binary_compatibility_not_patch_pin(self):
        from collections import namedtuple
        Version=namedtuple('Version','major minor micro releaselevel serial')
        actual=sys.version_info
        package=wheel_profile()
        for micro in (0,999):
            with patch.object(sys,'version_info',Version(actual.major,actual.minor,micro,'final',0)):
                self.assertEqual(wheel_profile(),package)
        with patch.object(sys,'version_info',Version(actual.major,actual.minor-1,0,'final',0)):
            with self.assertRaises(ProtocolError):wheel_profile()


if __name__=='__main__':unittest.main()
