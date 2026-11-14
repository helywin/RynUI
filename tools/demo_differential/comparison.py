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
from pathlib import Path

from .adapters import RunError
from .oracle import exact
from .protocol import ProtocolError, validate
from .regions import crop_rgba, geometry, physical_box, raster_metrics, rect, relative, union, sample_anchor, text_groups
from .runner_evidence import artifact, metrics
from .calibration import CAPS

LEGACY_DIAGNOSTIC_CAPS = {'geometry.logical_px': .25, 'pixels.max_channel_residual': 0,
        'pixels.coverage_mass_relative_error': .05, 'pixels.contour_distance_px': .5,
        'text.coverage_mass_relative_error': .05, 'text.contour_distance_px': 1}


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False).encode()).hexdigest()


def algorithm_hash():
    from .protocol import FIXTURES
    files={name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
           for name in ('comparison.py', 'regions.py', 'color_metrics.py','calibration.py','schema_definitions.py')}
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
    return {**required(capture,('framebuffer_format','channel_order','alpha','color_profile'),'native original storage'),
            'source_channel_order':raw['source_channel_order'],'clear_rgba':raw['clear_rgba'],
            'font_raster_policies':[policies[key] for key in sorted(policies)]}


def environment_description(case,native,web,actual_metrics,capture_facts):
    from .calibration import required
    from .device_metrics import stable_device_metrics
    native_fields=('preset','configuration','toolchain','platform','backend','shader','gpu','font_policy')
    n=required(native,native_fields,'native environment')
    required(n['gpu'],('name','driver_name','driver_version','driver_info'),'actual native GPU')
    required(web,('browser','environment'),'Web environment')
    required(web['browser'],('version','executable_sha256','revision','launch_args','actual_gpu','os','release','arch'),'actual browser')
    libraries=required(web['environment'],('browser_libraries','browser_sha256','node_sha256'),'browser library/tool closure')
    os_fields={'system':platform.system(),'release':platform.release(),'version':platform.version(),'machine':platform.machine()}
    required(os_fields,tuple(os_fields),'OS/build')
    return {'native':n,'native_capture':capture_facts,'metrics':stable_device_metrics(actual_metrics),'web':web['browser'],'libraries':libraries,
            'os':os_fields,'fonts':case['fixture']['fonts']}


def profile_thresholds(profile, expected, identity, *, image_module=None,native_binary_sha256=None):
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


def _shape_distance(point,bounds,radii):
    from .calibration import rounded_distance
    radius=radii[(0 if point[1]<bounds['y']+bounds['height']/2 else 3) if point[0]<bounds['x']+bounds['width']/2
                  else (1 if point[1]<bounds['y']+bounds['height']/2 else 2)] if isinstance(radii,(tuple,list)) else radii
    return rounded_distance(*point,bounds,radius)


def _layer_coverage(layer,point,dpr):
    """Independent source geometry; transitions are exactly <=1 physical pixel."""
    from .calibration import gaussian_rounded_shadow
    if any(not _contains(clip,point) for clip in layer.get('clips',[])):return 0,False
    b=layer['bounds'];kind=layer['kind'];r=layer.get('radii',0)
    distance=_shape_distance(point,b,r)
    if kind in ('glyph','caret'):
        return (1 if _contains(b,point) else 0),_contains(b,point)
    if kind in ('shadow','inset-shadow'):
        if kind=='inset-shadow' and distance>0:return 0,abs(distance)*dpr<=1
        if kind=='shadow' and layer.get('knockout') and distance*dpr < -1:return 0,False
        straight=(b['x']+r<=point[0]<=b['x']+b['width']-r or b['y']+r<=point[1]<=b['y']+b['height']-r)
        if kind=='shadow' and layer.get('knockout') and straight and distance*dpr<=-.5:return 0,False
        spread=layer['spread'] if kind=='shadow' else -layer['spread']
        shape=_inset(b,-spread);radius=max(0,r+spread) if r>0 else 0
        if shape['width']<=0 or shape['height']<=0:return (0 if kind=='shadow' else float(distance<=0)),False
        if layer['blur']>0:
            shadow={'offset':layer['offset'],'blur':layer['blur'],'spread':0}
            value=gaussian_rounded_shadow(*point,shape,shadow,radius)
        else:
            shift=dict(shape,x=shape['x']+layer['offset'][0],y=shape['y']+layer['offset'][1])
            edge=_shape_distance(point,shift,radius);value=float(edge<=0)
        if kind=='inset-shadow':value=1-value
        elif layer.get('knockout') and distance<0:value=0
        if layer['blur']>0:return value,True
        return value,abs(edge)*dpr<=1 or layer.get('knockout') and abs(distance)*dpr<=1
    if kind=='outline':
        outer=_inset(b,-layer['offset_width']-layer['width'])
        inner=_inset(b,-layer['offset_width'])
        od=_shape_distance(point,outer,r+layer['offset_width']+layer['width'])
        ind=_shape_distance(point,inner,r+layer['offset_width'])
        return float(od<=0 and ind>=0),min(abs(od),abs(ind))*dpr<=1
    if kind=='border':
        inner=_inset(b,layer['widths']);inner_r=tuple(max(0,v-max(layer['widths'])) for v in r)
        inner_distance=_shape_distance(point,inner,inner_r) if inner['width'] and inner['height'] else math.inf
        return float(distance<=0 and inner_distance>=0),min(abs(distance),abs(inner_distance))*dpr<=1
    return float(distance<=0),abs(distance)*dpr<=1


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
        if kind==0 and layer.get('role')=='caret':row['kind']='caret'
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
            elif effect not in (0,1):raise RunError(3,'calibration','non-round effect unsupported in finite pilot')
        for group in group_paths[actual[0]['command']]:
            requests=stack.get('composition_requests',[])
            if not 0<=group<len(requests):raise ProtocolError('composition group has no actual request')
            request=requests[group]
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


