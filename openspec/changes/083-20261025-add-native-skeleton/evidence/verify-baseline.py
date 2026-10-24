"""Verify this change against the parent's exact source and typed golden values."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[4]
BASE = "580a8c3d235c22eb95b11688b509ee1e2ea65e44"


def original(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode("utf-8")


source_path = "src/theme/theme_snapshot.cpp"
before_source = original(source_path).split("ThemeSnapshot resolve_theme(", 1)[1]
after_source = (ROOT / source_path).read_text(encoding="utf-8").split("ThemeSnapshot resolve_theme(", 1)[1]
pattern = r"^([ ]+)(?:auto )?(\w+) = \[&\] \{\n.*?^\1\}\(\);"
before_blocks = list(re.finditer(pattern, before_source, re.S | re.M))
after_blocks = list(re.finditer(pattern, after_source, re.S | re.M))
assert len(before_blocks) == 31 and len(after_blocks) == 32
assert [m.group(2) for m in before_blocks] == [m.group(2) for m in after_blocks[:31]]
for before, after in zip(before_blocks, after_blocks):
    assert before.group() == after.group(), f"Old IIFE changed: {before.group(2)}"

values = 0


def compare(before, after, path):
    global values
    assert type(before) is type(after), f"Type changed: {path}"
    if isinstance(before, dict):
        assert before.keys() == after.keys(), f"Keys changed: {path}"
        for key in before:
            compare(before[key], after[key], f"{path}.{key}")
    elif isinstance(before, list):
        assert len(before) == len(after), f"Length changed: {path}"
        for i, (a, b) in enumerate(zip(before, after)):
            compare(a, b, f"{path}[{i}]")
    else:
        assert before == after, f"Value changed: {path}: {before!r} != {after!r}"
        values += 1


for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
    path = f"design-tokens/ant-design/6.6.5/golden/{name}.json"
    before = json.loads(original(path))
    raw = (ROOT / path).read_bytes()
    after = json.loads(raw)
    assert "skeleton" not in before and "skeleton" in after
    skeleton = after.pop("skeleton")
    assert len(skeleton["colors"]) == 1 and len(skeleton["metrics"]) == 13
    before_identity = before.pop("identity")
    after_identity = after.pop("identity")
    assert before_identity != after_identity
    compare(before, after, name)
    print(f"{name}: old typed values preserved; SHA256={hashlib.sha256(raw).hexdigest()}")

print(f"PASS: {values} old scalar typed values; all 31 IIFE texts/order unchanged; five Skeleton additions only")
