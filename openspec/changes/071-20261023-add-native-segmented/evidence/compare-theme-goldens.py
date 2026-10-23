"""Compare every old Theme field against the committed 6968904 baseline."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[4]
DIRECTORY = Path("design-tokens/ant-design/6.6.5/golden")


def compare(old, new):
    if isinstance(old, dict):
        return sum(compare(value, new[key]) for key, value in old.items())
    if isinstance(old, list):
        assert isinstance(new, list) and len(old) == len(new)
        return sum(compare(left, right) for left, right in zip(old, new))
    assert old == new, (old, new)
    return 1


total = 0
paths = sorted((ROOT / DIRECTORY).glob("*.json"))
assert len(paths) == 5
for path in paths:
    relative = path.relative_to(ROOT).as_posix()
    old = json.loads(subprocess.check_output(["git", "show", "6968904:" + relative], cwd=ROOT))
    new = json.loads(path.read_text(encoding="utf-8"))
    assert set(new) - set(old) == {"segmented"}
    assert old.pop("identity") != new.pop("identity")
    count = compare(old, new)
    total += count
    print(f"{path.stem}: {count} old leaf fields preserved")
print(f"PASS: {len(paths)} goldens, {total} old leaf fields preserved against 6968904")
