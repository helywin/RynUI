"""Check that adding Select preserves every existing diagnostic Token value."""

import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[4]
directory = Path("design-tokens/ant-design/6.6.5/golden")
for path in sorted((root / directory).glob("*.json")):
    previous = json.loads(subprocess.check_output(
        ["git", "show", "4215d5f:" + path.relative_to(root).as_posix()], cwd=root
    ))
    current = json.loads(path.read_text(encoding="utf-8"))
    added = current.pop("select")
    assert added["metrics"][0] > 0 and len(added["colors"]) == 11
    previous.pop("identity")
    current.pop("identity")
    assert current == previous, path.name + ": an existing Token changed"
    print(path.name + ": existing Tokens unchanged; Select added")
