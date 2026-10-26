"""Audit exact merge ancestry, final deduplicated test logs and default x64 PE stack reserves."""
from hashlib import sha256
from pathlib import Path
import json
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[4]
EVIDENCE = Path(__file__).resolve().parent
TREE = "2fd96f6209e817e892f7684c7eae6891ee576d10"
AC = "ad779c595a63a39d05aa787d38dccda1fcaea9c7"
MERGE = "d38d7971c9c39711ee944511a57f787bea3feb5b"


def read_log(name):
    data = (EVIDENCE / "logs" / name).read_bytes()
    return data.decode("utf-16") if data.startswith(b"\xff\xfe") else data.decode("utf-8-sig")


def main():
    parents = subprocess.check_output(["git", "show", "-s", "--format=%P", MERGE], cwd=ROOT).decode().split()
    assert parents == [TREE, AC], parents
    subprocess.run(["git", "merge-base", "--is-ancestor", MERGE, "HEAD"], cwd=ROOT, check=True)
    print(f"exact merge {MERGE}: parents {' '.join(parents)}")
    subprocess.run([sys.executable, str(EVIDENCE / "verify-tree-ac-merge.py")], cwd=ROOT, check=True)
    frozen = ("src/component/auto_complete_component.cpp", "src/component/auto_complete_component.hpp",
              "src/component/auto_complete_model.cpp", "src/component/auto_complete_model.hpp",
              "include/ryn/auto_complete.hpp", "src/input/pointer_router.cpp", "src/input/pointer_router.hpp",
              "src/input/focus_manager.hpp", "tests/auto_complete_*", "tests/component_failure_retirement_tests.cpp",
              "tests/support/auto_complete_fixture.hpp")
    subprocess.run(["git", "diff", "--exit-code", AC, "--", *frozen], cwd=ROOT, check=True)
    preserved = ("tests/tree_component_tests.cpp", "tests/tree_review_tests.cpp", "include/ryn/avatar.hpp",
                 "src/component/avatar_component.cpp", "src/component/avatar_component.hpp",
                 "tests/avatar_component_tests.cpp", "tests/avatar_theme_tests.cpp", "CMakePresets.json", "README.md")
    subprocess.run(["git", "diff", "--exit-code", TREE, "--", *preserved], cwd=ROOT, check=True)
    print("reviewed AC implementation/R1-R3/shared tests and existing Tree/Avatar assertions retained")
    build = ROOT / "out/build/windows-msvc-headless"
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    for entry in ("CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config", "RYNUI_EXPECTED_TOOLCHAIN:STRING=msvc",
                  "RYNUI_PLATFORM_BACKEND:STRING=HEADLESS", "RYNUI_RENDER_BACKENDS:STRING=RECORDING",
                  f"CMAKE_HOME_DIRECTORY:INTERNAL={ROOT.as_posix()}"):
        assert entry in cache, entry
        print(entry)
    targets = re.findall(r"'(rynui_[^']+)'", (EVIDENCE / "run-r3.ps1").read_text(encoding="utf-8"))
    assert len(targets) == len(set(targets)) == 29
    for config in ("Debug", "Release"):
        inventory = json.loads(read_log(f"r3-tests-{config}.json"))["tests"]
        names = {test["name"] for test in inventory}
        assert len(names) == len(inventory) == 62
        log = read_log(f"r3-after-{config}.log")
        assert "100% tests passed, 0 tests failed out of 62" in log
        passed = set(re.findall(r"\d+/62 Test\s+#\d+: (\S+)\s+\.\.+\s+Passed", log))
        assert passed == names
        assert sum(n.startswith("rynui.tree_") for n in names) == 20
        assert sum(n.startswith("rynui.auto_complete_") for n in names) == 23
        assert "original=1 slot_threw=1 blur_escaped=0 blurs=0 editor_live=0" in log
        assert all(f"slot retirement mode={mode} original=1 blur=0 old_editor=0 resources_at_baseline=1" in log
                   for mode in range(6))
        print(f"{config}: 62/62 distinct tests; Tree 20 + AC 23 + directly affected representatives 19")
        for target in targets:
            directory = build / "tests/portable" if target.startswith("rynui_portable_") else build / "tests"
            path = directory / config / f"{target}.exe"
            data = path.read_bytes()
            pe = struct.unpack_from("<I", data, 0x3C)[0]
            assert data[pe:pe + 4] == b"PE\0\0" and struct.unpack_from("<H", data, pe + 4)[0] == 0x8664
            assert struct.unpack_from("<H", data, pe + 24)[0] == 0x20B
            assert struct.unpack_from("<Q", data, pe + 24 + 72)[0] == 1048576
            print(f"{config}/{target}: x64 PE32+ stack_reserve=1048576 sha256={sha256(data).hexdigest()}")
    print("58 final target PE stack reserves unchanged at the default 1MiB")


if __name__ == "__main__":
    main()
