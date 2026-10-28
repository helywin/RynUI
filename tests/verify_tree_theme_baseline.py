"""Compare fresh Tree snapshots and retained recipe/identity order to the pinned local base."""
import json
import re
import subprocess
import sys
from pathlib import Path

BASE = "34eefb82c57f3d3bdf687db477aef9de9247b4b3"
ROOT = Path(__file__).resolve().parents[1]


def baseline(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode("utf-8")


def recipes(source):
    return [m.group(0) for m in re.finditer(
        r"^( +)(?:auto )?\w+ = \[&\] \{.*?\n\1\}\(\);", source, re.S | re.M)]


def identities(source):
    body = re.search(r"enum class TokenIdentity[^\{]*\{(.*?)\n\};", source, re.S).group(1)
    return [p.strip() for p in re.sub(r"//[^\n]*", "", body).split(",") if p.strip()]


def main():
    fresh = Path(sys.argv[1])
    previous = baseline("src/theme/theme_snapshot.cpp")
    source = (ROOT / "src/theme/theme_snapshot.cpp").read_text(encoding="utf-8")
    old = recipes(previous)
    new = recipes(source)
    assert new[:len(old)] == old and len(new) == len(old) + 1, "old IIFE text/order changed"
    assert previous[previous.index("    auto button = [&]"):previous.rindex("    return ThemeSnapshot(")] == source[source.index("    auto button = [&]"):source.index("    auto tree = [&]")], "old component derivation text changed"
    ids = identities(baseline("src/theme/theme_runtime_types.hpp"))
    current = identities((ROOT / "src/theme/theme_runtime_types.hpp").read_text(encoding="utf-8"))
    assert current == ids[:-1] + ["tree_colors", "tree_metrics", "count"], "old identity order changed"
    fields = 0
    for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
        expected = json.loads(baseline(f"design-tokens/ant-design/6.6.5/golden/{name}.json"))
        actual = json.loads((fresh / f"{name}.json").read_text(encoding="utf-8"))
        assert "tree" in actual and "tree" not in expected
        assert actual.pop("identity") != expected.pop("identity")
        actual.pop("tree")
        assert actual == expected, f"old typed fields changed: {name}"
        fields += len(expected)
    print(f"Tree baseline passed: {len(old)} old IIFEs unchanged; {len(ids)-1} old identities unchanged; 5 fresh goldens deeply equal across {fields} old top-level fields")


if __name__ == "__main__":
    main()
