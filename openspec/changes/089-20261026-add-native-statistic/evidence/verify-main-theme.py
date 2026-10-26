"""Verify an integrated Theme against both published and incoming local commits."""
import json
import re
import subprocess
import sys
from pathlib import Path

parent, incoming, fresh_dir = sys.argv[1:]
root = Path.cwd()


def previous(commit, path):
    return subprocess.check_output(["git", "show", f"{commit}:{path}"], cwd=root).decode("utf-8")


def recipes(source):
    return [match.group(0) for match in re.finditer(
        r"^( +)(?:auto )?\w+ = \[&\] \{.*?\n\1\}\(\);", source, re.S | re.M)]


def identities(source):
    body = re.search(r"enum class TokenIdentity[^\{]*\{(.*?)\n\};", source, re.S).group(1)
    return [part.strip() for part in re.sub(r"//[^\n]*", "", body).split(",") if part.strip()]


def subset(old, new, path=""):
    assert type(old) is type(new), (path, type(old).__name__, type(new).__name__)
    if isinstance(old, dict):
        return sum(subset(value, new[key], path + "." + key)
                   for key, value in old.items() if path or key != "identity")
    if isinstance(old, list):
        assert len(old) == len(new), (path, len(old), len(new))
        return sum(subset(a, b, path + f"[{i}]") for i, (a, b) in enumerate(zip(old, new)))
    assert old == new, (path, old, new)
    return 1


snapshot = "src/theme/theme_snapshot.cpp"
actual_recipes = recipes((root / snapshot).read_text(encoding="utf-8"))
parent_recipes = recipes(previous(parent, snapshot))
incoming_recipes = recipes(previous(incoming, snapshot))
additions = [item for item in incoming_recipes if item not in parent_recipes]
assert actual_recipes == parent_recipes + additions, "IIFE text/order changed or unreviewed recipe added"
for recipe in incoming_recipes:
    assert actual_recipes.count(recipe) == 1, "Incoming recipe changed or duplicated"

runtime_types = "src/theme/theme_runtime_types.hpp"
actual_ids = identities((root / runtime_types).read_text(encoding="utf-8"))
parent_ids = identities(previous(parent, runtime_types))
incoming_ids = identities(previous(incoming, runtime_types))
assert parent_ids[-1] == incoming_ids[-1] == "count"
extra_ids = [name for name in incoming_ids[:-1] if name not in parent_ids]
assert actual_ids == parent_ids[:-1] + extra_ids + ["count"], "TokenIdentity prefix/order changed"

if fresh_dir == "--source-only":
    print(f"IIFEs preserved: {len(parent_recipes)} parent + {len(additions)} incoming")
    print(f"TokenIdentity append-only: {len(parent_ids)-1} parent + {len(extra_ids)} incoming")
    sys.exit(0)

for commit in (parent, incoming):
    leaves = 0
    for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
        path = f"design-tokens/ant-design/6.6.5/golden/{name}.json"
        old = json.loads(previous(commit, path))
        fresh_bytes = (Path(fresh_dir) / (name + ".json")).read_bytes()
        new = json.loads(fresh_bytes.decode("utf-8"))
        leaves += subset(old, new)
        assert fresh_bytes == (root / path).read_bytes(), f"Fresh/committed snapshot differs: {name}"
    print(f"{commit}: {leaves} old typed leaves retained")
print(f"IIFEs: {len(parent_recipes)} parent + {len(additions)} incoming, all exact")
print(f"TokenIdentity: {len(parent_ids)-1} parent + {len(extra_ids)} incoming, append-only")
