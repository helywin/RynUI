"""Source-only physical samples for locked rounded fills and borders."""
from __future__ import annotations

import hashlib
import math
import struct
import uuid

from .adapters import process
from .protocol import ProtocolError, read, relative_path
from .reporting import write_json
from .shape_blend_witness import bind_blends


def shape_request(row, dpr):
    if row['alpha'] != 1 or row['opacity'] != 1:
        return None
    kind = row['kind']
    if kind not in ('fill', 'outline', 'border'):
        return None
    bounds = row['bounds']
    radii = row['radii']
    if kind == 'fill' and row['side'] in ('native', 'web'):
        if isinstance(radii, (list, tuple)):
            if len(set(radii)) != 1:
                return None
            radii = radii[0]
        radius = radii
        width = offset = 0
    elif row['side'] == 'native' and kind == 'outline':
        width = row['width']
        offset = row['offset_width']
        radius = radii
    elif row['side'] == 'web' and kind == 'border' and len(set(radii)) == len(set(row['widths'])) == 1:
        width = row['widths'][0]
        offset = 0
        radius = radii[0] - width
        bounds = dict(x=bounds['x'] + width, y=bounds['y'] + width,
                      width=bounds['width'] - 2 * width, height=bounds['height'] - 2 * width)
    else:
        return None
    if radius <= 0 or (kind != 'fill' and width <= 0) or offset < 0 or min(bounds['width'], bounds['height']) <= 0:
        return None
    x, y, w, h, radius, width, offset = (
        value * dpr for value in (bounds['x'], bounds['y'], bounds['width'], bounds['height'], radius, width, offset))
    # The physical source has to survive normalization without a phase change.
    if not all(value * 4 == round(value * 4) for value in (x, y, w, h, radius, width, offset)):
        return None
    pad = math.ceil(width + offset) + 2
    if max(w, h) + 2 * pad + 2 > 512 or width + offset > 32 or radius > 256:
        return None
    dx, dy = math.floor(x) - pad, math.floor(y) - pad
    dark = sum(component * weight for component, weight in zip(row['color'], (.2126, .7152, .0722))) < .5
    return (kind, x - dx, y - dy, w, h, radius, width, offset, dark), (dx, dy)


def edge_samples(entry):
    if not isinstance(entry, dict) or set(entry) != {'coverage_extent', 'edges'}:
        raise ProtocolError('malformed CPU rounded witness')
    extent, samples = entry['coverage_extent'], entry['edges']
    if (not isinstance(extent, list) or len(extent) != 4 or any(type(value) is not int for value in extent)
            or not isinstance(samples, list)):
        raise ProtocolError('invalid CPU rounded witness extent')
    x, y, width, height = extent
    if min(x, y) < 0 or not 0 < width <= 512 or not 0 < height <= 512 or max(x + width, y + height) > 576:
        raise ProtocolError('CPU rounded witness exceeds its aperture')
    result = {}
    for pixel in samples:
        if (not isinstance(pixel, list) or len(pixel) != 3 or any(type(value) is not int for value in pixel)
                or not x <= pixel[0] < x + width or not y <= pixel[1] < y + height or not 0 < pixel[2] < 255
                or tuple(pixel[:2]) in result):
            raise ProtocolError('invalid or duplicate CPU rounded witness sample')
        result[tuple(pixel[:2])] = pixel[2] / 255
    return result


