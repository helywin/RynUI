"""Verify both reviewed parents' typed Theme values and Tree-first append ordering."""
from pathlib import Path
import argparse
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[4]
TREE = "2fd96f6209e817e892f7684c7eae6891ee576d10"
AC = "ad779c595a63a39d05aa787d38dccda1fcaea9c7"


def source(ref, path):
    return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT).decode("utf-8").replace("\r\n", "\n")


def recipes(text):
    return [m.group(0) for m in re.finditer(r"^( +)(?:auto )?\w+ = \[&\] \{.*?\n\1\}\(\);", text, re.M | re.S)]


def identities(text):
    body = re.search(r"enum class TokenIdentity[^\{]*\{(.*?)\n\};", text, re.S).group(1)
    return [p.strip() for p in re.sub(r"//[^\n]*", "", body).split(",") if p.strip() and p.strip() != "count"]


def common_prefix(a, b):
    n = 0
    while n < min(len(a), len(b)) and a[n] == b[n]:
        n += 1
    return n


def typed_equal(a, b):
    if type(a) is not type(b):
        return False
    if isinstance(a, dict):
        return a.keys() == b.keys() and all(typed_equal(v, b[k]) for k, v in a.items())
    if isinstance(a, list):
        return len(a) == len(b) and all(typed_equal(x, y) for x, y in zip(a, b))
    return a == b


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--update-goldens", action="store_true")
    args = parser.parse_args()
    for path, parse, name in (("src/theme/theme_snapshot.cpp", recipes, "IIFEs"),
                              ("src/theme/theme_runtime_types.hpp", identities, "TokenIdentity ordinals")):
        tree, ac = [parse(source(ref, path)) for ref in (TREE, AC)]
        merged = parse((ROOT / path).read_text(encoding="utf-8"))
        common = common_prefix(tree, ac)
        assert merged == tree + ac[common:], name
        print(f"{name}: exact Tree prefix {len(tree)} + AC suffix {len(ac)-common}; old common prefix {common}")
    pending = []
    for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
        path = f"design-tokens/ant-design/6.6.5/golden/{name}.json"
        a, b = [json.loads(source(ref, path)) for ref in (TREE, AC)]
        fresh = (ROOT / "out/tree-ac-goldens-tree" / f"{name}.json").read_bytes()
        other = (ROOT / "out/tree-ac-goldens-ac" / f"{name}.json").read_bytes()
        assert fresh == other, f"independent production exporters disagree: {name}"
        merged = json.loads(fresh)
        assert merged.keys() == a.keys() | b.keys(), f"unexpected key addition/removal: {name}"
        for label, old in (("Tree", a), ("AC", b)):
            for key, value in old.items():
                if key != "identity":
                    assert typed_equal(value, merged[key]), f"{name}/{label}/{key}: typed value changed"
            print(f"{name}/{label}: {len(old)-1} existing top-level fields and all typed leaves retained")
        pending.append((ROOT / path, fresh))
        if not args.update_goldens:
            assert (ROOT / path).read_bytes() == fresh, f"tracked combined golden differs: {name}"
    if args.update_goldens:
        for path, fresh in pending:
            path.write_bytes(fresh)
        print("five combined goldens updated only after both parents and both production exporters matched")
    else:
        print("five tracked combined goldens exactly match both fresh production exporters")


if __name__ == "__main__":
    main()
