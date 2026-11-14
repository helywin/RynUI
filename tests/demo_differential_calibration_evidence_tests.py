"""Simulated request/receipt mutations; never publishes a calibration profile."""
from copy import deepcopy
from pathlib import Path
import hashlib
import json
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from demo_differential.calibration import control_request,request_bytes,verify_trial_request,validate_control_count
from demo_differential.device_metrics import stable_device_metrics
from demo_differential.pilots import FONTS
from demo_differential.protocol import FIXTURES,ProtocolError,read


class EvidenceTests(unittest.TestCase):
    def manifest(self):
        return dict(corpus=read(FIXTURES/'pilot-calibration.json'),fonts=[{**f,'path':f'out/simulated/font-{i}.ttf'} for i,f in enumerate(FONTS)],
            scope=dict(actual_dpr=1.5),binding=dict(simulated=True),font_priors={},
            tools=dict(control_binary='out/simulated/control.exe'),environment=dict(native=dict(platform='windows'),
                native_capture=dict(clear_rgba=[1,1,1,1],font_raster_policies=[dict(antialias=True)])))

    def request(self,manifest,**kwargs):
        values=dict(side='native',partition='train',board='train',fault='none',identity='train-repeats',nonce='0'*32,
            launch_id='1'*32,repetitions=5,restart=False)
        values.update(kwargs)
        return control_request(manifest,**values)

    def test_dynamic_counters_are_retained_raw_but_not_compatible_identity(self):
        raw=dict(logical_width=1280,logical_height=900,pixel_width=1920,pixel_height=1350,dpr=1.5,
            submitted_frames=5,gpu_submissions=8,completed_readbacks=4,coordinate_width=1280,coordinate_height=900)
        before=deepcopy(raw)
        expected=stable_device_metrics(raw)
        raw.update(submitted_frames=55,gpu_submissions=88,completed_readbacks=44)
        self.assertEqual(stable_device_metrics(raw),expected)
        self.assertEqual(before['completed_readbacks'],4)
        for key in expected:
            wrong=deepcopy(raw);wrong[key]+=1
            with self.subTest(key=key),self.assertRaises(ProtocolError):stable_device_metrics(wrong)
        self.assertEqual(set(expected),{'logical_width','logical_height','pixel_width','pixel_height','dpr'})

    def test_canonical_request_has_exact_separate_sides_and_factors(self):
        manifest=self.manifest()
        native=self.request(manifest,partition='negative',board='train',fault='opacity-squared',identity='opacity-squared',repetitions=1)
        web=self.request(manifest,side='web',partition='negative',board='train',fault='opacity-squared',identity='opacity-squared',repetitions=1)
        self.assertEqual(len(native['cells']),312)
        pair=next((a,b) for a,b in zip(native['cells'],web['cells']) if a['kind']=='alpha' and a['paint_opacity']!=b['paint_opacity'])
        self.assertEqual(pair[0]['source_alpha'],pair[1]['source_alpha'])
        self.assertEqual(pair[0]['paint_opacity'],pair[1]['paint_opacity']**2)
        self.assertEqual(json.loads(request_bytes(native)),native)
        for values in [dict(repetitions=4),dict(restart=True),dict(board='holdout'),dict(fault='opacity-squared'),dict(launch_id='x')]:
            with self.subTest(values=values),self.assertRaises(ProtocolError):self.request(manifest,**values)

    def test_explicit_385_bound_does_not_truncate(self):
        cells=[{}]*385
        with self.assertRaises(ProtocolError):validate_control_count(cells)
        self.assertEqual(len(cells),385)
        bounded=cells[:384]
        self.assertIs(validate_control_count(bounded),bounded)

    def test_request_rebind_and_stale_ordinal_launch_or_snapshot_are_rejected(self):
        manifest=self.manifest();request=self.request(manifest)
        with tempfile.TemporaryDirectory(dir=ROOT/'out') as tmp:
            root=Path(tmp);out=root/'train-repeats/native';out.mkdir(parents=True)
            request_path=root/'train-repeats/native-request.json';request_path.write_bytes(request_bytes(request))
            launch_path=root/'train-repeats/launch.json'
            launch=dict(launch_id=request['process_launch_id'],pid=17,parent_pid=18,started_ns=1,
                argv=[str(ROOT/'out/simulated/control.exe'),'--request',str(request_path),'--output',str(out)])
            launch_path.write_text(json.dumps(launch))
            ref=dict(stream='train-repeats/native/stream-0.json',request='train-repeats/native-request.json',
                launch='train-repeats/launch.json',capture='train-repeats/native/capture-0.bmp',ordinal=0)
            trial=dict(identity='train-repeats.0',partition='train',board='train',fault='none',restart=False,native=ref)
            snapshot=json.dumps(dict(controls=[],commands=[]))
            record=dict(nonce=request['nonce']+'-0',request_sha256=hashlib.sha256(request_path.read_bytes()).hexdigest(),sample_ordinal=0,
                process=dict(pid=17,launch_id=request['process_launch_id'],started_ticks=33),process_nonce='17:33',
                controls=[],submitted_commands=[],snapshot_json=snapshot,snapshot_sha256=hashlib.sha256(snapshot.encode()).hexdigest())
            paths={ref['request']:request_path,ref['launch']:launch_path,ref['stream']:out/'stream-0.json'}
            verify_trial_request(manifest,trial,'native',record,paths)
            for fault in ('hash','ordinal','launch','pid','snapshot','request'):
                changed=deepcopy(record)
                if fault=='hash':changed['request_sha256']='0'*64
                elif fault=='ordinal':changed['sample_ordinal']=1
                elif fault=='launch':changed['process']['launch_id']='2'*32
                elif fault=='pid':changed['process']['pid']=19
                elif fault=='snapshot':changed['controls']=[dict(simulated_wrong=True)]
                else:
                    wrong=deepcopy(request);wrong['cells'][0]['rgb']=[1,2,3]
                    request_path.write_bytes(request_bytes(wrong));changed['request_sha256']=hashlib.sha256(request_path.read_bytes()).hexdigest()
                with self.subTest(fault=fault),self.assertRaises(ProtocolError):verify_trial_request(manifest,trial,'native',changed,paths)


if __name__=='__main__':unittest.main()
