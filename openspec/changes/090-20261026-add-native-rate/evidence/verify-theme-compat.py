"""Compare this change's fresh goldens and append-only Theme edits to its exact local baseline."""
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[4]
BASE = "8056de67953ee73015c7559f1782033d85e08573"


def original(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode("utf-8")


def leaves(value):
    if isinstance(value, dict):
        return sum(leaves(v) for v in value.values())
    if isinstance(value, list):
        return sum(leaves(v) for v in value)
    return 1


total = 0
directory = Path("design-tokens/ant-design/6.6.5/golden")
files = sorted((ROOT / directory).glob("*.json"))
assert len(files) == 5
for path in files:
    old = json.loads(original(path.relative_to(ROOT).as_posix()))
    new = json.loads(path.read_text(encoding="utf-8"))
    assert "rate" not in old and "rate" in new
    del new["rate"]
    del old["identity"]
    del new["identity"]
    assert old == new, path.name
    count = leaves(old)
    total += count
    print(f"{path.name}: {count} unchanged old typed leaves")

path = "src/theme/theme_snapshot.cpp"
old = original(path)
new = (ROOT / path).read_text(encoding="utf-8")
pattern = r"    auto \w+ = \[&\] \{.*?    \}\(\);"
before = re.findall(pattern, old, re.S)
after = re.findall(pattern, new, re.S)
assert before and after[:len(before)] == before and len(after) == len(before) + 1
print(f"{len(before)} original IIFEs unchanged in original order")
path = "src/theme/theme_runtime_types.hpp"
enum = r"enum class TokenIdentity[^\{]*\{(.*?)\n\};"
old = re.search(enum, original(path), re.S)[1]
new = re.search(enum, (ROOT / path).read_text(encoding="utf-8"), re.S)[1]
old = old[:old.rfind("    count,")]
assert new.startswith(old)
identities = len(re.findall(r"^    \w+(?:\s*=.*)?,", old, re.M))
print(f"{identities} original TokenIdentity entries unchanged; {total} old typed leaves compared")
