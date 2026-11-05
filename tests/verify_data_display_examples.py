#!/usr/bin/env python3
"""Compile the real old/new public examples without inventing helper APIs."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    count = 0
    for name in ('collapse', 'segmented', 'descriptions', 'timeline'):
        for body in re.findall(r'```cpp\n(.*?)```', (ROOT / 'docs' / f'{name}.md').read_text(), re.S):
            body = re.sub(r'^#include.*$', '', body, flags=re.M)
            if not re.search(r'\bvoid\s+\w+\s*\(', body):
                body = 'void example() {\n' + body + '\n}\n'
            source = '#include <ryn/rynui.hpp>\n' + body
            with tempfile.TemporaryDirectory(dir=ROOT / 'out') as directory:
                path = Path(directory) / 'example.cpp'
                path.write_text(source)
                subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-fsyntax-only', '-I', str(ROOT / 'include'), str(path)], check=True)
            count += 1
    print(f'{count} DataDisplay documentation examples compiled')

if __name__ == '__main__':
    main()
