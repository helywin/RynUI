"""Simulated source receipts exercise independent official-resource gates; no GPU."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from demo_differential.formal_icon_source import resource,verify_web_icon,verify_native_icon,official_font_bytes
from demo_differential.protocol import ProtocolError


class FormalIconTests(unittest.TestCase):
    def receipts(self):
        entry,svg,manifest,cmap=resource('SearchOutlined')
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
