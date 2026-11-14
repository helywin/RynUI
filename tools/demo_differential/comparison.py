"""Pilot AND gates using existing logical ROI and local raster metric contracts.

This is deliberately not the full stage 6 typography/color/animation comparator.
Missing calibration or actual font/ROI evidence is an error, never a weaker pass.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
from pathlib import Path

from .adapters import RunError
from .oracle import exact
from .protocol import ProtocolError, validate
from .regions import crop_rgba, geometry, physical_box, raster_metrics, rect, relative, union
from .runner_evidence import artifact, metrics

CAPS = {'geometry.logical_px': .25, 'pixels.max_channel_residual': 0,
        'pixels.coverage_mass_relative_error': .05, 'pixels.contour_distance_px': .5,
        'text.coverage_mass_relative_error': .05, 'text.contour_distance_px': 1}


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False).encode()).hexdigest()


def algorithm_hash():
    return digest({name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
                   for name in ('comparison.py', 'regions.py')})


def binding(case, native, web):
    n, w = native['hello']['provenance'], web['hello']['provenance']
    return {'algorithm_hash': algorithm_hash(),
            'source_hash': digest({'native': n.get('source'), 'official_commit': w.get('source_commit'),
                                   'official_tree': w.get('source_tree_sha256'), 'closure': w.get('source_closure')}),
            'environment_hash': digest({'native': {k: n.get(k) for k in
                ('preset', 'configuration', 'toolchain', 'platform', 'backend', 'shader', 'gpu', 'font_policy')},
                'metrics': metrics(native, 'native'), 'web': w.get('browser'), 'fonts': case['fixture']['fonts']}),
            'font_hashes': [f['sha256'] for f in case['fixture']['fonts']], 'font_policy': case['fixture']['font_policy']}


def profile_thresholds(profile, expected, identity):
    validate('profile', profile)
    if profile['identity'] != identity or not profile['calibrated']:
        raise RunError(3, 'calibration', 'no calibrated profile for selected case/environment')
    for key, value in expected.items():
        if not exact(profile[key], value):
            raise RunError(3, 'calibration', 'profile binding differs: ' + key)
    if profile['masks']:
        raise RunError(2, 'calibration', 'pilot requires empty masks; protected content cannot be excluded')
    rows = {r['identity']: r for r in profile['metrics']}
    if len(rows) != len(profile['metrics']) or set(rows) != set(CAPS):
        raise RunError(2, 'calibration', 'missing, duplicate or unsupported pilot metric')
    for name, row in rows.items():
        candidate = max(row['quantization_floor'], row['positive_max'] + row['margin'])
        if (any(row[k] < 0 for k in ('threshold', 'hard_cap', 'quantization_floor', 'positive_max', 'margin')) or
                row['threshold'] != candidate or row['threshold'] > row['hard_cap'] or row['hard_cap'] > CAPS[name] or
                row['negative_min'] <= row['threshold']):
            raise RunError(3, 'calibration', 'positive/negative controls not separable within hard cap: ' + name)
    return {name: row['threshold'] for name, row in rows.items()}


def decoded(image_module, path, capture):
    with image_module.open(path) as image:
        if image.size != (capture['width'], capture['height']):
            raise ProtocolError('decoded capture dimensions differ')
        return image.convert('RGBA').tobytes(), image.size


def _crop_pair(images, reference, native, origins, dpr, guard):
    common = union(relative(reference, origins[0]), relative(native, origins[1]))
    box = physical_box(common, dpr, guard)
    crops = []
    for (data, size), origin in zip(images, origins):
        # Fractional origin phases must match; no subpixel shift/resampling.
        ox, oy = origin['x'] * dpr, origin['y'] * dpr
        if abs(ox - round(ox)) > 1e-6 or abs(oy - round(oy)) > 1e-6:
            raise RunError(3, 'environment', 'sample origin has uncalibrated fractional physical phase')
        absolute = (box[0] + round(ox), box[1] + round(oy), box[2] + round(ox), box[3] + round(oy))
        crops.append(crop_rgba(data, size, absolute))
    return crops, (box[2] - box[0], box[3] - box[1]), box


def _font_evidence(target, side, fonts, provenance):
    allowed = {(f['sha256'], f['face_index']) for f in fonts}
    weights = {(f['sha256'], f['face_index']): f['weight'] for f in fonts}
    if side == 'native':
        for text in target['text']:
            if text.get('kind') == 'icon-font' or not text.get('content'):
                continue
            if (text.get('font_coverage_available') is not True or text.get('missing_font_scalars') != [] or
                    text.get('replacement_count') != 0 or not text.get('font_runs')):
                raise RunError(3, 'environment', 'native actual font coverage/run is missing or invalid')
            for run in text['font_runs']:
                font = run.get('font', {})
                key = (font.get('sha256'), font.get('face_index'))
                source = 'pinned-file' if provenance.get('font_policy', 'pinned') == 'pinned' else 'system-file'
                if ((font.get('sha256'), font.get('face_index')) not in allowed or
                        font.get('source') != source or type(font.get('logical_pixel_size')) not in (int, float) or
                        font['logical_pixel_size'] <= 0 or type(font.get('bold')) is not bool or type(font.get('italic')) is not bool or
                        font.get('bold') and weights.get(key, 0) < 600 or font.get('italic')):
                    raise RunError(3, 'environment', 'native font source/face or unlocked synthesis differs')
    else:
        sources = provenance.get('actual_font_sources', [])
        if {(f.get('sha256'), f.get('face_index')) for f in sources} != allowed:
            raise RunError(3, 'environment', 'Web actual font byte sources differ')
        names = {name for f in sources for name in f.get('names', {}).get('6', [])}
        faces = target['tokens'].get('platform_fonts')
        if any(t.get('content') for t in target['text']) and (not faces or any(
                f.get('isCustomFont') is not True or f.get('postScriptName') not in names for f in faces)):
            raise RunError(3, 'environment', 'Web selected font face/fallback is unverified')


def _css_number(value):
    if type(value) in (int, float):
        return value
    if isinstance(value, str) and value.endswith('px'):
        return float(value[:-2])
    raise ProtocolError('unsupported token unit; a recipe mapping is required')


def _text_rows(target, side):
    # Selected/placeholder layers remain in original evidence. For this pilot,
    # editing assertions use the actual base/display text, not duplicate layers.
    return [t for t in target['text'] if t.get('content') and
            t.get('kind') not in ('editor-selected', 'editor-placeholder', 'icon-font')]


def _declared_text(case, target):
    fixture = case['fixture']
    if target == case['native']['sample_id']:
        required = []
        def visit(node):
            if node.get('component') == 'Typography.Text':
                value = node.get('content') if 'content' in node else fixture['slots'].get(node.get('target'), {}).get(node.get('slot'))
                if isinstance(value, str) and value:
                    required.append(value)
            for child in node.get('children', []):
                visit(child)
        visit(fixture.get('props', {}).get('composition', {}))
        return required
    slots = fixture.get('slots', {}).get(target, {})
    return [value for key, value in slots.items() if key in ('content', 'prefix', 'suffix') and isinstance(value, str) and value]


def _local_background(target, context_background):
    value = target['tokens'].get('backgroundColor')
    if value is None:
        raise ProtocolError('missing actual computed local text/icon background')
    match = re.fullmatch(r'rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)(?:\s*,\s*([\d.]+))?\s*\)', value)
    if not match:
        raise RunError(3, 'calibration', 'text/icon background has no supported computed-color calibration')
    rgb = tuple(int(x) for x in match.groups()[:3])
    alpha = float(match[4]) if match[4] else 1
    if any(x > 255 for x in rgb) or alpha not in (0, 1):
        raise RunError(3, 'calibration', 'semi-transparent local background needs later alpha calibration')
    return (*rgb, 255) if alpha else context_background


def compare_checkpoint(case, declaration, native, web, images, thresholds, color):
    failures = []
    step = declaration['step']
    def fail(category, target, message, **values):
        failures.append({'category': category, 'step': step, 'target': target, 'message': message, **values})
    nt = {t['identity']: t for t in native['targets']}
    wt = {t['identity']: t for t in web['targets']}
    context = [r for r in declaration['rois'] if r['kind'] == 'sample-context']
    if len(context) != 1 or context[0]['target'] != case['native']['sample_id']:
        raise ProtocolError('pilot requires one complete real sample-context ROI')
    name = context[0]['target']
    origins = (rect(wt[name]['bounds']), rect(nt[name]['bounds']))
    dpr = case['fixture']['viewport']['dpr']
    for target in declaration['targets']:
        a, b = wt[target], nt[target]
        errors = geometry(a['bounds'], b['bounds'], *origins)
        if any(error > thresholds['geometry.logical_px'] for error in errors.values()):
            fail('layout', target, 'relative target bounds differ', metric='geometry.logical_px', actual=errors,
                 threshold=thresholds['geometry.logical_px'])
        # A clip beyond the context is immaterial to this finite sample, while
        # intersection with the context must remain identical.
        def clipped(clip, origin):
            clip = relative(clip, origin)
            x, y = max(clip['x'], 0), max(clip['y'], 0)
            return {'x': x, 'y': y, 'width': max(0, min(clip['x'] + clip['width'], origin['width']) - x),
                    'height': max(0, min(clip['y'] + clip['height'], origin['height']) - y)}
        if any(abs(clipped(a['clip'], origins[0])[k] - clipped(b['clip'], origins[1])[k]) >
               thresholds['geometry.logical_px'] for k in ('x', 'y', 'width', 'height')):
            fail('layout', target, 'effective context clip differs')
        component = case['fixture'].get('props', {}).get('controls', {}).get(target, {}).get('component')
        pairs = [('borderWidth', 'border_width'), ('borderRadius', 'border_radius'), ('fontSize', 'font_size')]
        if component in ('Button', 'Input'):
            # Logical CSS px and native logical component metrics. Preserve
            # each side separately so asymmetric padding cannot average away.
            pairs += [('paddingInlineStart', 'padding_inline'), ('paddingInlineEnd', 'padding_inline')]
        for source, destination in pairs:
            if target == name:
                continue
            if source not in a['tokens'] or destination not in b['tokens']:
                raise ProtocolError('missing actual token mapping: ' + target + '.' + destination)
            if abs(_css_number(a['tokens'][source]) - _css_number(b['tokens'][destination])) > thresholds['geometry.logical_px']:
                fail('token', target, 'effective token differs', token=destination,
                     expected=a['tokens'][source], actual=b['tokens'][destination])
        applicable = {'Button': ('disabled', 'loading'), 'Input': ('disabled', 'value'),
                      'Select': ('disabled', 'loading', 'value', 'open')}.get(component, ())
        for field in applicable:
            if field not in a['state'] or field not in b['state']:
                raise ProtocolError('missing paired observed state: ' + field)
            if field != 'value' and (type(a['state'][field]) is not bool or type(b['state'][field]) is not bool):
                raise ProtocolError('applicable state is unknown, not an observed false: ' + field)
            if not exact(a['state'][field], b['state'][field]):
                fail('state', target, field + ' differs', expected=a['state'][field], actual=b['state'][field])
    background = case['fixture']['theme'].get('token', {}).get('colorBgContainer')
    if background != '#ffffff':
        raise RunError(3, 'calibration', 'pilot requires independently declared/calibrated opaque white background')
    background = (255, 255, 255, 255)
    measurements = []
    def raster(target, a, b, guard, category, text=False, local_background=None):
        crops, size, box = _crop_pair(images, a, b, origins, dpr, guard)
        result = raster_metrics(*crops, size, background if local_background is None else local_background)
        if local_background is not None:
            # Guard pixels may be outside the material. They must not make a
            # pure-background glyph/icon interior appear to contain real ink.
            inner = (guard, guard, size[0] - guard, size[1] - guard)
            local = [crop_rgba(data, size, inner) for data in crops]
            presence = raster_metrics(*local, (size[0] - guard * 2, size[1] - guard * 2), local_background)
            result['required_ink_present'] = presence['required_ink_present']
        measurements.append({'target': target, 'category': category, 'box': box, 'metrics': result})
        if not result['required_ink_present']:
            fail(category, target, 'required local foreground/ink missing', roi=box)
        for field in ('max_channel_residual', 'coverage_mass_relative_error', 'contour_distance_px'):
            prefix = 'text' if text and field != 'max_channel_residual' else 'pixels'
            threshold = thresholds[prefix + '.' + field]
            value = result[field]
            if value is None or value > threshold:
                fail(category, target, 'local raster metric failed', roi=box, metric=prefix + '.' + field,
                     actual=value, threshold=threshold)
    for roi in declaration['rois']:
        guard = roi['guard_px']
        if type(guard) not in (float, int) or guard < 2 or int(guard) != guard:
            raise ProtocolError('pilot ROI guard requires an integer >= 2 physical px')
        a, b = wt[roi['target']], nt[roi['target']]
        # Compare the common union and its guard, even when native self-reports
        # smaller bounds. No independent tight crops, image registration or mask.
        raster(roi['target'], a['bounds'], b['bounds'], int(guard), 'visual')
    for target in declaration['targets']:
        a, b = wt[target], nt[target]
        for side, row, provenance in (('native', b, native['provenance']), ('web', a, web['provenance'])):
            _font_evidence(row, side, case['fixture']['fonts'], provenance)
        at, bt = _text_rows(a, 'web'), _text_rows(b, 'native')
        for required in _declared_text(case, target):
            for side, rows in (('web', at), ('native', bt)):
                if required not in ''.join(t['content'] for t in rows):
                    fail('typography', target, 'independently declared text is missing', side=side, expected=required)
        slots = case['fixture'].get('slots', {}).get(target, {})
        required_icons = [v['icon'] for v in slots.values() if isinstance(v, dict) and 'icon' in v]
        if required_icons and (len(a['icons']) < len(required_icons) or len(b['icons']) < len(required_icons)):
            fail('visual', target, 'independently declared required icon is missing', expected=required_icons)
        if [t['content'] for t in at] != [t['content'] for t in bt]:
            fail('typography', target, 'actual text content/run order differs',
                 expected=[t['content'] for t in at], actual=[t['content'] for t in bt])
        else:
            for aw, bn in zip(at, bt):
                boxes = aw.get('boxes')
                if not boxes or not bn.get('bounds'):
                    raise ProtocolError('text presence requires actual local run bounds on both sides: ' + target)
                ar = boxes[0]
                for box in boxes[1:]:
                    ar = union(ar, box)
                raster(target + '.text', ar, bn['bounds'], 2, 'typography', text=True,
                       local_background=_local_background(a, background))
        if len(a['icons']) != len(b['icons']):
            fail('visual', target, 'required icon identity/count differs')
        for aw, bn in zip(a['icons'], b['icons']):
            if not aw.get('identity') or not bn.get('identity'):
                raise ProtocolError('icon source identity is unavailable')
            if aw['identity'].replace('-', '').lower() != bn['identity'].removesuffix('Outlined').lower():
                fail('visual', target, 'required icon identity differs')
            raster(target + '.icon', aw['bounds'], bn['bounds'], 2, 'visual',
                   local_background=_local_background(a, background))
    return failures, measurements


def compare(case, native, web, directories, profile, image_module):
    expected = binding(case, native, web)
    thresholds = profile_thresholds(profile, expected, case['profile'])
    failures, measurements = [], {}
    for checkpoint_index, declaration in enumerate(case['checkpoints']):
        step = declaration['step']
        n, w = native['checkpoints'][step], web['checkpoints'][step]
        captures = (w['captures'][0], n['captures'][0])
        # No implicit conversion of unknown encoding/alpha or histogram repair.
        for capture in captures:
            if any(capture[key] != profile['color'][key] for key in ('channel_order', 'alpha')) or capture['color_profile'] != profile['color']['profile']:
                raise RunError(3, 'calibration', 'capture color/channel/alpha has no matching calibrated profile')
        images = [decoded(image_module, artifact(directory, capture['path']), capture)
                  for directory, capture in zip((directories['web'], directories['native']), captures)]
        context = next(r for r in declaration['rois'] if r['kind'] == 'sample-context')
        a = next(t for t in w['targets'] if t['identity'] == context['target'])['bounds']
        b = next(t for t in n['targets'] if t['identity'] == context['target'])['bounds']
        crops, size, box = _crop_pair(images, a, b, (a, b), case['fixture']['viewport']['dpr'], int(context['guard_px']))
        diff = bytes(abs(x - y) if index % 4 != 3 else 255 for index, (x, y) in enumerate(zip(*crops)))
        destination = Path(directories['native']).parent / 'comparison' / str(checkpoint_index)
        destination.mkdir(parents=True)
        image_module.frombytes('RGBA', size, diff).save(destination / 'diff.png')
        errors, measured = compare_checkpoint(case, declaration, n, w, images, thresholds, profile['color'])
        failures.extend(errors)
        measurements[step] = measured
    return failures, {'bindings': expected, 'thresholds': thresholds, 'checkpoints': measurements}
