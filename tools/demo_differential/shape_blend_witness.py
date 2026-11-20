"""Exact RGB8 encoding checks for finite opaque rounded and hard-shadow stacks."""
from __future__ import annotations

import math
import struct
import uuid

from .adapters import process
from .protocol import ProtocolError, read
from .reporting import write_json


def _radius(row):
    radius = row['radii']
    if isinstance(radius, (tuple, list)):
        return radius[0] if len(set(radius)) == 1 else 0
    return radius


def _inset(bounds, width):
    return dict(x=bounds['x'] + width, y=bounds['y'] + width, width=bounds['width'] - 2 * width,
                height=bounds['height'] - 2 * width)


def _rgb8(color):
    channels = tuple(round(channel * 255) for channel in color)
    return channels if struct.pack('<3f', *color) == struct.pack('<3f', *(v / 255 for v in channels)) else None


def _stacks(rows, side, dpr, split_coverage=False):
    translated_borders = set()
    for shadow, fill, border in zip(rows, rows[1:], rows[2:]):
        layers = (shadow, fill, border)
        if tuple(layer['kind'] for layer in layers) != ('shadow', 'fill', 'border' if side == 'web' else 'outline'):
            continue
        translated = any(shadow['offset'])
        if (any(opacity != 1 for layer in layers for _, opacity in layer['groups'])
                or shadow.get('knockout') != (side == 'web' or not translated) or shadow['blur'] != 0
                or (translated and not split_coverage)
                or not 0 < shadow['alpha'] < 1 or shadow['opacity'] != 1
                or any(layer['alpha'] != 1 or layer['opacity'] != 1 for layer in (fill, border))
                or fill['color'] != (1., 1., 1.)):
            continue
        bounds = shadow['bounds']
        radius = _radius(shadow)
        if side == 'web':
            if (not all(layer['source'] is shadow['source'] for layer in (fill, border))
                    or len(set(border['widths'])) != 1 or fill['bounds'] != bounds or border['bounds'] != bounds
                    or _radius(fill) != radius or _radius(border) != radius):
                continue
            width = border['widths'][0]
        else:
            width = border['width']
            fill_inset = 0 if translated else width * .5
            if (border['offset_width'] != 0 or fill['bounds'] != _inset(bounds, fill_inset)
                    or border['bounds'] != _inset(bounds, width)
                    or _radius(fill) != radius - fill_inset or _radius(border) != radius - width):
                continue
        shadow_color, border_color = _rgb8(shadow['color']), _rgb8(border['color'])
        if shadow_color is None or border_color is None:
            continue
        x, y, w, h, radius, width, inset, spread, ox, oy = (value * dpr for value in (
            bounds['x'], bounds['y'], bounds['width'], bounds['height'], radius, width,
            1 if translated else shadow.get('caster_inset', 0), shadow['spread'], *shadow['offset']))
        guard = 4 + math.ceil(max(spread, abs(ox), abs(oy)))
        if (not 0 < width <= 16 or not width < radius <= .5 * min(w, h)
                or not 0 < inset <= radius or max(w, h) + 2 * guard + 1 > 512
                or any(value * 4 != round(value * 4) for value in (x, y, w, h, radius, width, inset, spread))):
            continue
        split_border = (split_coverage and width == 1 and
                        all(value == round(value) for value in (x, y, w, h, radius)))
        if translated:
            if (not split_border or dpr != 1 or spread != 0
                    or max(abs(ox), abs(oy)) > 32 or any(value != round(value) for value in (ox, oy))
                    or (side == 'web' and shadow.get('caster_inset') != 1)):
                continue
            translated_borders.add(id(shadow))
        elif not 0 < spread <= 32:
            continue
        dx, dy = math.floor(x) - guard, math.floor(y) - guard
        alpha = struct.unpack('<f', struct.pack('<f', shadow['alpha']))[0]
        spec = (x - dx, y - dy, w, h, radius, width, inset, spread, alpha, shadow_color, border_color,
                (255, 255, 255), ox, oy, 0, split_border, False)
        yield (spec, dx, dy), layers
    for index, (shadow, following) in enumerate(zip(rows, rows[1:])):
        if shadow['kind'] != 'shadow' or id(shadow) in translated_borders:
            continue
        outline = None
        if following['kind'] == 'fill':
            fill = following
            if side == 'web':
                outlines = [row for row in rows[index+2:] if row['kind'] == 'outline' and
                            row['source'] is shadow['source']]
                if len(outlines) == 1:
                    outline = outlines[0]
        elif side == 'native' and following['kind'] == 'outline' and index+2 < len(rows):
            outline, fill = following, rows[index+2]
        else:
            continue
        layers = (shadow, fill)
        if (fill['kind'] != 'fill'
                or any(opacity != 1 for layer in layers for _, opacity in layer['groups'])
                or shadow['knockout'] != (side == 'web') or shadow['blur'] != 0 or shadow['spread'] != 0
                or not any(shadow['offset']) or not 0 < shadow['alpha'] < 1 or shadow['opacity'] != 1
                or fill['opacity'] != 1 or fill['alpha'] != 1 or fill['bounds'] != shadow['bounds']
                or _radius(fill) != _radius(shadow)):
            continue
        if side == 'web' and (fill['source'] is not shadow['source'] or shadow['caster_inset'] != 1):
            continue
        shadow_color, fill_color = _rgb8(shadow['color']), _rgb8(fill['color'])
        if shadow_color is None or fill_color is None:
            continue
        bounds = shadow['bounds']
        x, y, w, h, radius, inset, ox, oy = (value * dpr for value in (
            bounds['x'], bounds['y'], bounds['width'], bounds['height'], _radius(shadow), 1, *shadow['offset']))
        offset = max(abs(ox), abs(oy))
        guard = 4 + math.ceil(offset)
        if (not 0 < inset <= radius <= .5 * min(w, h) or offset > 32 or max(w, h) + 2 * guard + 1 > 512
                or any(value * 4 != round(value * 4) for value in (x, y, w, h, radius, inset, ox, oy))):
            continue
        dx, dy = math.floor(x) - guard, math.floor(y) - guard
        alpha = struct.unpack('<f', struct.pack('<f', shadow['alpha']))[0]
        split_fill = (split_coverage and dpr == 1 and
                      all(value == round(value) for value in (x, y, w, h, radius, ox, oy)))
        spec = (x - dx, y - dy, w, h, radius, 0, inset, 0, alpha, shadow_color, (0, 0, 0),
                fill_color, ox, oy, 0, False, split_fill)
        yield (spec, dx, dy), layers
        if (not outline or outline['bounds'] != bounds or _radius(outline) * dpr != radius
                or outline['alpha'] != 1 or outline['opacity'] != 1
                or any(opacity != 1 for _, opacity in outline['groups'])):
            continue
        focus_color = _rgb8(outline['color'])
        stroke, gap = outline['width'] * dpr, outline['offset_width'] * dpr
        pad = 4 + math.ceil(max(offset, stroke + gap))
        if (focus_color is None or not 0 < stroke <= 16 or stroke >= radius or not 0 < gap <= 16 or
                max(w, h) + 2 * pad + 1 > 512 or any(value * 4 != round(value * 4) for value in (stroke, gap))):
            continue
        dx, dy = math.floor(x) - pad, math.floor(y) - pad
        spec = (x - dx, y - dy, w, h, radius, stroke, inset, 0, alpha, shadow_color, focus_color,
                fill_color, ox, oy, gap, False, False)
        layers = (shadow, fill, outline) if side == 'web' else (shadow, outline, fill)
        yield (spec, dx, dy), layers
    for fill, border in zip(rows, rows[1:]):
        layers = (fill, border)
        if (fill['kind'] != 'fill' or border['kind'] != ('border' if side == 'web' else 'outline')
                or fill['color'] != (1., 1., 1.)
                or any(row['opacity'] != 1 or row['alpha'] != 1 or
                       any(opacity != 1 for _, opacity in row['groups']) for row in layers)):
            continue
        if side == 'web':
            if (border['source'] is not fill['source'] or len(set(border['widths'])) != 1
                    or border['bounds'] != fill['bounds'] or _radius(border) != _radius(fill)):
                continue
            bounds, radius, width = border['bounds'], _radius(border), border['widths'][0]
        else:
            width = border['width']
            radius = _radius(border) + width
            bounds = _inset(border['bounds'], -width)
            if (border['offset_width'] != 0 or fill['bounds'] != _inset(bounds, width * .5)
                    or _radius(fill) != radius - width * .5):
                continue
        color = _rgb8(border['color'])
        x, y, w, h, radius, width = (value * dpr for value in (
            bounds['x'], bounds['y'], bounds['width'], bounds['height'], radius, width))
        if (color is None or width != 1 or not width < radius <= .5 * min(w, h) or max(w, h) + 9 > 512
                or any(value != round(value) for value in (x, y, w, h, radius))):
            continue
        dx, dy = round(x) - 4, round(y) - 4
        spec = (4, 4, w, h, radius, width, 0, 0, 0, (0, 0, 0), color, (255, 255, 255), 0, 0, 0, False, False)
        yield (spec, dx, dy), layers


