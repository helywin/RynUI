"""Preserve the published parent, resource fixes and existing Gallery while integrating 093."""
from pathlib import Path
import hashlib
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[4]
PARENT = "ee289de2ad6851ededdddd1c45bd20612b7579a8"
WORKER = "862ff37ce6799db344bd57e188068060ce15cd7b"
TARGETS = {"ant.component.avatar", "ant.component.statistic", "ant.component.rate"}
CONTRACT_FIX = "tests/cmake/verify_token_gallery_contract.cmake"


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)


def previous(path):
    return git("show", f"{PARENT}:{path}").decode("utf-8").replace("\r\n", "\n")


path = "examples/token_gallery/token_gallery_definition.cpp"
before = previous(path)
after = (ROOT / path).read_text(encoding="utf-8")
ids = lambda text: re.findall(r'"([^"]+)"', text.split("constexpr auto stable_test_ids =", 1)[1].split("});", 1)[0])
old_ids, new_ids = ids(before), ids(after)
assert len(old_ids) == 390 and new_ids[:390] == old_ids
assert len(new_ids) == 419 and len(set(new_ids)) == 419
functions = lambda text: re.findall(r"\nvoid (add_\w+)\([^\n]*\) \{.*?\n\}", text, re.S)
old_names = functions(before)
added = ["add_member_avatar_samples", "add_statistic_metric_samples", "add_service_rate_samples"]
assert len(old_names) == 36 and functions(after) == old_names + added
body = lambda text, name: re.search(r"\nvoid " + name + r"\([^\n]*\) \{.*?\n\}", text, re.S).group()
for name in old_names:
    assert body(before, name) == body(after, name), f"published sample changed: {name}"
for name in added:
    worker = git("show", f"{WORKER}:{path}").decode("utf-8")
    assert body(worker, name) == body(after, name), f"reviewed new sample changed: {name}"
calls = lambda text: re.findall(r"^\s+(add_\w+)\(state\);", text, re.M)
assert calls(after) == calls(before) + added
for name in ["themed_button", "navigation_button"]:
    helper = lambda source: re.search(r"\nvoid " + name + r"\(.*?\n\}", source, re.S).group()
    assert helper(before) == helper(after), f"resource-era low-stack helper changed: {name}"
assert "void set_theme(const ryn::ThemeConfig& config" in after
print("390 published IDs/36 bodies/call order retained; 29 new IDs/3 reviewed bodies appended; low-stack helpers retained")

path = "gallery/ant-design/6.6.5/support-overlay.json"
a = json.loads(previous(path))["entries"]
b = json.loads((ROOT / path).read_text(encoding="utf-8"))["entries"]
assert [entry["identity"] for entry in a] == [entry["identity"] for entry in b]
lookup = {entry["identity"]: entry for entry in b}
for entry in a:
    if entry["identity"] not in TARGETS:
        assert lookup[entry["identity"]] == entry
for target in TARGETS:
    assert lookup[target]["status"] == "partial"
assert len(b) - len(TARGETS) == 70
print("70 non-target overlay entries unchanged")

frozen = []
for line in git("ls-tree", "-r", PARENT).decode().splitlines():
    metadata, path = line.split("\t", 1)
    if (path.startswith(("src/", "include/", "cmake/", "design-tokens/", "renderer/"))
            or "golden" in path.lower() or Path(path).name.startswith("CMake")
            or path.endswith((".cmake", ".cmake.in")) or path in {"README.md", "README.en.md"}):
        if path == CONTRACT_FIX:
            continue
        frozen.append((path, metadata.split()[2]))
actual = subprocess.check_output(["git", "hash-object", "--stdin-paths"], cwd=ROOT,
                                 input=("\n".join(path for path, _ in frozen) + "\n").encode()).decode().splitlines()
assert len(actual) == len(frozen)
for (path, expected), digest in zip(frozen, actual):
    assert digest == expected, f"published frozen blob changed: {path}"
print(f"{len(frozen)} published Core/API/Theme/golden/renderer/CMake/README blobs retained")
print("frozen manifest SHA256=" + hashlib.sha256(json.dumps(frozen).encode()).hexdigest())

# 095 removed external shader copies; retain the complete contract except for its stale deployment literals.
contract = (ROOT / CONTRACT_FIX).read_text(encoding="utf-8")
expected = previous(CONTRACT_FIX).replace(
    '        "rounded_effect.vertex.dxil"\n        "rounded_effect.vertex.spv"',
    '        "rynui_embed_target_shaders(rynui_token_gallery)"')
assert contract == expected, "Gallery contract changed outside the embedded-shader expectation"
print("Gallery contract only replaces external shader-copy literals with executable embedding")

allowed = {"examples/token_gallery/token_gallery_definition.cpp", "examples/token_gallery/gallery_document_viewport.cpp",
           "examples/token_gallery/gallery_document_viewport.hpp", "examples/token_gallery/generated_ant_design_reference_catalog.inc",
           "tests/token_gallery_frame_tests.cpp", "gallery/ant-design/6.6.5/support-overlay.json",
           "docs/avatar.md", "docs/statistic.md", "docs/rate.md", "docs/component-completion.md", CONTRACT_FIX}
changed = git("diff", "--name-only", PARENT).decode().splitlines()
changed += git("ls-files", "--others", "--exclude-standard").decode().splitlines()
for path in changed:
    assert path in allowed or path.startswith("openspec/changes/093-20261026-add-member-and-rating-gallery/"), path
print(f"all {len(set(changed))} changed/untracked paths are in the parent Gallery scope")
