"""Read actual PE headers; do not alter the default Windows stack reserve."""
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[4]
for config in sys.argv[1:] or ["Debug", "Release"]:
    for name in ["public_api", "model", "theme", "component", "integration", "examples"]:
        path = ROOT / f"out/build/windows-msvc-headless/tests/{config}/rynui_popover_{name}_tests.exe"
        data = path.read_bytes()
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        assert data[pe:pe + 4] == b"PE\0\0"
        optional = pe + 24
        assert struct.unpack_from("<H", data, optional)[0] == 0x20B
        reserve = struct.unpack_from("<Q", data, optional + 72)[0]
        assert reserve == 1024 * 1024, (path, reserve)
        print(f"{config}/{path.name}: PE32+ stack reserve {reserve}")
