"""Strict official icon resource gate shared by calibration and ordinary consumers.

No paired-pixel agreement grants source identity. Web supplies SVG facts; native
supplies the actual outlined-font glyphs and final submitted command membership.
"""
from functools import lru_cache
import hashlib
import math
import re
import struct
import xml.etree.ElementTree as ET

from .protocol import ROOT, ProtocolError, read


def need(value, message):
    if not value:
        raise ProtocolError('formal icon: '+message)


@lru_cache(maxsize=1)
def _official_source():
    root=ROOT/'third_party/ant-design-icons'
    manifest=read(root/'manifest.json')
    text=(ROOT/'src/icons/ant_design_icon_font.inc').read_text(encoding='utf-8')
    font=bytes(int(x,16) for x in re.findall(r'0x([0-9a-f]{2})',text))
    need(hashlib.sha256(font).hexdigest()==manifest['outline_container_sha256'],'embedded outline bytes changed')
    return root,manifest,font


def official_font_bytes():
    """Immutable hash-verified SFNT input for the shared independent outline math."""
    return _official_source()[2]


@lru_cache(maxsize=1)
def official():
    root,manifest,font=_official_source()
    tables={font[p:p+4]:struct.unpack_from('>II',font,p+8) for p in range(12,12+16*struct.unpack_from('>H',font,4)[0],16)}
    cmap_offset,_=tables[b'cmap']
    cmap={}
    for i in range(struct.unpack_from('>H',font,cmap_offset+2)[0]):
        offset=cmap_offset+struct.unpack_from('>I',font,cmap_offset+4+i*8+4)[0]
        if struct.unpack_from('>H',font,offset)[0]!=12:continue
        for j in range(struct.unpack_from('>I',font,offset+12)[0]):
            first,last,glyph=struct.unpack_from('>III',font,offset+16+j*12)
            need(last-first<100000,'unbounded official cmap')
            cmap.update((cp,glyph+cp-first) for cp in range(first,last+1))
    need(cmap,'official outline cmap12 missing')
    return root,manifest,cmap


def resource(name):
    root,manifest,cmap=official()
    entry=next((r for r in manifest['icons'] if r['name']==name),None)
    need(entry is not None,'unknown official identity')
    raw=(root/entry['file']).read_bytes()
    need(hashlib.sha256(raw).hexdigest()==entry['sha256'],'official SVG bytes changed')
    return entry,ET.fromstring(raw),manifest,cmap


@lru_cache(maxsize=256)
def single_path_for_glyph(glyph_id, *, include_view_box=False):
    """A single opaque path with no additional layer state."""
    _,manifest,cmap=official()
    matches=[entry for entry in manifest['icons'] if len(entry['layers'])==1 and
             cmap.get(entry['layers'][0]['codepoint'])==glyph_id and entry['layers'][0]['opacity']==1]
    if len(matches)!=1:
        return None
    _,svg,_,_=resource(matches[0]['name'])
    if len(svg)!=1:
        return None
    path=svg[0]
    if (path.tag.split('}')[-1]!='path' or set(path.attrib)-{'d','fill-rule'} or
            set(svg.attrib)-{'viewBox','fill-rule','focusable'} or 'd' not in path.attrib):
        return None
    signature=re.sub(r'\s+','',path.attrib['d']),path.get('fill-rule',svg.get('fill-rule','nonzero'))
    return (*signature,svg.attrib['viewBox']) if include_view_box else signature


