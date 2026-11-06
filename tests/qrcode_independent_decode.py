#!/usr/bin/env python3
"""Independent exported PNG/SVG decoding. Artifacts stay under the supplied ignored build directory."""
import binascii
from pathlib import Path
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
import zlib

model, decoder, rasterizer, destination = sys.argv[1:]
out = Path(destination)
subprocess.run([model, str(out)], check=True)
expected = [b'RynUI independent decode 107', '原生二维码 UTF-8 🌏'.encode(), b'A\0B\x7f']
checks = 0
for i, payload in enumerate(expected):
    path = out / f'{i}.png'
    png = path.read_bytes()
    assert png[:8] == b'\x89PNG\r\n\x1a\n'
    offset = 8
    data = b''
    while offset < len(png):
        n = struct.unpack('>I', png[offset:offset+4])[0]
        kind = png[offset+4:offset+8]
        content = png[offset+8:offset+8+n]
        assert binascii.crc32(kind + content) == struct.unpack('>I', png[offset+8+n:offset+12+n])[0]
        if kind == b'IDAT':
            data += content
        offset += n+12
    assert len(zlib.decompress(data)) == 464 * (464 * 4+1)
    result = subprocess.run([decoder, '--quiet', '--raw', str(path)], check=True, capture_output=True)
    assert result.stdout == payload+b'\n', (i, result.stdout)
    checks += 1
    svg = out / f'{i}.svg'
    assert ET.parse(svg).getroot().tag == '{http://www.w3.org/2000/svg}svg'
    rendered = out / f'{i}-svg.png'
    subprocess.run([rasterizer, '-w', '464', '-h', '464', '-o', str(rendered), str(svg)], check=True)
    result = subprocess.run([decoder, '--quiet', '--raw', str(rendered)], check=True, capture_output=True)
    assert result.stdout == payload+b'\n', (i, result.stdout)
    checks += 1
result = subprocess.run([decoder, '--quiet', '--raw', str(out/'logo.png')], check=True, capture_output=True)
assert result.stdout == expected[0]+b'\n'
checks += 1
print(f'{checks}/7 independent ZBar PNG/SVG/UTF8/binary/logo decodes passed')
