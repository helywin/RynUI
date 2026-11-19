"""Generate finite CPU glyph witnesses from pinned SFNT bytes and independent cmap IDs."""
from __future__ import annotations

import math
import uuid
from dataclasses import dataclass

from .adapters import process
from .protocol import ProtocolError, read, relative_path
from .reporting import write_json


@dataclass(frozen=True)
class GlyphWitness:
    left: int
    top: int
    width: int
    height: int
    ink: bytes

    def at(self, x, y):
        x -= self.left
        y -= self.top
        return self.ink[y * self.width + x] if 0 <= x < self.width and 0 <= y < self.height else 0


def build(manifest, root, cells):
    policy, = manifest['environment']['native_capture']['font_raster_policies']
    identity = policy.get('rasterizer')
    if identity not in ('directwrite-3x1', 'fontations-skia-a8'):
        return None
    dpr = manifest['scope']['actual_dpr']
    fonts = manifest['fonts']
    descriptors = {(item['identity'], item['weight']): (i, item['sha256']) for i, item in enumerate(fonts)}
    priors = manifest['font_priors']
    specs = {}
    for cell in cells:
        if cell['kind'] != 'text':
            continue
        content, size = cell['content'], cell['font_size']
        key = 'noto-sans' if content.isascii() else 'noto-sans-cjk-sc'
        font_index, font_sha = descriptors[key, cell['font_weight']]
        glyph_id = priors[font_sha + ':' + content]['glyph_id']
        px = cell['rect']['x'] * dpr
        phase = math.floor(px * 4 + .5) % 4 if identity == 'directwrite-3x1' else 0
        specs[font_sha, content, size, phase] = dict(font=font_index, glyph_id=glyph_id,
                                                    phase=phase, physical_em=size * dpr)
    admitted = [{**font, 'path': str(root / relative_path(font['path']))} for font in fonts]
    return identity, read_masks(manifest, root, identity, specs, policy=policy, fonts=admitted)


def read_masks(manifest, root, identity, specs, **inputs):
    from .calibration import exact_fields

    work = root / 'out/demo-differential/control-witnesses' / uuid.uuid4().hex
    work.mkdir(parents=True)
    request = work / 'request.json'
    write_json(request, dict(rasterizer=identity, glyphs=list(specs.values()), **inputs))
    output = work / 'output'
    binary = root / relative_path(manifest['tools']['control_binary'])
    process([binary, '--glyph-witnesses', request, '--output', output], work / 'process', 60, failure_code=3)
    receipt = exact_fields(read(output / 'glyph-witnesses.json'), 'rasterizer masks', 'glyph witness output')
    masks = receipt['masks']
    if receipt['rasterizer'] != identity or not isinstance(masks, list) or len(masks) != len(specs):
        raise ProtocolError('CPU witness set does not match the admitted glyphs')
    result = {}
    for key, entry in zip(specs, masks, strict=True):
        row = exact_fields(entry, 'left top width height r8', 'glyph witness mask')
        if (any(type(row[name]) is not int for name in ('left', 'top', 'width', 'height')) or
                not all(0 < row[name] <= 256 for name in ('width', 'height'))):
            raise ProtocolError('glyph witness extent is invalid')
        try:
            ink = bytes.fromhex(row['r8'])
        except (TypeError, ValueError) as error:
            raise ProtocolError('glyph witness is not encoded R8') from error
        if len(ink) != row['width'] * row['height'] or not any(ink):
            raise ProtocolError('glyph witness has no admitted outline coverage')
        result[key] = GlyphWitness(row['left'], row['top'], row['width'], row['height'], ink)
    return result


def icon_layers(cell, dpr):
    from .formal_icon_source import resource

    entry, _, _, cmap = resource(cell['content'])
    x, phase_x = divmod(math.floor(4 * cell['rect']['x'] * dpr + .5), 4)
    y, phase_y = divmod(math.floor(4 * cell['rect']['y'] * dpr + .5), 4)
    em = cell['font_size'] * dpr
    return [((cmap[layer['codepoint']], em, phase_x, phase_y), x, y, layer['opacity']) for layer in entry['layers']]


def build_icons(manifest, root, cells):
    native_capture = manifest['environment']['native_capture']
    if native_capture.get('icon_rasterizer') != 'skia-svg-a8':
        return None
    dpr = manifest['scope']['actual_dpr']
    specs = {}
    for cell in cells:
        if cell['kind'] != 'icon':
            continue
        for key, *_ in icon_layers(cell, dpr):
            glyph, em, phase_x, phase_y = key
            specs[key] = dict(glyph_id=glyph, physical_em=em, phase=phase_x, phase_y=phase_y)
    return read_masks(manifest, root, 'skia-svg-a8', specs)


def place_icon(masks, cell, dpr):
    return [(masks[key], x, y, opacity) for key, x, y, opacity in icon_layers(cell, dpr)]


def icon_ink_at(layers, x, y):
    coverage = 0.
    for mask, left, top, opacity in layers:
        alpha = opacity * mask.at(x-left, y-top) / 255
        coverage = alpha + coverage * (1-alpha)
    return True if coverage == 1 else False if coverage == 0 else None


def place(witnesses, source, size, origin, dpr):
    identity, masks = witnesses
    physical_x, physical_y = (value * dpr for value in origin)
    y = math.floor(physical_y + .5)
    if identity == 'directwrite-3x1':
        x, phase = divmod(math.floor(4 * physical_x + .5), 4)
    else:
        x, phase = math.floor(physical_x + .5), 0
    key = source['actual_font_sha256'], source['content'], size, phase
    try:
        return masks[key], x, y
    except KeyError as error:
        raise ProtocolError('control source has no independent glyph witness') from error
