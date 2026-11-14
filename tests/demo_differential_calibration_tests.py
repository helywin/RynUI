"""Finite calibration math/schema/evidence mechanisms, explicitly simulated inputs."""
from copy import deepcopy
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from demo_differential.calibration import (POLICY,CAPS,candidate_rows,control_cells,mutated_cells,
    png_metadata,verify_profile_evidence,evidence_manifest,outline_point)
from demo_differential.protocol import ProtocolError,FIXTURES,read,validate
from demo_differential.adapters import RunError


class CalibrationTests(unittest.TestCase):
    def test_independent_outline_winding_hole_and_distance(self):
        outer=[[0,0],[10,0],[10,10],[0,10],[0,0]]
        hole=[[3,3],[3,7],[7,7],[7,3],[3,3]]
        self.assertEqual(outline_point([outer,hole],1,5),(True,1))
        self.assertEqual(outline_point([outer,hole],5,5),(False,2))
        self.assertEqual(outline_point([outer,hole],12,5),(False,2))
        self.assertEqual(CAPS['pixels.coverage_local_relative_error'],.05)
        self.assertEqual(CAPS['text.projection_relative_error'],.05)
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
