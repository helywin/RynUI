"""Verify that Tabs adds a snapshot group without changing previous Token values."""
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[4]
for path in sorted((root / 'design-tokens/ant-design/6.6.5/golden').glob('*.json')):
    previous = json.loads(subprocess.check_output(
        ['git', 'show', 'd326573:' + path.relative_to(root).as_posix()], cwd=root))
    current = json.loads(path.read_text(encoding='utf-8'))
    tabs = current.pop('tabs')
    assert len(tabs['colors']) == 11 and len(tabs['metrics']) == 17
    previous.pop('identity')
    current.pop('identity')
    assert previous == current, path.name + ': previous Token values changed'
    print(path.name + ': previous Tokens unchanged; Tabs added')
