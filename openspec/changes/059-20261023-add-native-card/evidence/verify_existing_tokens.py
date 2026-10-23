"""Check Card's addition against the pre-implementation snapshot values."""
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[4]
for path in sorted((root / 'design-tokens/ant-design/6.6.5/golden').glob('*.json')):
    previous = json.loads(subprocess.check_output(
        ['git', 'show', '0ef80e0:' + path.relative_to(root).as_posix()], cwd=root))
    current = json.loads(path.read_text(encoding='utf-8'))
    card = current.pop('card')
    assert len(card['colors']) == 9 and len(card['metrics']) == 12
    previous.pop('identity')
    current.pop('identity')
    assert previous == current, path.name + ': existing Token values changed'
    print(path.name + ': existing Tokens unchanged; Card added')
