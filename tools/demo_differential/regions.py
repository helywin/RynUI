"""Logical ROI math and local raster evidence; no registration or global mean verdict."""
from __future__ import annotations

from collections import deque
import math

from .protocol import ProtocolError


def rect(value):
    if set(value) != {'x', 'y', 'width', 'height'} or any(
            type(item) not in (int, float) or not math.isfinite(item) for item in value.values()):
        raise ProtocolError('ROI requires a finite logical rectangle')
    if value['width'] <= 0 or value['height'] <= 0:
        raise ProtocolError('required ROI is empty')
    return dict(value)


def relative(value, sample_origin):
    value = rect(value)
    origin = rect(sample_origin)
    return {**value, 'x': value['x'] - origin['x'], 'y': value['y'] - origin['y']}


def union(reference, native):
    reference, native = rect(reference), rect(native)
    x = min(reference['x'], native['x'])
    y = min(reference['y'], native['y'])
    return {'x': x, 'y': y,
            'width': max(reference['x'] + reference['width'], native['x'] + native['width']) - x,
            'height': max(reference['y'] + reference['height'], native['y'] + native['height']) - y}


def physical_box(value, dpr, guard=2):
    value = rect(value)
    if type(dpr) not in (float, int) or not math.isfinite(dpr) or dpr <= 0:
        raise ProtocolError('ROI has no actual positive DPR')
    if type(guard) is not int or guard < 2:
        raise ProtocolError('ROI guard must include at least two physical pixels')
    return (math.floor(value['x'] * dpr) - guard, math.floor(value['y'] * dpr) - guard,
            math.ceil((value['x'] + value['width']) * dpr) + guard,
            math.ceil((value['y'] + value['height']) * dpr) + guard)


def geometry(reference, native, reference_origin, native_origin):
    """Exactly one declared sample translation. Never align individual targets."""
    a = relative(reference, reference_origin)
    b = relative(native, native_origin)
    return {key: abs(a[key] - b[key]) for key in a}


def crop_rgba(data, size, box):
    width, height = size
    if type(width) is not int or type(height) is not int or width <= 0 or height <= 0 or len(data) != width * height * 4:
        raise ProtocolError('RGBA capture extent or decoded byte count differs')
    left, top, right, bottom = box
    if any(type(value) is not int for value in box) or not (0 <= left < right <= width and 0 <= top < bottom <= height):
        raise ProtocolError('ROI is empty or needs an uncaptured tile/guard')
    return b''.join(data[(y * width + left) * 4:(y * width + right) * 4] for y in range(top, bottom))


def component_sizes(points):
    remaining = set(points)
    sizes = []
    while remaining:
        first = remaining.pop()
        queue = deque([first])
        count = 0
        while queue:
            x, y = queue.popleft()
            count += 1
            for neighbour in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if neighbour in remaining:
                    remaining.remove(neighbour)
                    queue.append(neighbour)
        sizes.append(count)
    return sorted(sizes, reverse=True)


def contour(points):
    return {(x, y) for x, y in points if any(neighbour not in points for neighbour in
            ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)))}


def contour_distance(reference, native, *, bound=2):
    """Exact nearest distance inside a finite rejection bound, in physical pixels."""
    if not reference or not native:
        return None  # No contour must never become a numeric zero/pass.
    offsets = sorted((math.hypot(x, y), x, y) for x in range(-bound, bound + 1)
                     for y in range(-bound, bound + 1) if math.hypot(x, y) <= bound)
    maximum = 0
    for source, target in ((reference, native), (native, reference)):
        for x, y in source:
            distance = next((distance for distance, dx, dy in offsets if (x + dx, y + dy) in target), None)
            if distance is None:
                return None
            maximum = max(maximum, distance)
    return maximum


def raster_metrics(reference, native, size, background, *, ink_minimum=1):
    """Call on each required icon/glyph/run as well as its complete context.

    Background is an independently declared/calibrated RGBA8 input. It is never
    estimated from either failed capture. Color/profile validation is a caller
    prerequisite; these raw-byte metrics do not claim a calibrated color verdict.
    """
    width, height = size
    if len(background) != 4 or any(type(x) is not int or not 0 <= x <= 255 for x in background):
        raise ProtocolError('region background must name four independent encoded bytes')
    if type(ink_minimum) is not int or ink_minimum < 1:
        raise ProtocolError('required foreground cannot have a zero occupancy gate')
    if width <= 0 or height <= 0 or len(reference) != width * height * 4 or len(native) != len(reference):
        raise ProtocolError('raster dimensions differ; resizing is forbidden')
    foreground = [set(), set()]
    mass = [0, 0]
    changed = set()
    max_channel = 0
    for index in range(width * height):
        a = reference[index * 4:index * 4 + 4]
        b = native[index * 4:index * 4 + 4]
        point = (index % width, index // width)
        residual = max(abs(x - y) for x, y in zip(a, b))
        max_channel = max(max_channel, residual)
        if residual:
            changed.add(point)
        for side, pixel in enumerate((a, b)):
            contrast = max(abs(x - y) for x, y in zip(pixel, background))
            if contrast:
                foreground[side].add(point)
                mass[side] += contrast / 255
    return {'max_channel_residual': max_channel, 'changed_pixels': len(changed),
            'changed_components': component_sizes(changed),
            'reference_occupancy': len(foreground[0]), 'native_occupancy': len(foreground[1]),
            'required_ink_present': all(len(points) >= ink_minimum for points in foreground),
            'coverage_mass_relative_error': abs(mass[0] - mass[1]) / mass[0] if mass[0] else None,
            'contour_distance_px': contour_distance(contour(foreground[0]), contour(foreground[1]))}
