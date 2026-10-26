"""Audit the R1 targeted logs, retained contracts and locally built x64 PE stack reserves."""
from hashlib import sha256
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[4]
EVIDENCE = Path(__file__).resolve().parent
BASE = "b106c0b089e1f5da5007fe18dbd56a1a90479d11"


def read_log(name):
    data = (EVIDENCE / "logs" / name).read_bytes()
    return data.decode("utf-16") if data.startswith(b"\xff\xfe") else data.decode("utf-8-sig")


def main():
    build = ROOT / "out/build/windows-msvc-headless"
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    for entry in ("CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config", "RYNUI_PLATFORM_BACKEND:STRING=HEADLESS",
                  "RYNUI_RENDER_BACKENDS:STRING=RECORDING", "RYNUI_EXPECTED_TOOLCHAIN:STRING=msvc",
                  f"CMAKE_HOME_DIRECTORY:INTERNAL={ROOT.as_posix()}"):
        assert entry in cache, entry
        print(entry)
    cases = ("mount_rollback", "zero_area", "translated_checkbox", "wheel_handoff")
    for case in cases:
        before = read_log(f"r1-before-{case}.log")
        assert "***Failed" in before and "0% tests passed, 1 tests failed out of 1" in before
        print(f"before/{case}: real failing CTest retained")
    for config in ("Debug", "Release"):
        after = read_log(f"r1-after-{config}.log")
        assert "100% tests passed, 0 tests failed out of 23" in after
        passed = re.findall(r"\d+/23 Test\s+#\d+: (\S+)\s+\.\.+\s+Passed", after)
        assert len(set(passed)) == 23, passed
        assert all(f"rynui.tree_review_{case}" in passed for case in cases)
        print(f"{config}: 23 distinct CTests passed (13 existing Tree, 4 R1, 6 shared)")
        paths = [build / "tests" / config / f"rynui_tree_{name}_tests.exe"
                 for name in ("component", "model", "theme", "review")]
        paths += [build / "tests/portable" / config / f"rynui_portable_{name}.exe"
                  for name in ("modal_component", "select_component", "input_component", "focus_lifecycle",
                               "text_component", "text_component_frame")]
        for path in paths:
            data = path.read_bytes()
            pe = struct.unpack_from("<I", data, 0x3C)[0]
            assert data[pe:pe + 4] == b"PE\0\0" and struct.unpack_from("<H", data, pe + 4)[0] == 0x8664
            assert struct.unpack_from("<H", data, pe + 24)[0] == 0x20B
            reserve = struct.unpack_from("<Q", data, pe + 24 + 72)[0]
            assert reserve == 1048576, path
            print(f"{config}/{path.name}: x64 PE32+ stack_reserve={reserve} sha256={sha256(data).hexdigest()}")
    protected = ("include/ryn/tree.hpp", "include/ryn/theme.hpp", "src/theme",
                 "design-tokens/ant-design/6.6.5/golden", "CMakePresets.json")
    subprocess.run(["git", "diff", "--exit-code", BASE, "--", *protected], cwd=ROOT, check=True)
    print(f"public Tree API, Theme, five goldens and presets unchanged from {BASE}")
    for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
        path = ROOT / "design-tokens/ant-design/6.6.5/golden" / f"{name}.json"
        print(f"retained golden {name}: sha256={sha256(path.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
