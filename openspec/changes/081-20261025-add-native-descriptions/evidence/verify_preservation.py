"""Compare freshly generated Theme goldens and retained resolver blocks to the parent commit."""
from pathlib import Path
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[4]
BASES = {
    "62c990a1b494532c358ac487b2bb5931c08a7231": (28, {"breadcrumb", "steps", "descriptions"}),
    "c083fed5a72f644b491118c6440908e0354d241e": (30, {"descriptions"}),
}


def old(base, path):
    return subprocess.check_output(["git", "show", f"{base}:{path}"], cwd=ROOT).decode("utf-8")


def compare(a, b, path):
    assert type(a) is type(b), f"type changed at {path}"
    if isinstance(a, dict):
        assert a.keys() == b.keys(), f"keys changed at {path}"
        return sum(compare(a[k], b[k], f"{path}.{k}") for k in a)
    if isinstance(a, list):
        assert len(a) == len(b), f"length changed at {path}"
        return sum(compare(x, y, f"{path}[{i}]") for i, (x, y) in enumerate(zip(a, b)))
    assert a == b, f"value changed at {path}: {a!r} != {b!r}"
    return 1


for base, (blocks, additions) in BASES.items():
    total = 0
    for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
        path = f"design-tokens/ant-design/6.6.5/golden/{name}.json"
        before = json.loads(old(base, path))
        after = json.loads((ROOT / path).read_text(encoding="utf-8"))
        assert after.keys() - before.keys() == additions
        assert before["identity"] != after["identity"]
        del before["identity"]
        del after["identity"]
        for key in additions:
            del after[key]
        count = compare(before, after, name)
        total += count
        print(f"{base[:7]} {name}: {count} old typed values unchanged")

    path = "src/theme/theme_snapshot.cpp"
    pattern = r"^( +)(?:auto )?\w+ = \[&\] \{.*?^\1\}\(\);"
    before = [m.group() for m in re.finditer(pattern, old(base, path), re.M | re.S)]
    after = [m.group() for m in re.finditer(pattern, (ROOT / path).read_text(encoding="utf-8"), re.M | re.S)]
    assert len(before) == blocks, f"unexpected parent block count: {len(before)}"
    assert len(after) == 31 and after[:blocks] == before, "old resolver block text/order changed"
    print(f"PASS {base[:7]}: 5 fresh goldens; {total} old typed values; {blocks} old resolver blocks unchanged")
