"""Pilot AND gates using existing logical ROI and local raster metric contracts.

This is deliberately not the full stage 6 typography/color/animation comparator.
Missing calibration or actual font/ROI evidence is an error, never a weaker pass.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
import platform
import struct
from collections import Counter
from functools import lru_cache, reduce
from pathlib import Path

from .adapters import RunError
from .oracle import exact
from .protocol import ROOT, ProtocolError, validate
from .regions import (crop_rgba, geometry, physical_box, raster_metrics, rect, relative, union, sample_anchor, text_groups,
                      capture_aperture, vertical_capture_slices)
from .runner_evidence import artifact, metrics
from .calibration import CAPS
from .pilots import displayed_slot_text
from .shape_blend_witness import verified_blend
from .shape_witness import ShapeWitnesses, locked_edge_value

LEGACY_DIAGNOSTIC_CAPS = {'geometry.logical_px': .25, 'pixels.max_channel_residual': 0,
        'pixels.coverage_mass_relative_error': .05, 'pixels.contour_distance_px': .5,
        'text.coverage_mass_relative_error': .05, 'text.contour_distance_px': 1}


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False).encode()).hexdigest()


def algorithm_hash():
    from .protocol import FIXTURES
    files={name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
           for name in ('comparison.py', 'regions.py', 'color_metrics.py','calibration.py','schema_definitions.py',
                        'pilots.py','glyph_witness.py','formal_icon_source.py','shape_witness.py','shape_blend_witness.py')}
    files['profile.schema.json']=hashlib.sha256((FIXTURES/'profile.schema.json').read_bytes()).hexdigest()
    return digest(files)


def binding(case, native, web):
    from .calibration import required
    n, w = native['hello']['provenance'], web['hello']['provenance']
    source=source_description(n,w)
    environment=environment_description(case,n,w,metrics(native,'native'),capture_description(native))
    return {'algorithm_hash': algorithm_hash(),
            'source_hash': digest(source), 'environment_hash': digest(environment),
            'font_hashes': [f['sha256'] for f in case['fixture']['fonts']], 'font_policy': case['fixture']['font_policy']}


def source_description(native,web):
    from .calibration import required
    required(native,('source',),'native source')
    required(web,('source_commit','source_tree_sha256','source_closure'),'official source')
    return {'native':native['source'],'official_commit':web['source_commit'],
            'official_tree':web['source_tree_sha256'],'closure':web['source_closure']}


def capture_description(native):
    from .calibration import required
    checkpoint=next(iter(native['checkpoints'].values()))
    capture=checkpoint['captures'][0]
    raw=checkpoint['resources'].get('readback',{})
    required(raw,('source_channel_order','clear_rgba'),'native actual readback')
    policies={}
    for checkpoint in native['checkpoints'].values():
        for target in checkpoint['targets']:
            for text in target['text']:
                if text.get('kind')=='icon-font':continue
                for run in text.get('font_runs',[]):
                    policy=required(run['font'],('raster_policy',),'actual native font')['raster_policy']
                    policies[digest(policy)]=policy
    if not policies:
        raise RunError(3,'calibration','actual native font raster policy is unavailable')
    icon_rasterizer = native.get('hello', {}).get('provenance', {}).get('icon_rasterizer', 'freetype')
    if icon_rasterizer not in ('freetype', 'skia-svg-a8'):
        raise RunError(3, 'calibration', 'actual native icon raster policy is unavailable')
    shape_rasterizer = native.get('hello', {}).get('provenance', {}).get('shape_rasterizer', 'shader')
    if shape_rasterizer not in ('shader', 'skia-rounded-coverage'):
        raise RunError(3, 'calibration', 'actual native shape raster policy is unavailable')
    return {**required(capture,('framebuffer_format','channel_order','alpha','color_profile'),'native original storage'),
            'source_channel_order':raw['source_channel_order'],'clear_rgba':raw['clear_rgba'],
            'font_raster_policies':[policies[key] for key in sorted(policies)],
            'icon_rasterizer': icon_rasterizer, 'shape_rasterizer': shape_rasterizer}


def environment_description(case,native,web,actual_metrics,capture_facts):
    from .calibration import required
    from .device_metrics import stable_device_metrics
    native_fields=('preset','configuration','toolchain','platform','backend','shader','gpu','font_policy')
    n=required(native,native_fields,'native environment')
    gpu = n['gpu']
    gpu_fields = ('name', 'driver_version', 'driver_name', 'driver_info')
    if not isinstance(gpu, dict) or any(type(gpu.get(key)) is not str for key in gpu_fields):
        raise RunError(3, 'calibration', 'actual native GPU binding lacks the driver observation fields')
    # SDL's D3D12 backend records the adapter description and DXGI UMD version.
    # Keep the recorded empty strings in the binding so a change still invalidates it.
    required_gpu_fields = gpu_fields[:2] if n['backend'].lower() in ('d3d12', 'direct3d12') else gpu_fields
    for key in required_gpu_fields:
        if not gpu[key].strip():
            raise RunError(3, 'calibration', 'actual native GPU binding unavailable: ' + key)
    required(web,('browser','environment'),'Web environment')
    required(web['browser'],('version','executable_sha256','revision','launch_args','actual_gpu','os','release','arch'),'actual browser')
    libraries=required(web['environment'],('browser_libraries','browser_sha256','node_sha256'),'browser library/tool closure')
    os_fields={'system':platform.system(),'release':platform.release(),'version':platform.version(),'machine':platform.machine()}
    required(os_fields,tuple(os_fields),'OS/build')
    return {'native':n,'native_capture':capture_facts,'metrics':stable_device_metrics(actual_metrics),'web':web['browser'],'libraries':libraries,
            'os':os_fields,'fonts':case['fixture']['fonts']}


def profile_thresholds(profile, expected, identity, *, image_module=None,native_binary_sha256=None, proof=None):
    if profile.get('calibration_kind') != 'pilot-independent-v1' or not profile.get('evidence'):
        raise RunError(3, 'calibration', 'profile has no independent pilot evidence; hand-written calibrated declarations are invalid')
    validate('profile', profile)
    if profile['identity'] != identity or not profile['calibrated']:
        raise RunError(3, 'calibration', 'no calibrated profile for selected case/environment')
    for key, value in expected.items():
        if not exact(profile[key], value):
            raise RunError(3, 'calibration', 'profile binding differs: ' + key)
    if profile['masks']:
        raise RunError(2, 'calibration', 'pilot requires empty masks; protected content cannot be excluded')
    if proof is not None:
        return proof.thresholds_for(profile,image_module,native_binary_sha256)
    from .calibration import verify_profile_evidence
    return verify_profile_evidence(profile,image_module=image_module,native_binary_sha256=native_binary_sha256)


def decoded(image_module, path, capture):
    with image_module.open(path) as image:
        if image.size != (capture['width'], capture['height']):
            raise ProtocolError('decoded capture dimensions differ')
        return image.convert('RGBA').tobytes(), image.size


def _crop_pair(images, reference, native, origins, dpr, guard):
    common = union(relative(reference, origins[0]), relative(native, origins[1]))
    box = physical_box(common, dpr, guard)
    return _crop_pair_box(images,box,origins,dpr)


def _crop_pair_box(images,box,origins,dpr):
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
        if _text_rows(target,'web') and (not faces or any(
                f.get('isCustomFont') is not True or f.get('postScriptName') not in names for f in faces)):
            raise RunError(3, 'environment', 'Web selected font face/fallback is unverified')


def _css_number(value):
    if type(value) in (int, float):
        return value
    if isinstance(value, str) and value.endswith('px'):
        return float(value[:-2])
    raise ProtocolError('unsupported token unit; a recipe mapping is required')


def _text_rows(target, side):
    rows=[t for t in target['text'] if t.get('content') and t.get('semantic_role')!='unused-base' and
          (t.get('semantic_role') in ('selector-caption','editor-hint') or
           t.get('kind') not in ('editor-selected','editor-placeholder','icon-font'))]
    if side=='web':
        rows=[text for text in rows if not _suppressed_web_text(text)]
    # DOM text and UA editor text are independent semantic streams. A Select
    # value can paint a static label with its editor's base or placeholder face.
    return sorted(rows,key=lambda t: bool(t.get('editable') or t.get('semantic_role')=='editor-hint' or
                                          t.get('kind')=='editor-base' and t.get('semantic_role')!='selector-caption'))


def _suppressed_web_text(text):
    clip=text.get('zero_clip',{})
    bounds=clip.get('bounds',{})
    clips=('hidden','clip','scroll','auto')
    return (text.get('actual_ancestor_opacity')==0 or text.get('hidden_visibility') in ('hidden','collapse') or
            bounds.get('width')==0 and clip.get('overflow_x') in clips or
            bounds.get('height')==0 and clip.get('overflow_y') in clips)


def _text_advance_error(web,native,*,layered):
    raw=abs(web['advance']-native['advance'])
    if not layered or len(web['content'])!=1 or len(web['boxes'])!=1:
        return raw
    box=web['boxes'][0]
    if (box['width']!=web['advance'] or any(abs(edge*64-round(edge*64))>1e-6
            for edge in (box['x'],box['x']+box['width']))):
        return raw
    # The locked DOM Range reports each endpoint in Blink's 26.6 layout grid.
    # Its width is a difference of two rounded endpoints; the native advance
    # retains the unrounded shaping position. Charge the residual outside the
    # two endpoint intervals, while preserving the raw hard cap.
    return raw if raw>CAPS['text.geometry_logical_px'] else max(0.,raw-2/64)


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
    return [displayed_slot_text(fixture,target,key,value) for key,value in slots.items()
            if key in ('content','prefix','suffix') and isinstance(value,str) and value]


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


def _css_rgba(value):
    if not isinstance(value,str):
        raise RunError(3,'calibration','actual computed color unavailable')
    match=re.fullmatch(r'rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)(?:\s*,\s*([\d.]+))?\s*\)',value)
    if not match:
        raise RunError(3,'calibration','unsupported actual CSS color')
    rgb=tuple(int(v)/255 for v in match.groups()[:3]);alpha=float(match[4] or 1)
    if any(not 0<=v<=1 for v in (*rgb,alpha)):
        raise ProtocolError('actual CSS color is outside encoded SDR scope')
    return (*rgb,alpha)


def _actual_material(target,role,expected,*,require_input_paint=False,shape=None):
    """Actual retained paint, never a native Theme recipe or fixture declaration."""
    tokens=target['tokens']
    if 'paint' in tokens:
        paint=tokens['paint']
        if paint.get('source')!='published-retained-input-fragment' or paint.get('status')!='published' or paint.get('branch_active') is not True:
            raise RunError(3,'calibration','Input/Select actual retained paint is unavailable')
        owner=paint.get('owner',{})
        if not owner.get('generation') or not paint.get('layers'):
            raise ProtocolError('Input paint has no live owner/generation/layers')
        layers=[r for r in paint['layers'] if r['role']==role]
        if not layers:
            raise RunError(3,'calibration','required retained Input paint role is absent: '+role)
        active=[r for r in layers if r['exists'] and not r['hidden'] and not r['clipped']]
        if expected[3]==0 and not active:
            return True
        if not active or any(not r['in_fragment'] or not r.get('actual') for r in active):
            raise RunError(3,'calibration','required retained Input paint is absent/clipped')
        for row in active:
            material=row['actual']
            if material['paint_present'] or material['translation'] != [0,0]:
                raise RunError(3,'calibration','Input paint is outside independent pilot material scope')
            if shape is not None:
                if (not row.get('primitive',{}).get('generation') or material.get('kind') not in (0,2) or
                        material.get('offset')!=[0,0] or material.get('blur')!=0 or material.get('spread')!=0):
                    raise RunError(3,'calibration','retained Input material/identity lacks matching finite controls')
                if role=='background' and material['kind']!=0:
                    return False
                bounds,radius,width=shape
                inset=dict(x=bounds['x']+width,y=bounds['y']+width,
                    width=max(0,bounds['width']-2*width),height=max(0,bounds['height']-2*width))
                allowed=[(bounds,radius),(inset,max(0,radius-width))]
                if not any(all(abs(material['bounds'][k]-candidate[k])<=.25 for k in candidate) and
                        abs(material['radius']-r)<=.25 for candidate,r in allowed):
                    return False
                if role=='border' and material['kind']==2 and abs(material.get('outline_width',-1)-width)>.25:
                    return False
            if any(abs(a-b)>1e-6 for a,b in zip(material['color'][:3],expected[:3])) or abs(material['color'][3]*material['opacity']-expected[3])>1e-6:
                return False
    else:
        raise RunError(3,'calibration','actual published retained paint is required; Theme/presentation colors are not paint evidence')
    return True


def _css_parts(value, delimiter=','):
    """Split only outside parentheses/quotes; rgba commas are not shadow separators."""
    if not isinstance(value,str):raise ProtocolError('CSS observation is not text')
    result=[];start=0;depth=0;quote=None;escaped=False
    for i,char in enumerate(value):
        if escaped:escaped=False;continue
        if char=='\\':escaped=True;continue
        if quote:
            if char==quote:quote=None
        elif char in ('"',"'"):quote=char
        elif char=='(':depth+=1
        elif char==')':
            depth-=1
            if depth<0:raise ProtocolError('unbalanced CSS function')
        elif char==delimiter and depth==0:
            result.append(value[start:i].strip());start=i+1
    if quote or depth:raise ProtocolError('unbalanced CSS value')
    result.append(value[start:].strip())
    return result


def css_shadows(value):
    if value=='none':return []
    result=[]
    for ordinal,part in enumerate(_css_parts(value)):
        colors=re.findall(r'rgba?\([^()]*\)',part)
        if len(colors)!=1:raise RunError(3,'calibration','shadow has unsupported computed color')
        color=_css_rgba(colors[0]);tokens=part.replace(colors[0],'').split()
        inset='inset' in tokens
        if inset:tokens.remove('inset')
        if not 2<=len(tokens)<=4:raise ProtocolError('shadow needs two to four finite lengths')
        lengths=[_css_number(token) for token in tokens]
        if any(not math.isfinite(x) for x in lengths):raise ProtocolError('nonfinite shadow geometry')
        lengths += [0]*(4-len(lengths))
        if lengths[2]<0:raise ProtocolError('negative shadow blur')
        result.append(dict(ordinal=ordinal,inset=inset,offset=lengths[:2],blur=lengths[2],spread=lengths[3],color=color))
    return result


def _inset(bounds,amount):
    top,right,bottom,left=(amount,)*4 if isinstance(amount,(int,float)) else amount
    return dict(x=bounds['x']+left,y=bounds['y']+top,width=max(0,bounds['width']-left-right),height=max(0,bounds['height']-top-bottom))


def _support(layer):
    b=layer['bounds'];kind=layer['kind']
    if kind=='shadow':
        # Four sigma includes every RGB8-observable Gaussian tail; guard remains
        # separate. This changes capture accounting, never the physical AA band.
        extent=max(0,layer['spread'])+2*layer['blur']
        return dict(x=b['x']+layer['offset'][0]-extent,y=b['y']+layer['offset'][1]-extent,
                    width=b['width']+2*extent,height=b['height']+2*extent)
    if kind=='outline':return _inset(b,-layer['offset_width']-layer['width'])
    return b


def _contains(bounds,point):
    return bounds['x']<=point[0]<bounds['x']+bounds['width'] and bounds['y']<=point[1]<bounds['y']+bounds['height']


def _glyph_in_sampled_region(row, box, dpr):
    left,top,right,bottom=box
    for rect in (row['bounds'],*row['clips']):
        left=max(left,math.ceil(rect['x']*dpr-.5))
        top=max(top,math.ceil(rect['y']*dpr-.5))
        right=min(right,math.ceil((rect['x']+rect['width'])*dpr-.5))
        bottom=min(bottom,math.ceil((rect['y']+rect['height'])*dpr-.5))
    return left<right and top<bottom


_PAINT_BIN_PHYSICAL_PX = 32


def _paint_bins(layers,box,dpr):
    """Index conservative physical support while preserving source paint order."""
    bins={}
    block=_PAINT_BIN_PHYSICAL_PX
    for row in layers:
        support=_inset(_support(row),-1/dpr)
        left=max(math.floor(support['x']*dpr),box[0])
        top=max(math.floor(support['y']*dpr),box[1])
        right=min(math.ceil((support['x']+support['width'])*dpr),box[2])
        bottom=min(math.ceil((support['y']+support['height'])*dpr),box[3])
        if right<=left or bottom<=top:
            continue
        for y in range(top//block,(bottom-1)//block+1):
            for x in range(left//block,(right-1)//block+1):
                bins.setdefault((x,y),[]).append((row,support))
    return bins


def _layered_pixels(crops,size,box,bins,counts):
    block=_PAINT_BIN_PHYSICAL_PX
    white=tuple(b'\xff\xff\xff\xff'*length for length in range(block+1))
    for y in range(box[1],box[3]):
        index=(y-box[1])*size[0]
        row_bin=y//block
        for column_bin in range(box[0]//block,(box[2]-1)//block+1):
            x=max(box[0],column_bin*block)
            end=min(box[2],(column_bin+1)*block)
            length=end-x
            key=(column_bin,row_bin)
            start_byte=index*4
            end_byte=start_byte+length*4
            # Both independent source predictions and both actual spans are
            # opaque white. A shared stray pixel still takes the normal path.
            if (key not in bins[0] and key not in bins[1] and
                    crops[0][start_byte:end_byte]==crops[1][start_byte:end_byte]==white[length]):
                counts['guard']+=length
            else:
                for offset in range(length):
                    yield index+offset,x+offset,y,key
            index+=length


def _shape_distance(point,bounds,radii):
    from .calibration import rounded_distance
    radius=radii[(0 if point[1]<bounds['y']+bounds['height']/2 else 3) if point[0]<bounds['x']+bounds['width']/2
                  else (1 if point[1]<bounds['y']+bounds['height']/2 else 2)] if isinstance(radii,(tuple,list)) else radii
    return rounded_distance(*point,bounds,radius)


def _shadow_edge(row):
    spread=row['spread'] if row['kind']=='shadow' else -row['spread']
    bounds=_inset(row['bounds'],-spread)
    bounds['x']+=row['offset'][0]
    bounds['y']+=row['offset'][1]
    radius=row['radii']
    return bounds,max(0,radius+spread) if radius>0 else 0


def _shadow_tail(row,point,dpr):
    bounds,radius=_shadow_edge(row)
    if min(bounds['width'],bounds['height'])<=0:
        return False
    distance=_shape_distance(point,bounds,radius)
    if row['kind']=='inset-shadow':
        distance=-distance
    return distance>max(math.sqrt(.5)/dpr,row['blur'])


def _square_border_side(bounds,widths,edge):
    """One CSS square side owns the miters adjoining transparent sides."""
    def corners(rect):
        x,y=rect['x'],rect['y']
        right,bottom=x+rect['width'],y+rect['height']
        return (x,y),(right,y),(right,bottom),(x,bottom)
    inner=_inset(bounds,widths)
    if min(inner['width'],inner['height'])<=0:
        raise RunError(3,'calibration','square border has no finite inner miter box')
    outside,inside=corners(bounds),corners(inner)
    other=(edge+1)%4
    polygon=(outside[edge],outside[other],inside[other],inside[edge])
    xs,ys=zip(*polygon)
    box=dict(x=min(xs),y=min(ys),width=max(xs)-min(xs),height=max(ys)-min(ys))
    return polygon,box


def _square_side_coverage(polygon,point,dpr):
    # Work in one unit pixel. Clipping before computing area keeps the result
    # independent of the location of the physical viewport and of paint color.
    clipped=[((x-point[0])*dpr,(y-point[1])*dpr) for x,y in polygon]
    for axis,bound,sign in ((0,-.5,1),(0,.5,-1),(1,-.5,1),(1,.5,-1)):
        if not clipped:
            return 0.
        original,clipped=clipped,[]
        previous=original[-1]
        for current in original:
            a,b=(previous[axis]-bound)*sign,(current[axis]-bound)*sign
            if (a>=0)!=(b>=0):
                ratio=a/(a-b)
                clipped.append(tuple(p+(c-p)*ratio for p,c in zip(previous,current)))
            if b>=0:
                clipped.append(current)
            previous=current
    return min(1.,.5*abs(sum(x*v-y*u for (x,y),(u,v) in zip(clipped,clipped[1:]+clipped[:1]))))


def _layer_coverage(layer,point,dpr):
    """Independent source geometry; transitions are exactly <=1 physical pixel."""
    from .calibration import gaussian_rounded_shadow
    if any(not _contains(clip,point) for clip in layer.get('clips',[])):return 0,False
    b=layer['bounds'];kind=layer['kind'];r=layer.get('radii',0)
    if kind=='border-side':
        coverage=_square_side_coverage(layer['polygon'],point,dpr)
        return coverage,0<coverage<1
    if layer.get('solid_status_caret'):
        return float(_contains(b,point)),False
    if layer.get('caret_paint_bounds'):
        half=.5/dpr
        x,y=point
        width=max(0.,min(x+half,b['x']+b['width'])-max(x-half,b['x']))
        height=max(0.,min(y+half,b['y']+b['height'])-max(y-half,b['y']))
        coverage=min(1.,width*dpr)*min(1.,height*dpr)
        return coverage,0<coverage<1-1e-10
    if layer.get('formal_svg'):
        # Chromium supplies continuous path bounds. Their intersection
        # with the pixel aperture can be nonempty outside the pixel center.
        reach=.5/dpr
        x,y=point
        exposed=(b['x']-reach<x<b['x']+b['width']+reach and
                 b['y']-reach<y<b['y']+b['height']+reach)
        return float(_contains(b,point)),exposed
    glyph_mask=layer.get('web_glyph_mask') or layer.get('native_glyph_mask')
    if glyph_mask is not None:
        mask,x,y=glyph_mask
        px,py=(math.floor(coordinate*dpr) for coordinate in point)
        if mask.at(px-x,py-y)==0 and not any(
                alternate.at(px-ax,py-ay) for alternate,ax,ay in layer.get('web_glyph_alternates',())):
            return 0,False
    if kind in ('glyph','caret'):
        return (1 if _contains(b,point) else 0),_contains(b,point)
    distance=_shape_distance(point,b,r)
    # A hard shape can partially cover a unit pixel only if an edge reaches
    # its square aperture. The half diagonal bounds every edge direction.
    pixel_reach=math.sqrt(.5)
    if kind in ('shadow','inset-shadow'):
        if (kind=='shadow' and layer.get('knockout') and layer['blur']==0 and layer['spread']==0 and
                not any(layer['offset'])):
            return 0,False
        if kind=='inset-shadow' and distance>0:return 0,abs(distance)*dpr<=1
        caster_distance = math.inf
        if kind == 'shadow' and layer.get('knockout'):
            inset = layer.get('caster_inset', 0)
            caster = _inset(b, inset)
            if caster['width'] > 0 and caster['height'] > 0:
                caster_radius = max(0, r - inset)
                caster_distance = _shape_distance(point, caster, caster_radius)
                x, y = point
                straight = (caster['x'] + caster_radius <= x <= caster['x'] + caster['width'] - caster_radius or
                            caster['y'] + caster_radius <= y <= caster['y'] + caster['height'] - caster_radius)
                if caster_distance * dpr < -pixel_reach or straight and caster_distance * dpr <= -.5:
                    return 0, False
        shape,radius=_shadow_edge(layer)
        if shape['width']<=0 or shape['height']<=0:return (0 if kind=='shadow' else float(distance<=0)),False
        if layer['blur']>0:
            shadow={'offset':[0,0],'blur':layer['blur'],'spread':0}
            value=gaussian_rounded_shadow(*point,shape,shadow,radius)
        else:
            if kind=='shadow' and any(p+.5<=lo or p-.5>=hi for p,lo,hi in (
                    (point[0]*dpr,shape['x']*dpr,(shape['x']+shape['width'])*dpr),
                    (point[1]*dpr,shape['y']*dpr,(shape['y']+shape['height'])*dpr))):
                return 0,False
            edge=_shape_distance(point,shape,radius);value=float(edge<=0)
            if kind=='shadow' and edge*dpr>pixel_reach:
                return 0,False
        if kind=='inset-shadow':value=1-value
        elif caster_distance<0:value=0
        if layer['blur']>0:return value,True
        return value,min(abs(edge),abs(caster_distance))*dpr<=pixel_reach
    if kind=='outline':
        outer=_inset(b,-layer['offset_width']-layer['width'])
        inner=_inset(b,-layer['offset_width'])
        od=_shape_distance(point,outer,r+layer['offset_width']+layer['width'])
        ind=_shape_distance(point,inner,r+layer['offset_width'])
        return float(od<=0 and ind>=0),min(abs(od),abs(ind))*dpr<=pixel_reach
    if kind=='border':
        inner=_inset(b,layer['widths']);inner_r=tuple(max(0,v-max(layer['widths'])) for v in r)
        inner_distance=_shape_distance(point,inner,inner_r) if inner['width'] and inner['height'] else math.inf
        if layer.get('device_aligned_square'):
            # Each aperture is wholly inside or outside both snapped
            # rectangles, including a pixel touching a straight edge.
            return float(distance <= 0 and inner_distance >= 0), False
        return float(distance<=0 and inner_distance>=0),min(abs(distance),abs(inner_distance))*dpr<=pixel_reach
    if layer.get('device_aligned_square'):
        return float(distance <= 0), False
    return float(distance<=0),abs(distance)*dpr<=pixel_reach


def _native_paint_inventory(checkpoint,origin,dpr=1):
    stack=checkpoint['resources'].get('paint_stack')
    if not isinstance(stack,dict) or stack.get('status')!='published':
        raise RunError(3,'calibration','actual native published paint inventory unavailable')
    capture=checkpoint['captures'][0];raw=checkpoint['resources'].get('readback',{});binding=stack.get('capture_binding',{})
    if (stack.get('source')!='actual-submitter-visible' or stack.get('scene_revision')!=capture['scene_revision'] or
            binding.get('run_nonce')!=checkpoint['nonce'] or binding.get('nonce')!=capture['nonce'] or
            binding.get('checkpoint')!=checkpoint['step'] or any(binding.get(k)!=raw.get(k) for k in
            ('scene_revision','readback_id','fence_id','renderer_owner','device_epoch'))):
        raise RunError(4,'protocol','paint inventory is not bound to this exact capture/fence/revision')
    commands=stack.get('commands');layers=stack.get('layers')
    if not isinstance(commands,list) or not isinstance(layers,list):raise ProtocolError('missing actual submitted commands/layers')
    if stack.get('composition') and (stack.get('composition_version')!=1 or stack.get('composition_scene_revision')!=stack['scene_revision']):
        raise RunError(4,'protocol','actual composition program does not bind the submitted scene revision')
    group_paths={i:[] for i in range(len(commands))};coverage_roles={i:0 for i in range(len(commands))};opened=[]
    for op in stack.get('composition',[]):
        if op['kind']==1:opened.append(op['request'])
        elif op['kind']==2:
            if not opened or opened.pop()!=op['request']:raise ProtocolError('unbalanced actual submitted composition')
        elif op['kind']==0:
            for ordinal in range(op['first'],op['first']+op['count']):
                if ordinal not in group_paths:raise ProtocolError('composition range escapes actual commands')
                group_paths[ordinal]=list(opened);coverage_roles[ordinal]=op['coverage']
        elif op['kind']!=3:raise ProtocolError('unknown actual composition operation')
    if opened:raise ProtocolError('unclosed actual submitted composition')
    sources={};seen=set();result=[]
    for text in stack.get('glyph_inventory',[]):
        for ink in text.get('ink_instances',[]):
            key=(ink.get('instance_index'),ink.get('atlas_page'))
            if key in sources and sources[key][1]!=ink:raise ProtocolError('conflicting actual glyph source inventory')
            sources[key]=(text,ink)
    for layer in layers:
        identity=layer.get('source_material_id')
        if not identity or identity in seen:raise ProtocolError('paint source identity missing/duplicated')
        seen.add(identity)
        for name in ('owner','fragment'):
            if not layer.get(name,{}).get('generation'):raise ProtocolError('paint owner/fragment generation missing')
        if not layer.get('publication_revision') or layer.get('in_fragment') is not True:
            raise ProtocolError('paint has no actual fragment publication')
        index=layer['packed_index'];kind=layer['kind'];page=layer['atlas_page']
        actual=[{'command':i,'instance':index-command['first']} for i,command in enumerate(commands)
                if command['kind']==kind and command['atlas_page']==page and command['first']<=index<command['first']+command['count']]
        supplied=layer.get('submitted_membership',[])
        if actual!=[{k:m.get(k) for k in ('command','instance')} for m in supplied] or bool(actual)!=layer.get('in_submitted_scene'):
            raise RunError(4,'protocol','paint final-command membership contradicts actual visible commands')
        for member in supplied:
            ordinal=member['command']
            if member.get('composition_group_path',[])!=group_paths[ordinal] or member.get('coverage_role',0)!=coverage_roles[ordinal]:
                raise RunError(4,'protocol','paint membership composition group/coverage contradicts actual submitted program')
        if not actual:continue
        if len(actual)!=1:raise ProtocolError('ambiguous/repeated native final paint membership')
        a=layer.get('actual')
        if not isinstance(a,dict):raise RunError(3,'calibration','actual image/unknown paint material unsupported')
        if a.get('paint_present') or a.get('path_present') or a.get('angle',0)!=0:
            raise RunError(3,'calibration','gradient/path/rotated glyph is outside finite pilot domain')
        rgba=a['color'];opacity=a['opacity']
        from .color_metrics import vector
        vector(rgba,4,unit=True);vector([opacity],1,unit=True)
        if not a.get('visible',True) or opacity==0 or rgba[3]==0:continue
        bounds=dict(a['bounds']);translation=a.get('translation',[0,0]);bounds['x']+=translation[0];bounds['y']+=translation[1]
        # The quad shader emits zero coverage for a degenerate extent. Keep
        # its verified command membership without treating it as visible ink.
        if kind==0 and (bounds['width']==0 or bounds['height']==0):continue
        bounds=relative(bounds,origin)
        clips=[]
        clip=a.get('clip')
        if kind==1:
            l,t,r,bottom=a['clip_bounds'];clip=dict(x=l,y=t,width=max(0,r-l),height=max(0,bottom-t))
        if clip:
            if clip['width']<=0 or clip['height']<=0:continue
            clips.append(relative(clip,origin))
        row=dict(identity=identity,side='native',bounds=bounds,radii=a.get('radius',0),color=tuple(rgba[:3]),
                 alpha=rgba[3],opacity=opacity,groups=[],clips=clips,order=(actual[0]['command'],actual[0]['instance']),
                 source=layer,kind='fill',role=layer.get('role'))
        if kind==0 and layer.get('role')=='caret':row.update(kind='caret',caret_paint_bounds=True)
        if kind==1:
            source=sources.get((index,page))
            if source is None:raise ProtocolError('submitted glyph has no actual scene/atlas/font inventory')
            text,ink=source
            if not text.get('scene_identity',{}).get('generation') or 'atlas_source' not in ink:
                raise RunError(3,'calibration','actual glyph generation/atlas source unavailable')
            if ink['bounds']!=a['bounds'] or ink['translation']!=translation or ink['clip']!=a['clip_bounds']:
                raise ProtocolError('glyph inventory differs from actual final submitted instance')
            row.update(kind='glyph',glyph_source=source)
        elif kind==2:
            if not layer.get('primitive',{}).get('generation'):raise ProtocolError('effect lacks actual generation-bearing source identity')
            effect=a['effect_kind']
            if effect in (0,1) and (a['blur']>0 or a['spread'] or a['offset']!=[0,0] or layer.get('role') in ('shadow','inset-shadow')):
                row.update(kind='shadow' if effect==0 else 'inset-shadow',offset=a['offset'],blur=a['blur'],spread=a['spread'],knockout=False)
            elif effect==2:row.update(kind='outline',width=a['outline_width'],offset_width=a['outline_offset'])
            elif effect==6:
                inset = a.get('shadow_caster_inset')
                if type(inset) not in (int, float) or not math.isfinite(inset) or inset < 0:
                    raise ProtocolError('submitted exterior shadow lacks its actual caster inset')
                row.update(kind='shadow',offset=a['offset'],blur=a['blur'],spread=a['spread'],knockout=True,
                           caster_inset=inset)
            elif effect not in (0,1):raise RunError(3,'calibration','non-round effect unsupported in finite pilot')
        for group in group_paths[actual[0]['command']]:
            requests=stack.get('composition_requests',[])
            if not 0<=group<len(requests):raise ProtocolError('composition group has no actual request')
            request=requests[group]
            if request.get('opacity_group') is True:
                opacity=request['activation']
                if request.get('backdrop') or type(opacity) not in (int,float) or not 0<=opacity<=1:
                    raise ProtocolError('invalid submitted opacity composition')
                row['clips'].append(relative(request['hard_clip'],origin))
                row['groups'].append((f'paint:{group}',opacity))
                continue
            if request.get('backdrop') or request.get('activation',1)!=1:
                raise RunError(3,'calibration','intersecting backdrop/animated mask outside finite pilot')
            support=_support(row);shape=relative(request['shape'],origin);hard=relative(request['hard_clip'],origin)
            corners=[(support['x']+x*support['width'],support['y']+y*support['height']) for x,y in ((0,0),(1,0),(0,1),(1,1))]
            if any(_shape_distance(point,shape,request['radius'])>=-1/dpr or not (hard['x']<=point[0]<=hard['x']+hard['width'] and
                    hard['y']<=point[1]<=hard['y']+hard['height']) for point in corners):
                raise RunError(3,'calibration','actual rounded composition mask intersects finite paint support')
            row.setdefault('verified_nonintersecting_masks',[]).append(group)
        result.append(row)
    # Retain real composition program separately. No claim that an unobserved mask
    # is equivalent to a hard clip or opacity applied to each child.
    return sorted(result,key=lambda row:row['order']),stack


def _empty_select_compositor_caret(node,viewport):
    actual=node['actual'];editor=actual.get('editor',{})
    fact=node.get('compositor_caret')
    if (actual.get('attributes',{}).get('role')!='combobox' or not editor.get('focused') or
            editor.get('value')!='' or any(editor.get('selection_'+end)!=0 for end in ('start','end')) or
            editor.get('caret_animation')!='manual' or
            not any(0<float(group['opacity'])<1 for group in node['composition_group_path'])):
        if fact is not None:raise ProtocolError('empty Select caret is not bound to its focused DOM owner')
        return None
    box=actual['bounds'];rtl=node['style'].get('direction')=='rtl';edge=box['x']+(box['width'] if rtl else 0)
    if not 0<=edge<viewport['width'] or box['y']>=viewport['height'] or box['y']+box['height']<=0:
        if fact is not None:raise ProtocolError('invisible empty Select caret has unexpected compositor evidence')
        return None
    if not isinstance(fact,dict) or fact.get('source')!='Chromium LayerTree.snapshotCommandLog':
        raise RunError(3,'calibration','actual empty Select compositor caret unavailable')
    geometry=fact.get('bounds',{});encoded=fact.get('argb','')
    if (not isinstance(geometry,dict) or set(geometry)!={'x','y','width','height'} or
            any(type(value) not in (int,float) or not math.isfinite(value) for value in geometry.values()) or
            geometry['width']!=1 or geometry['height']<=0 or
            abs(geometry['x']+(geometry['width'] if rtl else 0)-edge)>1 or geometry['y']<box['y'] or
            geometry['y']+geometry['height']>box['y']+box['height'] or
            not re.fullmatch(r'[0-9]+',str(fact.get('compositor_layer_id',''))) or
            not isinstance(encoded,str) or not re.fullmatch(r'#[0-9A-Fa-f]{8}',encoded)):
        raise ProtocolError('invalid observed empty Select compositor caret geometry or material')
    rgba=tuple(int(encoded[i:i+2],16)/255 for i in (3,5,7,1))
    if any(abs(a-b)>.5/255 for a,b in zip(rgba,_css_rgba(editor['caret_color']),strict=True)):
        raise ProtocolError('empty Select compositor caret differs from actual editor material')
    return geometry,rgba


def _web_paint_inventory(checkpoint,origin,dpr):
    stack=checkpoint['resources'].get('paint_stack')
    if not isinstance(stack,dict) or stack.get('status')!='published':raise RunError(3,'calibration','actual Chromium paint order inventory unavailable')
    binding=stack.get('capture_binding',{});capture=checkpoint['captures'][0]
    if (binding.get('run_nonce')!=checkpoint['nonce'] or binding.get('capture_nonce')!=capture['nonce'] or
            binding.get('checkpoint')!=checkpoint['step'] or binding.get('capture_sha256')!=capture['sha256'] or
            binding.get('scene_revision')!=capture['scene_revision']):
        raise RunError(4,'protocol','Web paint inventory/capture binding differs')
    result=[];seen=set()
    svg_roots={node['backend_node_id']:node for node in stack['layers'] if node.get('node_name','').lower()=='svg'}
    for node in stack['layers']:
        identity=node['source_material_id'];s=node['style'];a=node['actual']
        if identity in seen:raise ProtocolError('duplicate Web backend paint node')
        seen.add(identity)
        if (s.get('display')=='none' or s.get('visibility') in ('hidden','collapse') or s.get('opacity')=='0' or
                any(group.get('opacity')=='0' for group in node['composition_group_path'])):
            continue
        if not isinstance(node.get('paint_order'),int):raise RunError(3,'calibration','actual Chromium paint group missing')
        for field in ('background-image','filter','backdrop-filter','clip-path','mask-image','text-shadow'):
            if s.get(field,'none')!='none':raise RunError(3,'calibration','unsupported actual CSS '+field)
        if node.get('pseudo') and s.get('content','none') not in ('none','normal','""',"''"):
            raise RunError(3,'calibration','generated pseudo text has no finite actual-font outline control')
        if s.get('mix-blend-mode','normal')!='normal':raise RunError(3,'calibration','unsupported actual CSS blend')
        transform=s.get('transform','none')
        if transform!='none':
            match=re.fullmatch(r'matrix\(1, 0, 0, 1, [-\d.]+, [-\d.]+\)',transform)
            if not match:raise RunError(3,'calibration','nontranslation actual CSS transform unsupported')
        groups=[]
        for group in node['composition_group_path']:
            opacity=float(group['opacity'])
            if not 0<=opacity<=1:raise ProtocolError('invalid actual opacity group')
            groups.append((str(group['backend_node_id']),opacity))
        if any(opacity==0 for _,opacity in groups):continue
        bounds=a.get('bounds',node['bounds']) if node.get('node_type')!=3 else node['bounds']
        if bounds['width']<=0 or bounds['height']<=0:continue
        bounds=relative(bounds,origin)
        clips=[]
        clipped_out=False
        for clip in a.get('clips',[]):
            if clip['clip_path']!='none' or clip['transform']!='none' and not re.fullmatch(r'matrix\(1, 0, 0, 1, [-\d.]+, [-\d.]+\)',clip['transform']):
                raise RunError(3,'calibration','CSS ancestor clip transform/path unsupported')
            if clip['bounds']['width']==0 or clip['bounds']['height']==0:
                clipped_out=True
                break
            clips.append(relative(clip['bounds'],origin))
        if clipped_out:
            continue
        base=dict(identity=identity,side='web',bounds=bounds,groups=groups,clips=clips,alpha=1.,opacity=1.,
                  order=(node['paint_order'],node['layout_ordinal']),source=node,radii=(0,0,0,0))
        def add(kind,color,suffix,**fields):
            if color[3]==0:return
            result.append(dict(base,identity=identity+suffix,kind=kind,color=color[:3],alpha=color[3],**fields))
        compositor_caret=_empty_select_compositor_caret(node,stack.get('viewport',{}))
        if compositor_caret:
            geometry,rgba=compositor_caret
            add('caret',rgba,'/compositor-caret',bounds=relative(geometry,origin),caret_paint_bounds=True)
        if node['node_type']==3:
            text=a.get('text')
            if not text or text['content']!=node['layout_text']:raise RunError(3,'calibration','actual DOM/UA Text paint source unavailable')
            line_top=text.get('block_line_top')
            line_shift=0
            if line_top is not None:
                span=a.get('bounds',{})
                if (type(line_top) not in (float,int) or not math.isfinite(line_top) or
                        span.get('y')!=line_top or text['boxes']!=[span] or a.get('tag')!='SPAN'):
                    raise ProtocolError('invalid Chromium single-line block origin')
                # Blink snaps the line fragment in CSS pixels before glyphs
                # are rasterized at the device scale. The Range stays unsnapped.
                line_shift=math.floor(line_top+.5)-line_top
            for group in text['semantic_groups']:
                content=text['content'].encode()[group['byte_begin']:group['byte_end']].decode()
                if content.isspace():continue
                color=_css_rgba(group['color']);selection=text.get('selection');utf16_begin=len(text['content'].encode()[:group['byte_begin']].decode().encode('utf-16-le'))//2
                utf16_end=utf16_begin+len(content.encode('utf-16-le'))//2
                selected=selection and selection['start']<utf16_end and utf16_begin<selection['end']
                for i,box in enumerate(group['boxes']):
                    local=relative(box,origin)
                    if selected:add('fill',_css_rgba(selection['background']),f'/selection/{group["byte_begin"]}/{i}',bounds=local)
                    ink=group.get('inline_ink')
                    outline_origin=None
                    if ink is not None:
                        if (i!=0 or len(group['boxes'])!=1 or len(content)!=1 or
                                set(ink) not in ({'x','right'},{'x','right','top','bottom','baseline'}) or
                                not all(type(v) in (float,int) and math.isfinite(v) for v in ink.values()) or
                                ink['right']<ink['x'] or ('baseline' in ink and ink['bottom']<ink['top'])):
                            raise ProtocolError('invalid actual Chromium glyph ink metrics')
                        left=min(local['x'],ink['x']-origin['x'])
                        right=max(local['x']+local['width'],ink['right']-origin['x'])
                        local=dict(local,x=left,width=right-left)
                        if 'baseline' in ink:
                            if line_top is not None and box['y']!=line_top:
                                raise ProtocolError('Chromium glyph is outside its observed block line')
                            top=min(local['y'],ink['top']+line_shift-origin['y'])
                            bottom=max(local['y']+local['height'],ink['bottom']+line_shift-origin['y'])
                            local=dict(local,y=top,height=bottom-top)
                            outline_origin=(box['x']-origin['x'],ink['baseline']+line_shift-origin['y'])
                    add('glyph',_css_rgba(selection['color']) if selected else color,
                        f'/text/{group["byte_begin"]}/{i}',bounds=local,text_group=group,content=content,
                        outline_origin=outline_origin)
            caret=text.get('caret')
            if caret and caret['edge']['height']>0 and caret['animation']=='manual':
                edge=caret['edge'];top=math.floor(edge['y']*dpr)/dpr
                bottom=math.ceil((edge['y']+edge['height'])*dpr)/dpr
                b=dict(x=edge['x']-1/dpr,y=top,width=2/dpr,height=bottom-top)
                add('caret',_css_rgba(caret['color']),'/caret',bounds=relative(b,origin),
                    caret_edge=dict(edge,x=edge['x']-origin['x'],y=edge['y']-origin['y']),
                    geometry_status='actual collapsed UA Range edge; thickness measured from unmasked pixels within one physical pixel of edge')
            continue
        # SVG paths remain their real paint sources. Formal identity is checked by
        # the shared oracle before a path's local glyph-like raster is consumed.
        if node['node_name'].lower()=='path':
            if s.get('stroke','none')!='none':raise RunError(3,'calibration','SVG stroke is outside formal finite fill domain')
            color=_css_rgba(s['fill']);color=(*color[:3],color[3]*float(s.get('fill-opacity','1')))
            ancestors=node.get('ancestry',[])
            svg=svg_roots.get(ancestors[1]) if len(ancestors)>1 else None
            viewport=view_box=None
            if svg and svg['style'].get('transform','none')=='none' and svg['actual'].get('bounds'):
                attrs=svg['actual'].get('attributes',{})
                if attrs.get('transform') in (None,''):
                    viewport=relative(svg['actual']['bounds'],origin)
                    view_box=attrs.get('viewBox')
            add('glyph',color,'/path',formal_svg=True,svg_viewport=viewport,svg_view_box=view_box)
            continue
        radii=[]
        for corner in ('top-left','top-right','bottom-right','bottom-left'):
            pair=s.get('border-'+corner+'-radius','0px').split()
            if len(pair)>2:raise ProtocolError('invalid computed radius')
            values=[_css_number(p) for p in pair]
            if len(values)==2 and values[0]!=values[1]:raise RunError(3,'calibration','elliptical radius unsupported')
            radii.append(values[0])
        base['radii']=tuple(radii)
        widths=[];colors=[]
        for edge in ('top','right','bottom','left'):
            width=_css_number(s.get('border-'+edge+'-width','0px'));style=s.get('border-'+edge+'-style','none')
            if width and style not in ('none','solid'):raise RunError(3,'calibration','non-solid actual border unsupported')
            widths.append(width if style!='none' else 0);colors.append(_css_rgba(s.get('border-'+edge+'-color','rgba(0, 0, 0, 0)')))
        shadows=css_shadows(s.get('box-shadow','none'))
        if shadows and len(set(radii))!=1:raise RunError(3,'calibration','asymmetric-radius shadow has no fixed integral control')
        fill_color = _css_rgba(s.get('background-color','rgba(0, 0, 0, 0)'))
        if (node['node_name']=='BUTTON' and transform=='none' and fill_color[3]==1 and len(set(widths))==1 and
                len(set(colors))==1 and colors[0][3]==1 and len(set(radii))==1 and widths[0]>0 and
                widths[0]<=radii[0]<.5*min(bounds['width'],bounds['height'])):
            # Blink's uniform opaque rounded border paints a pixel-snapped
            # border box. Keep the actual DOM bounds on the source record.
            x=bounds['x']+origin['x']
            left=math.floor(x+.5)
            right=math.floor(x+bounds['width']+.5)
            if right-left>2*widths[0]:
                bounds=dict(bounds,x=left-origin['x'],width=right-left)
                base['bounds']=bounds
        for shadow in reversed(shadows):
            if not shadow['inset']:
                # Blink ClipToBorderEdge insets the knockout by one CSS pixel
                # when the local background paint is opaque.
                add('shadow',shadow['color'],'/shadow/'+str(shadow['ordinal']),offset=shadow['offset'],
                    blur=shadow['blur'],spread=shadow['spread'],knockout=True,radii=radii[0],
                    caster_inset=1 if fill_color[3] == 1 else 0)
        clip=s.get('background-clip','border-box');fill_bounds=bounds;fill_radii=tuple(radii)
        if clip in ('padding-box','content-box'):
            fill_bounds=_inset(fill_bounds,widths);fill_radii=tuple(max(0,r-max(widths)) for r in radii)
            if clip=='content-box':fill_bounds=_inset(fill_bounds,[_css_number(s['padding-'+edge]) for edge in ('top','right','bottom','left')])
        elif clip!='border-box':raise RunError(3,'calibration','unsupported actual background clip')
        def rect_on_device_grid(rect):
            x, y = rect['x'], rect['y']
            edges = (x, y, x+rect['width'], y+rect['height'])
            return all(edge * dpr == round(edge * dpr) for edge in edges)

        # With shadow ink beneath the square interfaces, keep fully covered
        # device pixels fixed so the hidden shadow is never an extra unknown.
        square_shadow = not any(radii) and any(
            not shadow['inset'] and (any(shadow['offset']) or shadow['blur'] or shadow['spread'])
            for shadow in shadows)
        if fill_bounds['width'] and fill_bounds['height']:
            add('fill',fill_color,'/fill',bounds=fill_bounds,radii=fill_radii,
                device_aligned_square=square_shadow and rect_on_device_grid(fill_bounds))
        for shadow in reversed(shadows):
            if shadow['inset']:
                padding_box=_inset(bounds,widths)
                if padding_box['width'] and padding_box['height']:
                    add('inset-shadow',shadow['color'],'/inset/'+str(shadow['ordinal']),bounds=padding_box,
                        offset=shadow['offset'],blur=shadow['blur'],spread=shadow['spread'],radii=max(0,radii[0]-max(widths)))
        if any(widths):
            visible=[edge for edge,(width,color) in enumerate(zip(widths,colors)) if width and color[3]]
            if visible and len({colors[edge] for edge,width in enumerate(widths) if width})==1:
                inner = _inset(bounds,widths)
                aligned = square_shadow and rect_on_device_grid(bounds) and (
                    min(inner['width'],inner['height']) == 0 or rect_on_device_grid(inner))
                add('border',colors[visible[0]],'/border',widths=widths,device_aligned_square=aligned)
            elif len(visible)==1 and not any(radii):
                edge=visible[0]
                polygon,box=_square_border_side(bounds,widths,edge)
                add('border-side',colors[edge],f'/border/{edge}',bounds=box,polygon=polygon)
            elif visible:
                raise RunError(3,'calibration','multi-color or rounded split border outside finite pilot')
        outline_width=_css_number(s.get('outline-width','0px'))
        if outline_width and s.get('outline-style')!='none':
            if s['outline-style']!='solid':raise RunError(3,'calibration','non-solid outline unsupported')
            add('outline',_css_rgba(s['outline-color']),'/outline',width=outline_width,offset_width=_css_number(s['outline-offset']),radii=radii[0])
            descendants=[n for n in stack['layers'] if node['backend_node_id'] in n.get('ancestry',[])]
            if descendants:
                last=max((n['paint_order'],n['layout_ordinal']) for n in descendants)
                result[-1]['order']=(last[0],last[1]+.5)
    return sorted(result,key=lambda row:row['order']),stack


def _composite_point(layers,coverages):
    """One finite source-over stack; actual opacity groups close exactly once."""
    materials=tuple((tuple(row['color']),row['alpha'],row['opacity'],tuple(tuple(group) for group in row['groups']))
                    for row in layers)
    return _composite_materials(materials,tuple(coverages))


@lru_cache(maxsize=32768)
def _composite_materials(materials,coverages):
    # Only the full ordered material stack and source coverages enter this cache.
    # Captured RGB never shares or substitutes any source-over calculation.
    from .color_metrics import source_over
    frames=[([1.,1.,1.],1.,None,1.)];opened=[]
    def close():
        rgb,alpha,_,opacity=frames.pop();opened.pop();effective=alpha*opacity
        if alpha:
            parent=frames[-1];straight=[v/alpha for v in rgb]
            frames[-1]=(list(source_over(straight,parent[0],effective)),effective+(1-effective)*parent[1],parent[2],parent[3])
    for (color,source_alpha,source_opacity,chain),coverage in zip(materials,coverages):
        common=0
        while common<min(len(chain),len(opened)) and chain[common][0]==opened[common]:common+=1
        while len(opened)>common:close()
        for name,opacity in chain[common:]:opened.append(name);frames.append(([0.,0.,0.],0.,name,opacity))
        rgb,alpha,name,opacity=frames[-1];effective=coverage*source_alpha*source_opacity
        frames[-1]=(list(source_over(color,rgb,effective)),effective+(1-effective)*alpha,name,opacity)
    while opened:close()
    return tuple(frames[0][0])


def _palette_basis(layers,background,*,byte_error=.5/255):
    """Choose at most three source color axes, preserving every source identity."""
    from itertools import combinations
    from .color_metrics import visible_basis,solve_visible_contributions
    colors=list(dict.fromkeys(layer['color'] for layer in layers if layer['color']!=background))
    if not colors:return visible_basis(background,[],byte_error=byte_error)
    # Same-axis sources are joint observations. They are never assigned separate
    # alpha values, even if a color basis happens to be full rank.
    joint=None
    try:joint=visible_basis(background,colors,byte_error=byte_error)
    except ProtocolError:pass
    if joint is not None:return joint
    for count in (1,2,3):
        for palette in combinations(colors,count):
            try:basis=visible_basis(background,palette,byte_error=byte_error)
            except ProtocolError:continue
            if basis['mode']!='visible-contribution':continue
            if all(solve_visible_contributions(color,basis)['valid'] for color in colors):return basis
    raise RunError(3,'calibration','source palette has no quantization-safe finite <=3-axis visible basis')


def _visible_weight(layers,coverages,position):
    source=layers[position];high=list(layers);low=list(layers);coverage=list(coverages);coverage[position]=1.
    high[position]=dict(source,color=(1.,1.,1.));low[position]=dict(source,color=(0.,0.,0.))
    white=_composite_point(high,coverage);black=_composite_point(low,coverage)
    weights=tuple(x-y for x,y in zip(white,black))
    if max(weights)-min(weights)>1e-10 or not -1e-10<=weights[0]<=1+1e-10:
        raise ProtocolError('fixed source-over visible weight is inconsistent')
    return min(1.,max(0.,weights[0]))


def _paint_endpoints(layers,coverages,variable):
    for mask in range(1<<len(variable)):
        candidate=coverages.copy()
        for bit,position in enumerate(variable):
            candidate[position]=float(bool(mask&(1<<bit)))
        yield _composite_point(layers,candidate)


def _matches_soft_shadow_rgb(layers,coverages,variable,observed,predicted):
    from .color_metrics import contribution_domain,in_contribution_domain

    hard=[position for position in variable if layers[position]['blur']==0]
    if not hard:
        expected=tuple(round(channel*255) for channel in predicted)
        return max(abs(a-b) for a,b in zip(observed,expected,strict=True))<=2
    # Only the hard edge has unknown aperture coverage. Keep every soft
    # source fixed at its independent profile and test the possible RGB
    # outputs with the same two-code bound.
    domain=contribution_domain(_paint_endpoints(layers,coverages,hard))
    return in_contribution_domain(tuple(channel/255 for channel in observed),domain,(2/255,)*3)


def _matches_isolated_shadow_rgb(layers,coverages,variable,observed,x,y):
    first=variable[0]
    if variable!=list(range(first,len(layers))):
        return False
    witness=layers[first].get('isolated_shadow_witness')
    if not witness or any(row.get('isolated_shadow_witness') is not witness for row in layers[first:]):
        return False
    if _composite_point(layers[:first],coverages[:first])!=(1,1,1):
        return False
    alpha,width,height,(left,top)=witness
    dx,dy=x-left,y-top
    if not 0<=dx<width or not 0<=dy<height:
        return False
    output=255-alpha[dy*width+dx]
    return tuple(observed)==(output,output,output)


def _opaque_white_border_box(layers):
    kinds=tuple(row['kind'] for row in layers)
    if kinds not in (('fill','border'),('shadow','fill','border')):
        return False
    fill,border=layers[-2:]
    return (fill['color']==(1.,1.,1.) and all(row['source'] is fill['source'] for row in layers) and
            (len(layers)==2 or layers[0].get('blur')==0) and
            all(opacity==1 for row in layers for _,opacity in row['groups']) and
            all(row['alpha']*row['opacity']==1 for row in (fill,border)))


def _hard_shadow_box_domain(layers,endpoints):
    """RGB8 domain for an opaque white box and its border over a hard shadow."""
    from .color_metrics import contribution_domain
    if len(layers)!=3 or not _opaque_white_border_box(layers):
        return None
    return contribution_domain(endpoints)


def _isolated_opacity_interface(layers,coverages,variable):
    if len(variable)!=1 or variable[0]!=len(layers)-1:
        return False
    groups=lambda row:tuple((name,opacity) for name,opacity in row['groups'] if opacity!=1)
    interface=layers[-1]
    opacity=groups(interface)
    if len(opacity)!=1 or any(groups(row)!=opacity for row in layers):
        return False
    if interface['kind']=='fill':
        # A fixed, unblurred shadow can be the backing at the lower edge.
        return interface['alpha']*interface['opacity']==1 and (
            len(layers)==1 or len(layers)==2 and layers[0]['kind']=='shadow' and layers[0].get('blur')==0)
    if len(layers)<2:
        return False
    # A fully covered opaque backing in the same group hides all earlier
    # members. The remaining interface is one glyph over this RGB8 backing.
    backing=layers[-2]
    return (interface['kind']=='glyph' and backing['kind']=='fill' and coverages[-2]==1 and
            backing['alpha']*backing['opacity']==1)


def _two_translucent_black_writes(layers,coverages,variable):
    return (len(layers)==2 and variable==[1] and coverages[0]==1 and
        tuple(row['kind'] for row in layers)==('fill','glyph') and
        all(not row['groups'] and row['color']==(0.,0.,0.) for row in layers) and
        all(0<row['alpha']*row['opacity']<1 for row in layers))


def _hard_shadow_fill_group_opacity(layers,variable):
    if len(layers)!=2 or variable!=[0,1]:
        return None
    shadow,fill=layers
    if (shadow['kind']!='shadow' or shadow.get('blur')!=0 or fill['kind']!='fill' or
            shadow['bounds']!=fill['bounds'] or fill['alpha']*fill['opacity']!=1):
        return None
    shadow_groups=tuple(group for group in shadow['groups'] if group[1]!=1)
    fill_groups=tuple(group for group in fill['groups'] if group[1]!=1)
    if shadow_groups==fill_groups and len(fill_groups)==1 and 0<fill_groups[0][1]<1:
        return fill_groups[0][1]
    return None


def _paint_byte_error(layers,coverages,variable):
    group_opacity=_hard_shadow_fill_group_opacity(layers,variable)
    if group_opacity is not None:
        # Each draw writes premultiplied RGB and alpha in the isolated target.
        # The transparent group's blit attenuates both errors before the final RGB write.
        return (1+4*group_opacity)*.5/255
    # Admit an extra half-code only for the witnessed intermediate RGB8 writes.
    if (_opaque_white_border_box(layers) or _isolated_opacity_interface(layers,coverages,variable) or
        _two_translucent_black_writes(layers,coverages,variable)):
        return 1/255
    return .5/255


def _exact_rgb8_color(color):
    candidate=tuple(round(channel*255)/255 for channel in color)
    return candidate if struct.pack('<3f',*candidate)==struct.pack('<3f',*color) else color


def _bounded_paint_basis(layers,coverages,variable):
    from .color_metrics import visible_basis
    # The CSS border path quantizes its PM color and then the final RGB8 blend.
    # Scope the two half-code terms to the observed single opaque CSS box.
    byte_error=_paint_byte_error(layers,coverages,variable)
    opaque_fills=all(row['kind']=='fill' and row['alpha']==1 and row['opacity']==1 and
                     not row['groups'] for row in layers)
    if len(variable)==1:
        lower,upper=_paint_endpoints(layers,coverages,variable)
        if opaque_fills:
            # An opaque fill on an RGB8 endpoint has the same source value
            # in CSS and in the packed float material. Its transition still
            # uses the unmodified observed pixels.
            lower=_exact_rgb8_color(lower)
            upper=_exact_rgb8_color(upper)
        # Known neighboring paint is the backdrop, not a second unknown.
        try:return visible_basis(lower,[] if lower==upper else [upper],byte_error=byte_error)
        except ProtocolError:pass
    palette=[dict(row,color=_exact_rgb8_color(row['color'])) for row in layers] if opaque_fills else layers
    try:return _palette_basis(palette,(1.,1.,1.),byte_error=byte_error)
    except RunError:
        joint=_joint_paint_output_basis(layers,coverages,variable)
        if joint is None:raise
        return joint


def _joint_paint_output_basis(layers,coverages,variable):
    """Observe inseparable RGB8 paint produced by a recognized finite stack.

    Their nearly parallel colors can leave individual weights unobservable.
    The fixed source-over endpoints still bound every visible pixel and
    determine an output scale without fitting it to either capture.
    """
    from .color_metrics import contribution_domain
    kinds=tuple(row['kind'] for row in layers)
    hard_outline=(kinds==('shadow','outline') and variable==[0,1] and layers[0].get('blur')==0)
    gapped_fill=(kinds in (('fill','outline'),('outline','fill')) and variable==[0,1] and
                 layers[kinds.index('outline')].get('offset_width',0)>0 and layers[0]['bounds']==layers[1]['bounds'] and
                 all(row['alpha']*row['opacity']==1 for row in layers))
    floating_shadow=(kinds[:3]==('shadow','fill','outline') and 4<=len(layers)<=6 and len(variable)>=2 and
                     layers[0].get('blur')==0 and layers[1]['color']==(1.,1.,1.) and
                     all(row['kind']=='shadow' and row.get('blur',0)>0 for row in layers[3:]))
    group_opacity=_hard_shadow_fill_group_opacity(layers,variable)
    if group_opacity is None and (not (hard_outline or gapped_fill or floating_shadow) or
                                  any(layer['groups'] for layer in layers)):
        return None
    endpoints=tuple(_paint_endpoints(layers,coverages,variable))
    # Set the origin at the lightest attainable value on each channel.
    # A white fill over a fixed lower shadow must not create negative ink.
    base=tuple(max(channel) for channel in zip(*endpoints,strict=True))
    span=max(abs(a-b) for endpoint in endpoints for a,b in zip(base,endpoint,strict=True))
    # Each physical RGB8 source-over can round the output by at most half
    # a code. Later layers attenuate earlier errors. The fixed output
    # domain must itself be distinguishable at the existing .05 cap.
    rounding=(len(layers)*.5/255 if group_opacity is None else _paint_byte_error(layers,coverages,variable))
    if not span or rounding/span>.05:
        return None
    return dict(mode='joint-paint-output',base=base,span=span,domain=contribution_domain(endpoints),rounding=rounding)


def _paint_coordinates(observed,basis):
    from .color_metrics import in_contribution_domain,solve_visible_contributions
    rgb=tuple(v/255 for v in observed)
    if basis['mode']=='joint-paint-output':
        valid=in_contribution_domain(rgb,basis['domain'],(basis['rounding'],)*3)
        values=tuple((b-v)/basis['span'] for b,v in zip(basis['base'],rgb,strict=True))
        return valid,values
    solution=solve_visible_contributions(rgb,basis)
    values=solution['contributions']
    return solution['valid'],tuple(values) if values is not None else (solution['joint_contrast'],)


def _formal_paint_gates(native,web,stack,origins,box,dpr):
    from .formal_icon_source import observed_web_icon_name,verify_native_icon

    def in_capture(bounds,clip,origin):
        left=max(bounds['x'],clip['x'],origin['x']+box[0]/dpr)
        top=max(bounds['y'],clip['y'],origin['y']+box[1]/dpr)
        right=min(bounds['x']+bounds['width'],clip['x']+clip['width'],origin['x']+box[2]/dpr)
        bottom=min(bounds['y']+bounds['height'],clip['y']+clip['height'],origin['y']+box[3]/dpr)
        return left<right and top<bottom

    commands=[dict(kind={0:'quad',1:'glyph',2:'rounded_effect',3:'image'}[c['kind']],
                   first_instance=c['first'],instance_count=c['count'],atlas_page=c['atlas_page']) for c in stack['commands']]
    def shown(ink):
        if ink.get('paint_opacity',0)<=0 or ink['color'][3]<=0:
            return False
        x,y=ink['translation']
        bounds=dict(ink['bounds'],x=ink['bounds']['x']+x,y=ink['bounds']['y']+y)
        left,top,right,bottom=ink['clip']
        clip=dict(x=left,y=top,width=right-left,height=bottom-top)
        return in_capture(bounds,clip,origins[1]) and any(c['kind']=='glyph' and
            c['atlas_page']==ink['atlas_page'] and
            c['first_instance']<=ink['instance_index']<c['first_instance']+c['instance_count'] for c in commands)

    seen=set();errors=[]
    nt={t['identity']:t for t in native['targets']};wt={t['identity']:t for t in web['targets']}
    for target in sorted(nt.keys() & wt.keys()):
        native_icons=Counter()
        web_icons=Counter()
        for icon in wt[target]['icons']:
            identity=observed_web_icon_name(icon)
            if in_capture(icon['bounds'],wt[target]['clip'],origins[0]) and any(
                    float(p['opacity'])*float(p['fill_opacity'])>0 for p in icon['paths']):
                web_icons[identity]+=1
        for icon in nt[target]['icons']:
            layers=icon.get('layers',[])
            if not any(shown(ink) for layer in layers for ink in layer.get('ink_instances',[])):
                continue
            identity=icon['identity']
            native_icons[identity]+=1
            key=tuple((layer['scene_identity']['index'],layer['scene_identity']['generation']) for layer in layers)
            if key in seen:
                continue
            seen.add(key)
            actual_layers=[]
            for layer in layers:
                ink=layer.get('ink_instances',[])
                if not ink:raise ProtocolError('formal icon layer has no actual atlas source')
                source=ink[0].get('atlas_source',{})
                font=source.get('font',{})
                actual_layers.append(dict(layer,layer_opacity=layer.get('layer_opacity'),
                    font_sha256=font.get('sha256'),font_source=font.get('source'),face_index=font.get('face_index'),glyph_id=source.get('glyph_id')))
            verify_native_icon(identity,actual_layers,commands)
        if web_icons!=native_icons:
            errors.append(dict(category='visual',step=native['step'],target=target,
                               message='actual formal icon submitted visibility differs',
                               expected=dict(sorted(web_icons.items())),actual=dict(sorted(native_icons.items()))))
    return errors


def _hard_paint_gates(native_layers,web_layers,step):
    """Source materials and canonical part geometry are checked before pixels.

    Surface implementations can use an opaque outer quad plus an inner fill
    instead of a CSS border. The two permitted boxes are derived from actual
    observed border geometry, never from native image residuals.
    """
    errors=[]
    def owners(row):return set(row['source'].get('owner_targets',[]))
    for row in native_layers:
        if row['kind'] in ('glyph','caret'):continue
        candidates=[r for r in web_layers if r['kind'] not in ('glyph','caret') and
                    (not owners(row) or not owners(r) or owners(row)&owners(r)) and
                    all(abs(x-y)<=1e-6 for x,y in zip(row['color'],r['color']))]
        if not candidates:
            errors.append(dict(category='visual',step=step,target=row['identity'],message='actual retained material has no matching observed Web part'))
            continue
        if row['kind'] in ('shadow','inset-shadow'):
            if not any(candidate['kind']==row['kind'] and
                       all(abs(a-b)<=.25 for a,b in zip(row['offset'],candidate['offset'],strict=True)) and
                       all(abs(row[field]-candidate[field])<=.25 for field in ('blur','spread','radii')) and
                       all(abs(row['bounds'][field]-candidate['bounds'][field])<=.25 for field in row['bounds'])
                       for candidate in candidates):
                errors.append(dict(category='layout',step=step,target=row['identity'],
                                   message='actual retained shadow geometry differs from observed Web shadow'))
            continue
        if row['kind']=='outline':continue
        matched=False
        for candidate in candidates:
            radii=candidate.get('radii',0);radii=(radii,)*4 if not isinstance(radii,(tuple,list)) else radii
            boxes=[(candidate['bounds'],radii)]
            if candidate['kind']=='border':boxes.append((_inset(candidate['bounds'],candidate['widths']),tuple(max(0,r-max(candidate['widths'])) for r in radii)))
            # CSS background paints under the border; native's inner fill is
            # derived from a real neighboring border part with the same owner.
            for border in web_layers:
                if border['kind']=='border' and owners(border)==owners(candidate) and border['bounds']==candidate['bounds']:
                    boxes.append((_inset(border['bounds'],border['widths']),tuple(max(0,r-max(border['widths'])) for r in radii)))
                    if (candidate['kind']=='fill' and any(radii) and border['alpha']==1 and
                            len(set(border['widths']))==1):
                        # Chromium's opaque rounded border uses a half-stroke
                        # inset to keep the background from bleeding at the edge.
                        inset=border['widths'][0]*.5
                        boxes.append((_inset(border['bounds'],inset),tuple(max(0,r-inset) for r in radii)))
            radius=row.get('radii',0)
            if any(all(abs(row['bounds'][k]-bounds[k])<=.25 for k in bounds) and
                   (all(abs(radius-r)<=.25 for r in expected) or row.get('clips') and any(abs(radius-r)<=.25 for r in expected))
                   for bounds,expected in boxes):matched=True;break
        if not matched:errors.append(dict(category='layout',step=step,target=row['identity'],message='actual retained part geometry differs from observed paint-bearing Web part'))
    return errors


def _glyph_priors_for_run(native,manifest,*,independent_masks=False,web=None):
    from .calibration import scenario_font_priors,font_priors_from_bytes
    from .formal_icon_source import official_font_bytes
    from .glyph_witness import scenario_masks,browser_scenario_masks,quarter_icon_masks
    from .protocol import ROOT,relative_path
    ordinary=set();icons=set();texts=[]
    for step,checkpoint in native['checkpoints'].items():
        views=(checkpoint['resources'],*native.get('context_tiles',{}).get(step,[]))
        for view in views:
            for text in view.get('paint_stack',{}).get('glyph_inventory',[]):
                texts.append(text)
                destination=icons if text.get('kind')=='icon-font' else ordinary
                destination.update(char for char in text['content'] if not char.isspace())
    web_rows=[]
    if web is not None:
        dpr=manifest['scope']['actual_dpr']
        for step,checkpoint in web['checkpoints'].items():
            views=[checkpoint]
            views.extend(dict(checkpoint,targets=[tile['target']],captures=[tile['capture']],resources=tile)
                         for tile in web.get('context_tiles',{}).get(step,[]))
            for view in views:
                anchor=next(row for row in view['targets'] if 'sample_anchor' in row['tokens'])
                rows,_=_web_paint_inventory(view,sample_anchor(anchor),dpr)
                for row in rows:
                    if row['kind']=='glyph' and not row.get('formal_svg'):
                        web_rows.append(row)
                        ordinary.update(row['content'])
    library=ROOT/relative_path(manifest['fonttools_library'])
    result=scenario_font_priors(manifest['fonts'],sorted(ordinary),library)
    aliases={};web_fonts={}
    for index,font in enumerate(manifest['fonts']):
        alias=aliases.setdefault(font['identity'],'ReferenceFont'+str(index))
        web_fonts[alias,int(font['weight'])]=font['sha256']
    result['web_fonts']=web_fonts
    if independent_masks:
        result['scenario_masks']=scenario_masks(manifest,ROOT,texts,result)
        result['quarter_icon_masks']=quarter_icon_masks(manifest,ROOT,texts)
        result['svg_tinted_framebuffer']=manifest['environment'].get('native',{}).get('backend') in ('direct3d12','d3d12')
        if web_rows:
            result['web_masks']=browser_scenario_masks(manifest,ROOT,(_web_glyph_source(row,result) for row in web_rows))
    if icons:result.update(font_priors_from_bytes(official_font_bytes(),[ord(c) for c in sorted(icons)],library))
    return result


def _glyph_witness_gate(row,point,dpr,priors,lower,observed,*,upper=None):
    """Independent SFNT interior witnesses; atlas padding and border are no ink."""
    from .calibration import outline_point
    from .color_metrics import unmix_coverage
    from .glyph_witness import glyph_ink_coverage,mask_identity,scenario_ink_byte
    text,ink=row['glyph_source'];source=ink['atlas_source'];font=source['font']
    encoded=text['content'].encode();content=encoded[ink['byte_begin']:ink['byte_end']].decode()
    if content.isspace():return None
    if len(content)!=1:raise RunError(3,'calibration','multi-scalar shaping group needs an independently supported outline prior')
    prior=priors.get(font['sha256']+':'+content)
    if prior is None:raise RunError(3,'calibration','actual glyph has no independently reconstructed SFNT prior')
    if source['glyph_id']!=prior['glyph_id']:raise ProtocolError('actual atlas glyph differs from independent SFNT cmap')
    display=source['display_scale'];scale=font['logical_pixel_size']/prior['units']
    if display!=dpr:raise RunError(3,'calibration','actual glyph atlas scale differs from capture DPR')
    masks=priors.get('scenario_masks')
    if masks is not None and text.get('kind')!='icon-font':
        rasterizer,witnesses=masks
        mask=witnesses.get(mask_identity(content,source))
        if mask is None:raise ProtocolError('submitted glyph has no independent device mask')
        byte=scenario_ink_byte(mask,source,row['bounds'],point,dpr)
        if byte==0:return None
        minimum=glyph_ink_coverage(byte,row['color'],rasterizer,row['alpha']*row['opacity'])
    else:
        baseline_x=row['bounds']['x']+(source['padding']-source['bearing_x']+source['phase']/4)/display
        baseline_y=row['bounds']['y']+(source['bearing_y']+source['padding']+source.get('phase_y',0)/4)/display
        if source.get('svg_viewport_origin') is True:
            baseline_y+=prior['ascent']*scale
        x=(point[0]-baseline_x)/scale;y=(baseline_y-point[1])/scale
        inside,distance=outline_point(prior['contours'],x,y)
        if not inside or distance*scale*dpr<=.125:return None
        # The interior disk is independently contained in the outline and pixel.
        minimum=math.pi*min(.5,max(0,distance*scale*dpr-1/1024))**2
    # A witness is used only where the caller proved this glyph is exposed.
    foreground,alpha=(row['color'],row['alpha']*row['opacity']) if upper is None else (upper,1.)
    try:unmixed=unmix_coverage(tuple(v/255 for v in observed),foreground,lower,alpha,byte_error=1/255)
    except ProtocolError:return False
    if minimum<=2*unmixed['quantization_bound']:return None
    return unmixed['coverage']+unmixed['quantization_bound']>=minimum


def _same_glyph_interface(first,second):
    if first['kind']!='glyph' or second['kind']!='glyph' or first['bounds']!=second['bounds']:return False
    a=first['glyph_source'][1]['atlas_source'];b=second['glyph_source'][1]['atlas_source']
    def identity(source):
        return (source['glyph_id'],source['font']['sha256'],source['phase'],source.get('phase_y',0),
                source.get('svg_viewport_origin',False))
    return identity(a)==identity(b)


def _glyph_samples_at_pixel(side,priors,point,dpr):
    from .glyph_witness import encoded_coverage,mask_identity,scenario_ink_byte
    pairs=[];coverages=side['coverages'].copy()
    rasterizer,masks=priors['scenario_masks']
    for position in side['variable']:
        row=side['active'][position]
        if (row['kind']!='glyph' or any(opacity!=1 for _,opacity in row['groups']) or
                row.get('formal_svg')):
            return None
        if row.get('web_glyph_mask') is not None:
            mask,x,y=row['web_glyph_mask']
            px,py=(math.floor(coordinate*dpr) for coordinate in point)
            byte=mask.at(px-x,py-y)
            if any(alternate.at(px-ax,py-ay)!=byte
                   for alternate,ax,ay in row.get('web_glyph_alternates',())):
                # The source does not identify one exact raster transfer here.
                return None
            source=row['outline_source']
            face=source['actual_font_sha256'];scalar=source['content']
        elif row.get('glyph_source') is not None:
            text,ink=row['glyph_source'];source=ink['atlas_source'];face=source['font']['sha256']
            scalar=text['content'].encode()[ink['byte_begin']:ink['byte_end']].decode()
            mask=masks.get(mask_identity(scalar,source))
            if mask is None:
                return None
            byte=scenario_ink_byte(mask,source,row['bounds'],point,dpr)
            x=round(row['bounds']['x']*dpr)+source['padding']-source['bearing_x']
            y=round(row['bounds']['y']*dpr)+source['padding']+source['bearing_y']
        else:
            return None
        coverages[position]=encoded_coverage(byte,row['color'],rasterizer)
        if byte:
            pairs.append(((face,scalar,mask,x,y,row['color']),row['alpha']*row['opacity']))
    return pairs,_composite_point(side['active'],coverages)


def _encoded_black_glyph_over_tint(sides,observations):
    alphas=[]
    for side in sides:
        rows=side['active']
        if (not 2<=len(rows)<=3 or side['variable']!=[len(rows)-1] or
                any(coverage!=1 for coverage in side['coverages'][:-1]) or
                any(row['kind']!='fill' for row in rows[:-1]) or rows[-1]['kind']!='glyph' or
                any(opacity!=1 for row in rows for _,opacity in row['groups'])):
            return None
        tint,glyph=rows[-2:]
        if (any(row['color']!=(1,1,1) or row['alpha']*row['opacity']!=1 for row in rows[:-2]) or
                tint['color']!=(0,0,0) or glyph['color']!=(0,0,0)):
            return None
        alphas.append((tint['alpha']*tint['opacity'],glyph['alpha']*glyph['opacity']))
    if any(not 0<a<1 for a in alphas[0]) or struct.pack('<2f',*alphas[0])!=struct.pack('<2f',*alphas[1]):
        return None
    # SkPMSrcOver uses a 256 denominator for its opaque destination.
    # The native target encodes each normal source-over write as UNORM8.
    browser=target=255
    for browser_alpha,target_alpha in zip(*alphas,strict=True):
        browser=(browser*(256-math.floor(255*browser_alpha+.5)))>>8
        target=round(target*(1-target_alpha))
    if tuple(observations)==((browser,)*3,(target,)*3):
        return (target,)*3
    return None


def _witnessed_black_glyph_over_tint(sides,samples,observations,point,dpr):
    if any(len(items)!=1 for items in samples):
        return None
    (web,_),(native,_)=(items[0] for items in samples)
    if web[:2]!=native[:2] or web[3:]!=native[3:]:
        return None
    px,py=(math.floor(value*dpr) for value in point)
    if any(sample[2].at(px-sample[3],py-sample[4])!=255 for sample in (web,native)):
        return None
    return _encoded_black_glyph_over_tint(sides,observations)


def _witnessed_glyph_rounding(sides,basis,observations,priors,point,dpr):
    """Resolve RGB8 rounding only where both captured glyph sources agree."""
    if basis.get('rank')!=1 or not priors or not priors.get('scenario_masks'):
        return None
    samples=[_glyph_samples_at_pixel(side,priors,point,dpr) for side in sides]
    if any(sample is None for sample in samples):
        return None
    (web,web_rgb),(native,native_rgb)=samples
    if not web or len(web)!=len(native) or len({key[-1] for key,_ in web})!=1:
        return None
    if (exact:=_witnessed_black_glyph_over_tint(sides,(web,native),observations,point,dpr)) is not None:
        return exact
    # CSS color numbers arrive as JSON doubles. The submitted native material
    # stores the same channels as float32, so compare that exact representation.
    if any(a[:-1]!=b[:-1] or struct.pack('<3f',*a[-1])!=struct.pack('<3f',*b[-1]) or abs(x-y)>1e-6
           for (a,x),(b,y) in zip(web,native,strict=True)):
        return None
    if any(abs(a-b)>1e-6 for a,b in zip(web_rgb,native_rgb,strict=True)):
        return None
    # Each glyph's quantized coverage and its RGB8 blend can contribute half
    # a code each. Source-over cannot amplify preceding rounding error.
    radius=sum((1+alpha)*.5 for _,alpha in web)
    if any(abs(actual-ideal*255)>radius+1e-9 for colors,rgb in zip(observations,(web_rgb,native_rgb),strict=True)
           for actual,ideal in zip(colors,rgb,strict=True)):
        return None
    return tuple(round(value*255) for value in web_rgb)


def _witnessed_svg_rounding(sides,observed,masks,point,dpr,placements,*,tinted_framebuffer=False):
    from .formal_icon_source import single_path_for_glyph
    from .glyph_witness import quarter_icon_key,scenario_origin,verified_quarter_icon_tie

    if (not masks or observed[0]==observed[1] or any(len(side['variable'])!=1 or
            side['variable'][0]!=len(side['active'])-1 for side in sides)):
        return None
    web,native=(side['active'][-1] for side in sides)
    if (not web.get('formal_svg') or not native.get('glyph_source') or
            any(row['color']!=(0,0,0) or row['alpha']*row['opacity']!=.25 or
                any(opacity!=1 for _,opacity in row['groups']) for row in (web,native))):
        return None
    text,ink=native['glyph_source']
    if text.get('kind')!='icon-font':
        return None
    key=web['identity'],native['identity']
    if key not in placements:
        source=ink['atlas_source'];glyph=source['glyph_id']
        mask=masks.get(quarter_icon_key(ink))
        attrs=web['source']['actual']['attributes'];style=web['source']['style']
        path=(re.sub(r'\s+','',attrs.get('d') or ''),style.get('fill-rule','nonzero'))
        viewport=web.get('svg_viewport')
        if viewport:
            signature=(*path,web['svg_view_box'])
        else:
            signature=path
        placements[key]=None
        if (mask and (viewport or not tinted_framebuffer) and
                attrs.get('transform') in (None,'') and style.get('transform','none')=='none' and
                signature==single_path_for_glyph(glyph,include_view_box=bool(viewport))):
            try:
                x,y=scenario_origin(mask,source,native['bounds'],dpr)
            except ProtocolError:
                pass
            else:
                region=tuple((viewport or web['bounds'])[name]*dpr for name in ('x','y','width','height'))
                if region==(x,y,mask.width,mask.height):
                    placements[key]=(mask,x,y)
    placement=placements[key]
    if placement is None:
        return None
    mask,x,y=placement
    coverage=mask.at(math.floor(point[0]*dpr)-x,math.floor(point[1]*dpr)-y)
    if tinted_framebuffer:
        return _encoded_black_glyph_over_tint(sides,observed) if coverage==255 else None
    if any(_composite_point(side['active'][:-1],side['coverages'][:-1])!=(1,1,1)
                                for side in sides):
        return None
    return verified_quarter_icon_tie(coverage,observed)


def _web_glyph_source(row,priors):
    group=row['text_group'];text=row['content'];baseline=row.get('outline_origin')
    if baseline is None or len(text)!=1:
        raise RunError(3,'calibration','actual Chromium glyph has no finite outline origin')
    families=[name.strip(' "\'') for name in row['source']['style']['font-family'].split(',')]
    weight=int(group['font_weight']);prior=None
    for family in families:
        sha=priors['web_fonts'].get((family,weight))
        if sha and (prior:=priors.get(f'{sha}:{text}')) is not None:
            break
    if prior is None:
        raise RunError(3,'calibration','actual Chromium glyph has no fixed-font SFNT prior')
    return dict(actual_font_sha256=sha,content=text),_css_number(group['font_size']),baseline,prior


def _web_glyph_support(row,priors,dpr,coverage,uncertainty,size,origin):
    """Check visible AA against the fixed SFNT outline, including thin strokes."""
    from .calibration import outline_witness
    _,font_size,baseline,prior=_web_glyph_source(row,priors)
    scale=font_size/prior['units'];reach=1/(dpr*scale)
    width=size[0]
    for index,(alpha,error) in enumerate(zip(coverage,uncertainty,strict=True)):
        if alpha<=error:
            continue
        x=((origin[0]+index%width+.5)/dpr-baseline[0])/scale
        y=(baseline[1]-(origin[1]+index//width+.5)/dpr)/scale
        if outline_witness(prior,x,y,reach) is False:
            return False
    return True


def _status_input_caret(case,targets,inventories,origins,dpr):
    """Bind the declared desktop status caret and Chromium UA caret separately."""
    if 'native-status-caret' not in case.get('features',()):
        return None
    controls=case['fixture']['props']['controls']
    names=[name for name,control in controls.items() if control['component']=='Input' and control.get('status') in ('warning','error')]
    if len(names)!=1:
        raise ProtocolError('status caret requires exactly one declared status Input')
    name=names[0]
    actual=[side[name] for side in targets]
    rows=[[row for row in side if row['kind']=='caret' and name in row['source'].get('owner_targets',())]
          for side in inventories]
    focus=[entry['state']['focused'] for entry in actual]
    if focus==[False,False] and rows==[[],[]]:
        return None
    if focus!=[True,True] or any(len(side)!=1 for side in rows):
        raise ProtocolError('focused status Input has no unique observed caret on both sides')
    web,native=(side[0] for side in rows)
    border=[row for row in inventories[0] if row['kind']=='border' and name in row['source'].get('owner_targets',())]
    edge=web['caret_edge'];bounds=native['bounds']
    editor=relative(actual[1]['tokens']['editor_clip'],origins[1])
    native_rgba=(*native['color'],native['alpha']*native['opacity'])
    web_rgba=(*web['color'],web['alpha']*web['opacity'])
    color=lambda row:(*row['color'],row['alpha']*row['opacity'])
    same=lambda a,b:all(abs(x-y)<=1e-6 for x,y in zip(a,b,strict=True))
    if (not border or not any(same(native_rgba,color(row)) for row in border) or
        not same(web_rgba,_css_rgba(web['source']['style']['color'])) or native_rgba[3]!=1 or
        any(opacity!=1 for row in (web,native) for _,opacity in row['groups'])):
        raise ProtocolError('observed status caret colors do not satisfy their source contracts')
    width=max(1,math.floor(dpr+.5))
    if (abs(bounds['width']*dpr-width)>1e-3 or any(abs(bounds[key]-editor[key])*dpr>1e-3 for key in ('y','height')) or
        bounds['x']<editor['x']-1e-4 or bounds['x']+bounds['width']>editor['x']+editor['width']+1e-4 or
        abs(bounds['x']-edge['x'])*dpr>1 or
        abs(bounds['y']+bounds['height']/2-edge['y']-edge['height']/2)*dpr>1):
        raise ProtocolError('status caret does not track the observed collapsed selection edge')
    return web,native


def _layered_context(case,declaration,native,web,images,thresholds,origins,*,glyph_priors=None,physical_region=None,
                     shape_witnesses=None):
    from .color_metrics import rgb8_delta_e00,solve_visible_contributions,unmix_coverage,contribution_domain,in_contribution_domain,visible_segment
    from .regions import coverage_metrics,shadow_profile_metrics
    dpr=case['fixture']['viewport']['dpr'];sample=case['native']['sample_id']
    native_layers,native_stack=_native_paint_inventory(native,origins[1],dpr)
    web_layers,web_stack=_web_paint_inventory(web,origins[0],dpr)
    web_masks=glyph_priors and glyph_priors.get('web_masks')
    if web_masks:
        from .glyph_witness import place_browser
        for row in web_layers:
            if row['kind']=='glyph' and not row.get('formal_svg'):
                source,font_size,baseline,_=_web_glyph_source(row,glyph_priors)
                placements=place_browser(web_masks,source,font_size,baseline,dpr)
                row['outline_source']=source
                row['web_glyph_mask']=placements[0]
                row['web_glyph_alternates']=placements[1:]
                bounds=[dict(x=(x+mask.left)/dpr,y=(y+mask.top)/dpr,
                             width=mask.width/dpr,height=mask.height/dpr) for mask,x,y in placements]
                row['bounds']=reduce(union,bounds)
    if glyph_priors and glyph_priors.get('scenario_masks'):
        from .glyph_witness import mask_identity,scenario_origin
        _,masks=glyph_priors['scenario_masks']
        for row in native_layers:
            if row['kind']!='glyph':
                continue
            text,ink=row['glyph_source']
            if text.get('kind')=='icon-font':
                continue
            scalar=text['content'].encode()[ink['byte_begin']:ink['byte_end']].decode()
            source=ink['atlas_source']
            if len(scalar)==1 and not scalar.isspace() and (mask:=masks.get(mask_identity(scalar,source))) is not None:
                row['native_glyph_mask']=(mask,*scenario_origin(mask,source,row['bounds'],dpr))
    inventories=(web_layers,native_layers)
    blend_pixels={}
    if shape_witnesses is not None:
        shape_witnesses.bind([*web_layers,*native_layers],dpr)
        shape_witnesses.bind_shadow_groups(web_layers,dpr)
        blend_pixels=shape_witnesses.blend_pixels(inventories,dpr)
    nt={t['identity']:t for t in native['targets']};wt={t['identity']:t for t in web['targets']}
    caret=_status_input_caret(case,(wt,nt),inventories,origins,dpr)
    if caret:
        caret[1]['solid_status_caret']=True
    caret_rows={}
    context=next(r for r in declaration['rois'] if r['kind']=='sample-context')
    scope=union(relative(wt[sample]['bounds'],origins[0]),relative(nt[sample]['bounds'],origins[1]))
    for rows in inventories:
        for row in rows:scope=union(scope,_support(row))
    absolute=[dict(scope,x=scope['x']+origin['x'],y=scope['y']+origin['y']) for origin in origins]
    try:
        if physical_region is not None:
            crops,size,box=_crop_pair_box(images,physical_region,origins,dpr)
        else:
            crops,size,box=_crop_pair(images,*absolute,origins,dpr,int(context['guard_px']))
    except ProtocolError as error:
        raise RunError(4,'capture-coverage','full paint support/shadow/outline/popup plus guard is not captured; required relative support '+str(scope)) from error
    failures=_formal_paint_gates(native,web,native_stack,origins,box,dpr)
    failures.extend(_hard_paint_gates(native_layers,web_layers,declaration['step']))
    measurements=[];counts={k:0 for k in ('flat','edge','glyph','shadow','joint','guard')}
    def fail(category,message,**data):
        if len(failures)<64:
            # Numeric loops use tuples for physical coordinates and source sets.
            # The strict report wire format represents both as JSON arrays.
            details={key:list(value) if isinstance(value,tuple) else value for key,value in data.items()}
            failures.append(dict(category=category,step=declaration['step'],target=sample,message=message,**details))
    basis_cache={};domain_cache={};presence={};paired_planes={};joint_planes={};max_joint=0.;max_color=0.;glyph_witnesses={}
    svg_masks=(glyph_priors or {}).get('quarter_icon_masks')
    svg_placements={}
    bins=[_paint_bins(rows,box,dpr) for rows in inventories]
    for index,x,y,key_bin in _layered_pixels(crops,size,box,bins,counts):
        point=((x+.5)/dpr,(y+.5)/dpr)
        sides=[]
        domain_failures=[]
        for side,indexed in enumerate(bins):
            active=[];coverages=[];variable=[]
            for row,support in indexed.get(key_bin,()):
                if not _contains(support,point):continue
                value,transition=_layer_coverage(row,point,dpr)
                if value==0 and not transition:continue
                active.append(row);coverages.append(value)
                if transition:variable.append(len(active)-1)
            # A white interface over provably white lower content has no visible
            # degree of freedom. Keep its source identity, geometry and order;
            # do not let it create a fictitious second coverage unknown.
            variable=[position for position in variable if not (active[position]['color']==(1.,1.,1.) and
                all(lower['color']==(1.,1.,1.) or coverages[i]==0 and i not in variable
                    for i,lower in enumerate(active[:position])))]
            variable=[position for position in variable if not any(i not in variable and coverages[i]==1 and
                active[i]['alpha']*active[i]['opacity']==1 and active[i]['groups']==active[position]['groups']
                for i in range(position+1,len(active)))]
            if not variable and shape_witnesses is not None:
                # Correct known material predictions only. Variable interfaces
                # continue through the same independently calibrated inverse.
                coverages=[locked_edge_value(row,point,dpr,value) for row,value in zip(active,coverages)]
            predicted=_composite_point(active,coverages)
            observed=tuple(crops[side][index*4:index*4+3])
            if not variable:
                error=rgb8_delta_e00(observed,tuple(round(v*255) for v in predicted));max_color=max(max_color,error)
                if error>thresholds['color.flat_delta_e00']:fail('visual','independent flat/material/guard color differs',side=side,pixel=point,actual=error)
                sides.append(dict(active=active,variable=variable,kind='flat' if active else 'guard',values=()))
                continue
            # The basis is chosen solely from retained/DOM source colors, never
            # fitted to either observed pixel. Full layer alpha is not recovered.
            key=tuple((r['identity'],r['color']) for r in active)
            basis_key=(key,tuple(variable),tuple(0 if i in variable else c for i,c in enumerate(coverages)))
            if basis_key not in basis_cache:
                basis_cache[basis_key]=_bounded_paint_basis(active,coverages,variable)
            basis=basis_cache[basis_key]
            if basis['mode']=='flat':
                error=rgb8_delta_e00(observed,tuple(round(v*255) for v in predicted))
                if error>thresholds['color.flat_delta_e00']:fail('visual','source-identical flat paint differs',side=side,pixel=point,actual=error)
                sides.append(dict(active=active,coverages=coverages,variable=[],kind='flat',values=()))
                continue
            valid,values=_paint_coordinates(observed,basis)
            if not valid:fail('typography' if any(r['kind']=='glyph' for r in active) else 'visual',
                                          'known source pixel violates visible color domain',side=side,pixel=point)
            # Joint same-color opacity/order bounds use actual finite stack
            # extrema. Enumerating endpoint coverages creates no pixel-fitted
            # alpha, palette, registration, or tolerance.
            if len(variable)>8:raise RunError(3,'calibration','more than eight simultaneous finite coverage interfaces')
            coordinates=lambda solution:tuple(solution['contributions']) if solution['contributions'] is not None else (solution['joint_contrast'],)
            if basis['mode']=='visible-contribution':
                domain_key=(key,tuple(variable),tuple(0 if i in variable else c for i,c in enumerate(coverages)))
                if domain_key not in domain_cache:
                    endpoints=tuple(_paint_endpoints(active,coverages,variable))
                    vertices=[coordinates(solve_visible_contributions(endpoint,basis)) for endpoint in endpoints]
                    domain_cache[domain_key]=(contribution_domain(vertices),_hard_shadow_box_domain(active,endpoints))
                domain,rounded_box=domain_cache[domain_key]
                if rounded_box is None:
                    inside=in_contribution_domain(values,domain,basis['quantization_bounds'])
                else:
                    # Shadow, fill and border each write an RGB8 result. A later
                    # source-over cannot amplify the preceding half-code error.
                    # Keep the accumulated bound in the physical RGB8 domain.
                    rounding=len(active)*.5/255
                    inside=in_contribution_domain(tuple(v/255 for v in observed),rounded_box,(rounding,)*3)
                if not inside:
                    domain_failures.append(side)
            if basis['mode']=='joint-collinear':
                domain_key=('collinear',key,tuple(variable),tuple(0 if i in variable else c for i,c in enumerate(coverages)))
                if domain_key not in domain_cache:
                    endpoints=[solve_visible_contributions(endpoint,basis)['joint_contrast']
                               for endpoint in _paint_endpoints(active,coverages,variable)]
                    domain_cache[domain_key]=(min(endpoints),max(endpoints))
                low,high=domain_cache[domain_key]
                eps=solve_visible_contributions(tuple(v/255 for v in observed),basis)['quantization_bound']
                if not low-eps<=values[0]<=high+eps:fail('visual','joint color violates actual source opacity/order extrema',side=side,pixel=point)
            kinds={active[i]['kind'] for i in variable}
            kind='joint' if len(variable)>1 else ('glyph' if kinds & {'glyph','caret'} else 'shadow' if kinds & {'shadow','inset-shadow'} else 'edge')
            if kinds <= {'shadow','inset-shadow'} and any(active[i].get('blur',0)>0 for i in variable):
                # Gaussian source geometry is independently fixed, and can
                # expose a missing same-color shadow even when its alpha cannot
                # be individually unmixed from the final RGB.
                if (not _matches_soft_shadow_rgb(active,coverages,variable,observed,predicted) and
                        not _matches_isolated_shadow_rgb(active,coverages,variable,observed,x,y)):
                    fail('visual','independent soft-shadow profile/tail differs',side=side,pixel=point)
            if len(variable)==1:
                position=variable[0];row=active[position]
                low=coverages.copy();high=coverages.copy();low[position]=0.;high[position]=1.
                lower=_composite_point(active,low);upper=_composite_point(active,high)
                uncertainty=0.
                try:
                    shadow=row['kind'] in ('shadow','inset-shadow')
                    if shadow:
                        solved=visible_segment(tuple(v/255 for v in observed),lower,upper,_visible_weight(active,coverages,position))
                        if not solved['valid']:raise ProtocolError('known visible alpha pixel differs')
                        value=solved['contribution']
                    else:
                        unmixed=unmix_coverage(tuple(v/255 for v in observed),upper,lower,1.,byte_error=1/255)
                        value=unmixed['coverage']
                        uncertainty=unmixed['quantization_bound']
                except ProtocolError:
                    fail('typography' if row['kind'] in ('glyph','caret') else 'visual','known local material pixel differs',side=side,pixel=point)
                    value=None
                if value is not None:
                    # Quantization-safe integration only; invalid model pixels
                    # already failed and are never clamped into a valid result.
                    value=min(1.,max(0.,value))
                    presence[(side,row['identity'])]=presence.get((side,row['identity']),0)+value
                    if caret and side==0 and row is caret[0]:
                        caret_rows[y]=caret_rows.get(y,0)+value+uncertainty
            if side==1:
                for position in variable:
                    row=active[position]
                    if row['kind']!='glyph' or any(active[i]['kind'] in ('glyph','caret') and
                        not _same_glyph_interface(row,active[i]) for i in variable if i!=position):continue
                    if any(coverages[i]>0 and active[i]['alpha']*active[i]['opacity']>=1 for i in range(position+1,len(active))):continue
                    if glyph_priors is None:raise RunError(3,'calibration','independent actual SFNT outline priors unavailable')
                    if _isolated_opacity_interface(active,coverages,variable):
                        # Applying the group's alpha to a white glyph changes
                        # its visible contrast against the composed backing.
                        below,upper=_paint_endpoints(active,coverages,variable)
                    else:
                        below=_composite_point(active[:position],coverages[:position])
                        if _two_translucent_black_writes(active,coverages,variable):
                            below=tuple(round(channel*255)/255 for channel in below)
                        upper=None
                    witness=_glyph_witness_gate(row,point,dpr,glyph_priors,below,observed,upper=upper)
                    if witness is not None:
                        glyph_witnesses[row['identity']]=glyph_witnesses.get(row['identity'],0)+1
                        if not witness:fail('typography','independent SFNT interior glyph witness has no ink',pixel=point,source=row['identity'])
            sides.append(dict(active=active,coverages=coverages,variable=variable,kind=kind,values=tuple(values),basis=basis))
        classification=sides[0]['kind'];counts[classification]+=1
        if caret and any(any(row is marker for row in sides[side]['active']) for side,marker in enumerate(caret)):
            # These source-specific pixels already went through independent
            # material/opacity checks. Retain the ordinary comparison everywhere
            # outside those two observed caret footprints.
            continue
        # Every actual context pixel is compared, including overlaps and tails.
        # No full-context byte equality veto is used for calibrated AA.
        a,b=[tuple(data[index*4:index*4+3]) for data in crops]
        # The same source stack can have two exact RGB8 encodings. Both actual
        # pixels must independently match their encoding before pairing them.
        encoded=verified_blend(blend_pixels,x,y,dpr,sides,(a,b)) if blend_pixels else None
        if encoded is not None:
            a=b=encoded
        else:
            # The continuous inverse can reject a boundary RGB8 value. A
            # verified pair already proves the exact encoded source order.
            for side in domain_failures:
                fail('visual','visible contributions violate actual source-over order/opacity domain',side=side,pixel=point)
        if svg_masks and (same:=_witnessed_svg_rounding(
                sides,(a,b),svg_masks,point,dpr,svg_placements,
                tinted_framebuffer=glyph_priors.get('svg_tinted_framebuffer',False))) is not None:
            a=b=same
        compared_coverage=False
        if not sides[0]['variable'] and not sides[1]['variable']:
            error=rgb8_delta_e00(a,b)
            if error>thresholds['color.flat_delta_e00']:fail('visual','paired flat/context/guard color differs',pixel=point,actual=error)
        if len(sides[0]['variable'])==1:
            reference=sides[0];position=reference['variable'][0];row=reference['active'][position]
            low=reference['coverages'].copy();high=reference['coverages'].copy();low[position]=0.;high[position]=1.
            lower=_composite_point(reference['active'],low);upper=_composite_point(reference['active'],high)
            values=[]
            reference_uncertainty=None
            shadow=row['kind'] in ('shadow','inset-shadow')
            maximum=_visible_weight(reference['active'],reference['coverages'],position) if shadow else 1.
            common_ink=(_witnessed_glyph_rounding(sides,sides[0]['basis'],(a,b),glyph_priors,point,dpr)
                        if a!=b and row['kind']=='glyph' and sides[1]['variable'] else None)
            paint_pixels=(common_ink,common_ink) if common_ink is not None else (a,b)
            for side,rgb in enumerate(paint_pixels):
                try:
                    if shadow:
                        solved=visible_segment(tuple(v/255 for v in rgb),lower,upper,maximum)
                        value=solved['contribution'] if solved['valid'] else None
                    else:
                        unmixed=unmix_coverage(tuple(v/255 for v in rgb),upper,lower,1.,byte_error=1/255)
                        value=unmixed['coverage']
                        if side==0:reference_uncertainty=unmixed['quantization_bound']
                except ProtocolError:value=None
                values.append(None if value is None else min(1.,max(0.,value)))
            if all(v is not None for v in values):
                compared_coverage=True
                if row['identity'] not in paired_planes:paired_planes[row['identity']]=(row,{},{},{})
                _,first,second,uncertainty=paired_planes[row['identity']]
                first[index],second[index]=values
                if reference_uncertainty is not None:uncertainty[index]=reference_uncertainty
                if row['kind'] in ('glyph','caret') and min(values)>=.98:
                    error=rgb8_delta_e00(*paint_pixels)
                    if error>thresholds['color.text_interior_delta_e00']:fail('typography','high contrast glyph/caret interior color differs',pixel=point,actual=error)
        if sides[0]['variable'] or sides[1]['variable']:
            if not sides[0]['variable'] or not sides[1]['variable']:
                # A source geometry disagreement is already independently known.
                # Keep comparing actual RGB rather than turning it into code 3.
                if not compared_coverage:
                    value=rgb8_delta_e00(a,b)
                    if value>thresholds['color.flat_delta_e00']:fail('visual','actual paint support/transition differs',pixel=point,actual=value)
            else:
                # One reference basis for both pixels; material differences are
                # output differences, never a choice of a favorable native basis.
                basis=sides[0]['basis'];solutions=[_paint_coordinates(rgb,basis) for rgb in (a,b)]
                if not all(valid for valid,_ in solutions):fail('visual','known paired pixel is outside fixed reference visible basis',pixel=point)
                else:
                    coordinates=[values for _,values in solutions]
                    error=max((abs(x-y) for x,y in zip(*coordinates)),default=0);max_joint=max(max_joint,error)
                    cap=thresholds.get('overlap.contribution_max_error',thresholds['pixels.coverage_local_relative_error'])
                    if classification=='joint':
                        if error>cap:fail('visual','joint visible contribution differs',pixel=point,actual=error,threshold=cap)
                        group=tuple(row['identity'] for row in sides[0]['active'])
                        common_ink=_witnessed_glyph_rounding(sides,basis,(a,b),glyph_priors,point,dpr) if a!=b else None
                        shared=(_paint_coordinates(common_ink,basis)[1] if common_ink is not None else None)
                        for axis,(first,second) in enumerate(zip(*coordinates)):
                            key=(group,axis)
                            if key not in joint_planes:joint_planes[key]=({}, {})
                            joint_planes[key][0][index]=min(1.,max(0.,shared[axis] if shared else first))
                            joint_planes[key][1][index]=min(1.,max(0.,shared[axis] if shared else second))
    if sum(counts.values())!=size[0]*size[1]:raise ProtocolError('paint strata did not account for the entire crop exactly once')
    if caret:
        edge=caret[0]['caret_edge']
        core=range(math.ceil(edge['y']*dpr+.5),math.ceil((edge['y']+edge['height'])*dpr-1.5))
        if not core or any(caret_rows.get(row,0)<.5 for row in core):
            fail('typography','browser caret has a missing exposed physical stroke row')
        measurements.append(dict(target=sample,category='native-status-caret',physical_rows=len(core),
                                 native_source=caret[1]['identity'],web_source=caret[0]['identity']))
    for identity,(row,reference,native_plane,uncertainty) in paired_planes.items():
        if not reference:continue
        coordinates=[(i%size[0],i//size[0]) for i in reference]
        left=min(x for x,y in coordinates);right=max(x for x,y in coordinates)+1
        top=min(y for x,y in coordinates);bottom=max(y for x,y in coordinates)+1
        local_size=(right-left,bottom-top)
        local=[[],[]];tail=[];local_uncertainty=[]
        shadow=row['kind'] in ('shadow','inset-shadow')
        for y in range(top,bottom):
            for x in range(left,right):
                index=y*size[0]+x;local_index=(y-top)*local_size[0]+x-left
                for values,plane in zip(local,(reference,native_plane)):values.append(plane.get(index,0.))
                if uncertainty:local_uncertainty.append(uncertainty.get(index,0.))
                point=((box[0]+x+.5)/dpr,(box[1]+y+.5)/dpr)
                if shadow and _shadow_tail(row,point,dpr):tail.append(local_index)
        prefix='shadow' if shadow else 'text' if row['kind'] in ('glyph','caret') else 'pixels'
        thin_ink = row.get('formal_svg') is True
        result=shadow_profile_metrics(*local,local_size,tail_indices=tail) if shadow else coverage_metrics(
            *local,local_size,thin_ink=thin_ink)
        if prefix=='text':
            from .regions import reference_aa_is_bounded
            if row.get('web_glyph_mask') is not None:
                # Only physical pixels with an independently reconstructed
                # nonzero raster sample could enter this coverage plane.
                bounded=True
            elif row['kind']=='glyph' and not thin_ink and glyph_priors and 'web_fonts' in glyph_priors:
                bounded=_web_glyph_support(row,glyph_priors,dpr,local[0],local_uncertainty,local_size,
                                           (box[0]+left,box[1]+top))
            else:
                bounded=reference_aa_is_bounded(local[0],local_size,uncertainty=local_uncertainty,thin_ink=thin_ink)
            if not bounded:
                fail('typography','reference glyph transition exceeds the fixed one physical pixel AA bound',source=identity)
        if not result['required_ink_present']:fail('typography' if prefix=='text' else 'visual','required independently exposed paint ink missing',source=identity)
        fields=('coverage_mass_relative_error','projection_relative_error','profile_max_error','tail_max_error') if shadow else (
            'coverage_mass_relative_error','contour_distance_px','projection_relative_error' if prefix=='text' else 'coverage_local_relative_error')
        for field in fields:
            key=prefix+'.'+field
            if key not in thresholds:raise RunError(3,'calibration','actual stratum lacks independent frozen metric: '+key)
            if result[field] is None or result[field]>thresholds[key]:fail('typography' if prefix=='text' else 'visual','local paint contribution profile failed',source=identity,metric=key,actual=result[field],threshold=thresholds[key])
        measurements.append(dict(target=identity,category=prefix,metrics=result,exposed_pixels=len(reference)))
    for row in native_layers:
        if (row['kind']=='glyph' and row['identity'] not in glyph_witnesses and
                _glyph_in_sampled_region(row,box,dpr)):
            # Thin glyphs may have no >1px core; they still require exposed local
            # coverage, projections and the strict actual atlas/SFNT source gate.
            joint_witness=any(_same_glyph_interface(row,other) and other['identity'] in glyph_witnesses for other in native_layers)
            if joint_witness:
                measurements.append(dict(target=row['identity'],category='typography',pixel_evidence='joint shared SFNT outline; this layer identity/order independently checked'))
            elif presence.get((1,row['identity']),0)<=0:
                fail('typography','visible glyph has no independent exposed ink witness',source=row['identity'])
    for (sources,axis),(first,second) in joint_planes.items():
        if not any(first.values()):continue
        xs=[i%size[0] for i in first];ys=[i//size[0] for i in first]
        left,right,top,bottom=min(xs),max(xs)+1,min(ys),max(ys)+1
        local_size=(right-left,bottom-top)
        planes=[[plane.get(y*size[0]+x,0.) for y in range(top,bottom) for x in range(left,right)] for plane in (first,second)]
        result=shadow_profile_metrics(*planes,local_size)
        for field,cap in (('profile_max_error',thresholds.get('overlap.contribution_max_error',thresholds['pixels.coverage_local_relative_error'])),
                          ('coverage_mass_relative_error',thresholds['pixels.coverage_mass_relative_error']),
                          ('projection_relative_error',thresholds['pixels.coverage_local_relative_error'])):
            if result[field] is None or result[field]>cap:fail('visual','joint local visible-contribution plane failed',sources=sources,axis=axis,metric=field,actual=result[field],threshold=cap)
        measurements.append(dict(target=sample,category='joint',sources=sources,axis=axis,metrics=result))
    measurements.append(dict(target=sample,category='paint-strata',box=box,size=size,classified_pixels=size[0]*size[1],counts=counts,
                             maximum_joint_error=max_joint,maximum_flat_delta_e00=max_color,
                             native_snapshot=native_stack.get('snapshot_digest'),web_snapshot=web_stack.get('snapshot_digest')))
    measurements.append(dict(target=sample,category='raw-byte-diagnostic',metrics=raster_metrics(*crops,size,(255,255,255,255))))
    return failures,measurements


def _clip_intersection(a,b):
    x=max(a['x'],b['x']);y=max(a['y'],b['y'])
    width=min(a['x']+a['width'],b['x']+b['width'])-x
    height=min(a['y']+a['height'],b['y']+b['height'])-y
    if width<=0 or height<=0:return dict(x=0,y=0,width=0,height=0)
    return dict(x=x,y=y,width=width,height=height)


def _tiled_target_clips(web,native,origins,camera):
    # Primary checkpoint metadata can come from independent scroll cameras.
    # Compare the target's actual ancestor clip within their common aperture.
    # The verified tile partition covers all remaining occupied rows and guards.
    targets=(web,native)
    scope=_clip_intersection(camera,union(*(relative(row['bounds'],origin)
                                           for row,origin in zip(targets,origins,strict=True))))
    return tuple(_clip_intersection(scope,relative(row['clip'],origin))
                 for row,origin in zip(targets,origins,strict=True))


def compare_checkpoint(case, declaration, native, web, images, thresholds, color,*,glyph_priors=None,context_tiles=(),
                       shape_witnesses=None):
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
    origins = (sample_anchor(wt[name]), sample_anchor(nt[name]))
    camera = (_clip_intersection(relative(wt[name]['clip'],origins[0]),relative(nt[name]['clip'],origins[1]))
              if context_tiles else None)
    dpr = case['fixture']['viewport']['dpr']
    for side, target in (('web',wt[name]),('native',nt[name])):
        anchor = target['tokens']['sample_anchor']
        declared = case['fixture']['container']
        if abs(anchor['container']['width']-declared['width']) > thresholds['geometry.logical_px']:
            fail('layout',name,'fixed container width differs from independent fixture',side=side)
        if any(abs(v-declared['padding']) > thresholds['geometry.logical_px'] for v in anchor['padding'].values()):
            fail('layout',name,'fixed container padding differs from independent fixture',side=side)
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
        a_clip,b_clip=(_tiled_target_clips(a,b,origins,camera) if camera is not None else
                       (clipped(a['clip'],origins[0]),clipped(b['clip'],origins[1])))
        if any(abs(a_clip[k] - b_clip[k]) >
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
    layered='transport.max_channel_residual' in thresholds
    def raster(target, a, b, guard, category, text=False, local_background=None):
        if layered:
            return
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
            fail(category, target, 'required local foreground/ink missing', roi=list(box))
        for field in ('max_channel_residual', 'coverage_mass_relative_error', 'contour_distance_px'):
            prefix = 'text' if text and field != 'max_channel_residual' else 'pixels'
            threshold = thresholds[prefix + '.' + field]
            value = result[field]
            if value is None or value > threshold:
                fail(category, target, 'local raster metric failed', roi=list(box), metric=prefix + '.' + field,
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
                if layered:
                    web_size=_css_number(aw['font_size'])
                    web_weight=int(aw['font_weight'])
                    weights={(f['sha256'],f['face_index']):f['weight'] for f in case['fixture']['fonts']}
                    if any(run['font']['logical_pixel_size']!=web_size or
                           weights.get((run['font']['sha256'],run['font']['face_index']))!=web_weight for run in bn['font_runs']):
                        fail('typography',target,'actual font face weight/size differs between paired semantic runs')
                ag = text_groups(aw, 'web')
                bg = text_groups(bn, 'native', allow_native_cull=layered)
                if [(g['byte_begin'],g['byte_end'],g['content']) for g in ag] != [
                        (g['byte_begin'],g['byte_end'],g['content']) for g in bg]:
                    raise RunError(3,'calibration','actual glyph/grapheme groups cannot be paired without guessing')
                for ga,gb in zip(ag,bg):
                    advance_cap=thresholds.get('text.geometry_logical_px',.5)
                    error=_text_advance_error(ga,gb,layered=layered)
                    if error>advance_cap:
                        fail('typography',target,'semantic group advance differs',metric='text.geometry_logical_px',
                             actual=error,raw=abs(ga['advance']-gb['advance']),threshold=advance_cap)
                    if layered or ga['content'].isspace():
                        continue
                    def group_rect(group):
                        value=group['boxes'][0]
                        for box in group['boxes'][1:]:
                            value=union(value,box)
                        return value
                    # Presence uses the common semantic interior, without guards.
                    # Full union+guard is separately compared by raster().
                    ar,br=group_rect(ga),group_rect(gb)
                    local_background=_local_background(a,background)
                    for (data,size),box in zip(images,(ar,br)):
                        l,t,r,bottom=physical_box(box,dpr,2)
                        raw=crop_rgba(data,size,(l+2,t+2,r-2,bottom-2))
                        presence=raster_metrics(raw,raw,(r-l-4,bottom-t-4),local_background)
                        if not presence['required_ink_present']:
                            fail('typography',target+'.text','required semantic glyph ink missing')
                    raster(target + '.text', ar, br, 2, 'typography', text=True,
                           local_background=local_background)
        if layered:
            continue  # Formal identities and visible membership are checked per captured aperture.
        if len(a['icons']) != len(b['icons']):
            fail('visual', target, 'required icon identity/count differs')
        for aw, bn in zip(a['icons'], b['icons']):
            if not aw.get('identity') or not bn.get('identity'):
                raise ProtocolError('icon source identity is unavailable')
            if aw['identity'].replace('-', '').lower() != re.sub(r'(Outlined|Filled|TwoTone)$','',bn['identity']).lower():
                fail('visual', target, 'required icon identity differs')
            raster(target + '.icon', aw['bounds'], bn['bounds'], 2, 'visual',
                   local_background=_local_background(a, background))
    if layered:
        if context_tiles:
            for tile_index,(n,w,frames,tile_origins,region) in enumerate(context_tiles):
                errors,observed=_layered_context(case,declaration,n,w,frames,thresholds,tile_origins,
                                                glyph_priors=glyph_priors,physical_region=region,
                                                shape_witnesses=shape_witnesses)
                failures.extend(dict(error,tile=tile_index) for error in errors)
                measurements.extend(dict(value,tile=tile_index) for value in observed)
        else:
            errors,layer_measurements=_layered_context(case,declaration,native,web,images,thresholds,origins,
                                                       glyph_priors=glyph_priors,shape_witnesses=shape_witnesses)
            failures.extend(errors);measurements.extend(layer_measurements)
    return failures, measurements


def _read_pair(native,web,directories,image_module,color):
    from .calibration import png_metadata
    captures=(web['captures'][0],native['captures'][0])
    for side,capture in zip(('web','native'),captures):
        descriptor=color[side]
        if capture['channel_order']!='RGBA' or capture['color_profile']!=descriptor['encoding']:
            raise RunError(3,'calibration','capture original channel/encoding differs from independent controls')
        if side=='native' and (capture['alpha']!='premultiplied' or capture['framebuffer_format']!=descriptor['framebuffer_format']):
            raise RunError(3,'calibration','native original storage differs from independent controls')
        if side=='web' and capture['alpha'] not in ('straight','opaque'):
            raise RunError(3,'calibration','Web original PNG alpha differs')
    images=[decoded(image_module,artifact(directories[side],capture['path']),capture)
            for side,capture in zip(('web','native'),captures)]
    if any(any(a!=255 for a in pixels[3::4]) for pixels,_ in images):
        raise RunError(3,'calibration','pilot decoded capture is not all opaque')
    raw=native['resources'].get('readback',{})
    width,height=images[1][1]
    if (raw.get('raw_alpha_available') is not True or raw.get('raw_alpha_min')!=255 or raw.get('raw_alpha_max')!=255 or
            raw.get('raw_opaque_pixels')!=width*height or raw.get('raw_total_pixels')!=width*height or
            raw.get('clear_rgba')!=color['native']['clear_rgba'] or
            raw.get('source_channel_order')!=color['native']['source_channel_order']):
        raise RunError(3,'calibration','native opaque working domain lacks actual raw alpha/clear/channel proof')
    fingerprint=14695981039346656037
    for byte in images[1][0]:
        fingerprint=((fingerprint^byte)*1099511628211)&((1<<64)-1)
    if raw.get('raw_rgba_fnv1a64')!=str(fingerprint):
        raise RunError(3,'calibration','native opaque BMP differs from original owned raw RGBA')
    png=png_metadata(artifact(directories['web'],captures[0]['path']))
    if png['format'] not in color['web']['file_formats'] or png['color_chunks']!=color['web']['png_color_chunks']:
        raise RunError(3,'calibration','Web PNG format differs from the independent controls')
    return images


def _context_tile_pairs(native,web,sides,context,directories,image_module,color,dpr,required):
    if not any(sides):
        return []
    if len(sides[0])!=len(sides[1]) or not 1<=len(sides[0])<=8:
        raise RunError(4,'capture-coverage','actual context tile identities are incomplete')
    frames=[]
    apertures=[]
    for wt,nt in zip(*sides,strict=True):
        def observed(parent,tile):
            return dict(parent,targets=[tile['target']],captures=[tile['capture']],resources=tile)
        w=observed(web,wt)
        n=observed(native,nt)
        images=_read_pair(n,w,directories,image_module,color)
        origins=tuple(sample_anchor(tile['target']) for tile in (wt,nt))
        apertures.append(tuple(capture_aperture(origin,tile['target']['clip'],image[1],dpr)
                               for origin,tile,image in zip(origins,(wt,nt),images,strict=True)))
        frames.append((n,w,images,origins))
    try:
        slices=vertical_capture_slices(required,apertures,int(context['guard_px']))
    except ProtocolError as error:
        raise RunError(4,'capture-coverage',str(error)) from error
    return [(*frames[index],region) for index,region in slices]


def _is_window_paint(row):
    source=row['source']
    if row['side']=='native':
        return source.get('window_layer') is True
    return any(name.endswith('.popup') for name in source.get('owner_targets',[]))


def compare(case, native, web, directories, profile, image_module, *, profile_proof=None):
    expected = binding(case, native, web)
    thresholds = profile_thresholds(profile, expected, case['profile'],image_module=image_module,proof=profile_proof,
                                    native_binary_sha256=native['hello']['provenance']['binary_sha256'])
    from .calibration import evidence_manifest
    manifest=profile_proof.manifest() if profile_proof is not None else evidence_manifest(profile)[0]
    glyph_priors=_glyph_priors_for_run(native,manifest,independent_masks=True,web=web)
    shape_witnesses=(ShapeWitnesses(manifest,ROOT)
                     if manifest['environment']['native_capture'].get('shape_rasterizer') == 'skia-rounded-coverage'
                     else None)
    scope=profile['scope']
    fixture=case['fixture']
    if (case['identity'] not in scope['cases'] or fixture['locale']!=scope['locale'] or fixture['direction']!=scope['direction'] or
            fixture['motion']!=scope['motion'] or fixture['font_policy']!=scope['font_policy'] or
            fixture['theme'].get('algorithm')!=scope['theme'] or fixture['viewport']['dpr']!=scope['actual_dpr']):
        raise RunError(3,'calibration','selected case is outside the finite independent pilot scope')
    allowed={(f['size'],f['weight']) for f in scope['font_sizes_weights']}
    weights={(f['sha256'],f['face_index']):f['weight'] for f in fixture['fonts']}
    for checkpoint in native['checkpoints'].values():
        for target in checkpoint['targets']:
            for text in _text_rows(target,'native'):
                for run in text['font_runs']:
                    font=run['font']
                    if (font['logical_pixel_size'],weights.get((font['sha256'],font['face_index']))) not in allowed:
                        raise RunError(3,'calibration','observed font size/weight has no independent pilot control')
    color=profile['color']
    if digest(web['hello']['provenance']['browser']['launch_args'])!=color['web']['launch_args_hash']:
        raise RunError(3,'calibration','Web actual sRGB launch parameters differ')
    failures, measurements = [], {}
    for checkpoint_index, declaration in enumerate(case['checkpoints']):
        step = declaration['step']
        n, w = native['checkpoints'][step], web['checkpoints'][step]
        images=_read_pair(n,w,directories,image_module,color)
        context = next(r for r in declaration['rois'] if r['kind'] == 'sample-context')
        at=next(t for t in w['targets'] if t['identity']==context['target'])
        bt=next(t for t in n['targets'] if t['identity']==context['target'])
        origins=(sample_anchor(at),sample_anchor(bt))
        dpr=fixture['viewport']['dpr']
        guard=int(context['guard_px'])
        required=union(relative(at['bounds'],origins[0]),relative(bt['bounds'],origins[1]))
        if required['height']>fixture['viewport']['height']*.75:
            # Document tiles have a document clip. A window-layer popup can
            # paint beyond that clip and moves with its trigger during a sweep.
            # Its complete paint and guard therefore come from the primary pair.
            web_rows,_=_web_paint_inventory(w,origins[0],dpr)
            native_rows,_=_native_paint_inventory(n,origins[1],dpr)
            overlay=[]
            for row in (*web_rows,*native_rows):
                if _is_window_paint(row):
                    overlay.append(_support(row))
                else:
                    required=union(required,_support(row))
            sides=(web['context_tiles'].get(step,[]),native['context_tiles'].get(step,[]))
            regions=_context_tile_pairs(n,w,sides,context,directories,image_module,color,dpr,physical_box(required,dpr,guard))
            if not regions:
                raise RunError(4,'capture-coverage','long context has no actual viewport tiles')
            by_target=({t['identity']:t for t in w['targets']},{t['identity']:t for t in n['targets']})
            for roi in declaration['rois']:
                if roi['kind']!='popup':
                    continue
                identity=roi['target']
                overlay.extend(relative(targets[identity]['bounds'],origin)
                               for targets,origin in zip(by_target,origins,strict=True))
                guard=max(guard,int(roi['guard_px']))
            if overlay:
                regions.append((n,w,images,origins,physical_box(reduce(union,overlay),dpr,guard)))
        else:
            regions=[]
        destination = Path(directories['native']).parent / 'comparison' / str(checkpoint_index)
        destination.mkdir(parents=True)
        try:
            if regions:
                crops_to_write=[(f'tile-{index}-diff.png',_crop_pair_box(frames,box,tile_origins,dpr))
                                for index,(_,_,frames,tile_origins,box) in enumerate(regions)]
            else:
                crops_to_write=[('diff.png',_crop_pair(images,at['bounds'],bt['bounds'],origins,dpr,guard))]
        except ProtocolError as error:
            raise RunError(4,'capture-coverage','full context or window-layer popup plus guard was not captured') from error
        for filename,(crops,size,_) in crops_to_write:
            diff=bytes(abs(x-y) if index%4!=3 else 255 for index,(x,y) in enumerate(zip(*crops)))
            image_module.frombytes('RGBA',size,diff).save(destination/filename)
        errors, measured = compare_checkpoint(case, declaration, n, w, images, thresholds, color,
                                              glyph_priors=glyph_priors,context_tiles=regions,
                                              shape_witnesses=shape_witnesses)
        failures.extend(errors)
        measurements[step] = measured
    return failures, {'bindings': expected, 'thresholds': thresholds, 'checkpoints': measurements}
