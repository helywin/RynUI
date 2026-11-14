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


def coverage_metrics(reference, native, size):
    """Material-unmixed coverage, per semantic group; never a whole-context mean.

    Integer .5-isocontour is intentionally retained: nonzero distance starts at
    one pixel, so a .5 shape cap accepts identical binary contours only.
    """
    width, height = size
    if width <= 0 or height <= 0 or len(reference) != width*height or len(native) != len(reference):
        raise ProtocolError('coverage plane dimensions differ')
    if any(type(v) not in (int, float) or not math.isfinite(v) or not 0 <= v <= 1
           for plane in (reference, native) for v in plane):
        raise ProtocolError('coverage must be validated before integration; no implicit clamp')
    mass = [sum(plane) for plane in (reference, native)]
    def error(indices):
        a, b = [sum(plane[i] for i in indices) for plane in (reference, native)]
        return abs(a-b)/max(a, 1/255)
    rows = [error(list(range(y*width, (y+1)*width))) for y in range(height)]
    columns = [error(list(range(x, width*height, width))) for x in range(width)]
    local = [error([yy*width+xx for yy in range(y, min(y+2,height)) for xx in range(x,min(x+2,width))])
             for y in range(0,height,2) for x in range(0,width,2)]
    points = [{(i%width, i//width) for i,v in enumerate(plane) if v >= .5} for plane in (reference,native)]
    return {'required_ink_present': all(v > 0 for v in mass),
            'coverage_mass_relative_error': abs(mass[0]-mass[1])/mass[0] if mass[0] else None,
            # Measurement reach covers the fixed 1 logical px negatives at
            # DPR<=4. Acceptance caps remain .5/1 physical px, unchanged.
            'contour_distance_px': contour_distance(*[contour(p) for p in points],bound=8),
            'projection_relative_error': max(rows+columns),
            'coverage_local_relative_error': max(local),
            'reference_mass': mass[0], 'native_mass': mass[1]}


def shadow_profile_metrics(reference, native, size, *, tail_indices=(), profile_indices=()):
    """Soft ink uses low-alpha profiles and tails, never a .5 isocontour.

    Indices come exclusively from fixed source geometry. Missing/extra ink is
    checked locally even when a shadow's total mass is small beside a control.
    """
    width,height=size
    if width<=0 or height<=0 or len(reference)!=width*height or len(native)!=len(reference):
        raise ProtocolError('shadow plane dimensions differ')
    if any(type(v) not in (int,float) or not math.isfinite(v) or not 0<=v<=1
           for plane in (reference,native) for v in plane):
        raise ProtocolError('shadow contribution outside validated domain')
    if any(type(i) is not int or not 0<=i<len(reference) for i in (*tail_indices,*profile_indices)):
        raise ProtocolError('shadow profile/tail escapes fixed support')
    def projection(indices):
        a,b=[sum(p[i] for i in indices) for p in (reference,native)]
        return abs(a-b)/max(a,1/255)
    projections=[projection(range(y*width,(y+1)*width)) for y in range(height)]
    projections.extend(projection(range(x,width*height,width)) for x in range(width))
    mass=sum(reference),sum(native)
    samples=tuple(profile_indices) or tuple(range(len(reference)))
    return {'required_ink_present':all(v>0 for v in mass),
            'coverage_mass_relative_error':abs(mass[0]-mass[1])/mass[0] if mass[0] else None,
            'projection_relative_error':max(projections),
            'profile_max_error':max((abs(reference[i]-native[i]) for i in samples),default=0),
            'tail_max_error':max((abs(reference[i]-native[i]) for i in tail_indices),default=0),
            'reference_mass':mass[0],'native_mass':mass[1]}


def reference_aa_is_bounded(coverage,size):
    """Reference-defined transition only, with a fixed one-physical-pixel bound."""
    width,height=size
    if len(coverage)!=width*height:
        raise ProtocolError('reference AA coverage extent differs')
    boundary=contour({(i%width,i//width) for i,v in enumerate(coverage) if v>=.5})
    for i,value in enumerate(coverage):
        if 1/255<value<1-1/255:
            x,y=i%width,i//width
            if not any((x+dx,y+dy) in boundary for dx,dy in ((0,0),(-1,0),(1,0),(0,-1),(0,1))):
                return False
    return True


def sample_anchor(target):
    """Content anchor is observed retained host/fixed wrapper, never the ink union."""
    anchor = target.get('tokens', {}).get('sample_anchor')
    if not isinstance(anchor, dict) or set(anchor) != {'source', 'container', 'content', 'padding'}:
        raise ProtocolError('sample has no independently observed fixed host content anchor')
    if anchor['source'] not in ('retained-host-layout', 'fixed-web-wrapper'):
        raise ProtocolError('sample anchor cannot come from content union or a source recipe')
    container, content = rect(anchor['container']), rect(anchor['content'])
    padding = anchor['padding']
    if (not isinstance(padding, dict) or set(padding) != {'left','top','right','bottom'} or
            any(type(v) not in (int,float) or not math.isfinite(v) or v < 0 for v in padding.values())):
        raise ProtocolError('sample host padding is unavailable')
    if any(abs(a-b) > 1e-6 for a,b in ((content['x'],container['x']+padding['left']),
            (content['y'],container['y']+padding['top']),
            (content['width'],container['width']-padding['left']-padding['right']),
            (content['height'],container['height']-padding['top']-padding['bottom']))):
        raise ProtocolError('sample content anchor contradicts retained host geometry')
    return content


def text_groups(text, side):
    """Common UTF-8 semantic groups. Borders/background are not glyph geometry.

    Web groups are actual Range/UA Range observations, not evenly divided boxes.
    Native coverage must be exported from GlyphPrimitive alongside each instance.
    No bounding-box fallback is permitted for missing observations.
    """
    content = text['content']
    encoded = content.encode('utf-8')
    groups = text.get('semantic_groups')
    if not isinstance(groups,list) or not groups:
        raise ProtocolError('text lacks actual semantic glyph/group observations')
    boundaries = {0}
    offset = 0
    for scalar in content:
        offset += len(scalar.encode('utf-8'))
        boundaries.add(offset)
    seen = set()
    result = []
    for group in groups:
        begin, end = group.get('byte_begin'), group.get('byte_end')
        if (type(begin) is not int or type(end) is not int or begin not in boundaries or end not in boundaries
                or begin >= end or (begin,end) in seen):
            raise ProtocolError('invalid or duplicate semantic UTF-8 group')
        seen.add((begin,end))
        string = encoded[begin:end].decode('utf-8')
        boxes = group.get('boxes')
        if (not isinstance(boxes,list) or not boxes and not string.isspace() or
                type(group.get('advance')) not in (int,float) or not math.isfinite(group['advance']) or group['advance'] < 0):
            raise ProtocolError('semantic group has no actual layout boxes')
        for box in boxes:
            rect(box)
        if side == 'native':
            instances = [i for i in text.get('ink_instances', []) if i.get('byte_begin') == begin and i.get('byte_end') == end]
            if not string.isspace() and not instances:
                raise ProtocolError('printable semantic group has no actual glyph primitive')
            for instance in instances:
                if (instance.get('rotation_degrees') != 0 or not isinstance(instance.get('line'),int)
                        or 'color' not in instance or 'clip' not in instance or 'translation' not in instance):
                    raise ProtocolError('glyph material/clip/ownership unavailable or unsupported')
        result.append({**group, 'content': string})
    intervals = sorted(seen)
    if intervals[0][0] != 0 or intervals[-1][1] != len(encoded) or any(a[1] != b[0] for a,b in zip(intervals,intervals[1:])):
        raise ProtocolError('semantic groups do not completely cover the actual text')
    return result
