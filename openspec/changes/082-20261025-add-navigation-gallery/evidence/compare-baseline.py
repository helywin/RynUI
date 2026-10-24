"""Verify the append-only Gallery boundary against the assigned parent."""
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
BASE = "62c990a1b494532c358ac487b2bb5931c08a7231"
PARENT = "c083fed5a72f644b491118c6440908e0354d241e"


def old(path, ref=BASE):
    return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT).decode("utf-8")


path = "examples/token_gallery/token_gallery_definition.cpp"
before = old(path)
after = (ROOT / path).read_text(encoding="utf-8")
ids = lambda text: re.findall(r'"([^"]+)"', text.split("constexpr auto stable_test_ids =", 1)[1].split("});", 1)[0])
previous_ids = ids(before)
current_ids = ids(after)
assert len(previous_ids) == 297
assert current_ids[:len(previous_ids)] == previous_ids
assert len(current_ids) == len(set(current_ids))
functions = lambda text: re.findall(r"\nvoid (add_\w+)\([^\n]*\) \{.*?\n\}", text, re.S)
names = functions(before)
body = lambda text, name: re.search(r"\nvoid " + name + r"\([^\n]*\) \{.*?\n\}", text, re.S).group()
assert [name for name in functions(after) if name in names] == names
for name in names:
    assert body(before, name) == body(after, name), f"old function changed: {name}"
calls = lambda text: re.findall(r"^\s+(add_\w+)\(state\);", text, re.M)
assert [name for name in calls(after) if name in names] == calls(before)
print(f"old_ids={len(previous_ids)} unchanged ordered; current_ids={len(current_ids)}; old_add_functions={len(names)} byte-identical ordered")

overlay = "gallery/ant-design/6.6.5/support-overlay.json"
current = json.loads((ROOT / overlay).read_text(encoding="utf-8"))
for ref in (BASE, PARENT):
    previous = json.loads(old(overlay, ref))
    lookup = {entry["identity"]: entry for entry in current["entries"]}
    assert list(lookup) == [entry["identity"] for entry in previous["entries"]]
    for entry in previous["entries"]:
        if entry["identity"] not in ("ant.component.breadcrumb", "ant.component.steps"):
            assert lookup[entry["identity"]] == entry, f"non-target overlay changed: {entry['identity']}"
    print(f"non-target overlay entries identical to {ref[:7]}: {len(lookup) - 2}")

for path in ("README.md", "README.en.md", "docs/component-completion.md"):
    assert (ROOT / path).read_text(encoding="utf-8") == old(path, PARENT), f"parent metadata changed: {path}"
print("parent README and completion metadata preserved")
forbidden = subprocess.check_output(
    ["git", "diff", "--name-only", PARENT, "--", "include", "src", "design-tokens", "CMakeLists.txt", "CMakePresets.json", "tests/CMakeLists.txt"],
    cwd=ROOT,
).decode().strip()
assert not forbidden, f"Core/API/Theme/ABI change beyond parent integration: {forbidden}"
print("Core/API/Theme/goldens/CMake byte-identical to parent integration")
