"""Confirm Radio's schema addition leaves every previous golden field intact."""
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[5]
directory = root / "design-tokens/ant-design/6.6.5/golden"
for file in sorted(directory.glob("*.json")):
    path = file.relative_to(root).as_posix()
    previous = json.loads(subprocess.check_output(["git", "show", f"d2132a9:{path}"], cwd=root))
    current = json.loads(file.read_text(encoding="utf-8"))
    radio = current.pop("radio")
    assert len(radio["metrics"]) == 16 and len(radio["effects"]) == 5 and len(radio["colors"]) == 21
    assert previous.pop("identity") != current.pop("identity")
    assert previous == current, f"pre-existing fields changed: {path}"
    print(f"unchanged previous token fields: {file.name}; Radio 16/5/21")
