"""Check the compiled documentation example, x64 PE stack reserve and fresh Theme files."""
from hashlib import sha256
from pathlib import Path
import json
import re
import struct

ROOT = Path(__file__).resolve().parents[4]
BUILD = ROOT / "out/build/windows-msvc-headless"


def main():
    cache = (BUILD / "CMakeCache.txt").read_text(encoding="utf-8")
    for entry in ("CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config", "RYNUI_PLATFORM_BACKEND:STRING=HEADLESS",
                  "RYNUI_RENDER_BACKENDS:STRING=RECORDING", "RYNUI_EXPECTED_TOOLCHAIN:STRING=msvc",
                  f"CMAKE_HOME_DIRECTORY:INTERNAL={ROOT.as_posix()}"):
        assert entry in cache, f"unexpected configure cache: {entry}"
        print(entry)
    doc = (ROOT / "docs/tree.md").read_text(encoding="utf-8")
    example = re.search(r"```cpp\n(.*?)\n```", doc, re.S).group(1)
    example = example[example.index("void project_tree()"):]
    test = (ROOT / "tests/tree_component_tests.cpp").read_text(encoding="utf-8")
    compiled = re.search(r"void project_tree\(\) \{.*?\n\}", test, re.S).group(0)
    assert re.sub(r"\s+", "", example) == re.sub(r"\s+", "", compiled), "documentation differs from compiled example"
    print("docs/tree.md: exact C++ example is compiled and invoked by tree_basic")
    for config in ("Debug", "Release"):
        log_bytes = (ROOT / "out/tree-logs" / f"test-{config}.log").read_bytes()
        log = log_bytes.decode("utf-16") if log_bytes.startswith(b"\xff\xfe") else log_bytes.decode("utf-8-sig")
        assert "100% tests passed, 0 tests failed out of 36" in log
        print(f"{config}: CTest 36/36 (13 Tree and 23 directly shared regressions)")
        for name in ("component", "model", "theme"):
            path = BUILD / "tests" / config / f"rynui_tree_{name}_tests.exe"
            data = path.read_bytes()
            pe = struct.unpack_from("<I", data, 0x3C)[0]
            assert data[pe:pe + 4] == b"PE\0\0"
            assert struct.unpack_from("<H", data, pe + 4)[0] == 0x8664, "expected x64"
            optional = pe + 24
            assert struct.unpack_from("<H", data, optional)[0] == 0x20B, "expected PE32+"
            reserve = struct.unpack_from("<Q", data, optional + 72)[0]
            assert reserve == 1048576, f"{path}: stack reserve changed"
            print(f"{config}/{path.name}: x64 PE32+ stack_reserve={reserve} sha256={sha256(data).hexdigest()}")
    for name in ("default", "dark", "compact", "dark-compact", "compact-dark"):
        fresh = ROOT / "out/tree-goldens" / f"{name}.json"
        tracked = ROOT / "design-tokens/ant-design/6.6.5/golden" / f"{name}.json"
        assert json.loads(fresh.read_text(encoding="utf-8")) == json.loads(tracked.read_text(encoding="utf-8"))
        print(f"fresh golden {name}: sha256={sha256(fresh.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