def _web_paint_inventory(checkpoint,origin,dpr):
    stack=checkpoint['resources'].get('paint_stack')
    if not isinstance(stack,dict) or stack.get('status')!='published':raise RunError(3,'calibration','actual Chromium paint order inventory unavailable')
    binding=stack.get('capture_binding',{});capture=checkpoint['captures'][0]
    if (binding.get('run_nonce')!=checkpoint['nonce'] or binding.get('capture_nonce')!=capture['nonce'] or
            binding.get('checkpoint')!=checkpoint['step'] or binding.get('capture_sha256')!=capture['sha256'] or
            binding.get('scene_revision')!=capture['scene_revision']):
        raise RunError(4,'protocol','Web paint inventory/capture binding differs')
    result=[];seen=set()
    for node in stack['layers']:
        identity=node['source_material_id'];s=node['style'];a=node['actual']
        if identity in seen:raise ProtocolError('duplicate Web backend paint node')
        seen.add(identity)
        if not isinstance(node.get('paint_order'),int):raise RunError(3,'calibration','actual Chromium paint group missing')
        if s.get('display')=='none' or s.get('visibility') in ('hidden','collapse'):continue
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
        for clip in a.get('clips',[]):
            if clip['clip_path']!='none' or clip['transform']!='none' and not re.fullmatch(r'matrix\(1, 0, 0, 1, [-\d.]+, [-\d.]+\)',clip['transform']):
                raise RunError(3,'calibration','CSS ancestor clip transform/path unsupported')
            clips.append(relative(clip['bounds'],origin))
        base=dict(identity=identity,side='web',bounds=bounds,groups=groups,clips=clips,alpha=1.,opacity=1.,
                  order=(node['paint_order'],node['layout_ordinal']),source=node,radii=(0,0,0,0))
        def add(kind,color,suffix,**fields):
            if color[3]==0:return
            result.append(dict(base,identity=identity+suffix,kind=kind,color=color[:3],alpha=color[3],**fields))
        if node['node_type']==3:
            text=a.get('text')
            if not text or text['content']!=node['layout_text']:raise RunError(3,'calibration','actual DOM/UA Text paint source unavailable')
            for group in text['semantic_groups']:
                content=text['content'].encode()[group['byte_begin']:group['byte_end']].decode()
                if content.isspace():continue
                color=_css_rgba(group['color']);selection=text.get('selection');utf16_begin=len(text['content'].encode()[:group['byte_begin']].decode().encode('utf-16-le'))//2
                utf16_end=utf16_begin+len(content.encode('utf-16-le'))//2
                selected=selection and selection['start']<utf16_end and utf16_begin<selection['end']
                for i,box in enumerate(group['boxes']):
                    local=relative(box,origin)
                    if selected:add('fill',_css_rgba(selection['background']),f'/selection/{group["byte_begin"]}/{i}',bounds=local)
                    add('glyph',_css_rgba(selection['color']) if selected else color,f'/text/{group["byte_begin"]}/{i}',bounds=local,text_group=group,content=content)
            caret=text.get('caret')
            if caret and caret['edge']['height']>0 and caret['animation']=='manual':
                edge=caret['edge'];b=dict(edge,x=edge['x']-1/dpr,width=2/dpr)
                add('caret',_css_rgba(caret['color']),'/caret',bounds=relative(b,origin),
                    caret_edge=dict(edge,x=edge['x']-origin['x'],y=edge['y']-origin['y']),
                    geometry_status='actual collapsed UA Range edge; thickness measured from unmasked pixels within one physical pixel of edge')
            continue
        # SVG paths remain their real paint sources. Formal identity is checked by
        # the shared oracle before a path's local glyph-like raster is consumed.
        if node['node_name'].lower()=='path':
            if s.get('stroke','none')!='none':raise RunError(3,'calibration','SVG stroke is outside formal finite fill domain')
            color=_css_rgba(s['fill']);color=(*color[:3],color[3]*float(s.get('fill-opacity','1')))
            add('glyph',color,'/path',formal_svg=True)
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
        for shadow in reversed(shadows):
            if not shadow['inset']:add('shadow',shadow['color'],'/shadow/'+str(shadow['ordinal']),offset=shadow['offset'],blur=shadow['blur'],spread=shadow['spread'],knockout=True,radii=radii[0])
        clip=s.get('background-clip','border-box');fill_bounds=bounds;fill_radii=tuple(radii)
        if clip in ('padding-box','content-box'):
            fill_bounds=_inset(fill_bounds,widths);fill_radii=tuple(max(0,r-max(widths)) for r in radii)
            if clip=='content-box':fill_bounds=_inset(fill_bounds,[_css_number(s['padding-'+edge]) for edge in ('top','right','bottom','left')])
        elif clip!='border-box':raise RunError(3,'calibration','unsupported actual background clip')
        if fill_bounds['width'] and fill_bounds['height']:add('fill',_css_rgba(s.get('background-color','rgba(0, 0, 0, 0)')),'/fill',bounds=fill_bounds,radii=fill_radii)
        for shadow in reversed(shadows):
            if shadow['inset']:
                padding_box=_inset(bounds,widths)
                if padding_box['width'] and padding_box['height']:
                    add('inset-shadow',shadow['color'],'/inset/'+str(shadow['ordinal']),bounds=padding_box,
                        offset=shadow['offset'],blur=shadow['blur'],spread=shadow['spread'],radii=max(0,radii[0]-max(widths)))
        if any(widths):
            if len(set(colors))!=1:
                # Four independent side sources, with exact triangular corner
                # ownership, remain future domain; never average their colors.
                raise RunError(3,'calibration','four-color border corner domain unsupported')
            add('border',colors[0],'/border',widths=widths)
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
    from .color_metrics import source_over
    frames=[([1.,1.,1.],1.,None,1.)];opened=[]
    def close():
        rgb,alpha,_,opacity=frames.pop();opened.pop();effective=alpha*opacity
        if alpha:
            parent=frames[-1];straight=[v/alpha for v in rgb]
            frames[-1]=(list(source_over(straight,parent[0],effective)),effective+(1-effective)*parent[1],parent[2],parent[3])
    for layer,coverage in zip(layers,coverages):
        chain=layer['groups'];common=0
        while common<min(len(chain),len(opened)) and chain[common][0]==opened[common]:common+=1
        while len(opened)>common:close()
        for name,opacity in chain[common:]:opened.append(name);frames.append(([0.,0.,0.],0.,name,opacity))
        rgb,alpha,name,opacity=frames[-1];effective=coverage*layer['alpha']*layer['opacity']
        frames[-1]=(list(source_over(layer['color'],rgb,effective)),effective+(1-effective)*alpha,name,opacity)
    while opened:close()
    return tuple(frames[0][0])


