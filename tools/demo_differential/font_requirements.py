"""Actual pinned face/weight admission, separate from renderer and resource locks."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import struct

from .adapters import RunError
from .protocol import output_path


def required_faces(case):
    if case['fixture']['font_policy'] != 'pinned':
        return []
    # Gallery source/support paragraphs request 500; the shell and Select also
    # request 600. Neither can use relabeled regular bytes or a system fallback.
    result = [{'identity': identity, 'weight': weight, 'slant': 'upright', 'face_index': 0,
             'required_by': ('native Gallery source/support paragraphs' if weight == 500 else
                             'native Gallery shell / Select'), 'lock_status': 'central-resource-lock',
             'required_resource_fields': ['source_url', 'sha256', 'license', 'face_index', 'weight']}
            for weight in (500, 600) for identity in ('noto-sans', 'noto-sans-cjk-sc')]
    if '\U0001f642' in json.dumps([case['fixture'], case.get('actions', [])], ensure_ascii=False):
        result.append({'identity': 'noto-emoji', 'weight': 400, 'slant': 'upright', 'face_index': 0,
                       'required_by': 'Input actual U+1F642 at weight 400', 'lock_status': 'central-resource-lock',
                       'required_resource_fields': ['source_url', 'sha256', 'license', 'face_index', 'weight']})
    return result


def sfnt_weight(path, face_index):
    data = Path(path).read_bytes()
    offset = 0
    try:
        if data[:4] == b'ttcf':
            count = struct.unpack_from('>I', data, 8)[0]
            if not 0 <= face_index < count:
                raise ValueError('font collection face index out of range')
            offset = struct.unpack_from('>I', data, 12 + face_index * 4)[0]
        elif face_index != 0:
            raise ValueError('single font requires face index zero')
        count = struct.unpack_from('>H', data, offset + 4)[0]
        for index in range(count):
            tag, _, start, length = struct.unpack_from('>4sIII', data, offset + 12 + index * 16)
            if tag == b'OS/2':
                if length < 64 or start + length > len(data):
                    raise ValueError('font OS/2 table is truncated')
                weight = struct.unpack_from('>H', data, start + 4)[0]
                flags = struct.unpack_from('>H', data, start + 62)[0]
                return weight, bool(flags & (1 | 512))
    except (struct.error, IndexError) as error:
        raise ValueError('font has no complete actual face weight metadata') from error
    raise ValueError('font has no actual OS/2 face weight')


def preflight(case, files):
    expected = case['fixture']['fonts']
    if [{k: f[k] for k in ('identity', 'sha256', 'face_index', 'weight')} for f in files] != expected:
        raise RunError(3, 'environment', 'pinned requested font chain differs from explicit file chain')
    available = {(f['identity'], f['weight']) for f in files}
    if len(available) != len(files):
        raise RunError(3, 'environment', 'duplicate explicit font identity/weight')
    for item in files:
        path = output_path(item['path'])
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != item['sha256']:
            raise RunError(3, 'environment', 'explicit font bytes/hash unavailable: ' + item['identity'])
        try:
            weight, italic = sfnt_weight(path, item['face_index'])
        except ValueError as error:
            raise RunError(3, 'environment', str(error)) from error
        if weight != item['weight'] or italic:
            raise RunError(3, 'environment', 'actual font weight/slant differs; regular bytes cannot be relabeled 500/600: ' + item['identity'])
    missing = [f for f in required_faces(case) if (f['identity'], f['weight']) not in available]
    if missing:
        error = RunError(3, 'environment', 'missing actual required font faces; prepare the central acceptance font chain')
        error.details = {'missing_font_faces': missing, 'implicit_fallback': False,
                         'normal_gallery_system_default': 'unchanged'}
        raise error
    # Same exact bytes, face, weight and PostScript identity as the Web loader.
    from demo_reference_web.font_resources import admit_font
    try:
        for requested, actual in zip(expected, files):
            admit_font(requested, actual, case['fixture']['font_policy'])
        if any(row['identity'] == 'noto-emoji' for row in required_faces(case)):
            from demo_reference_web.font_metadata import font_metadata
            emoji = next(row for row in files if row['identity'] == 'noto-emoji' and row['weight'] == 400)
            metadata = font_metadata(emoji['path'], emoji['face_index'], [0x1f642])
            if not metadata['nominal_glyphs'][str(0x1f642)]:
                raise ValueError('explicit weight-400 emoji face has no nominal U+1F642 glyph')
    except (ValueError, OSError) as error:
        raise RunError(3, 'environment', str(error)) from error
