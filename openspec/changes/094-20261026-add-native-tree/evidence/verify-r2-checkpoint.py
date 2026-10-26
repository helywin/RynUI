"""Audit a partial R2 checkpoint: nested wheel passes, shared throwing-blur retirement is still failing."""
from hashlib import sha256
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[4]
EVIDENCE = Path(__file__).resolve().parent
BASE = "b4b8c0ceeceb4b5e24aa85298a7219b5c41dfa3b"


def read_log(name):
    data = (EVIDENCE / "logs" / name).read_bytes()
    return data.decode("utf-16") if data.startswith(b"\xff\xfe") else data.decode("utf-8-sig")


def main():
    build = ROOT / "out/build/windows-msvc-headless"
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    for entry in ("CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config", "RYNUI_PLATFORM_BACKEND:STRING=HEADLESS",
                  "RYNUI_EXPECTED_TOOLCHAIN:STRING=msvc", f"CMAKE_HOME_DIRECTORY:INTERNAL={ROOT.as_posix()}"):
        assert entry in cache, entry
        print(entry)
    assert "0% tests passed, 2 tests failed out of 2" in read_log("r2-before-Debug.log")
    for config in ("Debug", "Release"):
        log = read_log(f"r2-after-{config}.log")
        passed = set(re.findall(r"\d+/15 Test\s+#\d+: (\S+)\s+\.\.+\s+Passed", log))
        failed = set(re.findall(r"\d+/15 Test\s+#\d+: (\S+)\s+\.\.+\*\*\*Failed", log))
        assert len(passed) == 14 and failed == {"rynui.tree_r2_throwing_blur_rollback"}
        assert "rynui.tree_r2_nested_wheel" in passed
        for case in ("mount_rollback", "zero_area", "translated_checkbox", "wheel_handoff"):
            assert f"rynui.tree_review_{case}" in passed
        assert "original=1 slot_threw=1 blur_escaped=0 blurs=1 editor_live=1" in log
        print(f"{config}: 14 passed / 1 unresolved shared-cleanup failure; NOT complete R2 acceptance")
        paths = [build / "tests" / config / f"rynui_tree_{name}_tests.exe"
                 for name in ("component", "review", "r2")]
        paths += [build / "tests/portable" / config / f"rynui_portable_{name}.exe"
                  for name in ("modal_component", "select_component", "input_component", "focus_lifecycle")]
        for path in paths:
            data = path.read_bytes()
            pe = struct.unpack_from("<I", data, 0x3C)[0]
            assert data[pe:pe + 4] == b"PE\0\0" and struct.unpack_from("<H", data, pe + 4)[0] == 0x8664
            assert struct.unpack_from("<H", data, pe + 24)[0] == 0x20B
            assert struct.unpack_from("<Q", data, pe + 24 + 72)[0] == 1048576
            print(f"{config}/{path.name}: x64 PE32+ stack_reserve=1048576 sha256={sha256(data).hexdigest()}")
    protected = ("src/component/input_component.cpp", "src/component/window_component_services.cpp",
                 "src/component/window_component_services.hpp", "src/input", "src/runtime", "src/theme",
                 "include/ryn/tree.hpp", "include/ryn/theme.hpp", "CMakePresets.json",
                 "design-tokens/ant-design/6.6.5/golden", "tests/tree_review_tests.cpp", "tests/tree_component_tests.cpp")
    subprocess.run(["git", "diff", "--exit-code", BASE, "--", *protected], cwd=ROOT, check=True)
    print("shared Input/focus/runtime/WindowServices, API/Theme/goldens/presets and existing assertions unchanged")
    print("R2.1 remains unresolved: await parent-verified shared retirement interface and compatible baseline")


if __name__ == "__main__":
    main()
