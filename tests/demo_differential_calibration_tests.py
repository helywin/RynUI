"""Finite calibration math/schema/evidence mechanisms, explicitly simulated inputs."""
from copy import deepcopy
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from demo_differential.calibration import (POLICY,CAPS,candidate_rows,control_cells,mutated_cells,
    png_metadata,verify_profile_evidence,evidence_manifest,outline_point,outline_witness,measure_controls,
    weak_fill_pixel_matches,control_icon_outlines,control_text_origin,software_box_source_over)
from demo_differential.protocol import ProtocolError,FIXTURES,read,validate
from demo_differential.adapters import RunError


class CalibrationTests(unittest.TestCase):
    def test_weak_fill_checks_exact_endpoints_and_the_edge_envelope(self):
        for fg,bg in (([1]*3,[0]*3),([2]*3,[0]*3),([254]*3,[255]*3)):
            self.assertTrue(weak_fill_pixel_matches(fg,fg,bg,-1.5))
            self.assertFalse(weak_fill_pixel_matches(bg,fg,bg,-1.5))
            self.assertTrue(weak_fill_pixel_matches(bg,fg,bg,1.5))
            self.assertFalse(weak_fill_pixel_matches(fg,fg,bg,1.5))
            self.assertTrue(weak_fill_pixel_matches(fg,fg,bg,0))
            self.assertTrue(weak_fill_pixel_matches(bg,fg,bg,0))
            self.assertFalse(weak_fill_pixel_matches([128]*3,fg,bg,0))

    def test_independent_outline_winding_hole_and_distance(self):
        outer=[[0,0],[10,0],[10,10],[0,10],[0,0]]
        hole=[[3,3],[3,7],[7,7],[7,3],[3,3]]
        self.assertEqual(outline_point([outer,hole],1,5),(True,1))
        self.assertEqual(outline_point([outer,hole],5,5),(False,2))
        self.assertEqual(outline_point([outer,hole],12,5),(False,2))
        glyph=dict(bounds=[0,0,10,10],contours=[outer,hole])
        self.assertIs(outline_witness(glyph,1,5,.5),True)
        self.assertIs(outline_witness(glyph,5,5,.5),False)
        self.assertIs(outline_witness(glyph,10.5,5,.5),None)
        self.assertIs(outline_witness(glyph,11,5,.5),False)
        self.assertEqual(CAPS['pixels.coverage_local_relative_error'],.05)
        self.assertEqual(CAPS['text.projection_relative_error'],.05)

    def test_actual_control_text_origin_checks_primary_line_and_raster_motion(self):
        rect=dict(x=20,y=40,width=30,height=32)
        line=dict(units=1000,ascent=1069,descent=-293)
        native=dict(layout_origin=[20,60],raster_origin=[20,60])
        reference=dict(layout_origin=[20,60])
        self.assertEqual(control_text_origin(native,rect,line,12,1.5,native=True),native['raster_origin'])
        self.assertEqual(control_text_origin(reference,rect,line,12,1.5,native=False),reference['layout_origin'])
        for fields,native_side in ((dict(layout_origin=[21,60]),False),
                                   (dict(layout_origin=[20,58]),False),
                                   (dict(layout_origin=[20,59],raster_origin=[20,60]),True),
                                   (dict(layout_origin=[20,60],raster_origin=[20,62]),True),
                                   (dict(layout_origin=[20,60],raster_origin=[20,float('nan')]),True)):
            with self.subTest(fields=fields,native=native_side),self.assertRaises(ProtocolError):
                control_text_origin(fields,rect,line,12,1.5,native=native_side)

    def test_formal_icon_checks_viewport_and_atlas_origin_independently(self):
        rect=dict(x=32,y=40,width=12,height=12)
        cell=dict(rect=rect,font_size=12)
        prior=dict(units=1024,ascent=896)
        layer=dict(ink_instances=[dict(raster_origin=[32,51])])
        native=dict(source=dict(icon_layers=[layer]))
        web=dict(source=dict(svg=dict(bounds=rect.copy())))
        outlines=control_icon_outlines(cell,(native,web),(prior,),1)
        self.assertEqual(outlines[0][0][1],[32,51])
        self.assertEqual(outlines[1][0][1],[32,50.5])
        wrong=deepcopy(web)
        wrong['source']['svg']['bounds']['y']+=1
        with self.assertRaises(ProtocolError):
            control_icon_outlines(cell,(native,wrong),(prior,),1)
        layer['ink_instances'][0]['raster_origin'][1]+=1
        with self.assertRaises(ProtocolError):
            control_icon_outlines(cell,(native,web),(prior,),1)
    def test_frozen_candidate_and_holdout_are_independent(self):
        positive={'simulated-train':{name:0 for name in POLICY}}
        holdout={'simulated-holdout':{name:0 for name in POLICY}}
        negative={'simulated-negative':{name:max(values[-1],1) for name,values in POLICY.items()}}
        rows=candidate_rows(positive,holdout,negative)
        self.assertEqual([r['identity'] for r in rows],list(POLICY))
        for row in rows:
            self.assertEqual(row['threshold'],max(row['quantization_floor'],row['positive_max']+row['margin']))
            self.assertLessEqual(row['threshold'],row['hard_cap'])
        for fault in ('positive-over-cap','holdout-over-candidate','negative-no-gap','missing','unknown-nan'):
            p,h,n=deepcopy((positive,holdout,negative))
            name='pixels.coverage_mass_relative_error'
            if fault=='positive-over-cap':p['simulated-train'][name]=.051
            elif fault=='holdout-over-candidate':h['simulated-holdout'][name]=.002
            elif fault=='negative-no-gap':n['simulated-negative'][name]=.001
            elif fault=='missing':n['simulated-negative'].pop(name)
            else:p['simulated-train'][name]=float('nan')
            with self.subTest(fault=fault),self.assertRaises(RunError):candidate_rows(p,h,n)

    def test_failed_candidate_identifies_the_control_and_measured_bound(self):
        train={'train-0':{name:0 for name in POLICY}}
        holdout={'holdout-5':{name:0 for name in POLICY}}
        negatives={'negative-1':{name:1 for name in POLICY}}
        holdout['holdout-5']['pixels.coverage_mass_relative_error']=.038
        with self.assertRaisesRegex(RunError, r'pixels\.coverage_mass_relative_error: holdout holdout-5 measured 0\.038 exceeds 0\.001'):
            candidate_rows(train,holdout,negatives)

    def test_old_handwritten_profile_cannot_validate_or_publish(self):
        template=read(FIXTURES/'profiles/pilot-pinned-font.uncalibrated.json')
        validate('profile',template)
        with self.assertRaises(RunError):verify_profile_evidence(template)
        old=deepcopy(template)
        old.pop('calibration_kind')
        old['calibrated']=True
        with self.assertRaises(ProtocolError):validate('profile',old)
        wrong=deepcopy(template)
        wrong['color']={'profile':'sRGB-SDR','alpha':'opaque','channel_order':'RGBA'}
        with self.assertRaises(ProtocolError):validate('profile',wrong)

    def test_train_holdout_content_and_fault_input_are_frozen(self):
        corpus=read(FIXTURES/'pilot-calibration.json')
        before=deepcopy(corpus)
        a,b=control_cells(corpus,'train'),control_cells(corpus,'holdout')
        self.assertEqual(len(a),312) # Seven-font baseline306 + dual-factor2 + shadow-fill4.
        self.assertLessEqual(len(a),384)
        self.assertEqual(len(a),len(b))
        self.assertTrue(set(corpus['train']['text']).isdisjoint(corpus['holdout']['text']))
        moved=mutated_cells(corpus,'train','translate-all')
        self.assertTrue(all(y['rect']['x']==x['rect']['x']+1 for x,y in zip(a,moved)))
        erased=mutated_cells(corpus,'train','delete-icon')
        self.assertEqual(sum(bool(c.get('omit_draw')) for c in erased),1)
        self.assertEqual(corpus,before)
        self.assertEqual(corpus['masks'],[])

    def test_alpha_and_opacity_are_separate_actual_inputs(self):
        corpus=read(FIXTURES/'pilot-calibration.json')
        cells=control_cells(corpus,'train')
        for fault,changed,stable in [('alpha-squared','source_alpha','paint_opacity'),
                                    ('opacity-squared','paint_opacity','source_alpha')]:
            mutated=mutated_cells(corpus,'train',fault)
            pairs=[(a,b) for a,b in zip(cells,mutated) if a['kind']=='alpha']
            self.assertTrue(any(a[changed]!=b[changed] for a,b in pairs))
            self.assertTrue(all(a[stable]==b[stable] and b[changed]==a[changed]**2 for a,b in pairs))

    def test_translucent_shadow_still_checks_dual_background_alpha(self):
        black, white = [0, 0, 0], [255, 255, 255]
        shadow = dict(identity='shadow', kind='shadow', rect=dict(x=24, y=24, width=12, height=12),
                      radius=2, foreground=black, background=white,
                      shadow=dict(rgb=black, alpha=.5, offset=[0, 0], blur=4, spread=0),
                      occluding_fill=dict(rgb=white, alpha=.5))
        cells = [shadow]
        observed = [dict(identity='shadow', kind='shadow', rect=shadow['rect'], source=dict(
            foreground=black, alpha=.5, source_alpha=.5, paint_opacity=1, encoded_rgba=[0, 0, 0, .5], opacity=1,
            radius=2, offset=[0, 0], blur=4, spread=0,
            occluding_fill=dict(foreground=white, source_alpha=.5, paint_opacity=1)))]
        for panel, background in enumerate((black, white)):
            identity = f'p{panel}.alpha0'
            rect = dict(x=4 + 8 * panel, y=4, width=4, height=4)
            cells.append(dict(identity=identity, kind='alpha', rect=rect, foreground=black, background=background,
                              source_alpha=.5, paint_opacity=1))
            observed.append(dict(identity=identity, kind='alpha', rect=rect, source=dict(foreground=black, alpha=.5,
                source_alpha=.5, paint_opacity=1, encoded_rgba=[0, 0, 0, .5], opacity=1)))
        snapshots = [dict(capture={'dpr': 1}, controls=observed)] * 2
        images = [(bytes([255]) * (64 * 64 * 4), (64, 64))] * 2
        with patch('demo_differential.calibration.control_cells', return_value=cells), \
                patch('demo_differential.calibration.canvas_guard', return_value=({}, set())):
            inputs=({'alpha': [.5], 'alpha_rgb': black}, {'board': 'train'}, snapshots, images, {})
            details={}
            values, failures=measure_controls(*inputs,details=details)
            self.assertEqual((values,failures),measure_controls(*inputs))
        self.assertIn('shadow.independent-gaussian-profile', failures)
        self.assertIn('alpha.independent-unmix', failures)
        first=details['first_hard_failures']['shadow.independent-gaussian-profile']
        self.assertEqual((first['control'],first['side']),('shadow','native'))
        self.assertEqual(first['logical_rect'],shadow['rect'])
        self.assertEqual(len(first['physical_pixel']),2)
        self.assertEqual(details['first_hard_failures']['alpha.independent-unmix']['controls'],
                         ['p0.alpha0','p1.alpha0'])
        for name, row in details['metrics'].items():
            self.assertEqual((row['value'],row['hard_cap']),(values[name],CAPS[name]))

    def test_ink_edge_and_solid_color_are_measured_independently(self):
        from demo_differential.pilots import FONTS
        fingerprint=next(face['sha256'] for face in FONTS if face['identity']=='noto-sans' and face['weight']==400)
        rect=dict(x=10,y=10,width=10,height=10)
        white=[255,255,255]
        cell=dict(identity='p0.glyph0',kind='text',rect=rect,content='A',font_size=12,font_weight=400,
                  foreground=white,background=[0,0,0])
        source=dict(foreground=white,alpha=1,source_alpha=1,paint_opacity=1,encoded_rgba=[1,1,1,1],opacity=1,
                    layout_origin=[10,20],actual_font_sha256=fingerprint,primary_font_sha256=fingerprint,
                    content='A',font_size=12,font_weight=400,advance=8,glyph_id=25,ink_boxes=[rect])
        observations=[dict(capture={'dpr':1},controls=[dict(identity=cell['identity'],kind='text',rect=rect,source=source)])]*2
        priors={fingerprint+':line':{},fingerprint+':A':dict(glyph_id=25,units=12,advance=8,bounds=[0,0,8,8],
                    contours=[[[0,0],[8,0],[8,8],[0,8],[0,0]]])}
        web=bytearray((0,0,0,255)*32*32)
        for y in range(12,20):
            for x in range(10,18):
                i=4*(y*32+x)
                web[i:i+3]=bytes(white)
        with patch('demo_differential.calibration.control_cells',return_value=[cell]), \
                patch('demo_differential.calibration.canvas_guard',return_value=({},set())), \
                patch('demo_differential.calibration.control_text_origin',return_value=[10,20]):
            for x,y,is_core in ((10,14,False),(13,15,True)):
                native=web.copy()
                i=4*(y*32+x)
                native[i:i+3]=bytes([128]*3)
                images=[(native,(32,32)),(web,(32,32))]
                details={}
                values,failures=measure_controls({'alpha':[]},{'board':'train'},observations,images,priors,details=details)
                self.assertGreater(values['text.coverage_mass_relative_error'],0)
                self.assertEqual(values['color.text_interior_delta_e00']>1,is_core)
                self.assertEqual('font.independent-outline' in failures,is_core)
                if is_core:
                    self.assertEqual(details['metrics']['color.text_interior_delta_e00']['physical_pixel'],[x,y])

    def test_fractional_box_uses_its_source_geometry_to_bound_antialiasing(self):
        rect=dict(x=10.75,y=10.75,width=8,height=8)
        cell=dict(identity='p0.fill',kind='fill',rect=rect,foreground=[255]*3,background=[0]*3)
        row=dict(identity=cell['identity'],kind='fill',rect=rect,source=dict(foreground=[255]*3,alpha=1,
                 source_alpha=1,paint_opacity=1,encoded_rgba=[1,1,1,1],opacity=1))
        observations=[dict(capture={'dpr':1},controls=[row])]*2
        image=bytearray((0,0,0,255)*32*32)
        for y in range(10,19):
            for x in range(10,19):
                width=min(x+1,18.75)-max(x,10.75)
                height=min(y+1,18.75)-max(y,10.75)
                i=4*(y*32+x)
                image[i:i+3]=bytes([round(255*width*height)]*3)
        with patch('demo_differential.calibration.control_cells',return_value=[cell]), \
                patch('demo_differential.calibration.canvas_guard',return_value=({},set())):
            inputs=({'alpha':[]},{'board':'holdout'},observations,[(image,(32,32))]*2,{})
            self.assertEqual(measure_controls(*inputs)[1],[])
            # Even the same leaked pixel on both sides must fail the source gate.
            i=4*(10*32+8)
            image[i:i+3]=bytes([32]*3)
            details={}
            _,failures=measure_controls(*inputs,details=details)
            name='geometry.independent-shape-or-guard'
            self.assertIn(name,failures)
            self.assertEqual(details['first_hard_failures'][name]['physical_pixel'],[8,10])

    def test_independent_overlap_allows_the_exact_software_edge_byte_encoding(self):
        background=[0,0,0]
        first,second=[17,83,201],[201,17,83]
        rect=dict(x=10.75,y=10.75,width=8,height=8)
        cell=dict(identity='p0.sourceover',kind='sourceover',rect=rect,foreground=first,background=background,
                  rgb=first,source_alpha=.5,second_rgb=second,second_alpha=.5)
        row=dict(identity=cell['identity'],kind='sourceover',rect=rect,source=dict(
            foreground=first,alpha=.5,source_alpha=.5,paint_opacity=1,encoded_rgba=[*[v/255 for v in first],.5],
            opacity=1,second_foreground=second,second_alpha=.5,primitive_index=0,primitive_index_commands=[0],
            second_primitive_index=1,second_primitive_index_commands=[1],clip_kind='viewport',translation=[0,0],
            second_translation=[0,0]))
        prov=dict(browser=dict(version='141.0.7390.37',headless=True,actual_gpu=dict(feature_status=dict(
            rasterization='disabled_software',gpu_compositing='disabled_software'))))
        commands=[dict(kind='quad',first_instance=i,instance_count=1) for i in range(2)]
        observations=[dict(capture={'dpr':1},controls=[row],submitted_commands=commands),
                      dict(capture={'dpr':1},controls=[row],provenance=prov)]
        web=bytearray((0,0,0,255)*32*32)
        native=web.copy()
        for y in range(10,19):
            for x in range(10,19):
                k=(min(x+1,18.75)-max(x,10.75))*(min(y+1,18.75)-max(y,10.75))
                i=4*(y*32+x)
                web[i:i+3]=bytes(software_box_source_over(background,(first,second),(.5,.5),k))
                a=.5*k
                native[i:i+3]=bytes(round(v*a+round(u*a)*(1-a)) for u,v in zip(first,second))
        i=4*(10*32+10)
        self.assertEqual(tuple(web[i:i+3]),(6,1,7))
        with patch('demo_differential.calibration.control_cells',return_value=[cell]), \
                patch('demo_differential.calibration.canvas_guard',return_value=({},set())):
            inputs=({'alpha':[]},{'board':'holdout'},observations,[(native,(32,32)),(web,(32,32))],{})
            values,failures=measure_controls(*inputs)
            self.assertEqual(failures,[])
            self.assertGreater(values['overlap.contribution_max_error'],0)
            observations[1].pop('provenance')
            self.assertIn('overlap.output-material',measure_controls(*inputs)[1])
            observations[1]['provenance']=prov
            web[i+1]=0
            self.assertIn('overlap.output-material',measure_controls(*inputs)[1])

    def test_seven_font_scope_and_fixed_glyph_layout(self):
        from demo_differential.pilots import FONTS
        corpus=read(FIXTURES/'pilot-calibration.json')
        template=read(FIXTURES/'profiles/pilot-pinned-font.uncalibrated.json')
        self.assertEqual(template['font_hashes'],[f['sha256'] for f in FONTS])
        self.assertEqual(len(template['scope']['font_sizes_weights']),12)
        glyphs=[c for c in control_cells(corpus,'holdout') if c['kind']=='text']
        self.assertEqual(len(glyphs),192)
        self.assertTrue(all(c['rect']['y']+c['rect']['height']+4<760 for c in glyphs))

    def test_png_crc_chunks_and_true_rgb_format(self):
        def chunk(kind,data):
            return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
        png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b'\0\x11\x53\xc9'))+chunk(b'IEND',b'')
        with tempfile.TemporaryDirectory(dir=ROOT/'out') as directory:
            path=Path(directory)/'simulated.png'
            path.write_bytes(png)
            facts=png_metadata(path)
            self.assertEqual(facts['format'],'PNG RGB8')
            self.assertEqual(facts['color_chunks'],[])
            for broken in (png[:-1],png+b'x',png[:35]+bytes([png[35]^1])+png[36:]):
                path.write_bytes(broken)
                with self.assertRaises(ProtocolError):png_metadata(path)

    def test_summary_without_owned_capture_evidence_is_rejected(self):
        template=read(FIXTURES/'profiles/pilot-pinned-font.uncalibrated.json')
        template['calibrated']=True
        # Explicit mechanism forgery, never a platform-generated profile.
        template['evidence']={'path':'out/absent/'+('0'*64)+'.json','sha256':'0'*64}
        with self.assertRaises(RunError):evidence_manifest(template)


if __name__=='__main__':unittest.main()
