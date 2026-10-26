import struct
from pathlib import Path

root = Path(__file__).resolve().parent / "build/windows-msvc-headless/tests"
for mode in ("Debug", "Release"):
    for kind in ("model", "theme", "component", "integration", "review"):
        path = root / mode / f"rynui_auto_complete_{kind}_tests.exe"
        data = path.read_bytes()
        pe = struct.unpack_from("<I", data, 0x3c)[0]
        assert data[:2] == b"MZ" and data[pe:pe+4] == b"PE\0\0"
        optional = pe + 24
        assert struct.unpack_from("<H", data, optional)[0] == 0x20b
        reserve = struct.unpack_from("<Q", data, optional + 72)[0]
        assert reserve == 1048576, (str(path), reserve)
        print(f"{mode}/{path.name}: PE32+ stack reserve = {reserve}")
