"""Validate fresh Theme JSON semantics and preserve baseline component IIFEs."""
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[4]
BASE = "d89eac780eb4eb981813a5704dd51e7e79d4b2a9"
GOLDENS = Path("design-tokens/ant-design/6.6.5/golden")


def committed(path):
    return subprocess.check_output(["git", "show", BASE + ":" + str(path).replace("\\", "/")], cwd=ROOT).decode("utf-8")


def compare(old, new, path=""):
    assert type(old) is type(new), (path, type(old), type(new))
    if isinstance(old, dict):
        return sum(compare(value, new[key], path + "." + key) for key, value in old.items())
    if isinstance(old, list):
        assert len(old) == len(new), path
        return sum(compare(a, b, path + f"[{i}]") for i, (a, b) in enumerate(zip(old, new)))
    assert old == new, (path, old, new)
    return 1


total = 0
files = sorted((ROOT / GOLDENS).glob("*.json"))
assert len(files) == 5
for file in files:
    old = json.loads(committed(file.relative_to(ROOT)))
    new = json.loads(file.read_text(encoding="utf-8"))
    assert set(new) - set(old) in ({"breadcrumb"}, {"breadcrumb", "steps"})
    assert type(old.pop("identity")) is type(new.pop("identity"))
    count = compare(old, new)
    total += count
    print(f"{file.stem}: {count} old typed leaf values preserved")

source = Path("src/theme/theme_snapshot.cpp")
pattern = re.compile(r"^( +)(?:auto )?(\w+) = \[&\] \{\n.*?^\1\}\(\);", re.M | re.S)
old_blocks = [(m.group(2), m.group(0)) for m in pattern.finditer(committed(source))]
new_blocks = [(m.group(2), m.group(0)) for m in pattern.finditer((ROOT / source).read_text(encoding="utf-8"))]
assert len(old_blocks) == 28, len(old_blocks)
assert [block for block in new_blocks if block[0] in {name for name, _ in old_blocks}] == old_blocks
print(f"PASS: 5 fresh goldens, {total} old typed leaves, all 28 original IIFEs unchanged and ordered")
