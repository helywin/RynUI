#!/usr/bin/env python3
"""Compile the public Form examples directly from their documented source."""
import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True)
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
examples = re.findall(r'```cpp\n(.*?)```', (args.root / 'docs/form.md').read_text(encoding='utf-8'), re.S)
if len(examples) < 3:
    raise SystemExit('Form documentation examples are missing')
args.output.mkdir(parents=True, exist_ok=True)
source = args.output / 'form_documentation.cpp'
source.write_text('\n'.join(examples) + '\nint main() {}\n', encoding='utf-8')
msvc = Path(args.compiler).name.lower() in ('cl', 'cl.exe')
flags = ['/nologo', '/std:c++20', '/utf-8', '/Zs', '/I' + str(args.root / 'include')] if msvc else ['-std=c++20', '-fsyntax-only', '-I' + str(args.root / 'include')]
subprocess.run([args.compiler, *flags, str(source)], check=True)
print(f'Compiled {len(examples)} documented Form examples')
