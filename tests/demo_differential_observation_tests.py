"""Missing execution is nonzero even when an adapter process reports success."""
from copy import deepcopy
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from demo_differential.observations import IncompleteRun, validate_observations, image_extent
from demo_differential.protocol import ProtocolError
from demo_differential.oracle import evaluate, projected_trace
from demo_differential_protocol_tests import valid_case


def bmp(width=4,height=4):
    stride=(width*3+3)//4*4
    pixels=bytes([255,255,255]*width+[0]*(stride-width*3))*height
    return b'BM'+struct.pack('<IHHI',54+len(pixels),0,0,54)+struct.pack('<IiiHHIIiiII',40,width,height,1,24,0,len(pixels),0,0,0,0)+pixels


def records_for(case,directory,nonce='run-1'):
    metrics={'dpr':1,'pixel_width':4,'pixel_height':4,'logical_width':4,'logical_height':4}
    completed=[]; checked=[]; records=[]
    def record(kind,step=''):
        return {'schema_version':1,'type':kind,'nonce':nonce,'case':case['identity'],'step':step,'adapter':'control.v1',
            'provenance':{'source':{'source_hash':'a'*64},'preset':'linux-gcc','configuration':'Release'},
            'effective_inputs':case['fixture'],'receipt':{'dispatched':kind=='receipt','hit':'','consumed':False,'layer':'observer'},
            'state':{'focused_target':''},'callbacks':[],'targets':[],'captures':[],
            'resources':{'metrics':metrics},'completed_steps':list(completed),'completed_checkpoints':list(checked)}
    records.append(record('hello'))
    for action in case['actions']:
        completed.append(action['step']);records.append(record('receipt',action['step']))
        if action['kind']!='checkpoint': continue
        checked.append(action['step']);row=record('checkpoint',action['step'])
        item=next(c for c in case['checkpoints'] if c['step']==action['step'])
        for identity in item['targets']:
            row['targets'].append({'identity':identity,'key':'','bounds':{'x':0,'y':0,'width':4,'height':4},
                'clip':{'x':0,'y':0,'width':4,'height':4},'scroll':{},'state':{'value':'correct'},'text':[],'icons':[],
                'tokens':{},'owner_generation':1,'scene_revision':1})
        name=f'capture-{len(checked)}.bmp';path=Path(directory)/name;path.write_bytes(bmp())
        row['captures']=[{'path':name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'nonce':f'{nonce}-capture-{len(checked)}','checkpoint':action['step'],'scene_revision':1,
            'width':4,'height':4,'dpr':1,'fence_completed':True,'framebuffer_format':'unit-control',
            'channel_order':'RGBA','alpha':'opaque','color_profile':'unit-control'}]
        records.append(row)
    records.append(record('complete'))
    return records


