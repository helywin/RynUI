"""Repeatable, local-only preservation checks for Gallery 093."""
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
GALLERY = "6118f67728aede9985867377b122274ac86f31d7"
CORE = "29154cda31f3a8e7c5da06b660182fc75a2697fc"
TARGETS = {"ant.component.avatar", "ant.component.statistic", "ant.component.rate"}


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)


def old(path):
    return git("show", f"{GALLERY}:{path}").decode("utf-8")


path = "examples/token_gallery/token_gallery_definition.cpp"
before = old(path)
after = (ROOT / path).read_text(encoding="utf-8")
ids = lambda text: re.findall(r'"([^"]+)"', text.split("constexpr auto stable_test_ids =", 1)[1].split("});", 1)[0])
previous_ids = ids(before)
current_ids = ids(after)
assert len(previous_ids) == 390
assert current_ids[:390] == previous_ids
assert len(current_ids) == 419 and len(current_ids) == len(set(current_ids))
functions = lambda text: re.findall(r"\nvoid (add_\w+)\([^\n]*\) \{.*?\n\}", text, re.S)
names = functions(before)
assert len(names) == 36, len(names)
added = ["add_member_avatar_samples", "add_statistic_metric_samples", "add_service_rate_samples"]
assert functions(after) == names + added
body = lambda text, name: re.search(r"\nvoid " + name + r"\([^\n]*\) \{.*?\n\}", text, re.S).group()
assert [name for name in functions(after) if name in names] == names
for name in names:
    assert body(before, name) == body(after, name), f"old sample changed: {name}"
calls = lambda text: re.findall(r"^\s+(add_\w+)\(state\);", text, re.M)
assert [name for name in calls(after) if name in names] == calls(before)
assert calls(after) == calls(before) + added
print(f"old_ids=390 ordered; current_ids={len(current_ids)}; old_sample_functions=36 unchanged; current_functions={len(functions(after))}")

overlay = "gallery/ant-design/6.6.5/support-overlay.json"
previous = json.loads(old(overlay))["entries"]
current = json.loads((ROOT / overlay).read_text(encoding="utf-8"))["entries"]
assert [e["identity"] for e in current] == [e["identity"] for e in previous]
lookup = {e["identity"]: e for e in current}
assert all(lookup[target]["status"] == "partial" for target in TARGETS)
for entry in previous:
    if entry["identity"] not in TARGETS:
        assert lookup[entry["identity"]] == entry, entry["identity"]
assert len(current) - len(TARGETS) == 70
print("non-target support entries=70 unchanged")

frozen = []
frozen_blobs = []
for line in git("ls-tree", "-r", CORE).decode().splitlines():
    metadata, path = line.split("\t", 1)
    if (path.startswith(("src/", "include/", "cmake/", "design-tokens/", "renderer/"))
            or "golden" in path.lower() or Path(path).name.startswith("CMake") or path.endswith((".cmake", ".cmake.in"))
            or path in {"README.md", "docs/component-completion.md"}):
        blob = metadata.split()[2]
        frozen.append((path, blob))
        frozen_blobs.append(path)
actual_blobs = subprocess.check_output(
    ["git", "hash-object", "--stdin-paths"], cwd=ROOT,
    input=("\n".join(frozen_blobs) + "\n").encode(),
).decode().splitlines()
assert len(actual_blobs) == len(frozen)
for (path, expected), actual in zip(frozen, actual_blobs):
    assert actual == expected, f"frozen blob changed: {path}"
print(f"frozen Core/API/Theme/goldens/renderer/CMake/parent-doc blobs={len(frozen)} identical to {CORE}")
print("frozen manifest sha256=" + hashlib.sha256(json.dumps(frozen).encode()).hexdigest())

allowed = {
    "examples/token_gallery/token_gallery_definition.cpp",
    "examples/token_gallery/gallery_document_viewport.cpp",
    "examples/token_gallery/gallery_document_viewport.hpp",
    "examples/token_gallery/generated_ant_design_reference_catalog.inc",
    "tests/token_gallery_frame_tests.cpp",
    "gallery/ant-design/6.6.5/support-overlay.json",
    "docs/avatar.md", "docs/statistic.md", "docs/rate.md",
}
changed = git("diff", "--name-only", "0f578ddebc3307f5d95807745b91ef29667d7b43").decode().splitlines()
changed += git("ls-files", "--others", "--exclude-standard").decode().splitlines()
for path in changed:
    assert path in allowed or path.startswith("openspec/changes/093-20261026-add-member-and-rating-gallery/"), path
print(f"all changed/untracked paths={len(set(changed))} inside the authorized Gallery/093 scope")
