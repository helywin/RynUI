"""Source-only physical samples for locked rounded fills and borders."""
from __future__ import annotations

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


class ShapeWitnesses:
    def __init__(self, manifest, root):
        self.root = root
        self.binary = root / relative_path(manifest['tools']['control_binary'])
        self.samples = {}
        self.shared = {}
        self.blend_samples = {}
        environment = manifest.get('environment', {})
        native = environment.get('native', {})
        capture = environment.get('native_capture', {})
        self.split_coverage = (native.get('backend') == 'vulkan' and
                               native.get('gpu', {}).get('driver_name') == 'llvmpipe' and
                               capture.get('framebuffer_format') == 'BGRA8_UNORM')

    def blend_pixels(self, inventories, dpr):
        return bind_blends(inventories, dpr, self.root, self.binary, self.blend_samples,
                           split_coverage=self.split_coverage)

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