def shadow_group_request(rows, dpr):
    source = rows[0]['source']
    fact = source.get('isolated_shadow_group')
    if fact is None:
        return None
    if (not isinstance(fact, dict) or fact.get('source') != 'Chromium LayerTree.snapshotCommandLog' or
            not isinstance(fact.get('layer_id'), str) or not fact['layer_id'].isdigit() or
            fact.get('bounds') != source['actual']['bounds']):
        raise ProtocolError('invalid Chromium isolated shadow source')
    if (not 2 <= len(rows) <= 4 or any(row['kind'] != 'shadow' or row['color'] != (0, 0, 0) or
            row['bounds'] != rows[0]['bounds'] or row['clips'] or row['opacity'] != 1 or
            not row.get('knockout') or row.get('caster_inset') != 1 or
            any(opacity != 1 for _, opacity in row['groups']) for row in rows)):
        return None
    if (fact.get('shadows') != [dict(spread=row['spread'], offset=list(row['offset'])) for row in rows]
            or any(row['radii'] != fact.get('radius') for row in rows)):
        raise ProtocolError('Chromium isolated shadow draw differs from its actual CSS source')
    box = rows[0]['bounds']
    x, y, w, h = [box[key] * dpr for key in ('x', 'y', 'width', 'height')]
    radius = fact['radius'] * dpr
    values = tuple((row['offset'][0] * dpr, row['offset'][1] * dpr, row['blur'] * dpr,
                    row['spread'] * dpr, row['alpha']) for row in rows)
    if (not all(v * 4 == round(v * 4) for v in (x, y, w, h, radius)) or radius <= dpr or
            any(not 0 < blur <= 64 or abs(spread) > 32 or max(abs(ox), abs(oy)) > 64 or not 0 < alpha < 1 or
                radius + spread <= 0 for ox, oy, blur, spread, alpha in values)):
        return None
    pad = math.ceil(max(2 + max(abs(ox), abs(oy)) + abs(spread) + 2 * blur
                        for ox, oy, blur, spread, _ in values))
    left, top = math.floor(x) - pad, math.floor(y) - pad
    width, height = math.ceil(x + w) - left + pad, math.ceil(y + h) - top + pad
    if max(width, height) > 1024:
        return None
    return ((x - left, y - top, w, h, radius, dpr, width, height, values), (left, top))


