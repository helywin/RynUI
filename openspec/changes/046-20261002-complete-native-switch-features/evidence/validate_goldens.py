"""Verify additive Switch/Alias tokens preserve all prior diagnostic values."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[4]
BASE = "485c9f1"
ADDED = {
    "innerMinMargin", "innerMaxMargin", "innerMinMarginSM", "innerMaxMarginSM",
    "contentFontSize", "loadingOpacity", "waveSpread", "waveWidth", "waveOpacity", "handleShadow",
    "colors", "focusWidth", "focusOffset",
}

for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
    relative = f"design-tokens/ant-design/6.6.5/golden/{name}.json"
    before = json.loads(subprocess.check_output(["git", "show", BASE + ":" + relative], cwd=ROOT))
    after = json.loads((ROOT / relative).read_text(encoding="utf-8"))
    assert set(after["switch"]) - set(before["switch"]) == ADDED
    assert set(after["alias"]) - set(before["alias"]) == {"opacityLoading"}
    assert after["identity"] != before["identity"]
    for key in ADDED:
        del after["switch"][key]
    del after["alias"]["opacityLoading"]
    del before["identity"]
    del after["identity"]
    assert before == after, name + " changed a pre-existing diagnostic field"
    print(name + ": additive Switch/Alias tokens; all prior values preserved")
