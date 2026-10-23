"""Run Tabs acceptance on the actual Windows D3D12 window."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('configuration', choices=['Debug', 'Release'])
args = parser.parse_args()
root = Path(__file__).resolve().parents[4]
base = Path(__file__).resolve().parent / 'windows'
exe = root / 'out/build/windows-msvc/examples' / args.configuration / 'rynui_token_gallery.exe'


def sha(path):
    data = path.read_bytes()
    if path.suffix == '.log':
        data = data.replace(b'\r\n', b'\n')
    return hashlib.sha256(data).hexdigest()


exe_hash = sha(exe)
runs = []
for theme in ['light', 'dark']:
    for scale in ['system', '1', '1.25', '1.5', '2']:
        directory = base / args.configuration.lower() / theme / scale
        directory.mkdir(parents=True, exist_ok=True)
        command = [str(exe), '--tabs-acceptance', '--evidence-dir=' + str(directory)]
        if scale != 'system':
            command.append('--acceptance-scale=' + scale)
        if theme == 'dark':
            command.append('--acceptance-theme=dark')
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=90)
        log = result.stdout + result.stderr
        (directory / 'run.log').write_text(log, encoding='utf-8')
        if result.returncode or 'disposed=1 exit_code=0' not in log:
            raise RuntimeError(f'{theme}/{scale}: {log}')
        images = {}
        for bmp in sorted(directory.glob('*.bmp')):
            png = bmp.with_suffix('.png')
            with Image.open(bmp) as picture:
                pixels = picture.convert('RGB')
                assert all(high - low > 50 for low, high in pixels.getextrema()), (bmp, 'empty GPU readback')
                picture.save(png)
            images[png.name] = sha(png)
            bmp.unlink()
        assert len(images) == 18, (directory, len(images))
        actual_scale = float(re.search(r'render_scale=([\d.]+)', log).group(1))
        runs.append({'theme': theme, 'requested_scale': scale, 'render_scale': actual_scale,
                     'exit_code': result.returncode, 'log': str((directory / 'run.log').relative_to(base)),
                     'log_sha256': sha(directory / 'run.log'), 'png_sha256': images})
        print(f'{args.configuration} {theme} {scale}: passed; screenshots={len(images)}', flush=True)
assert sha(exe) == exe_hash, 'EXE changed during matrix'
report = {'configuration': args.configuration, 'preset': 'windows-msvc', 'window_system': 'win32',
          'gpu': 'direct3d12', 'shader': 'DXIL', 'executable_sha256': exe_hash, 'runs': runs,
          'log_sha256_format': 'UTF-8 LF',
          'input_scope': 'automated logical pointer/keyboard/composition on actual native window; not manual system IME'}
(base / ('windows-' + args.configuration.lower() + '.json')).write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
