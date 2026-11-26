"""Generate finite CPU glyph witnesses from pinned SFNT bytes and independent cmap IDs."""
from __future__ import annotations

import math
import uuid
from dataclasses import dataclass
from functools import lru_cache

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


def scenario_masks(manifest, root, texts, priors):
    """Rasterize independently identified SFNT glyphs at submitted phases."""
    policy, = manifest['environment']['native_capture']['font_raster_policies']
    rasterizer = policy['rasterizer']
    if rasterizer not in ('directwrite-3x1', 'fontations-skia-a8'):
        return None
    fonts = [{**font, 'path': str(root / relative_path(font['path']))} for font in manifest['fonts']]
    font_indices = {(font['sha256'], font['face_index']): i for i, font in enumerate(fonts)}
    specs = {}
    for text in texts:
        if text.get('kind') == 'icon-font':
            continue
        content = text['content'].encode()
        for ink in text.get('ink_instances', []):
            left, top, right, bottom = ink['clip']
            if left >= right or top >= bottom or ink['paint_opacity'] == 0 or ink['color'][3] == 0:
                continue
            scalar = content[ink['byte_begin']:ink['byte_end']].decode()
            if scalar.isspace():
                continue
            source = ink['atlas_source']
            font = source['font']
            key = mask_identity(scalar, source)
            prior = priors.get(font['sha256'] + ':' + scalar)
            index = font_indices.get((font['sha256'], font['face_index']))
            if prior is None:
                raise ProtocolError('no independent cmap witness for scenario cluster ' + repr(scalar))
            if source['glyph_id'] != prior['glyph_id'] or index is None:
                raise ProtocolError('scenario glyph differs from the independent face/cmap for ' + repr(scalar))
            if font['raster_policy'] != policy or source.get('phase_y', 0) != 0:
                raise ProtocolError('scenario glyph raster policy differs for ' + repr(scalar))
            specs[key] = dict(font=index, glyph_id=prior['glyph_id'], phase=source['phase'],
                              physical_em=font['logical_pixel_size'] * source['display_scale'])
    if not specs:
        return rasterizer, {}
    return rasterizer, read_masks(manifest, root, rasterizer, specs, policy=policy, fonts=fonts)


def browser_scenario_masks(manifest, root, glyphs):
    """Rasterize the independently resolved Web faces and observed placements."""
    policy, = manifest['environment']['native_capture']['font_raster_policies']
    rasterizer = policy['rasterizer']
    if rasterizer not in ('directwrite-3x1', 'fontations-skia-a8'):
        return None
    dpr=manifest['scope']['actual_dpr']
    fonts=[{**font,'path':str(root/relative_path(font['path']))} for font in manifest['fonts']]
    indices={font['sha256']:i for i,font in enumerate(fonts)}
    specs={}
    for source,size,origin,prior in glyphs:
        sha=source['actual_font_sha256'];scalar=source['content']
        if sha not in indices:
            raise ProtocolError('browser glyph has no independent locked face or cmap')
        for _,_,phase in browser_placements(rasterizer,origin,dpr):
            specs[sha,scalar,size,phase]=dict(font=indices[sha],glyph_id=prior['glyph_id'],phase=phase,
                                               physical_em=size*dpr)
    return rasterizer,read_masks(manifest,root,rasterizer,specs,policy=policy,fonts=fonts) if specs else {}


def mask_identity(scalar, source):
    font = source['font']
    return (font['sha256'], font['face_index'], scalar, font['logical_pixel_size'],
            source['display_scale'], source['phase'])


def scenario_origin(mask, source, bounds, dpr):
    if (mask.left != source['bearing_x'] or mask.top != -source['bearing_y'] or
            mask.width != source['coverage_width'] or mask.height != source['coverage_height']):
        raise ProtocolError('submitted glyph extent differs from the independently rasterized face')
    x, y = (bounds[name] * dpr for name in ('x', 'y'))
    if abs(x - round(x)) > 1/64 or abs(y - round(y)) > 1/64:
        raise ProtocolError('submitted unrotated glyph is not positioned on the capture device grid')
    return round(x) + source['padding'] - mask.left, round(y) + source['padding'] - mask.top