def _palette_basis(layers,background):
    """Choose at most three source color axes, preserving every source identity."""
    from itertools import combinations
    from .color_metrics import visible_basis,solve_visible_contributions
    colors=list(dict.fromkeys(layer['color'] for layer in layers if layer['color']!=background))
    if not colors:return visible_basis(background,[])
    # Same-axis sources are joint observations. They are never assigned separate
    # alpha values, even if a color basis happens to be full rank.
    joint=visible_basis(background,colors) if len(colors)==1 else None
    try:joint=visible_basis(background,colors)
    except ProtocolError:pass
    if joint is not None:return joint
    for count in (1,2,3):
        for palette in combinations(colors,count):
            try:basis=visible_basis(background,palette)
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


def _formal_paint_gates(native,web,stack):
    from .formal_icon_source import verify_formal_icon,verify_web_icon
    commands=[dict(kind={0:'quad',1:'glyph',2:'rounded_effect',3:'image'}[c['kind']],
                   first_instance=c['first'],instance_count=c['count'],atlas_page=c['atlas_page']) for c in stack['commands']]
    seen=set();errors=[]
    nt={t['identity']:t for t in native['targets']};wt={t['identity']:t for t in web['targets']}
    for target in nt.keys() & wt.keys():
        for a,b in zip(wt[target]['icons'],nt[target]['icons']):
            key=tuple((layer['scene_identity']['index'],layer['scene_identity']['generation']) for layer in b.get('layers',[]))
            if key in seen:continue
            seen.add(key);layers=[]
            native_visible=any(ink.get('paint_opacity',ink.get('opacity',0))>0 and
                any(c['kind']=='glyph' and c['atlas_page']==ink.get('atlas_page') and
                    c['first_instance']<=ink.get('instance_index',-1)<c['first_instance']+c['instance_count'] for c in commands)
                for layer in b.get('layers',[]) for ink in layer.get('ink_instances',[]))
            web_visible=a.get('bounds',{}).get('width',0)>0 and a.get('bounds',{}).get('height',0)>0 and any(
                float(p.get('opacity',0))*float(p.get('fill_opacity',0))>0 for p in a.get('paths',[]))
            verify_web_icon(b['identity'],a)
            if not native_visible and not web_visible:continue
            if native_visible!=web_visible:
                errors.append(dict(category='visual',step=native['step'],target=target,message='actual formal icon submitted visibility differs'))
                continue
            for layer in b.get('layers',[]):
                ink=layer.get('ink_instances',[])
                if not ink:raise ProtocolError('formal icon layer has no actual atlas source')
                source=ink[0].get('atlas_source',{})
                font=source.get('font',{})
                layers.append(dict(layer,layer_opacity=layer.get('layer_opacity'),
                    font_sha256=font.get('sha256'),font_source=font.get('source'),face_index=font.get('face_index'),glyph_id=source.get('glyph_id')))
            verify_formal_icon(b['identity'],a,layers,commands)
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
        if row['kind'] in ('shadow','inset-shadow','outline'):continue
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
            radius=row.get('radii',0)
            if any(all(abs(row['bounds'][k]-bounds[k])<=.25 for k in bounds) and
                   (all(abs(radius-r)<=.25 for r in expected) or row.get('clips') and any(abs(radius-r)<=.25 for r in expected))
                   for bounds,expected in boxes):matched=True;break
        if not matched:errors.append(dict(category='layout',step=step,target=row['identity'],message='actual retained part geometry differs from observed paint-bearing Web part'))
    return errors


