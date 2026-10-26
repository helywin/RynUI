"""Verify actual MSVC x64 test images retain the default 1 MiB PE stack reserve."""
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[4]
build = ROOT / "out/build/windows-msvc-headless/tests"
names = ["rate_model", "rate_theme", "rate_public_api", "rate_component", "rate_lifecycle",
         "theme_algorithm", "timeline_review", "popover_integration", "notice_review"]
count = 0
for config in ["Debug", "Release"]:
    for name in names:
        path = build / config / f"rynui_{name}_tests.exe"
        data = path.read_bytes()
        assert data[:2] == b"MZ", path
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        assert data[pe:pe + 4] == b"PE\0\0", path
        assert struct.unpack_from("<H", data, pe + 4)[0] == 0x8664, path
        optional = pe + 24
        assert struct.unpack_from("<H", data, optional)[0] == 0x20B, path
        reserve = struct.unpack_from("<Q", data, optional + 72)[0]
        assert reserve == 1048576, (path, reserve)
        count += 1
print(f"{count} actual MSVC x64 PE images retain SizeOfStackReserve=1048576 bytes")
