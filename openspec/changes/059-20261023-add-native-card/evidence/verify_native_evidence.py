"""Verify saved Card native readbacks, logs, matrix and executable provenance."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('--check-executables', action='store_true')
args = parser.parse_args()
base = Path(__file__).resolve().parent / 'windows'
root = Path(__file__).resolve().parents[4]
total = 0
for configuration in ['Debug', 'Release']:
    report = json.loads((base / ('windows-' + configuration.lower() + '.json')).read_text(encoding='utf-8'))
    assert report['configuration'] == configuration and report['log_sha256_format'] == 'UTF-8 LF'
    assert report['preset'] == 'windows-msvc' and report['window_system'] == 'win32'
    assert report['gpu'] == 'direct3d12' and report['shader'] == 'DXIL'
    expected = {(theme, scale) for theme in ['light', 'dark'] for scale in ['system', '1', '1.25', '1.5', '2']}
    assert len(report['runs']) == 10
    assert {(run['theme'], run['requested_scale']) for run in report['runs']} == expected
    if args.check_executables:
        exe = root / 'out/build/windows-msvc/examples' / configuration / 'rynui_token_gallery.exe'
        assert hashlib.sha256(exe.read_bytes()).hexdigest() == report['executable_sha256']
    for run in report['runs']:
        log_path = base / run['log']
        log = log_path.read_bytes().replace(b'\r\n', b'\n')
        assert hashlib.sha256(log).hexdigest() == run['log_sha256']
        assert b'disposed=1 exit_code=0' in log and run['exit_code'] == 0
        for marker in [b'card_acceptance=passed gpu_driver=direct3d12 shader_format=DXIL',
                       b'nested_input=passed automatic_ime=passed hover=passed loading=passed restoration=passed actions=passed',
                       b'narrow=passed resize=passed idle_polls=3 identity=passed', b'font=400', b'font=600']:
            assert marker in log, (log_path, marker)
        assert len(run['png_sha256']) == 18
        for name, expected_hash in run['png_sha256'].items():
            png = log_path.parent / name
            assert hashlib.sha256(png.read_bytes()).hexdigest() == expected_hash
            with Image.open(png) as picture:
                picture.verify()
            total += 1
assert total == 360
print('Verified 20 native runs, 360 PNGs and canonical UTF-8 LF log hashes' +
      (' plus both local EXE hashes.' if args.check_executables else '.'))