def _glyph_priors_for_run(native,manifest):
    from .calibration import font_priors,font_priors_from_bytes
    from .formal_icon_source import official_font_bytes
    from .protocol import ROOT,relative_path
    ordinary=set();icons=set()
    for checkpoint in native['checkpoints'].values():
        for text in checkpoint['resources'].get('paint_stack',{}).get('glyph_inventory',[]):
            destination=icons if text.get('kind')=='icon-font' else ordinary
            destination.update(char for char in text['content'] if not char.isspace())
    library=ROOT/relative_path(manifest['fonttools_library'])
    result=font_priors(manifest['fonts'],dict(train=dict(text=''.join(sorted(ordinary))),holdout=dict(text='')),library)
    if icons:result.update(font_priors_from_bytes(official_font_bytes(),[ord(c) for c in sorted(icons)],library))
    return result


def _glyph_witness_gate(row,point,dpr,priors,lower,observed):
    """Independent SFNT interior witnesses; atlas padding and border are no ink."""
    from .calibration import outline_point
    from .color_metrics import unmix_coverage
    text,ink=row['glyph_source'];source=ink['atlas_source'];font=source['font']
    encoded=text['content'].encode();content=encoded[ink['byte_begin']:ink['byte_end']].decode()
    if content.isspace():return None
    if len(content)!=1:raise RunError(3,'calibration','multi-scalar shaping group needs an independently supported outline prior')
    prior=priors.get(font['sha256']+':'+content)
    if prior is None:raise RunError(3,'calibration','actual glyph has no independently reconstructed SFNT prior')
    if source['glyph_id']!=prior['glyph_id']:raise ProtocolError('actual atlas glyph differs from independent SFNT cmap')
    display=source['display_scale'];scale=font['logical_pixel_size']/prior['units']
    if display!=dpr:raise RunError(3,'calibration','actual glyph atlas scale differs from capture DPR')
    baseline_x=row['bounds']['x']+(source['padding']-source['bearing_x']+source['phase']/4)/display
    baseline_y=row['bounds']['y']+(source['bearing_y']+source['padding'])/display
    x=(point[0]-baseline_x)/scale;y=(baseline_y-point[1])/scale
    inside,distance=outline_point(prior['contours'],x,y)
    if not inside or distance*scale*dpr<=.125:return None
    # A witness is used only where the caller proved this glyph is exposed.
    try:unmixed=unmix_coverage(tuple(v/255 for v in observed),row['color'],lower,row['alpha']*row['opacity'],byte_error=1/255)
    except ProtocolError:return False
    # A disk centered at the pixel center and contained in both the SFNT outline
    # and the unit pixel gives a conservative nonzero area witness. The fixed
    # outline subdivision error is below 1/1024 physical px at pilot sizes.
    minimum=math.pi*min(.5,max(0,distance*scale*dpr-1/1024))**2
    if minimum<=2*unmixed['quantization_bound']:return None
    return unmixed['coverage']+unmixed['quantization_bound']>=minimum


def _same_glyph_interface(first,second):
    if first['kind']!='glyph' or second['kind']!='glyph' or first['bounds']!=second['bounds']:return False
    a=first['glyph_source'][1]['atlas_source'];b=second['glyph_source'][1]['atlas_source']
    return (a['glyph_id'],a['font']['sha256'],a['phase'])==(b['glyph_id'],b['font']['sha256'],b['phase'])


