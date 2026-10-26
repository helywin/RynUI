"""Run the explicitly authorized OpenSpec 1.14 entry and preserve native output/exit."""
import subprocess
import sys
from pathlib import Path

node = 'C:/Program Files/nodejs/node.exe'
cli = ('C:/Users/jiang/AppData/Local/pnpm/global/v11/'
       '571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/'
       'node_modules/@fission-ai/openspec/bin/openspec.js')
for name, args in (
    ('version.log', ['--version']),
    ('doctor.json', ['doctor', '--json']),
    ('strict.log', ['validate', '--all', '--strict', '--no-interactive']),
    ('apply.json', ['instructions', 'apply', '--change', '092-20261026-add-native-auto-complete', '--json']),
):
    result = subprocess.run([node, cli, *args], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    Path('out/092-R3-' + name).write_bytes(result.stdout)
    print(name, 'native exit', result.returncode)
    if result.returncode:
        sys.stdout.buffer.write(result.stdout)
        raise SystemExit(result.returncode)
