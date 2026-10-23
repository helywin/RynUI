"""Verify Modal adds one snapshot group and preserves every prior Token value."""
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[4]
for path in sorted((root / 'design-tokens/ant-design/6.6.5/golden').glob('*.json')):
    previous = json.loads(subprocess.check_output(
        ['git', 'show', 'd1365a5:' + path.relative_to(root).as_posix()], cwd=root))
    current = json.loads(path.read_text(encoding='utf-8'))
    modal = current.pop('modal')
    assert len(modal['colors']) == 10 and len(modal['metrics']) == 11
    previous.pop('identity')
    current.pop('identity')
    assert previous == current, path.name + ': previous Token values changed'
    print(path.name + ': previous Tokens unchanged; Modal added')
