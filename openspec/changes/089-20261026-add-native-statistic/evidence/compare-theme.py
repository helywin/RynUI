"""Verify additive Theme output against the exact Statistic parent commit."""
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[4]
BASE = "8056de67953ee73015c7559f1782033d85e08573"


def original(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode("utf-8")


def compare(old, new, path=""):
    if isinstance(old, dict):
        assert set(new) == set(old), path
        return sum(compare(value, new[key], f"{path}/{key}") for key, value in old.items())
    if isinstance(old, list):
        assert isinstance(new, list) and len(old) == len(new), path
        return sum(compare(a, b, f"{path}/{i}") for i, (a, b) in enumerate(zip(old, new)))
    assert old == new, (path, old, new)
    return 1


total = 0
paths = sorted((ROOT / "design-tokens/ant-design/6.6.5/golden").glob("*.json"))
assert len(paths) == 5
for path in paths:
    old = json.loads(original(path.relative_to(ROOT).as_posix()))
    new = json.loads(path.read_text(encoding="utf-8"))
    assert set(new) - set(old) == {"statistic"}
    assert old.pop("identity") != new.pop("identity")
    new.pop("statistic")
    count = compare(old, new)
    total += count
    print(f"{path.stem}: {count} old leaves preserved")

source = "src/theme/theme_snapshot.cpp"
pattern = r"^([ \t]*)(?:auto )?\w+ = \[&\] \{\n.*?^\1\}\(\);"
old_iife = [m.group() for m in re.finditer(pattern, original(source), re.S | re.M)]
new_iife = [m.group() for m in re.finditer(pattern, (ROOT / source).read_text(encoding="utf-8"), re.S | re.M)]
assert old_iife == new_iife[:-1], "Existing IIFE text or order drifted"
identities = "src/theme/theme_runtime_types.hpp"
old_enum = original(identities).split("enum class TokenIdentity", 1)[1].split("count,", 1)[0]
new_enum = (ROOT / identities).read_text(encoding="utf-8").split("enum class TokenIdentity", 1)[1].split("statistic_colors,", 1)[0]
assert old_enum == new_enum, "Existing TokenIdentity order drifted"
old_count = len(re.findall(r"^    \w+,", old_enum, re.M))
print(f"PASS: 5 fresh goldens, {total} old leaves, {len(old_iife)} unchanged IIFEs, {old_count} unchanged TokenIdentities")
