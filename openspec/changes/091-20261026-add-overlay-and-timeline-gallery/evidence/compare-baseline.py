"""Check Gallery additions against the published component baseline."""
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
BASE = "8056de67953ee73015c7559f1782033d85e08573"
TARGETS = {"ant.component.popover", "ant.component.message", "ant.component.notification", "ant.component.timeline"}


def old(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode("utf-8")


path = "examples/token_gallery/token_gallery_definition.cpp"
before = old(path)
after = (ROOT / path).read_text(encoding="utf-8")
ids = lambda text: re.findall(r'"([^"]+)"', text.split("constexpr auto stable_test_ids =", 1)[1].split("});", 1)[0])
previous_ids = ids(before)
current_ids = ids(after)
assert len(previous_ids) == 358
assert current_ids[:len(previous_ids)] == previous_ids
assert len(current_ids) == 390 and len(current_ids) == len(set(current_ids))
functions = lambda text: re.findall(r"\nvoid (add_\w+)\([^\n]*\) \{.*?\n\}", text, re.S)
names = functions(before)
body = lambda text, name: re.search(r"\nvoid " + name + r"\([^\n]*\) \{.*?\n\}", text, re.S).group()
assert [name for name in functions(after) if name in names] == names
for name in names:
    assert body(before, name) == body(after, name), f"old sample changed: {name}"
calls = lambda text: re.findall(r"^\s+(add_\w+)\(state\);", text, re.M)
assert [name for name in calls(after) if name in names] == calls(before)
print(f"old_ids={len(previous_ids)} ordered; current_ids={len(current_ids)}; old_sample_functions={len(names)} unchanged")

overlay = "gallery/ant-design/6.6.5/support-overlay.json"
previous = json.loads(old(overlay))["entries"]
current = json.loads((ROOT / overlay).read_text(encoding="utf-8"))["entries"]
assert [e["identity"] for e in current] == [e["identity"] for e in previous]
lookup = {e["identity"]: e for e in current}
for entry in previous:
    if entry["identity"] not in TARGETS:
        assert lookup[entry["identity"]] == entry, f"non-target support entry changed: {entry['identity']}"
print(f"non-target support entries unchanged: {len(current) - len(TARGETS)}")
forbidden = subprocess.check_output(
    ["git", "diff", "--name-only", BASE, "--", "include", "src", "design-tokens", "CMakeLists.txt", "CMakePresets.json", "tests/CMakeLists.txt"],
    cwd=ROOT,
).decode().strip()
assert not forbidden, f"Core/API/Theme/goldens/CMake changed: {forbidden}"
print("Core/API/Theme/goldens/CMake unchanged")
