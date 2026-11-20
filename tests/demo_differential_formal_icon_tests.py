"""Simulated source receipts exercise independent official-resource gates; no GPU."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from demo_differential.formal_icon_source import resource,verify_web_icon,verify_native_icon,official_font_bytes
from demo_differential.comparison import _formal_paint_gates
from demo_differential.protocol import ProtocolError


class FormalIconTests(unittest.TestCase):
    def receipts(self,name='SearchOutlined'):
        entry,svg,manifest,cmap=resource(name)
        neutral=dict(transform_attr=None,transform_css='none',clip_path='none',mask='none',filter='none')
        paths=[]
        def visit(node,rule='nonzero',opacity=1):
            rule=node.get('fill-rule',rule);opacity*=float(node.get('fill-opacity','1'))
            if node.tag.endswith('path'):
                paths.append(dict(ordinal=len(paths),d=node.attrib['d'],fill_rule=rule,fill_opacity=opacity,
                    ancestry=[neutral],stroke='none'))
            for child in node:visit(child,rule,opacity)
        visit(svg)
        web=dict(source_kind='svg',view_box=svg.attrib['viewBox'],paths=paths)
        native=[dict(layer_ordinal=i,codepoint=l['codepoint'],role=l['role'],layer_opacity=l['opacity'],font_sha256=manifest['outline_container_sha256'],
            font_source='bundled-icon',face_index=0,glyph_id=cmap[l['codepoint']],scene_identity=dict(index=1,generation=1),
            ink_instances=[dict(instance_index=i,atlas_page=0,atlas_uv=[0,0,.1,.1],clip=[0,0,16,16],bounds=[0,0,16,16],
                paint_opacity=l['opacity'])]) for i,l in enumerate(entry['layers'])]
        commands=[dict(kind='glyph',first_instance=0,instance_count=len(native),atlas_page=0)]
        return web,native,commands

    def test_tiled_formal_icons_match_visible_sources_independent_of_declaration_order(self):
        native=[]
        web=[]
        bounds=lambda x,y:dict(x=x,y=y,width=12,height=12)
        for index,(name,label,y,opacity) in enumerate((
                ('DownOutlined','down',8,1),('CloseCircleFilled','close-circle',8,0),
                ('SearchOutlined','search',80,1))):
            wr,nr,_=self.receipts(name)
            web.append(dict(wr,identity=label,bounds=bounds(8,y)))
            for path in wr['paths']:
                path['opacity']=opacity
            nr[0]['scene_identity']['index']=index
            nr[0]['ink_instances'][0].update(bounds=bounds(8,y),instance_index=index,clip=[0,0,100,100],
                                             translation=[0,0],color=[0,0,0,1],paint_opacity=opacity)
            for layer in nr:
                layer['ink_instances'][0]['atlas_source']=dict(glyph_id=layer['glyph_id'],
                    font=dict(sha256=layer['font_sha256'],source=layer['font_source'],face_index=layer['face_index']))
            native.insert(0,dict(identity=name,layers=nr))
        clip=dict(x=0,y=0,width=100,height=100)
        native_frame=dict(step='tile',targets=[dict(identity='sample',icons=native,clip=clip)])
        web_frame=dict(step='tile',targets=[dict(identity='sample',icons=web,clip=clip)])
        stack=dict(commands=[dict(kind=1,first=0,count=3,atlas_page=0)])
        origins=({'x':0,'y':0},)*2
        self.assertEqual(_formal_paint_gates(native_frame,web_frame,stack,origins,(0,0,64,64),1),[])
        native[1]['layers'][0]['ink_instances'][0]['paint_opacity']=1
        errors=_formal_paint_gates(native_frame,web_frame,stack,origins,(0,0,64,64),1)
        self.assertEqual(errors[0]['expected'],{'DownOutlined':1})
        self.assertEqual(errors[0]['actual'],{'CloseCircleFilled':1,'DownOutlined':1})
        web[1]['paths'][0]['d']='M0 0L1 1Z'
        with self.assertRaises(ProtocolError):
            _formal_paint_gates(native_frame,web_frame,stack,origins,(0,0,64,64),1)

    def test_actual_svg_and_actual_font_are_different_source_kinds(self):
        web,native,commands=self.receipts()
        verify_web_icon('SearchOutlined',web)
        verify_native_icon('SearchOutlined',native,commands)
        web['actual_font_sha256']=native[0]['font_sha256']
        with self.assertRaises(ProtocolError):verify_web_icon('SearchOutlined',web)

    def test_same_wrong_path_on_both_sides_cannot_grant_identity(self):
        web,native,commands=self.receipts()
        web['paths'][0]['d']='M0 0L1 1Z';native[0]['glyph_id']+=1
        with self.assertRaises(ProtocolError):verify_web_icon('SearchOutlined',web)
        with self.assertRaises(ProtocolError):verify_native_icon('SearchOutlined',native,commands)

    def test_web_path_rule_transform_clip_and_order_are_independent(self):
        for fault in ('missing','rule','transform','clip','order','stroke'):
            web,_,_=self.receipts()
            if fault=='missing':web['paths']=[]
            elif fault=='rule':web['paths'][0]['fill_rule']='invalid'
            elif fault=='transform':web['paths'][0]['ancestry'][0]['transform_attr']='translate(1)'
            elif fault=='clip':web['paths'][0]['ancestry'][0]['clip_path']='url(#wrong)'
            elif fault=='order':web['paths'][0]['ordinal']=1
            else:web['paths'][0]['stroke']='rgb(0,0,0)'
            with self.subTest(fault=fault),self.assertRaises(ProtocolError):verify_web_icon('SearchOutlined',web)

    def test_fractional_svg_accepts_only_the_declared_outer_calibration_translation(self):
        web,_,_=self.receipts()
        for path in web['paths']:
            neutral=path['ancestry'][0]
            path['ancestry']=[neutral,dict(neutral,svg_root=True),
                dict(neutral,calibration_owner=True,transform_css='matrix(1, 0, 0, 1, 0.5, 0.5)')]
        verify_web_icon('SearchOutlined',web,calibration_offset=(.5,.5))
        with self.assertRaises(ProtocolError):verify_web_icon('SearchOutlined',web)
        with self.assertRaises(ProtocolError):verify_web_icon('SearchOutlined',web,calibration_offset=(.25,.5))
        for fault in ('path','svg','clip','scale','missing-owner','owner-inside-svg'):
            bad=deepcopy(web)
            chain=bad['paths'][0]['ancestry']
            if fault=='path':chain[0]['transform_css']='matrix(1,0,0,1,0.5,0.5)'
            elif fault=='svg':chain[1]['transform_css']='matrix(1,0,0,1,0.5,0.5)'
            elif fault=='clip':chain[2]['clip_path']='inset(1px)'
            elif fault=='scale':chain[2]['transform_css']='matrix(1.2,0,0,1,0.5,0.5)'
            elif fault=='missing-owner':chain.pop()
            else:chain[1],chain[2]=chain[2],chain[1]
            with self.subTest(fault=fault),self.assertRaises(ProtocolError):
                verify_web_icon('SearchOutlined',bad,calibration_offset=(.5,.5))

    def test_native_layer_glyph_face_atlas_and_final_membership(self):
        for fault in ('layer','codepoint','glyph','font','face','atlas','command','generation'):
            _,native,commands=self.receipts()
            if fault=='layer':native=[]
            elif fault=='codepoint':native[0]['codepoint']+=1
            elif fault=='glyph':native[0]['glyph_id']+=1
            elif fault=='font':native[0]['font_sha256']='0'*64
            elif fault=='face':native[0]['face_index']=1
            elif fault=='atlas':native[0]['ink_instances'][0]['atlas_page']=1
            elif fault=='command':commands=[]
            else:native[0]['scene_identity']['generation']=0
            with self.subTest(fault=fault),self.assertRaises(ProtocolError):verify_native_icon('SearchOutlined',native,commands)

    def test_intrinsic_layer_opacity_stays_independent_of_effective_fade(self):
        _,native,commands=self.receipts()
        native[0]['ink_instances'][0]['paint_opacity']=.25
        verify_native_icon('SearchOutlined',native,commands)
        native[0]['layer_opacity']=.25
        with self.assertRaises(ProtocolError):verify_native_icon('SearchOutlined',native,commands)

    def test_invalid_effective_or_missing_intrinsic_opacity_is_rejected(self):
        for opacity in (float('nan'),-.1,1.1,True):
            _,native,commands=self.receipts()
            native[0]['ink_instances'][0]['paint_opacity']=opacity
            with self.subTest(opacity=opacity),self.assertRaises(ProtocolError):
                verify_native_icon('SearchOutlined',native,commands)
        _,native,commands=self.receipts()
        del native[0]['layer_opacity']
        with self.assertRaises(ProtocolError):verify_native_icon('SearchOutlined',native,commands)

    def test_official_outline_bytes_are_owned_immutable_and_hash_verified(self):
        import hashlib
        _,_,manifest,_=resource('SearchOutlined')
        data=official_font_bytes()
        self.assertIs(type(data),bytes)
        self.assertEqual(hashlib.sha256(data).hexdigest(),manifest['outline_container_sha256'])


if __name__=='__main__':unittest.main()
