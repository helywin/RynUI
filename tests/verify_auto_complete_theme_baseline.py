"""Verify old recipes, ordinals and typed leaves against the exact integrated Avatar base."""
import json
import re
import subprocess
import sys
from pathlib import Path

BASE = "6118f67728aede9985867377b122274ac86f31d7"
ROOT = Path(__file__).resolve().parents[1]

def baseline(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode("utf-8")

def recipes(source):
    return [m.group(0) for m in re.finditer(r"^( +)(?:auto )?\w+ = \[&\] \{.*?\n\1\}\(\);", source, re.S | re.M)]

def identities(source):
    enum = re.search(r"enum class TokenIdentity[^\{]*\{(.*?)\n\};", source, re.S).group(1)
    return [p.strip() for p in re.sub(r"//[^\n]*", "", enum).split(",") if p.strip()]

def typed_equal(expected, actual):
    if type(expected) is not type(actual):
        return False
    if isinstance(expected, dict):
        return expected.keys() == actual.keys() and all(typed_equal(v, actual[k]) for k, v in expected.items())
    if isinstance(expected, list):
        return len(expected) == len(actual) and all(typed_equal(a, b) for a, b in zip(expected, actual))
    return expected == actual

def main():
    fresh = Path(sys.argv[1])
    path = "src/theme/theme_snapshot.cpp"
    old = baseline(path)
    current = (ROOT / path).read_text(encoding="utf-8")
    assert recipes(current)[:-1] == recipes(old), "old IIFE text/order changed"
    assert old[old.index("    auto button = [&]"):old.rindex("    return ThemeSnapshot(")] == current[current.index("    auto button = [&]"):current.index("    auto auto_complete = [&]")], "old derivation text changed"
    path = "src/theme/theme_runtime_types.hpp"
    ids = identities(baseline(path))
    assert identities((ROOT / path).read_text(encoding="utf-8")) == ids[:-1] + ["auto_complete_colors", "auto_complete_metrics", "auto_complete_typography", "auto_complete_effects", "auto_complete_order", "count"]
    for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
        expected = json.loads(baseline(f"design-tokens/ant-design/6.6.5/golden/{name}.json"))
        actual = json.loads((fresh / f"{name}.json").read_text(encoding="utf-8"))
        assert actual.pop("identity") != expected.pop("identity")
        assert "autoComplete" in actual and "autoComplete" not in expected
        actual.pop("autoComplete")
        assert typed_equal(expected, actual), name
    print(f"AutoComplete baseline passed: {len(recipes(old))} old IIFEs; {len(ids)-1} old ordinals; 5 fresh goldens with all old typed leaves unchanged")

if __name__ == "__main__":
    main()
