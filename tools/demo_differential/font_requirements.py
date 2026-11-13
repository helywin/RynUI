"""Actual pinned face/weight admission, separate from renderer and resource locks."""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct

from .adapters import RunError
from .protocol import output_path


def required_faces(case):
    if case['fixture']['font_policy'] != 'pinned':
        return []
    # The real Gallery shell and Select request upright weight 600. There is no
    # permission to relabel regular bytes or silently select a system face.
    return [{'identity': identity, 'weight': 600, 'slant': 'upright', 'face_index': 0,
             'required_by': 'native Gallery shell / Select', 'lock_status': 'missing-real-weight-600-resource',
             'required_resource_fields': ['source_url', 'sha256', 'license', 'face_index', 'weight']}
            for identity in ('noto-sans', 'noto-sans-cjk-sc')]


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
    for item in files:
        path = output_path(item['path'])
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != item['sha256']:
            raise RunError(3, 'environment', 'explicit font bytes/hash unavailable: ' + item['identity'])
        try:
            weight, italic = sfnt_weight(path, item['face_index'])
        except ValueError as error:
            raise RunError(3, 'environment', str(error)) from error
        if weight != item['weight'] or italic:
            raise RunError(3, 'environment', 'actual font weight/slant differs; regular bytes cannot be relabeled 600: ' + item['identity'])
    available = {(f['identity'], f['weight']) for f in files}
    missing = [f for f in required_faces(case) if (f['identity'], f['weight']) not in available]
    if missing:
        error = RunError(3, 'environment', 'missing real upright 600 faces for Gallery shell/Select; central resource and Web regular-only locks need an explicit extension')
        error.details = {'missing_font_faces': missing, 'implicit_fallback': False,
                         'normal_gallery_system_default': 'unchanged'}
        raise error
