"""Check Checkbox snapshots add one section without altering existing golden fields."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[5]
BASELINE = "206bb5e"


def main():
    files = sorted((ROOT / "design-tokens/ant-design/6.6.5/golden").glob("*.json"))
    if len(files) != 5:
        raise RuntimeError("Unexpected Theme golden inventory")
    for path in files:
        relative = path.relative_to(ROOT).as_posix()
        old = json.loads(subprocess.check_output(["git", "show", BASELINE + ":" + relative], cwd=ROOT))
        new = json.loads(path.read_text(encoding="utf-8"))
        checkbox = new.pop("checkbox")
        if set(checkbox) != {"metrics", "effects", "colors"} or len(checkbox["metrics"]) != 8 or len(checkbox["effects"]) != 5 or len(checkbox["colors"]) != 10:
            raise RuntimeError(path.name + ": incomplete Checkbox token section")
        for value in (old, new):
            value.pop("identity")
        if old != new:
            raise RuntimeError(path.name + ": existing Theme golden field changed")
        print(path.name + ": additive Checkbox section; existing fields unchanged")


if __name__ == "__main__":
    main()