def _samples(samples):
    if not isinstance(samples, list) or len(samples) > 512 * 512:
        raise ProtocolError('invalid rounded blend witness extent')
    result = {}
    for row in samples:
        if (not isinstance(row, list) or len(row) != 8 or any(type(v) is not int for v in row)
                or any(not 0 <= coordinate < 512 for coordinate in row[:2])
                or any(not 0 <= channel <= 255 for channel in row[2:]) or row[2:5] == row[5:]
                or tuple(row[:2]) in result):
            raise ProtocolError('malformed rounded blend witness sample')
        result[tuple(row[:2])] = tuple(row[2:5]), tuple(row[5:])
    return result


def _batch(root, binary, needed, cache):
    work = root / 'out/demo-differential/shape-blend-witnesses' / uuid.uuid4().hex
    work.mkdir(parents=True)
    request, output = work / 'request.json', work / 'output'
    stacks = [dict(bounds=dict(x=x, y=y, width=w, height=h), radius=radius, border_width=width,
                   caster_inset=inset, spread=spread, shadow_alpha=alpha, shadow_color=shadow, border_color=border,
                   fill_color=fill, offset_x=ox, offset_y=oy, outline_gap=gap,
                   split_border=split_border, split_fill=split_fill)
              for x, y, w, h, radius, width, inset, spread, alpha, shadow, border, fill, ox, oy, gap,
              split_border, split_fill in needed]
    write_json(request, dict(stacks=stacks))
    process([binary, '--shape-blend-witnesses', request, '--output', output], work / 'process', 60, failure_code=3)
    receipt = read(output / 'shape-blend-witnesses.json')
    if (not isinstance(receipt, dict) or set(receipt) != {'rasterizer', 'stacks'}
            or receipt['rasterizer'] != 'skia-and-sdl-rgb8-source-over'
            or not isinstance(receipt['stacks'], list) or len(receipt['stacks']) != len(needed)):
        raise ProtocolError('rounded blend witness set differs from requested sources')
    cache.update({spec: _samples(item) for spec, item in zip(needed, receipt['stacks'], strict=True)})


