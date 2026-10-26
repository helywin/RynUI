import struct
from pathlib import Path

root = Path(__file__).resolve().parent / 'build/windows-msvc-headless/tests'
for config in ('Debug', 'Release'):
    for kind in ('model', 'theme', 'component', 'integration', 'review', 'r2'):
        path = root / config / f'rynui_auto_complete_{kind}_tests.exe'
        data = path.read_bytes()
        pe = struct.unpack_from('<I', data, 0x3C)[0]
        assert data[:2] == b'MZ' and data[pe:pe+4] == b'PE\0\0'
        assert struct.unpack_from('<H', data, pe + 24)[0] == 0x20B
        reserve = struct.unpack_from('<Q', data, pe + 24 + 72)[0]
        assert reserve == 1048576, (path, reserve)
        print(f'{config}/{path.name}: PE32+ stack reserve = {reserve}')