class ObservationTests(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(dir=ROOT/'out',prefix='receipt-unit-')
        self.addCleanup(self.directory.cleanup)
        self.case=valid_case();self.records=records_for(self.case,self.directory.name)

    def check_records(self,records=None,**kwargs):
        return validate_observations(records or self.records,self.directory.name,self.case,'run-1',**kwargs)

    def test_complete_receipts_and_current_capture(self):
        result=self.check_records(adapter='control.v1',source_hash='a'*64,preset='linux-gcc',configuration='Release')
        self.assertEqual(list(result['checkpoints']),['initial'])

    def test_exit_zero_with_missing_or_incomplete_records(self):
        for fault in ('empty','hello','complete','receipt','dispatched','target','capture','case','nonce','order'):
            rows=deepcopy(self.records)
            if fault=='empty': rows=[]
            elif fault=='hello': rows.pop(0)
            elif fault=='complete': rows.pop()
            elif fault=='receipt': rows.pop(1)
            elif fault=='dispatched': rows[1]['receipt']['dispatched']=False
            elif fault=='target': rows[2]['targets']=[]
            elif fault=='capture': rows[2]['captures']=[]
            elif fault=='case': rows[1]['case']='antd665.other.wrong.v1'
            elif fault=='nonce': rows[1]['nonce']='old-run'
            else: rows[1:3]=reversed(rows[1:3])
            with self.subTest(fault=fault),self.assertRaises(IncompleteRun):
                validate_observations(rows,self.directory.name,self.case,'run-1')

    def test_old_source_wrong_preset_and_configuration_rejected(self):
        for kwargs in ({'source_hash':'b'*64},{'preset':'linux-clang'},{'configuration':'Debug'}):
            with self.subTest(kwargs=kwargs),self.assertRaises(ProtocolError): self.check_records(**kwargs)

    def test_current_capture_hash_fence_extent_dpr_and_scene_closure(self):
        for fault in ('hash','fence','extent','dpr','revision','missing','escape'):
            rows=deepcopy(self.records);capture=rows[2]['captures'][0]
            if fault=='hash': capture['sha256']='0'*64
            elif fault=='fence': capture['fence_completed']=False
            elif fault=='extent': capture['width']=3
            elif fault=='dpr': capture['dpr']=2
            elif fault=='revision': capture['scene_revision']=2
            elif fault=='missing': capture['path']='not-produced.bmp'
            else: capture['path']='../escape.bmp'
            with self.subTest(fault=fault),self.assertRaises(ProtocolError): self.check_records(rows)

    def test_callback_stream_cannot_be_rewritten_or_deduplicated(self):
        callback={'seq':0,'step':'initial','time_us':0,'target':'gallery.state.default','key':'',
                  'event':'activate','payload':None,'owner_generation':1,'layer':'native-core-dispatch'}
        rows=deepcopy(self.records);rows[1]['callbacks']=[callback]
        with self.assertRaises(IncompleteRun): self.check_records(rows)
        rows[2]['callbacks']=[callback];rows[3]['callbacks']=[{**callback,'seq':1}]
        with self.assertRaises(IncompleteRun): self.check_records(rows)

    def test_seventeen_requested_actions_cannot_pass_with_old_eight_action_cap(self):
        case=valid_case();checkpoint=case['actions'][0]
        case['actions']=[{**deepcopy(checkpoint),'step':f'click-{i}','kind':'pointer-cancel'} for i in range(17)]+[checkpoint]
        rows=records_for(case,self.directory.name)
        rows=[row for row in rows if row['type']!='receipt' or row['step']=='initial' or int(row['step'].split('-')[1])<8]
        rows[-1]['completed_steps']=[action['step'] for action in case['actions'][:8]]+['initial']
        with self.assertRaises(IncompleteRun): validate_observations(rows,self.directory.name,case,'run-1')

    def test_checksum_valid_png_must_contain_complete_decodable_pixels(self):
        import zlib
        def chunk(tag, contents):
            return struct.pack('>I', len(contents)) + tag + contents + struct.pack('>I', zlib.crc32(tag + contents))
        header = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0))
        path = Path(self.directory.name) / 'png-control.png'
        valid = zlib.compress(b'\0\xff\xff\xff\xff')
        path.write_bytes(header + chunk(b'IDAT', valid) + chunk(b'IEND', b''))
        self.assertEqual(image_extent(path), (1, 1))
        for payload in (b'not-zlib', valid[:-1], zlib.compress(b'\0'),
                        zlib.compress(b'\x05\xff\xff\xff\xff'), valid + b'trailing'):
            path.write_bytes(header + chunk(b'IDAT', payload) + chunk(b'IEND', b''))
            with self.subTest(payload=payload), self.assertRaises(IncompleteRun):
                image_extent(path)

    def test_double_wrong_sides_still_fail_independent_oracle(self):
        case=deepcopy(self.case)
        case['oracle']=[{'op':'equals','step':'initial','side':'both','origin':'independent-contract',
                         'path':'/targets/0/state/value','value':'correct'}]
        rows=deepcopy(self.records);rows[2]['targets'][0]['state']['value']='both-wrong'
        observed=self.check_records(rows)
        self.assertEqual(projected_trace(observed),[])
        self.assertEqual(len(evaluate(case,observed,'web')),1)
        self.assertEqual(len(evaluate(case,observed,'native')),1)

    def test_absent_dispatch_cannot_satisfy_zero_callback_oracle(self):
        rows=deepcopy(self.records);rows[1]['receipt']['dispatched']=False
        with self.assertRaises(IncompleteRun): self.check_records(rows)


if __name__=='__main__':unittest.main()
