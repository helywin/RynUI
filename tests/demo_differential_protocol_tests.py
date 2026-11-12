"""Finite action/oracle protocol controls independent of native/Web execution."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_differential.protocol import ProtocolError, validate, loads


def valid_case():
    return {'schema_version':1, 'identity':'antd665.button.protocol-control.v1', 'revision':1,
        'official':{'repository':'https://github.com/ant-design/ant-design',
                    'commit':'4a39f54842eade4e565ab336ef6097cd7e723cdd',
                    'demo_path':'components/button/demo/basic.tsx', 'demo_sha256':'a'*64,
                    'source_paths':[{'path':'components/button/button.tsx','sha256':'b'*64}],
                    'adapter_id':'button', 'adapter_sha256':'c'*64, 'mode':'official-component-adapted',
                    'adaptation':[]},
        'native':{'route':'button','family':'input','sample_id':'gallery.sample.button.basics',
                  'targets':['gallery.state.default'], 'source_function':'add_state_samples',
                  'public_header':'include/ryn/button.hpp'}, 'mapping':'paired',
        'fixture':{'data':{},'props':{},'slots':{},'parent_policy':'accept','clock_epoch_us':0,
                   'seed':1,'timezone':'UTC','viewport':{'width':600,'height':400,'dpr':'native'},
                   'container':{'width':400,'height':200,'padding':0,'gap':8,'layout':'row'},
                   'theme':{},'locale':'zh-CN','format_locale':'zh-CN','direction':'ltr',
                   'fonts':[{'identity':'noto-sans','sha256':'d'*64,'face_index':0,'weight':400}],
                   'font_policy':'pinned','motion':'disabled'},'profile':'linux-pinned-font',
        'actions':[{'step':'initial','kind':'checkpoint','target':'sample','time_us':0,
                    'layer':'observer','preconditions':{},'parameters':{}}],
        'checkpoints':[{'step':'initial','targets':['gallery.state.default'],'layers':['geometry'],
                        'rois':[{'identity':'context','kind':'sample-context','target':'sample','guard_px':2}]}],
        'oracle':[{'op':'count','side':'both','origin':'independent-contract','step':'initial',
                   'stream':'callbacks','target':'gallery.state.default','event':'activate','equals':0}],
        'features':['default-visible-context'], 'dependencies':['include/ryn/button.hpp']}


class ProtocolTests(unittest.TestCase):
    def test_valid_independent_case(self):
        validate('case',valid_case())

    def test_missing_oracle_unknown_fields_and_native_expected(self):
        for fault in ('no-oracle','unknown','native-expected','executable'):
            value=valid_case()
            if fault=='no-oracle':
                value['oracle']=[]
            elif fault=='unknown':
                value['fixture']['measurement_from_native']=True
            elif fault=='native-expected':
                value['oracle'][0]['origin']='native-observation'
            else:
                value['actions'][0]['parameters']['script']='setValue(0)'
            with self.subTest(fault=fault), self.assertRaises(ProtocolError):
                validate('case',value)

    def test_duplicate_steps_wrong_order_and_checkpoint_gap(self):
        for fault in ('duplicate','clock','gap','missing-action'):
            value=valid_case()
            if fault=='duplicate':
                value['actions']*=2
            elif fault=='clock':
                value['actions'][0]['time_us']=20
                value['actions'].append({**deepcopy(value['actions'][0]),'step':'later','time_us':10})
            elif fault=='gap':
                value['checkpoints'][0]['step']='never-executed'
            else:
                value['actions'][0].update(kind='advance-clock',parameters={'to_us':0})
            with self.subTest(fault=fault), self.assertRaises(ProtocolError):
                validate('case',value)

    def test_non_finite_nested_unknown_action_and_path(self):
        for fault in ('nan','action','path','bool-number'):
            value=valid_case()
            if fault=='nan':
                value['fixture']['data']['x']=float('nan')
            elif fault=='action':
                value['actions'][0]['kind']='eval'
            elif fault=='path':
                value['official']['demo_path']='../secret'
            else:
                value['fixture']['viewport']['width']=True
            with self.subTest(fault=fault), self.assertRaises(ProtocolError):
                validate('case',value)

    def test_json_limits(self):
        with self.assertRaises(ProtocolError):
            loads('['*60+'0'+']'*60)


if __name__=='__main__':
    unittest.main()