def scenario_ink_byte(mask, source, bounds, point, dpr):
    x, y = scenario_origin(mask, source, bounds, dpr)
    return mask.at(math.floor(point[0] * dpr) - x, math.floor(point[1] * dpr) - y)


@lru_cache(maxsize=4096)
def encoded_coverage(byte, color, rasterizer):
    from .color_metrics import srgb_to_linear

    coverage = byte / 255
    if byte in (0, 255):
        return coverage
    def canonical(value):
        high = value & 224
        return high | (high >> 3) | (high >> 6)
    r,g,b = (canonical(math.floor(channel * 255 + .5)) for channel in color)
    luma = canonical((r*54 + g*183 + b*19) >> 8) / 255
    inverse = 1 - luma
    srgb = rasterizer == 'directwrite-3x1'
    # The grayscale mask table receives Skia's 2.6 fixed-point gamma.
    gamma = 76 / 64
    back, front = srgb_to_linear((inverse, luma, 0))[:2] if srgb else (inverse**gamma, luma**gamma)
    weight = coverage + (1 - coverage) * coverage * back * (1 if srgb else .2)
    if abs(luma - inverse) > 1/256:
        linear = back * (1 - weight) + front * weight
        if srgb:
            encoded = 12.92 * linear if linear <= .0031308 else 1.055 * linear**(1/2.4) - .055
        else:
            encoded = linear**(1/gamma)
        weight = (encoded - inverse) / (luma - inverse)
    return math.floor(255 * min(1, max(0, weight)) + .5) / 255


