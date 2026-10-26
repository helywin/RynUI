"""Verify the default MSVC 1 MiB stack reserve on all new test executables."""
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[4]
count = 0
for config in ("Debug", "Release"):
    for name in ("model", "theme", "component", "public_api"):
        path = root / "out/build/windows-msvc-headless/tests" / config / f"rynui_statistic_{name}_tests.exe"
        data = path.read_bytes()
        pe = struct.unpack_from("<I", data, 60)[0]
        assert data[pe:pe + 4] == b"PE\0\0", path
        optional_header = pe + 24
        assert struct.unpack_from("<H", data, optional_header)[0] == 0x20B, path
        stack = struct.unpack_from("<Q", data, optional_header + 72)[0]
        assert stack == 1048576, (path, stack)
        count += 1
        print(f"{config}/{path.name}: SizeOfStackReserve={stack}")
print(f"PASS: {count} fresh test executables retain the default 1 MiB PE stack")
