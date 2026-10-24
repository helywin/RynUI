"""Check the frozen R1/main merge as a disjoint union of Git blobs."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[4]
BASE = "580a8c3d235c22eb95b11688b509ee1e2ea65e44"
R1 = "9a83644c41d6db9d82a6a63e662a8cb207b295ab"
MAIN = "22ee615347967dc204d89ff3508b74e63c68ad5e"
ADDITIONS = {
    "openspec/changes/083-20261025-add-native-skeleton/evidence/merge-main-22ee615.md",
    "openspec/changes/083-20261025-add-native-skeleton/evidence/verify-main-22ee615.py",
}


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)


def tree(ref):
    result = {}
    for entry in git("ls-tree", "-r", "-z", "--full-tree", ref).split(b"\0"):
        if entry:
            metadata, path = entry.split(b"\t", 1)
            result[path.decode("utf-8")] = metadata.decode("ascii")
    return result


def changed(before, after):
    return {path for path in before.keys() | after.keys() if before.get(path) != after.get(path)}


parser = argparse.ArgumentParser()
parser.add_argument("--ref", default="INDEX")
parser.add_argument("--output", type=Path)
args = parser.parse_args()
assert git("merge-base", R1, MAIN).decode().strip() == BASE
base, r1, main = (tree(ref) for ref in (BASE, R1, MAIN))
ours, theirs = changed(base, r1), changed(base, main)
assert not ours & theirs, f"Overlapping changes: {sorted(ours & theirs)}"
protected = lambda p: p.startswith(("src/", "include/", "cmake/", "design-tokens/")) or p in {
    "CMakeLists.txt", "CMakePresets.json", "tests/CMakeLists.txt", "openspec/config.yaml"
}
assert not {path for path in theirs if protected(path)}, "Published main changed Core/API/Theme/build config"
expected = base.copy()
for paths, source in ((ours, r1), (theirs, main)):
    for path in paths:
        if path in source:
            expected[path] = source[path]
        else:
            expected.pop(path, None)

ref = git("write-tree").decode().strip() if args.ref == "INDEX" else args.ref
actual = tree(ref)
assert {path: value for path, value in actual.items() if path not in ADDITIONS} == expected, \
    "Candidate tree is not the exact frozen blob union"
assert not ADDITIONS & (base.keys() | r1.keys() | main.keys()), "Merge evidence would replace existing artifacts"
subprocess.run(["git", "diff", "--quiet"], cwd=ROOT, check=True)
if args.ref != "INDEX":
    parents = git("show", "-s", "--format=%P", args.ref).decode().split()
    assert parents == [R1, MAIN], f"Unexpected merge parents: {parents}"

gallery = {path for path in main if path.startswith(("examples/", "gallery/"))}
assert all(actual[path] == main[path] for path in gallery)
assert all(actual[path] == r1[path] for path in ours if path in r1)
shared = {path for path in ours & main.keys() if protected(path)}
assert all(base[path] == main[path] and actual[path] == r1[path] for path in shared)

cache = ROOT / "out/build/windows-msvc-headless/CMakeCache.txt"
home = next(line.split("=", 1)[1] for line in cache.read_text(encoding="utf-8").splitlines()
            if line.startswith("CMAKE_HOME_DIRECTORY:INTERNAL="))
assert Path(home).resolve() == ROOT, "Existing build cache belongs to another checkout"
binaries = []
for config in ("Debug", "Release"):
    for name in ("model", "component", "theme"):
        path = ROOT / f"out/build/windows-msvc-headless/tests/{config}/rynui_skeleton_{name}_tests.exe"
        data = path.read_bytes()
        binaries.append({"path": path.relative_to(ROOT).as_posix(), "bytes": len(data),
                         "sha256": hashlib.sha256(data).hexdigest()})

report = {
    "base": BASE, "r1": R1, "main": MAIN, "checked_tree": git("rev-parse", f"{ref}^{{tree}}").decode().strip(),
    "repository_files": len(expected), "r1_changed_files": len(ours), "main_changed_files": len(theirs),
    "overlapping_changed_files": 0, "parent_gallery_blobs": len(gallery),
    "preserved_main_blobs": len(main.keys() - ours), "preserved_r1_blobs": len(r1.keys() - theirs),
    "shared_core_api_theme_build_paths": sorted(shared), "main_changed_paths": sorted(theirs),
    "r1_changed_paths": sorted(ours), "new_merge_evidence_paths": sorted(actual.keys() - expected.keys()),
    "binaries": binaries,
}
if args.output:
    destination = (ROOT / args.output).resolve()
    assert destination.is_relative_to(ROOT / "out"), "Report must stay in this worktree's out directory"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(f"PASS: exact blob union over {len(expected)} files; R1 {len(ours)} + main {len(theirs)} disjoint changes")
print(f"PASS: {len(gallery)} parent examples/gallery blobs; {len(shared)} frozen shared Core/API/Theme/build paths")
print(f"PASS: original 083 tasks/history retained; {len(binaries)} existing binaries fingerprinted in this worktree")
