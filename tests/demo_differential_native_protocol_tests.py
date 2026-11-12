"""Bounded native JSON/scenario parser negative controls; no window or GPU."""
from copy import deepcopy
import json
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from demo_differential.source_closure import source_manifest
from demo_differential.inventory import source_inventory
from demo_differential_protocol_tests import valid_case


@unittest.skipUnless(os.environ.get('RYNUI_DEMO_PROTOCOL_BINARY'), 'use formal HEADLESS protocol target')
class NativeProtocolTests(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory(prefix='native-protocol-',dir=ROOT/'out')
        self.addCleanup(self.directory.cleanup)
        self.path=Path(self.directory.name)/'request.json'
        font=Path(self.directory.name)/'font.ttf'; font.write_bytes(b'parser-path-control')
        case=valid_case()
        sample=next(r for r in source_inventory()['samples'] if r['identity']==case['native']['sample_id'])
        case['native'].update(source_function=sample['source_function'])
        self.request={'schema_version':1,'nonce':'parser-control-1','definition':case,'fault':'none',
                      'font_files':[{'identity':'path-control','path':str(font),'sha256':hashlib.sha256(font.read_bytes()).hexdigest(),'face_index':0,'weight':400}]}
        self.binary=os.environ['RYNUI_DEMO_PROTOCOL_BINARY']

    def execute(self, value=None, raw=None):
        self.path.write_text(raw if raw is not None else json.dumps(value or self.request),encoding='utf-8')
        return subprocess.run([self.binary,str(self.path)],capture_output=True,text=True,timeout=30)

    def test_sha256_standard_vectors(self):
        result=subprocess.run([self.binary,'--hash-controls'],capture_output=True,text=True,timeout=30)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_valid_scenario(self):
        result=self.execute()
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('protocol-control.v1 1 1',result.stdout)

    def test_duplicate_unknown_truncated_unicode_and_nonfinite(self):
        for raw in ('{"x":1,"x":2}', '{"schema_version":', '{"x":NaN}', '{"x":1e999}',
                    '{"x":"\\ud800"}', '{"x":01}', '{"x":1.}', '['*60+'0'+']'*60):
            with self.subTest(raw=raw): self.assertEqual(self.execute(raw=raw).returncode,2)
        for fault in ('unknown','no-oracle','missing-action','bad-source','escape','unknown-fault','unknown-fixture','scripted-policy'):
            request=deepcopy(self.request)
            if fault=='unknown': request['command']='eval'
            elif fault=='no-oracle': request['definition']['oracle']=[]
            elif fault=='missing-action': request['definition']['actions']=[]
            elif fault=='bad-source': request['definition']['native']['source_function']='fake_factory'
            elif fault=='escape': request['font_files'][0]['path']=str(ROOT/'README.md')
            elif fault=='unknown-fixture':request['definition']['fixture']['silent_override']=True
            elif fault=='scripted-policy':request['definition']['fixture']['parent_policy']='scripted'
            else: request['fault']='overwrite-reference'
            with self.subTest(fault=fault): self.assertEqual(self.execute(request).returncode,2)

    def test_action_receipt_grammar_order_and_checkpoint(self):
        for fault in ('duplicate','clock','checkpoint','extra-parameter','unknown-kind','outside-pointer'):
            request=deepcopy(self.request); definition=request['definition']; action=definition['actions'][0]
            if fault=='duplicate': definition['actions'].append(deepcopy(action))
            elif fault=='clock': action['time_us']=-1
            elif fault=='checkpoint': definition['checkpoints'][0]['step']='missing'
            elif fault=='extra-parameter': action['parameters']['callback']='execute'
            elif fault=='unknown-kind': action['kind']='eval'
            else: action.update(kind='pointer-move',parameters={'x_fraction':2,'y_fraction':.5})
            with self.subTest(fault=fault): self.assertEqual(self.execute(request).returncode,2)

    def test_actual_build_input_closure(self):
        result=subprocess.run([self.binary,'--manifest'],capture_output=True,text=True,timeout=30)
        self.assertEqual(result.returncode,0,result.stderr)
        embedded=json.loads(result.stdout)
        self.assertEqual(embedded,source_manifest(ROOT),'source changed without rebuilding the protocol binary')
        paths={row['path'] for row in embedded['files']}
        self.assertIn('src/component/button_component.cpp',paths)
        self.assertIn('examples/token_gallery/gallery_demo_protocol.cpp',paths)
        self.assertIn('cmake/dependencies/RynUIDependencyLock.cmake',paths)


if __name__=='__main__': unittest.main()
