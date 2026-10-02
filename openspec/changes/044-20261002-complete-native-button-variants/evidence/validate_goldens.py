"""Validate the additive Button diagnostic snapshot migration against the preceding phase."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[4]
BASE = "226a6d8"
ADDED = {"borderWidth", "dashLength", "dashGap", "waveSpread", "waveWidth", "waveOpacity",
         "variantColors", "variantAppearance"}

for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
    relative = f"design-tokens/ant-design/6.6.5/golden/{name}.json"
    before = json.loads(subprocess.check_output(["git", "show", BASE + ":" + relative], cwd=ROOT))
    after = json.loads((ROOT / relative).read_text(encoding="utf-8"))
    assert set(after["button"]) - set(before["button"]) == ADDED
    assert len(after["button"]["variantColors"]) == 16
    assert all(len(item["colors"]) == 7 for item in after["button"]["variantColors"])
    assert len(after["button"]["variantAppearance"]) == 10
    assert after["identity"] != before["identity"]
    for key in ADDED:
        del after["button"][key]
    del before["identity"]
    del after["identity"]
    assert before == after, name + " changed a pre-existing diagnostic field"
    print(name + ": additive Button diagnostics; all prior values preserved")