def glyph_ink_coverage(byte, color, rasterizer, alpha):
    """Measure source coverage in the compositor's declared material-alpha units."""
    coverage=encoded_coverage(byte,color,rasterizer)
    if rasterizer in ('directwrite-3x1','fontations-skia-a8') and color==(0.,0.,0.) and 0<alpha<1:
        # The pinned black A8 path writes an integer source alpha before the
        # framebuffer blend. Keep the same units as the unmixing denominator.
        material_byte=math.floor(alpha*255+.5)
        mask_byte=round(coverage*255)
        return (material_byte*(mask_byte+1)//256)/(255*alpha)
    return coverage


def read_masks(manifest, root, identity, specs, **inputs):
    # Each process has the same bounded input as the finite control producer.
    ordered=list(specs.items())
    result={}
    for start in range(0,len(ordered),384):
        result.update(_read_mask_batch(manifest,root,identity,dict(ordered[start:start+384]),**inputs))
    return result


def _read_mask_batch(manifest, root, identity, specs, **inputs):
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


def quarter_icon_key(ink):
    from .formal_icon_source import official, single_path_for_glyph

    _, source_manifest, _ = official()
    source = ink['atlas_source']
    font = source['font']
    glyph = source['glyph_id']
    em = font['logical_pixel_size'] * source['display_scale']
    if (not source.get('svg_viewport_origin') or source['phase'] != 0 or source.get('phase_y', 0) != 0 or
            font['sha256'] != source_manifest['outline_container_sha256'] or font['face_index'] != 0 or
            font['source'] != 'bundled-icon' or font['raster_policy']['rasterizer'] != 'skia-svg-a8' or
            tuple(ink['color'][:3]) != (0, 0, 0) or ink['color'][3] * ink['paint_opacity'] != .25 or
            not 0 < em <= 256 or em != int(em) or not single_path_for_glyph(glyph)):
        return None
    return glyph, em


def quarter_icon_masks(manifest, root, texts):
    environment = manifest['environment']
    capture = environment['native_capture']
    if (environment.get('native',{}).get('backend') not in ('vulkan','direct3d12','d3d12') or
            capture.get('icon_rasterizer')!='skia-svg-a8' or capture.get('framebuffer_format')!='BGRA8_UNORM'):
        return {}
    specs = {}
    for text in texts:
        if text.get('kind')!='icon-font':
            continue
        for ink in text.get('ink_instances',[]):
            if (key := quarter_icon_key(ink)) is not None:
                glyph, em = key
                specs[key] = dict(glyph_id=glyph, physical_em=em, phase=0, phase_y=0)
    return read_masks(manifest, root, 'skia-svg-a8', specs) if specs else {}


def layout_icon_key(ink):
    from .formal_icon_source import official

    _, source_manifest, _ = official()
    source = ink['atlas_source']
    font = source['font']
    em = font['logical_pixel_size'] * source['display_scale']
    if (source.get('svg_layout_snap') is not True or source.get('svg_viewport_origin') is not True
            or ink.get('rotation_degrees') != 0
            or font['sha256'] != source_manifest['outline_container_sha256'] or font['face_index'] != 0
            or font['source'] != 'bundled-icon' or font['raster_policy']['rasterizer'] != 'skia-svg-a8'
            or not 0 < em <= 128):
        return None
    return source['glyph_id'], em, source['phase'], source['phase_y']


def layout_icon_masks(manifest, root, texts):
    if manifest['environment']['native_capture'].get('icon_rasterizer') != 'skia-svg-a8':
        return {}
    specs = {}
    for text in texts:
        if text.get('kind') != 'icon-font':
            continue
        for ink in text.get('ink_instances', ()):
            if (key := layout_icon_key(ink)) is not None:
                glyph, em, x, y = key
                specs[key] = dict(glyph_id=glyph, physical_em=em, phase=x, phase_y=y, layout_snap=True)
    return read_masks(manifest, root, 'skia-svg-a8', specs) if specs else {}


def place_layout_icon(masks, ink, bounds, dpr):
    mask = masks.get(layout_icon_key(ink))
    if mask is None:
        return None
    source = ink['atlas_source']
    pad = source['padding']
    if (source['display_scale'] != dpr or
            abs(bounds['width']*dpr - mask.width - 2*pad) > 1e-6 or
            abs(bounds['height']*dpr - mask.height - 2*pad) > 1e-6 or
            any(abs(value-round(value)) > 1/64 for value in (bounds['x']*dpr,bounds['y']*dpr))):
        return None
    return mask, *scenario_origin(mask, source, bounds, dpr)


def verified_quarter_icon_tie(coverage,observed):
    # Chromium rounds the destination gray. Vulkan's BGRA8 black source-over
    # rounds the source contribution; only exact half codes can differ.
    if coverage % 4 != 2:
        return None
    web=(255-(coverage+1)//4,)*3
    native=(255-(coverage+2)//4,)*3
    return web if observed==(web,native) else None


def place_icon(masks, cell, dpr):
    return [(masks[key], x, y, opacity) for key, x, y, opacity in icon_layers(cell, dpr)]


def icon_ink_at(layers, x, y):
    coverage = 0.
    for mask, left, top, opacity in layers:
        alpha = opacity * mask.at(x-left, y-top) / 255
        coverage = alpha + coverage * (1-alpha)
    return True if coverage == 1 else False if coverage == 0 else None


def placement(identity, origin, dpr):
    physical_x, physical_y = (value * dpr for value in origin)
    y = math.floor(physical_y + .5)
    if identity == 'directwrite-3x1':
        x, phase = divmod(math.floor(4 * physical_x + .5), 4)
    else:
        x, phase = math.floor(physical_x + .5), 0
    return x,y,phase


def browser_placements(rasterizer, origin, dpr):
    # Chromium's horizontal DOM Range left edge floors the shaped float to a
    # LayoutUnit. Enumerate only the raster phases in that half-open CSS cell.
    phases=4 if rasterizer=='directwrite-3x1' else 1
    left,baseline=origin
    first=math.floor(left*dpr*phases+.5)
    stop=math.ceil((left+1/64)*dpr*phases+.5)
    y=math.floor(baseline*dpr+.5)
    for ordinal in range(first,max(first+1,stop)):
        x,phase=divmod(ordinal,phases)
        yield x,y,phase


def place_browser(witnesses, source, size, origin, dpr):
    rasterizer,masks=witnesses
    face=source['actual_font_sha256'];scalar=source['content']
    try:
        return tuple((masks[face,scalar,size,phase],x,y)
                     for x,y,phase in browser_placements(rasterizer,origin,dpr))
    except KeyError as error:
        raise ProtocolError('browser source has no independent layout-cell glyph witness') from error


def place(witnesses, source, size, origin, dpr):
    identity, masks = witnesses
    x,y,phase=placement(identity,origin,dpr)
    key = source['actual_font_sha256'], source['content'], size, phase
    try:
        return masks[key], x, y
    except KeyError as error:
        raise ProtocolError('control source has no independent glyph witness') from error
