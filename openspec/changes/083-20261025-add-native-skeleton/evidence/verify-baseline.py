"""Verify this change against the parent's exact source and typed golden values."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[4]
BASE = "580a8c3d235c22eb95b11688b509ee1e2ea65e44"


def original(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode("utf-8")


source_path = "src/theme/theme_snapshot.cpp"
before_source = original(source_path).split("ThemeSnapshot resolve_theme(", 1)[1]
after_source = (ROOT / source_path).read_text(encoding="utf-8").split("ThemeSnapshot resolve_theme(", 1)[1]
pattern = r"^([ ]+)(?:auto )?(\w+) = \[&\] \{\n.*?^\1\}\(\);"
before_blocks = list(re.finditer(pattern, before_source, re.S | re.M))
after_blocks = list(re.finditer(pattern, after_source, re.S | re.M))
assert len(before_blocks) == 31 and len(after_blocks) == 32
assert [m.group(2) for m in before_blocks] == [m.group(2) for m in after_blocks[:31]]
for before, after in zip(before_blocks, after_blocks):
    assert before.group() == after.group(), f"Old IIFE changed: {before.group(2)}"

runtime_path = "src/theme/theme_runtime.cpp"
old_runtime = original(runtime_path)
new_runtime = (ROOT / runtime_path).read_text(encoding="utf-8")
for signature in ("std::shared_ptr<ThemeScope> ThemeScope::create_default()",
                  "std::shared_ptr<const ThemeSnapshot> resolve_validation_snapshot(",
                  "void ThemeScope::validate_children(", "void ThemeScope::commit_snapshot("):
    function_pattern = re.escape(signature) + r".*?\n\}"
    assert re.search(function_pattern, old_runtime, re.S).group() == re.search(function_pattern, new_runtime, re.S).group(), signature

subprocess.run(["git", "diff", "--quiet", BASE, "--", "include/ryn/time_picker.hpp", "include/ryn/prop.hpp",
                "src/runtime/prop_connection.hpp", "src/component/theme_component.cpp"], cwd=ROOT, check=True)
identity_path = "src/theme/theme_runtime_types.hpp"
old_identity = original(identity_path).split("enum class TokenIdentity", 1)[1].split("    count,", 1)[0]
new_identity = (ROOT / identity_path).read_text(encoding="utf-8").split("enum class TokenIdentity", 1)[1].split("    count,", 1)[0]
assert new_identity == old_identity + "    skeleton_colors,\n    skeleton_metrics,\n"

values = 0


def compare(before, after, path):
    global values
    assert type(before) is type(after), f"Type changed: {path}"
    if isinstance(before, dict):
        assert before.keys() == after.keys(), f"Keys changed: {path}"
        for key in before:
            compare(before[key], after[key], f"{path}.{key}")
    elif isinstance(before, list):
        assert len(before) == len(after), f"Length changed: {path}"
        for i, (a, b) in enumerate(zip(before, after)):
            compare(a, b, f"{path}[{i}]")
    else:
        assert before == after, f"Value changed: {path}: {before!r} != {after!r}"
        values += 1


for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
    path = f"design-tokens/ant-design/6.6.5/golden/{name}.json"
    before = json.loads(original(path))
    raw = (ROOT / path).read_bytes()
    after = json.loads(raw)
    assert "skeleton" not in before and "skeleton" in after
    skeleton = after.pop("skeleton")
    assert len(skeleton["colors"]) == 1 and len(skeleton["metrics"]) == 13
    before_identity = before.pop("identity")
    after_identity = after.pop("identity")
    assert before_identity != after_identity
    compare(before, after, name)
    print(f"{name}: old typed values preserved; SHA256={hashlib.sha256(raw).hexdigest()}")

print(f"PASS: {values} old scalar typed values; all 31 IIFE texts/order unchanged; five Skeleton additions only")
print("PASS: TokenIdentity append-only; default scope/heap preflight/config self guard and Time/Prop bridge source preserved")