def verify_web_icon(name, observed, *, calibration_offset=None):
    """The calibration fixture may translate only its outer, measured owner."""
    entry,svg,_,_=resource(name)
    need(observed.get('source_kind')=='svg' and 'actual_font_sha256' not in observed,
         'Web SVG may not claim a native font')
    need(observed.get('view_box')==svg.attrib['viewBox'],'viewBox differs')
    expected=[]
    def visit(node, rule='nonzero', opacity=1):
        rule=node.get('fill-rule',rule)
        opacity*=float(node.get('fill-opacity','1'))
        need(not node.get('transform') and not node.get('clip-path'),'unsupported official transform/clip')
        if node.tag.split('}')[-1]=='path':expected.append((node.attrib['d'],rule,opacity))
        for child in node:visit(child,rule,opacity)
    visit(svg)
    paths=observed.get('paths',[])
    need(len(paths)==len(expected) and paths,'missing/reordered SVG paths')
    for ordinal,(actual,(d,rule,opacity)) in enumerate(zip(paths,expected)):
        need(actual.get('ordinal')==ordinal and re.sub(r'\s+','',actual.get('d') or '')==re.sub(r'\s+','',d),'SVG path/order differs')
        need(actual.get('fill_rule')==rule and abs(float(actual.get('fill_opacity',-1))-opacity)<1e-6,'fill rule/opacity differs')
        chain=actual.get('ancestry')
        need(isinstance(chain,list) and chain,'missing actual SVG ancestry')
        seen_svg=False
        seen_owner=False
        for node in chain:
            need(node.get('transform_attr') in (None,'') and node.get('clip_path')=='none' and
                 node.get('mask')=='none' and node.get('filter')=='none','actual transform/clip/mask/filter differs')
            transform=node.get('transform_css')
            if calibration_offset is not None and node.get('calibration_owner') is True:
                need(seen_svg and not seen_owner and node.get('svg_root') is not True,'invalid calibration owner')
                seen_owner=True
                matrix=(1.,0.,0.,1.,0.,0.) if transform=='none' else ()
                if isinstance(transform,str) and transform.startswith('matrix(') and transform.endswith(')'):
                    try:matrix=tuple(float(field) for field in transform[7:-1].split(','))
                    except ValueError:matrix=()
                need(matrix==(1.,0.,0.,1.,*calibration_offset),'actual calibration owner translation differs')
            else:
                need(transform in ('none','matrix(1, 0, 0, 1, 0, 0)'),'actual SVG transform differs')
            if node.get('svg_root') is True:
                need(not seen_svg,'duplicate actual SVG root')
                seen_svg=True
        if calibration_offset is not None:
            need(seen_owner,'missing actual calibration owner')
        need(actual.get('stroke')=='none','unexpected SVG stroke')
    return entry


@lru_cache(maxsize=1024)
def _aria_candidates(label):
    _,manifest,_=official()
    return tuple(icon['name'] for icon in manifest['icons']
                 if re.sub(r'(Outlined|Filled|TwoTone)$','',icon['name']).lower()==label)


def observed_web_icon_name(observed):
    """Resolve the measured SVG against the pinned formal variants of its aria label."""
    label=observed.get('identity','').replace('-','').lower()
    candidates=_aria_candidates(label)
    need(candidates,'unknown Web aria identity')
    matches=[]
    for name in candidates:
        try:
            verify_web_icon(name,observed)
        except ProtocolError:
            continue
        matches.append(name)
    need(len(matches)==1,'actual Web path/style does not resolve uniquely to its formal aria identity')
    return matches[0]


def verify_native_icon(name, layers, commands):
    entry,_,manifest,cmap=resource(name)
    need(isinstance(layers,list) and len(layers)==len(entry['layers']),'native layer count differs')
    need(isinstance(commands,list) and commands,'final submitted command stream missing')
    last=-1
    for ordinal,(actual,declared) in enumerate(zip(layers,entry['layers'])):
        need(actual.get('layer_ordinal')==ordinal and actual.get('codepoint')==declared['codepoint']
             and actual.get('role')==declared['role'],'native layer/codepoint/role differs')
        need(actual.get('font_sha256')==manifest['outline_container_sha256'] and actual.get('font_source')=='bundled-icon'
             and actual.get('face_index')==0,'actual native face/source differs')
        need(actual.get('glyph_id')==cmap.get(declared['codepoint']) and actual.get('glyph_id',0)>0,'actual cmap glyph differs')
        need(actual.get('scene_identity',{}).get('generation',0)>0,'native scene generation missing')
        layer_opacity=actual.get('layer_opacity')
        need(type(layer_opacity) in (int,float) and math.isfinite(layer_opacity)
             and abs(layer_opacity-declared['opacity'])<1e-6,'intrinsic native layer opacity differs')
        instances=actual.get('ink_instances',[])
        need(instances,'actual glyph atlas instances missing')
        for instance in instances:
            index=instance.get('instance_index');page=instance.get('atlas_page')
            need(type(index) is int and type(page) is int and page>=0,'instance/atlas identity missing')
            need(instance.get('atlas_uv') and instance.get('clip') and instance.get('bounds'),'actual atlas/clip/geometry missing')
            memberships=[i for i,c in enumerate(commands) if c.get('kind')=='glyph' and c.get('atlas_page')==page
                         and c.get('first_instance',-1)<=index<c.get('first_instance',-1)+c.get('instance_count',0)]
            need(len(memberships)==1 and memberships[0]>=last,'glyph absent/duplicated/reordered in final commands')
            last=memberships[0]
            paint_opacity=instance.get('paint_opacity')
            need(type(paint_opacity) in (int,float) and math.isfinite(paint_opacity) and 0<=paint_opacity<=1,
                 'effective native paint opacity is invalid')
    return entry


def verify_formal_icon(name, web, native_layers, commands):
    verify_web_icon(name,web)
    return verify_native_icon(name,native_layers,commands)
