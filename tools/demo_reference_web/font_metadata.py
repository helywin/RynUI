"""Read actual static SFNT face metadata/cmap without optional font tooling."""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct


def font_metadata(path, face_index=0, codepoints=()):
    data = Path(path).read_bytes()
    def unpack(fmt, start):
        try:
            return struct.unpack_from('>' + fmt, data, start)
        except struct.error as error:
            raise ValueError('truncated SFNT face/table') from error
    if type(face_index) is not int or face_index < 0:
        raise ValueError('invalid actual face index')
    offset = 0
    if data[:4] == b'ttcf':
        count, = unpack('I', 8)
        if face_index >= count:
            raise ValueError('font collection face index out of range')
        offset, = unpack('I', 12 + face_index * 4)
    elif face_index != 0:
        raise ValueError('single font requires face index zero')
    if data[offset:offset + 4] not in (b'\x00\x01\x00\x00', b'OTTO'):
        raise ValueError('not an SFNT font')
    tables = {}
    count, = unpack('H', offset + 4)
    for index in range(count):
        tag, _, start, length = unpack('4sIII', offset + 12 + index * 16)
        if start + length > len(data) or tag in tables:
            raise ValueError('truncated/duplicate SFNT table')
        tables[tag] = (start, length)
    def table(tag, minimum):
        if tag not in tables or tables[tag][1] < minimum:
            raise ValueError('missing/truncated actual ' + tag.decode() + ' table')
        return tables[tag][0]
    os2 = table(b'OS/2', 64)
    weight, = unpack('H', os2 + 4)
    flags, = unpack('H', os2 + 62)
    head = table(b'head', 54)
    style, = unpack('H', head + 44)
    post = table(b'post', 8)
    angle, = unpack('i', post + 4)
    italic = bool(flags & (1 | 512) or style & 2 or angle)
    names = {}
    start = table(b'name', 6)
    _, count, strings = unpack('HHH', start)
    end = start + tables[b'name'][1]
    for row in range(count):
        record = start + 6 + row * 12
        if record + 12 > end:
            raise ValueError('truncated SFNT name record')
        platform, _, _, name_id, size, position = unpack('HHHHHH', record)
        if name_id not in (1, 2, 4, 5, 6, 16, 17):
            continue
        position += start + strings
        if position + size > end:
            raise ValueError('truncated SFNT name string')
        encoding = 'utf-16-be' if platform in (0, 3) else 'mac_roman' if platform == 1 else None
        if encoding:
            value = data[position:position + size].decode(encoding)
            if value not in names.setdefault(str(name_id), []):
                names[str(name_id)].append(value)
    if not names.get('6'):
        raise ValueError('font has no actual PostScript face identity')
    variable = b'fvar' in tables
    outline = 'glyf' if b'glyf' in tables else 'CFF' if b'CFF ' in tables else None
    if not outline:
        raise ValueError('font has no monochrome SFNT outlines')
    glyphs = {}
    for scalar in codepoints:
        glyphs[str(scalar)] = nominal_glyph(data, tables, scalar)
    return {'sha256': hashlib.sha256(data).hexdigest(), 'face_index': face_index, 'names': names,
            'weight': weight, 'slant': 'italic' if italic else 'upright', 'variable': variable,
            'outline': outline, 'nominal_glyphs': glyphs}


def nominal_glyph(data, tables, scalar):
    """Unicode cmap -> nominal glyph ID; zero means missing, never U+FFFD."""
    def read(fmt, start):
        return struct.unpack_from('>' + fmt, data, start)
    if b'cmap' not in tables:
        raise ValueError('font has no cmap')
    start, length = tables[b'cmap']
    _, count = read('HH', start)
    if 4 + count * 8 > length:
        raise ValueError('truncated cmap records')
    candidates = []
    for index in range(count):
        platform, encoding, position = read('HHI', start + 4 + index * 8)
        if platform != 0 and (platform, encoding) not in ((3, 1), (3, 10)):
            continue
        position += start
        if not start <= position <= start + length - 2:
            raise ValueError('cmap subtable out of range')
        format_id, = read('H', position)
        if format_id in (4, 12):
            candidates.append((format_id, position))
    for format_id, position in sorted(set(candidates), reverse=True):
        size, = read('I' if format_id == 12 else 'H', position + (4 if format_id == 12 else 2))
        if position + size > start + length:
            raise ValueError('truncated cmap subtable')
        if format_id == 12:
            groups, = read('I', position + 12)
            if 16 + groups * 12 > size:
                raise ValueError('truncated cmap groups')
            for row in range(groups):
                first, last, glyph = read('III', position + 16 + row * 12)
                if first <= scalar <= last:
                    return glyph + scalar - first
        elif scalar <= 0xffff:
            segments, = read('H', position + 6)
            segments //= 2
            if 16 + segments * 8 > size:
                raise ValueError('truncated cmap segments')
            ends = position + 14
            starts = ends + segments * 2 + 2
            deltas = starts + segments * 2
            ranges = deltas + segments * 2
            for row in range(segments):
                first, = read('H', starts + row * 2)
                last, = read('H', ends + row * 2)
                if first <= scalar <= last:
                    delta, = read('h', deltas + row * 2)
                    shift, = read('H', ranges + row * 2)
                    if not shift:
                        return (scalar + delta) & 0xffff
                    address = ranges + row * 2 + shift + (scalar - first) * 2
                    if address + 2 > position + size:
                        raise ValueError('cmap glyph out of range')
                    glyph, = read('H', address)
                    return (glyph + delta) & 0xffff if glyph else 0
    return 0
