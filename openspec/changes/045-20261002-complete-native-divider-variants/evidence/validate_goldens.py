"""Verify additive Divider metrics while preserving every existing diagnostic value."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[4]
BASE = "7e2eba9"
ADDED = {"smallHorizontalMargin", "middleHorizontalMargin"}

for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
    relative = f"design-tokens/ant-design/6.6.5/golden/{name}.json"
    before = json.loads(subprocess.check_output(["git", "show", BASE + ":" + relative], cwd=ROOT))
    after = json.loads((ROOT / relative).read_text(encoding="utf-8"))
    assert set(after["divider"]["metrics"]) - set(before["divider"]["metrics"]) == ADDED
    assert after["identity"] != before["identity"]
    for key in ADDED:
        del after["divider"]["metrics"][key]
    del before["identity"]
    del after["identity"]
    assert before == after, name + " changed a pre-existing diagnostic field"
    print(name + ": additive Divider metrics; all prior values preserved")