class ShapeWitnesses:
    def __init__(self, manifest, root):
        self.root = root
        self.binary = root / relative_path(manifest['tools']['control_binary'])
        self.samples = {}
        self.shared = {}
        self.blend_samples = {}
        self.shadow_groups = {}
        environment = manifest.get('environment', {})
        native = environment.get('native', {})
        capture = environment.get('native_capture', {})
        self.split_coverage = (native.get('backend') == 'vulkan' and
                               native.get('gpu', {}).get('driver_name') == 'llvmpipe' and
                               capture.get('framebuffer_format') == 'BGRA8_UNORM')
        self.opacity_surface_rgb8 = (
            environment.get('web', {}).get('version') == '141.0.7390.37' and
            (self.split_coverage or (native.get('backend', '').lower() in ('d3d12', 'direct3d12')
                                     and capture.get('framebuffer_format') == 'BGRA8_UNORM')))

    def blend_pixels(self, inventories, dpr):
        return bind_blends(inventories, dpr, self.root, self.binary, self.blend_samples,
                           split_coverage=self.split_coverage, isolated_opacity=self.opacity_surface_rgb8)

    def bind_shadow_groups(self, layers, dpr):
        owned = {}
        for row in layers:
            if row['kind'] in ('shadow', 'inset-shadow') and row['source'].get('isolated_shadow_group'):
                owned.setdefault(row['source']['source_material_id'], []).append(row)
        inputs = [(rows, request) for rows in owned.values()
                  if (request := shadow_group_request(rows, dpr)) is not None]
        needed = list(dict.fromkeys(spec for _, (spec, _) in inputs if spec not in self.shadow_groups))
        for start in range(0, len(needed), 8):
            self._shadow_batch(needed[start:start+8])
        for rows, (spec, shift) in inputs:
            witness = self.shadow_groups[spec], spec[6], spec[7], shift
            for row in rows:
                row['isolated_shadow_witness'] = witness

    def _shadow_batch(self, specs):
        work = self.root / 'out/demo-differential/shadow-group-witnesses' / uuid.uuid4().hex
        work.mkdir(parents=True)
        request, output = work / 'request.json', work / 'output'
        groups = [dict(caster=dict(x=x, y=y, width=w, height=h), radius=r, caster_inset=i, width=width, height=height,
                       shadows=[dict(offset_x=ox, offset_y=oy, blur=b, spread=s, alpha=a)
                                for ox, oy, b, s, a in shadows])
                  for x, y, w, h, r, i, width, height, shadows in specs]
        write_json(request, dict(groups=groups))
        process([self.binary, '--shadow-group-witnesses', request, '--output', output], work / 'process', 60,
                failure_code=3)
        receipt = read(output / 'shadow-group-witnesses.json')
        if (not isinstance(receipt, dict) or receipt.get('rasterizer') != 'skia-isolated-shadow-n32' or
                not isinstance(receipt.get('groups'), list) or len(receipt['groups']) != len(specs)):
            raise ProtocolError('CPU shadow witness set differs from its source requests')
        for index, (spec, result) in enumerate(zip(specs, receipt['groups'], strict=True)):
            name = f'shadow-alpha-{index}.r8'
            width, height = spec[6:8]
            path = output / name
            if (not isinstance(result, dict) or result.get('path') != name or result.get('width') != width or
                    result.get('height') != height or not path.is_file() or path.stat().st_size != width * height):
                raise ProtocolError('CPU shadow witness alpha extent differs')
            alpha = path.read_bytes()
            if hashlib.sha256(alpha).hexdigest() != result.get('sha256'):
                raise ProtocolError('CPU shadow witness alpha bytes differ')
            self.shadow_groups[spec] = alpha

    def bind(self, layers, dpr):
        paired = {}
        for row in layers:
            if (request := shape_request(row, dpr)) is not None:
                spec, shift = request
                key = spec[1:], shift, struct.pack('<3f', *row['color'])
                paired.setdefault(key, {}).setdefault(row['side'], []).append((row, spec))
        needed = list(dict.fromkeys(
            spec for sides in paired.values() if len(sides) == 2
            for rows in sides.values() for _, spec in rows if spec not in self.samples))
        for start in range(0, len(needed), 128):
            self._batch(needed[start:start+128])
        for (_, shift, _), sides in paired.items():
            if len(sides) != 2:
                continue
            web = sides['web'][0][1]
            native = sides['native'][0][1]
            key = web, native
            if key not in self.shared:
                left, right = self.samples[web], self.samples[native]
                self.shared[key] = {point: value for point, value in left.items() if right.get(point) == value}
            for rows in sides.values():
                for row, _ in rows:
                    row['locked_edge_witness'] = (self.shared[key], shift)

    def _batch(self, specs):
        work = self.root / 'out/demo-differential/shape-witnesses' / uuid.uuid4().hex
        work.mkdir(parents=True)
        request, output = work / 'request.json', work / 'output'
        shapes = [dict(kind=kind, bounds=dict(x=x, y=y, width=w, height=h), radius=radius,
                       outline_width=width, outline_offset=offset, dark_ink=dark)
                  for kind, x, y, w, h, radius, width, offset, dark in specs]
        write_json(request, dict(shapes=shapes))
        process([self.binary, '--shape-witnesses', request, '--output', output], work / 'process', 60, failure_code=3)
        receipt = read(output / 'shape-witnesses.json')
        if (not isinstance(receipt, dict) or set(receipt) != {'rasterizer', 'shapes'}
                or receipt['rasterizer'] != 'skia-rounded-coverage' or not isinstance(receipt['shapes'], list)
                or len(receipt['shapes']) != len(specs)):
            raise ProtocolError('CPU shape witness set differs from the requested sources')
        self.samples.update({spec: edge_samples(result) for spec, result in zip(specs, receipt['shapes'], strict=True)})


def locked_edge_value(layer, point, dpr, value):
    witness = layer.get('locked_edge_witness')
    if witness and value == 1:
        samples, (dx, dy) = witness
        return samples.get((math.floor(point[0] * dpr) - dx, math.floor(point[1] * dpr) - dy), value)
    return value
