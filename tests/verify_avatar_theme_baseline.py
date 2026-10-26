"""Compare fresh Avatar snapshots and existing recipe/identity order to the local base commit."""
import json
import re
import subprocess
import sys
from pathlib import Path

BASE = "8056de67953ee73015c7559f1782033d85e08573"
ROOT = Path(__file__).resolve().parents[1]

def baseline(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode("utf-8")

def recipes(source):
    return re.findall(r"    auto \w+ = \[&\] \{.*?\n    \}\(\);", source, re.S)

def identities(source):
    enum = re.search(r"enum class TokenIdentity[^\{]*\{(.*?)\n\};", source, re.S).group(1)
    return [part.strip() for part in re.sub(r"//[^\n]*", "", enum).split(",") if part.strip()]

def main():
    fresh = Path(sys.argv[1])
    old = recipes(baseline("src/theme/theme_snapshot.cpp"))
    source = (ROOT / "src/theme/theme_snapshot.cpp").read_text(encoding="utf-8")
    previous = baseline("src/theme/theme_snapshot.cpp")
    new = recipes(source)
    assert new[:len(old)] == old and len(new) == len(old) + 1, "old IIFE text/order changed"
    assert previous[previous.index("    auto button = [&]"):previous.rindex("    return ThemeSnapshot(")] == source[source.index("    auto button = [&]"):source.index("    auto avatar = [&]")], "old component derivation text changed"
    ids = identities(baseline("src/theme/theme_runtime_types.hpp"))
    current = identities((ROOT / "src/theme/theme_runtime_types.hpp").read_text(encoding="utf-8"))
    assert current == ids[:-1] + ["avatar_colors", "avatar_metrics", "count"], "old identity order changed"
    fields = 0
    for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
        path = f"design-tokens/ant-design/6.6.5/golden/{name}.json"
        expected = json.loads(baseline(path))
        actual = json.loads((fresh / f"{name}.json").read_text(encoding="utf-8"))
        assert "avatar" in actual and "avatar" not in expected
        assert actual.pop("identity") != expected.pop("identity")
        actual.pop("avatar")
        assert actual == expected, f"old typed fields changed: {name}"
        fields += len(expected)
    print(f"Avatar baseline passed: {len(old)} old IIFEs unchanged; {len(ids)-1} old identities unchanged; 5 fresh goldens deeply equal across {fields} old top-level fields")

if __name__ == "__main__":
    main()