def bind_blends(inventories, dpr, root, binary, cache, *, split_coverage=False):
    paired = {}
    for side, rows in zip(('web', 'native'), inventories, strict=True):
        for key, layers in _stacks(rows, side, dpr, split_coverage):
            paired.setdefault(key, {}).setdefault(side, []).append(layers)
    valid = {key: (sides['web'][0], sides['native'][0]) for key, sides in paired.items()
             if len(sides) == 2 and all(len(value) == 1 for value in sides.values())}
    needed = list(dict.fromkeys(spec for spec, _, _ in valid if spec not in cache))
    for start in range(0, len(needed), 32):
        _batch(root, binary, needed[start:start+32], cache)
    pixels = {}
    for (spec, dx, dy), layers in valid.items():
        for (x, y), values in cache[spec].items():
            pixels.setdefault((x + dx, y + dy), []).append((layers, values))
    return pixels


def verified_blend(pixels, x, y, dpr, sides, observed):
    if observed[0] == observed[1]:
        return None
    px, py = (x + .5) / dpr, (y + .5) / dpr
    def admissible(allowed, active):
        observed_order = [item for item in allowed if any(item is row for row in active)]
        return (active and len(observed_order) == len(active) and
                all(a is b for a, b in zip(observed_order, active, strict=True)) and
                all(clip['x'] <= px < clip['x'] + clip['width'] and clip['y'] <= py < clip['y'] + clip['height']
                    for row in allowed for clip in row.get('clips', ())))

    matches = [expected for layers, expected in pixels.get((x, y), ()) if expected == observed and
               all(admissible(rows, side['active']) for rows, side in zip(layers, sides, strict=True))]
    return matches[0][0] if len(matches) == 1 else None