def _layered_context(case,declaration,native,web,images,thresholds,origins,*,glyph_priors=None):
    from .color_metrics import rgb8_delta_e00,solve_visible_contributions,unmix_coverage,contribution_domain,in_contribution_domain,visible_segment
    from .regions import coverage_metrics,shadow_profile_metrics
    dpr=case['fixture']['viewport']['dpr'];sample=case['native']['sample_id']
    native_layers,native_stack=_native_paint_inventory(native,origins[1],dpr)
    web_layers,web_stack=_web_paint_inventory(web,origins[0],dpr)
    inventories=(web_layers,native_layers)
    hard_failures=_formal_paint_gates(native,web,native_stack)
    hard_failures.extend(_hard_paint_gates(native_layers,web_layers,declaration['step']))
    nt={t['identity']:t for t in native['targets']};wt={t['identity']:t for t in web['targets']}
    context=next(r for r in declaration['rois'] if r['kind']=='sample-context')
    scope=union(relative(wt[sample]['bounds'],origins[0]),relative(nt[sample]['bounds'],origins[1]))
    for rows in inventories:
        for row in rows:scope=union(scope,_support(row))
    absolute=[dict(scope,x=scope['x']+origin['x'],y=scope['y']+origin['y']) for origin in origins]
    try:crops,size,box=_crop_pair(images,*absolute,origins,dpr,int(context['guard_px']))
    except ProtocolError as error:
        raise RunError(4,'capture-coverage','full paint support/shadow/outline/popup plus guard is not captured; required relative support '+str(scope)) from error
    failures=list(hard_failures);measurements=[];counts={k:0 for k in ('flat','edge','glyph','shadow','joint','guard')}
    def fail(category,message,**data):
        if len(failures)<64:failures.append(dict(category=category,step=declaration['step'],target=sample,message=message,**data))
    basis_cache={};domain_cache={};presence={};paired_planes={};joint_planes={};max_joint=0.;max_color=0.;glyph_witnesses={}
    for index in range(size[0]*size[1]):
        point=((box[0]+index%size[0]+.5)/dpr,(box[1]+index//size[0]+.5)/dpr)
        sides=[]
        for side,rows in enumerate(inventories):
            active=[];coverages=[];variable=[]
            for row in rows:
                support=_support(row)
                if not _contains(_inset(support,-1/dpr),point):continue
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
            if key not in basis_cache:basis_cache[key]=_palette_basis(active,(1.,1.,1.))
            basis=basis_cache[key]
            if basis['mode']=='flat':
                error=rgb8_delta_e00(observed,tuple(round(v*255) for v in predicted))
                if error>thresholds['color.flat_delta_e00']:fail('visual','source-identical flat paint differs',side=side,pixel=point,actual=error)
                sides.append(dict(active=active,coverages=coverages,variable=[],kind='flat',values=()))
                continue
            solution=solve_visible_contributions(tuple(v/255 for v in observed),basis)
            if not solution['valid']:fail('typography' if any(r['kind']=='glyph' for r in active) else 'visual',
                                          'known source pixel violates visible color domain',side=side,pixel=point)
            values=solution.get('contributions')
            if values is None:values=(solution['joint_contrast'],)
            # Joint same-color opacity/order bounds use actual finite stack
            # extrema. Enumerating endpoint coverages creates no pixel-fitted
            # alpha, palette, registration, or tolerance.
            if len(variable)>8:raise RunError(3,'calibration','more than eight simultaneous finite coverage interfaces')
            coordinates=lambda solution:tuple(solution['contributions']) if solution['contributions'] is not None else (solution['joint_contrast'],)
            if basis['mode']=='visible-contribution':
                domain_key=(key,tuple(variable),tuple(0 if i in variable else c for i,c in enumerate(coverages)))
                if domain_key not in domain_cache:
                    vertices=[]
                    for mask in range(1<<len(variable)):
                        candidate=coverages.copy()
                        for bit,position in enumerate(variable):candidate[position]=float(bool(mask&(1<<bit)))
                        vertices.append(coordinates(solve_visible_contributions(_composite_point(active,candidate),basis)))
                    domain_cache[domain_key]=contribution_domain(vertices)
                if not in_contribution_domain(values,domain_cache[domain_key],basis['quantization_bounds']):
                    fail('visual','visible contributions violate actual source-over order/opacity domain',side=side,pixel=point)
            if basis['mode']=='joint-collinear':
                domain_key=('collinear',key,tuple(variable),tuple(0 if i in variable else c for i,c in enumerate(coverages)))
                if domain_key not in domain_cache:
                    endpoints=[]
                    for mask in range(1<<len(variable)):
                        candidate=coverages.copy()
                        for bit,position in enumerate(variable):candidate[position]=float(bool(mask&(1<<bit)))
                        endpoint=_composite_point(active,candidate)
                        endpoints.append(solve_visible_contributions(endpoint,basis)['joint_contrast'])
                    domain_cache[domain_key]=(min(endpoints),max(endpoints))
                low,high=domain_cache[domain_key]
                eps=solution['quantization_bound']
                if not low-eps<=values[0]<=high+eps:fail('visual','joint color violates actual source opacity/order extrema',side=side,pixel=point)
            kinds={active[i]['kind'] for i in variable}
            kind='joint' if len(variable)>1 else ('glyph' if kinds & {'glyph','caret'} else 'shadow' if kinds & {'shadow','inset-shadow'} else 'edge')
            if kinds <= {'shadow','inset-shadow'} and any(active[i].get('blur',0)>0 for i in variable):
                # Gaussian source geometry is independently fixed, and can
                # expose a missing same-color shadow even when its alpha cannot
                # be individually unmixed from the final RGB.
                predicted8=tuple(round(v*255) for v in predicted)
                if max(abs(a-b) for a,b in zip(observed,predicted8))>2:
                    fail('visual','independent soft-shadow profile/tail differs',side=side,pixel=point)
            if len(variable)==1:
                position=variable[0];row=active[position]
                low=coverages.copy();high=coverages.copy();low[position]=0.;high[position]=1.
                lower=_composite_point(active,low);upper=_composite_point(active,high)
                try:
                    soft=row['kind'] in ('shadow','inset-shadow') and row.get('blur',0)>0
                    if soft:
                        solved=visible_segment(tuple(v/255 for v in observed),lower,upper,_visible_weight(active,coverages,position))
                        if not solved['valid']:raise ProtocolError('known visible alpha pixel differs')
                        value=solved['contribution']
                    else:value=unmix_coverage(tuple(v/255 for v in observed),upper,lower,1.,byte_error=1/255)['coverage']
                except ProtocolError:
                    fail('typography' if row['kind'] in ('glyph','caret') else 'visual','known local material pixel differs',side=side,pixel=point)
                    value=None
                if value is not None:
                    # Quantization-safe integration only; invalid model pixels
                    # already failed and are never clamped into a valid result.
                    value=min(1.,max(0.,value))
                    prefix='text' if row['kind'] in ('glyph','caret') else 'shadow' if row['kind'] in ('shadow','inset-shadow') and row['blur']>0 else 'pixels'
                    presence[(side,row['identity'])]=presence.get((side,row['identity']),0)+value
            if side==1:
                for position in variable:
                    row=active[position]
                    if row['kind']!='glyph' or any(active[i]['kind'] in ('glyph','caret') and
                        not _same_glyph_interface(row,active[i]) for i in variable if i!=position):continue
                    if any(coverages[i]>0 and active[i]['alpha']*active[i]['opacity']>=1 for i in range(position+1,len(active))):continue
                    if glyph_priors is None:raise RunError(3,'calibration','independent actual SFNT outline priors unavailable')
                    below=_composite_point(active[:position],coverages[:position])
                    witness=_glyph_witness_gate(row,point,dpr,glyph_priors,below,observed)
                    if witness is not None:
                        glyph_witnesses[row['identity']]=glyph_witnesses.get(row['identity'],0)+1
                        if not witness:fail('typography','independent SFNT interior glyph witness has no ink',pixel=point,source=row['identity'])
            sides.append(dict(active=active,coverages=coverages,variable=variable,kind=kind,values=tuple(values),basis=basis))
        classification=sides[0]['kind'];counts[classification]+=1
        # Every actual context pixel is compared, including overlaps and tails.
        # No full-context byte equality veto is used for calibrated AA.
        a,b=[tuple(data[index*4:index*4+3]) for data in crops]
        if not sides[0]['variable'] and not sides[1]['variable']:
            error=rgb8_delta_e00(a,b)
            if error>thresholds['color.flat_delta_e00']:fail('visual','paired flat/context/guard color differs',pixel=point,actual=error)
        if len(sides[0]['variable'])==1:
            reference=sides[0];position=reference['variable'][0];row=reference['active'][position]
            low=reference['coverages'].copy();high=reference['coverages'].copy();low[position]=0.;high[position]=1.
            lower=_composite_point(reference['active'],low);upper=_composite_point(reference['active'],high)
            values=[]
            soft=row['kind'] in ('shadow','inset-shadow') and row.get('blur',0)>0
            maximum=_visible_weight(reference['active'],reference['coverages'],position) if soft else 1.
            for rgb in (a,b):
                try:
                    if soft:
                        solved=visible_segment(tuple(v/255 for v in rgb),lower,upper,maximum)
                        value=solved['contribution'] if solved['valid'] else None
                    else:value=unmix_coverage(tuple(v/255 for v in rgb),upper,lower,1.,byte_error=1/255)['coverage']
                except ProtocolError:value=None
                values.append(None if value is None else min(1.,max(0.,value)))
            if all(v is not None for v in values):
                if row['identity'] not in paired_planes:paired_planes[row['identity']]=(row,{}, {})
                for plane,value in zip(paired_planes[row['identity']][1:],values):plane[index]=value
                if row['kind'] in ('glyph','caret') and values[0]>=.98:
                    error=rgb8_delta_e00(a,b)
                    if error>thresholds['color.text_interior_delta_e00']:fail('typography','high contrast glyph/caret interior color differs',pixel=point,actual=error)
        if sides[0]['variable'] or sides[1]['variable']:
            if not sides[0]['variable'] or not sides[1]['variable']:
                # A source geometry disagreement is already independently known.
                # Keep comparing actual RGB rather than turning it into code 3.
                value=rgb8_delta_e00(a,b)
                if value>thresholds['color.flat_delta_e00']:fail('visual','actual paint support/transition differs',pixel=point,actual=value)
            else:
                # One reference basis for both pixels; material differences are
                # output differences, never a choice of a favorable native basis.
                basis=sides[0]['basis'];solutions=[solve_visible_contributions(tuple(v/255 for v in rgb),basis) for rgb in (a,b)]
                if not all(s['valid'] for s in solutions):fail('visual','known paired pixel is outside fixed reference visible basis',pixel=point)
                else:
                    coordinates=[s['contributions'] if s['contributions'] is not None else (s['joint_contrast'],) for s in solutions]
                    error=max((abs(x-y) for x,y in zip(*coordinates)),default=0);max_joint=max(max_joint,error)
                    cap=thresholds.get('overlap.contribution_max_error',thresholds['pixels.coverage_local_relative_error'])
                    if classification=='joint':
                        if error>cap:fail('visual','joint visible contribution differs',pixel=point,actual=error,threshold=cap)
                        group=tuple(row['identity'] for row in sides[0]['active'])
                        for axis,(first,second) in enumerate(zip(*coordinates)):
                            key=(group,axis)
                            if key not in joint_planes:joint_planes[key]=({}, {})
                            joint_planes[key][0][index]=min(1.,max(0.,first));joint_planes[key][1][index]=min(1.,max(0.,second))
    if sum(counts.values())!=size[0]*size[1]:raise ProtocolError('paint strata did not account for the entire crop exactly once')
    for identity,(row,reference,native_plane) in paired_planes.items():
        if not reference:continue
        coordinates=[(i%size[0],i//size[0]) for i in reference]
        left=min(x for x,y in coordinates);right=max(x for x,y in coordinates)+1
        top=min(y for x,y in coordinates);bottom=max(y for x,y in coordinates)+1
        local_size=(right-left,bottom-top)
        local=[[],[]];tail=[]
        for y in range(top,bottom):
            for x in range(left,right):
                index=y*size[0]+x;local_index=(y-top)*local_size[0]+x-left
                for values,plane in zip(local,(reference,native_plane)):values.append(plane.get(index,0.))
                point=((box[0]+x+.5)/dpr,(box[1]+y+.5)/dpr)
                if not _contains(_inset(row['bounds'],-max(1/dpr,row.get('blur',0)/2)),point):tail.append(local_index)
        soft=row['kind'] in ('shadow','inset-shadow') and row['blur']>0
        prefix='shadow' if soft else 'text' if row['kind'] in ('glyph','caret') else 'pixels'
        result=shadow_profile_metrics(*local,local_size,tail_indices=tail) if soft else coverage_metrics(*local,local_size)
        if not soft:
            from .regions import reference_aa_is_bounded
            if not reference_aa_is_bounded(local[0],local_size):fail('typography' if prefix=='text' else 'visual','reference hard-edge transition exceeds the fixed one physical pixel AA bound',source=identity)
        if not result['required_ink_present']:fail('typography' if prefix=='text' else 'visual','required independently exposed paint ink missing',source=identity)
        fields=('coverage_mass_relative_error','projection_relative_error','profile_max_error','tail_max_error') if soft else (
            'coverage_mass_relative_error','contour_distance_px','projection_relative_error' if prefix=='text' else 'coverage_local_relative_error')
        for field in fields:
            key=prefix+'.'+field
            if key not in thresholds:raise RunError(3,'calibration','actual stratum lacks independent frozen metric: '+key)
            if result[field] is None or result[field]>thresholds[key]:fail('typography' if prefix=='text' else 'visual','local paint contribution profile failed',source=identity,metric=key,actual=result[field],threshold=thresholds[key])
        measurements.append(dict(target=identity,category=prefix,metrics=result,exposed_pixels=len(reference)))
    for row in native_layers:
        if row['kind']=='glyph' and row['identity'] not in glyph_witnesses:
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


def compare_checkpoint(case, declaration, native, web, images, thresholds, color,*,glyph_priors=None):
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
                if layered:
                    web_size=_css_number(aw['font_size'])
                    web_weight=int(aw['font_weight'])
                    weights={(f['sha256'],f['face_index']):f['weight'] for f in case['fixture']['fonts']}
                    if any(run['font']['logical_pixel_size']!=web_size or
                           weights.get((run['font']['sha256'],run['font']['face_index']))!=web_weight for run in bn['font_runs']):
                        fail('typography',target,'actual font face weight/size differs between paired semantic runs')
                ag, bg = text_groups(aw,'web'), text_groups(bn,'native')
                if [(g['byte_begin'],g['byte_end'],g['content']) for g in ag] != [
                        (g['byte_begin'],g['byte_end'],g['content']) for g in bg]:
                    raise RunError(3,'calibration','actual glyph/grapheme groups cannot be paired without guessing')
                for ga,gb in zip(ag,bg):
                    advance_cap=thresholds.get('text.geometry_logical_px',.5)
                    if abs(ga['advance']-gb['advance']) > advance_cap:
                        fail('typography',target,'semantic group advance differs',metric='text.geometry_logical_px',
                             actual=abs(ga['advance']-gb['advance']),threshold=advance_cap)
                    if ga['content'].isspace():
                        continue
                    def group_rect(group):
                        value=group['boxes'][0]
                        for box in group['boxes'][1:]:
                            value=union(value,box)
                        return value
                    # Presence uses the common semantic interior, without guards.
                    # Full union+guard is separately compared by raster().
                    ar,br=group_rect(ga),group_rect(gb)
                    if not layered:
                        local_background=_local_background(a,background)
                        for (data,size),box in zip(images,(ar,br)):
                            l,t,r,bottom=physical_box(box,dpr,2)
                            raw=crop_rgba(data,size,(l+2,t+2,r-2,bottom-2))
                            presence=raster_metrics(raw,raw,(r-l-4,bottom-t-4),local_background)
                            if not presence['required_ink_present']:
                                fail('typography',target+'.text','required semantic glyph ink missing')
                        raster(target + '.text', ar, br, 2, 'typography', text=True,
                               local_background=local_background)
        if len(a['icons']) != len(b['icons']):
            fail('visual', target, 'required icon identity/count differs')
        for aw, bn in zip(a['icons'], b['icons']):
            if not aw.get('identity') or not bn.get('identity'):
                raise ProtocolError('icon source identity is unavailable')
            if aw['identity'].replace('-', '').lower() != re.sub(r'(Outlined|Filled|TwoTone)$','',bn['identity']).lower():
                fail('visual', target, 'required icon identity differs')
            if not layered:
                raster(target + '.icon', aw['bounds'], bn['bounds'], 2, 'visual',
                       local_background=_local_background(a, background))
    if layered:
        errors,layer_measurements=_layered_context(case,declaration,native,web,images,thresholds,origins,glyph_priors=glyph_priors)
        failures.extend(errors);measurements.extend(layer_measurements)
    return failures, measurements


def compare(case, native, web, directories, profile, image_module):
    expected = binding(case, native, web)
    thresholds = profile_thresholds(profile, expected, case['profile'],image_module=image_module,
                                    native_binary_sha256=native['hello']['provenance']['binary_sha256'])
    from .calibration import evidence_manifest
    glyph_priors=_glyph_priors_for_run(native,evidence_manifest(profile)[0])
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
    failures, measurements = [], {}
    for checkpoint_index, declaration in enumerate(case['checkpoints']):
        step = declaration['step']
        n, w = native['checkpoints'][step], web['checkpoints'][step]
        captures = (w['captures'][0], n['captures'][0])
        # Raw side descriptions stay distinct. Opaque identity is evidence-bound;
        # no relabelling of native encoded/premultiplied into Web sRGB/straight.
        from .calibration import png_metadata
        color=profile['color']
        for side,capture in zip(('web','native'),captures):
            descriptor=color[side]
            if capture['channel_order']!='RGBA' or capture['color_profile']!=descriptor['encoding']:
                raise RunError(3,'calibration','capture original channel/encoding differs from independent controls')
            if side=='native' and (capture['alpha']!='premultiplied' or capture['framebuffer_format']!=descriptor['framebuffer_format']):
                raise RunError(3,'calibration','native original storage differs from independent controls')
            if side=='web' and capture['alpha'] not in ('straight','opaque'):
                raise RunError(3,'calibration','Web original PNG alpha differs')
        images = [decoded(image_module, artifact(directory, capture['path']), capture)
                  for directory, capture in zip((directories['web'], directories['native']), captures)]
        if any(any(a!=255 for a in data[3::4]) for data,_ in images):
            raise RunError(3,'calibration','pilot decoded capture is not all opaque')
        raw=n['resources'].get('readback',{})
        width,height=images[1][1]
        if (raw.get('raw_alpha_available') is not True or raw.get('raw_alpha_min')!=255 or raw.get('raw_alpha_max')!=255 or
                raw.get('raw_opaque_pixels')!=width*height or raw.get('raw_total_pixels')!=width*height or
                raw.get('clear_rgba')!=color['native']['clear_rgba'] or
                raw.get('source_channel_order')!=color['native']['source_channel_order']):
            raise RunError(3,'calibration','native opaque working domain lacks actual raw alpha/clear/channel proof')
        fingerprint=14695981039346656037
        for byte in images[1][0]:fingerprint=((fingerprint^byte)*1099511628211)&((1<<64)-1)
        if raw.get('raw_rgba_fnv1a64')!=str(fingerprint):
            raise RunError(3,'calibration','native opaque BMP differs from original owned raw RGBA')
        png=png_metadata(artifact(directories['web'],captures[0]['path']))
        if (png['format'] not in color['web']['file_formats'] or png['color_chunks']!=color['web']['png_color_chunks'] or
                digest(web['hello']['provenance']['browser']['launch_args'])!=color['web']['launch_args_hash']):
            raise RunError(3,'calibration','Web PNG format/actual sRGB launch parameters differ')
        context = next(r for r in declaration['rois'] if r['kind'] == 'sample-context')
        a = next(t for t in w['targets'] if t['identity'] == context['target'])['bounds']
        b = next(t for t in n['targets'] if t['identity'] == context['target'])['bounds']
        at=next(t for t in w['targets'] if t['identity']==context['target'])
        bt=next(t for t in n['targets'] if t['identity']==context['target'])
        crops, size, box = _crop_pair(images, a, b, (sample_anchor(at),sample_anchor(bt)),
                                     case['fixture']['viewport']['dpr'], int(context['guard_px']))
        diff = bytes(abs(x - y) if index % 4 != 3 else 255 for index, (x, y) in enumerate(zip(*crops)))
        destination = Path(directories['native']).parent / 'comparison' / str(checkpoint_index)
        destination.mkdir(parents=True)
        image_module.frombytes('RGBA', size, diff).save(destination / 'diff.png')
        errors, measured = compare_checkpoint(case, declaration, n, w, images, thresholds, profile['color'],glyph_priors=glyph_priors)
        failures.extend(errors)
        measurements[step] = measured
    return failures, {'bindings': expected, 'thresholds': thresholds, 'checkpoints': measurements}
