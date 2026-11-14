"""Finite independent pilot calibration; ordinary run only verifies/consumes it.

Only the fixed control corpus is admissible. All evidence remains under out/.
Saved statistics are never trusted in lieu of their underlying capture streams.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

from .adapters import RunError
from .protocol import ROOT, FIXTURES, ProtocolError, read, relative_path, output_path, validate, finite
from .color_metrics import rgb8_delta_e00, source_over, unmix_coverage,dual_background,visible_basis,solve_visible_contributions
from .regions import crop_rgba, coverage_metrics,reference_aa_is_bounded,shadow_profile_metrics
from .device_metrics import stable_device_metrics

KIND = 'pilot-independent-v1'
CASES = ['antd665.button.basic-states.v1','antd665.input.controlled-edit.v1','antd665.select.popup-lifecycle.v1']
MAX_CONTROLS = 384
# Fixed before observing a control. No histogram, gamma fit, or adaptive margins.
POLICY = {
    'geometry.logical_px': ('logical-px','relative-fixed-host',.25,0,0,.25),
    'text.geometry_logical_px': ('logical-px','semantic-group',.5,0,0,.25),
    'transport.max_channel_residual': ('byte','opaque-transport-sentinel',0,0,0,1),
    'color.flat_delta_e00': ('delta-e00','srgb-D65-flat',1,0,.01,.1),
    'color.text_interior_delta_e00': ('delta-e00','srgb-D65-ink-core',1,0,.01,.1),
    'pixels.coverage_mass_relative_error': ('relative','per-shape',.05,0,.001,.002),
    'pixels.contour_distance_px': ('physical-px','integer-coverage-half-isocontour',.5,0,0,.5),
    'pixels.coverage_local_relative_error': ('relative','fixed-2x2-coverage-cells',.05,0,.001,.002),
    'text.coverage_mass_relative_error': ('relative','per-semantic-group',.05,0,.001,.002),
    'text.contour_distance_px': ('physical-px','integer-coverage-half-isocontour',1,0,0,.5),
    'text.projection_relative_error': ('relative','per-group-row-and-column',.05,0,.001,.002),
    'shadow.coverage_mass_relative_error': ('relative','soft-shadow-visible-support',.05,0,.001,.002),
    'shadow.projection_relative_error': ('relative','soft-shadow-row-and-column',.05,0,.001,.002),
    'shadow.profile_max_error': ('normalized-alpha','soft-shadow-fixed-profile',.02,0,.001,.002),
    'shadow.tail_max_error': ('normalized-alpha','soft-shadow-fixed-tail',2/255,0,0,1/255),
    'overlap.contribution_max_error': ('normalized-alpha','fixed-source-visible-contribution',.05,0,.001,.002),
}
CAPS = {name: values[2] for name,values in POLICY.items()}


def digest(value):
    finite(value)
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode()).hexdigest()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def exact_fields(value, names, label):
    if not isinstance(value,dict) or set(value) != set(names.split()):
        raise ProtocolError(label+' has missing or unknown fields')
    return value


def required(value, keys, label):
    for key in keys:
        if key not in value or value[key] is None or value[key] == '' or value[key] == {} or value[key] == []:
            raise RunError(3,'calibration',label+' binding unavailable: '+key)
    finite(value)
    return {key:value[key] for key in keys}


def request_bytes(request):
    finite(request)
    return json.dumps(request,ensure_ascii=False,sort_keys=True,separators=(',',':'),allow_nan=False).encode()+b'\n'


def control_request(manifest, *,side,partition,board,fault,identity,nonce,launch_id,repetitions,restart):
    """Canonical producer input rebuilt solely from the fixed corpus and bindings."""
    import re
    if side not in ('native','web') or partition not in ('train','holdout','negative'):
        raise ProtocolError('unknown canonical control side/partition')
    if not re.fullmatch(r'[0-9a-f]{32}',nonce) or not re.fullmatch(r'[0-9a-f]{32}',launch_id):
        raise ProtocolError('canonical request nonce/launch identity malformed')
    if type(repetitions) is not int or not 1<=repetitions<=15 or type(restart) is not bool:
        raise ProtocolError('canonical request repetition/restart malformed')
    if partition=='negative':
        declaration=next((n for n in manifest['corpus']['negative'] if n['identity']==identity),None)
        if not declaration or fault!=declaration['fault'] or board!='train' or restart or repetitions!=1:
            raise ProtocolError('canonical negative identity differs')
    elif (board!=partition or fault!='none' or identity!=partition+('-restart' if restart else '-repeats') or
            restart and repetitions!=1 or not restart and repetitions<5):
        raise ProtocolError('canonical positive identity differs')
    mutation=fault if side=='native' or identity.startswith('both-') else 'none'
    cells=mutated_cells(manifest['corpus'],board,mutation)
    for cell in cells:
        if cell['kind']=='text' and cell.get('clip_glyph_right_px'):
            descriptor=next(f for f in manifest['fonts'] if f['weight']==cell['font_weight'] and
                f['identity']==('noto-sans' if ord(cell['content'])<128 else 'noto-sans-cjk-sc'))
            prior=manifest['font_priors'][descriptor['sha256']+':'+cell['content']]
            cell['outline_right_logical']=prior['bounds'][2]*cell['font_size']/prior['units']
    return dict(schema_version=1,calibration_kind=KIND,nonce=nonce,process_launch_id=launch_id,
        side=side,partition=partition,board=board,fault=fault,restart=restart,repetitions=repetitions,
        actual_dpr=manifest['scope']['actual_dpr'],font_files=[{**f,'path':str(output_path(ROOT/relative_path(f['path'])))} for f in manifest['fonts']],
        binding=manifest['binding'],cells=cells,panels=manifest['corpus']['panels'],
        clear_rgba=manifest['environment']['native_capture']['clear_rgba'],
        raster_policy=manifest['environment']['native_capture']['font_raster_policies'][0])


def verify_trial_request(manifest,trial,side,record,paths):
    ref=trial[side]
    for role in ('request','launch'):
        if ref[role] not in paths:raise RunError(4,'protocol','missing canonical '+role+' artifact')
    request=read(paths[ref['request']]);launch=read(paths[ref['launch']])
    identity=Path(ref['stream']).parts[0]
    if trial['identity']!=(identity if trial['partition']=='negative' else identity+'.'+str(ref['ordinal'])):
        raise ProtocolError('trial identity differs from canonical request/repetition')
    rebuilt=control_request(manifest,side=side,partition=trial['partition'],board=trial['board'],fault=trial['fault'],
        identity=identity,nonce=request.get('nonce',''),launch_id=request.get('process_launch_id',''),
        repetitions=request.get('repetitions'),restart=trial['restart'])
    if request!=rebuilt or paths[ref['request']].read_bytes()!=request_bytes(rebuilt):
        raise ProtocolError('canonical '+side+' request bytes/semantics differ')
    if record.get('request_sha256')!=sha(paths[ref['request']]) or record.get('sample_ordinal')!=ref['ordinal']:
        raise RunError(4,'protocol','control capture is bound to another request/repetition')
    if (record['nonce']!=request['nonce']+'-'+str(ref['ordinal']) or not 0<=ref['ordinal']<request['repetitions']):
        raise RunError(4,'protocol','control sample ordinal/nonce differs')
    actual=record.get('process',{})
    if (actual.get('launch_id')!=launch.get('launch_id') or launch.get('launch_id')!=request['process_launch_id'] or
            type(actual.get('pid')) is not int or actual['pid']<=0 or type(launch.get('pid')) is not int or launch['pid']<=0 or
            type(launch.get('started_ns')) is not int or launch['started_ns']<=0):
        raise RunError(4,'protocol','actual process launch receipt differs')
    if side=='web' or manifest['environment']['native']['platform']=='windows':
        if actual['pid']!=launch['pid']:raise RunError(4,'protocol','worker PID differs from actual launched process')
    elif launch['pid'] not in actual.get('ancestry',[]):
        raise RunError(4,'protocol','actual worker is outside the private launcher process ancestry')
    if record['process_nonce']!=str(actual['pid'])+':'+str(actual.get('started_ticks')):
        raise RunError(4,'protocol','producer process identity is not actual PID/start')
    snapshot=record.get('snapshot_json')
    if (not isinstance(snapshot,str) or hashlib.sha256(snapshot.encode()).hexdigest()!=record.get('snapshot_sha256') or
            json.loads(snapshot)!=dict(controls=record['controls'],commands=record['submitted_commands'])):
        raise RunError(4,'protocol','per-capture actual source/command snapshot is not bound')
    tools=manifest['tools']
    argv=launch.get('argv',[])
    output=paths[ref['stream']].parent
    if side=='native':
        expected=[str(ROOT/relative_path(tools['control_binary'])),'--request',str(paths[ref['request']]),'--output',str(output)]
        if argv[-5:]!=expected:raise ProtocolError('actual native launch argv/request differs')
    else:
        worker_name=(Path(ref['stream']).parent/'worker.json').as_posix()
        if worker_name not in paths:raise ProtocolError('actual Web worker configuration absent')
        worker=read(paths[worker_name])
        if (worker.get('request_path')!=str(paths[ref['request']]) or worker.get('output')!=str(output) or
                Path(worker.get('build','')).resolve()!=paths[manifest['bootstrap']['control_bundle']['app.js']].parent or
                worker.get('prepared')!=read(paths[manifest['bootstrap']['prepared']])):
            raise ProtocolError('actual Web launch config/request/bundle differs')
        if argv!=[worker['prepared']['node'],str(ROOT/'tools/demo_reference_web/calibration.mjs'),str(paths[worker_name])]:
            raise ProtocolError('actual Web launcher argv differs')
    return request


def validate_control_count(cells):
    if not isinstance(cells,list) or len(cells)>MAX_CONTROLS:
        raise ProtocolError('control cell count exceeds 384')
    return cells


def candidate_rows(positives, holdouts, negatives):
    """Input values must have been recomputed from verified control captures."""
    if any(set(values)-set(POLICY) for vector in (positives,holdouts,negatives) for values in vector.values()):
        raise ProtocolError('unknown calibration metric input')
    rows=[]
    for name,(unit,domain,cap,floor,margin,gap) in POLICY.items():
        vectors=[]
        for label,source in (('positive',positives),('holdout',holdouts),('negative',negatives)):
            vector=[(identity,values[name]) for identity,values in source.items() if name in values]
            if not vector or any(type(v) not in (int,float) or not math.isfinite(v) or v < 0 for _,v in vector):
                raise RunError(3,'calibration',label+' controls missing/non-finite: '+name)
            vectors.append(vector)
        positive=max(v for _,v in vectors[0])
        holdout=max(v for _,v in vectors[1])
        negative=min(v for _,v in vectors[2])
        candidate=max(floor,positive+margin)
        if candidate > cap or holdout > candidate or negative < candidate+gap:
            raise RunError(3,'calibration','controls not separable inside fixed hard cap: '+name)
        rows.append(dict(identity=name,unit=unit,domain=domain,threshold=candidate,hard_cap=cap,
            quantization_floor=floor,positive_max=positive,margin=margin,holdout_max=holdout,
            negative_min=negative,safety_gap=gap,positive_ids=[i for i,_ in vectors[0]],
            holdout_ids=[i for i,_ in vectors[1]],negative_ids=[i for i,_ in vectors[2]]))
    return rows


def checked_artifact(root, artifact):
    exact_fields(artifact,'path sha256 bytes','calibration artifact')
    root=output_path(root)
    path=(root/relative_path(artifact['path'])).resolve()
    if not path.is_relative_to(root) or path == root or not path.is_file():
        raise ProtocolError('calibration artifact escapes or is absent')
    if (type(artifact['bytes']) is not int or artifact['bytes'] < 1 or path.stat().st_size != artifact['bytes'] or
            sha(path) != artifact['sha256']):
        raise ProtocolError('calibration artifact bytes/hash differ: '+artifact['path'])
    return path


def evidence_manifest(profile):
    """Verify the finite envelope before opening any image or executable path."""
    evidence=profile['evidence']
    if not evidence:
        raise RunError(3,'calibration','independent calibration evidence is unavailable')
    path=output_path(ROOT/relative_path(evidence['path']))
    if not path.is_file() or sha(path) != evidence['sha256']:
        raise RunError(3,'calibration','calibration manifest is absent or changed')
    if path.name != evidence['sha256']+'.json':
        raise ProtocolError('calibration manifest must be content addressed')
    manifest=read(path)
    exact_fields(manifest,'schema_version calibration_kind binding source environment tools scope color corpus policy artifacts trials fonts fonttools_library font_priors bootstrap','calibration manifest')
    if manifest['schema_version'] != 1 or manifest['calibration_kind'] != KIND:
        raise ProtocolError('unknown calibration evidence kind')
    if manifest['policy'] != {key:list(value) for key,value in POLICY.items()}:
        raise ProtocolError('calibration policy changed')
    if evidence['policy_sha256'] != digest(manifest['policy']):
        raise ProtocolError('calibration policy hash differs')
    corpus=read(FIXTURES/'pilot-calibration.json')
    if (manifest['corpus'] != corpus or sha(FIXTURES/'pilot-calibration.json') != evidence['control_definition_sha256'] or
            profile['corpus_hash'] != digest(corpus)):
        raise ProtocolError('calibration corpus changed or was replaced by observed pilot output')
    for key in ('algorithm_hash','source_hash','environment_hash','tool_hash','font_hashes','font_policy'):
        if key not in manifest['binding'] or manifest['binding'][key] != profile[key]:
            raise RunError(3,'calibration','calibration manifest binding differs: '+key)
        if manifest['binding'][key] is None:
            raise RunError(3,'calibration','calibration binding is unavailable')
    if digest(manifest['source']) != profile['source_hash'] or digest(manifest['environment']) != profile['environment_hash']:
        raise ProtocolError('source/environment descriptions do not reconstruct binding hashes')
    if digest(manifest['tools']) != profile['tool_hash']:
        raise ProtocolError('tool closure does not reconstruct binding hash')
    exact_fields(manifest['tools'],'python files native_binary control_binary','tool closure')
    from .comparison import algorithm_hash
    from .source_closure import source_manifest
    from .pilots import FONTS
    if (profile['algorithm_hash'] != algorithm_hash() or manifest['source']['native'] != source_manifest(ROOT) or
            profile['font_hashes'] != [f['sha256'] for f in FONTS]):
        raise RunError(3,'calibration','algorithm/full native source/ordered seven fonts changed')
    if [{k:f.get(k) for k in ('identity','sha256','face_index','weight')} for f in manifest['fonts']]!=FONTS:
        raise RunError(3,'calibration','actual ordered font descriptors differ from the frozen seven-file scope')
    for item in manifest['tools']['files']:
        exact_fields(item,'path sha256 bytes','calibration tool closure')
        path_tool=(ROOT/relative_path(item['path'])).resolve()
        if not path_tool.is_relative_to(ROOT) or not path_tool.is_file() or sha(path_tool) != item['sha256'] or path_tool.stat().st_size != item['bytes']:
            raise RunError(3,'calibration','calibration tool/source bytes changed: '+item['path'])
    file_table={item['path']:item for item in manifest['tools']['files']}
    if len(file_table)!=len(manifest['tools']['files']):raise ProtocolError('duplicate tool closure path')
    mandatory={ROOT/'tools/run_demo_differential.py',ROOT/'third_party/ant-design-icons/manifest.json'}
    for area in (ROOT/'tools/demo_differential',ROOT/'tools/demo_reference_web',ROOT/relative_path(manifest['fonttools_library'])):
        mandatory.update(p for p in area.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc')
    mandatory.update(ROOT/relative_path(font['path']) for font in manifest['fonts'])
    if any(p.relative_to(ROOT).as_posix() not in file_table for p in mandatory):
        raise ProtocolError('incomplete actual algorithm/worker/font tool closure')
    for role in ('native_binary','control_binary'):
        if manifest['tools'][role] not in file_table:raise ProtocolError('missing actual binary tool role')
    hashes={item['sha256'] for item in file_table.values()}
    if any(manifest['environment']['libraries'][key] not in hashes for key in ('browser_sha256','node_sha256')):
        raise ProtocolError('missing actual browser/Node executable from tool closure')
    import sys
    if manifest['tools']['python'] != sys.version:
        raise RunError(3,'calibration','actual Python interpreter changed')
    if all_font_priors(manifest['fonts'],corpus=read(FIXTURES/'pilot-calibration.json'),
                   library=output_path(ROOT/relative_path(manifest['fonttools_library']))) != manifest['font_priors']:
        raise RunError(3,'calibration','SFNT cmap/metrics/outline priors do not reconstruct')
    if manifest['scope'] != profile['scope'] or manifest['color'] != profile['color']:
        raise RunError(3,'calibration','calibration scope/color differs')
    if (profile['scope']['cases'] != CASES or profile['scope']['actual_dpr'] is None or
            profile['scope']['origin_phase_px'] != [0,0] or profile['masks']):
        raise RunError(3,'calibration','unsupported or incomplete pilot scope')
    expected_scope=dict(cases=CASES,locale='zh-CN',direction='ltr',theme='default',motion='disabled',
        font_policy='pinned',actual_dpr=profile['scope']['actual_dpr'],origin_phase_px=[0,0],
        materials=corpus['materials'],font_sizes_weights=[dict(size=s,weight=w) for s in corpus['font_sizes'] for w in corpus['font_weights']])
    if profile['scope']!=expected_scope:
        raise RunError(3,'calibration','scope does not reconstruct exactly from the fixed independent corpus')
    artifacts=manifest['artifacts']
    if not artifacts or len({a['path'] for a in artifacts}) != len(artifacts):
        raise ProtocolError('missing or duplicate evidence artifact')
    if digest(artifacts) != evidence['observations_and_captures_manifest_sha256']:
        raise ProtocolError('observation/capture artifact manifest differs')
    if (manifest['color']['control_evidence_sha256']!=digest(artifacts) or
            manifest['color']['web']['launch_args_hash']!=digest(manifest['environment']['web']['launch_args']) or
            '--force-color-profile=srgb' not in manifest['environment']['web']['launch_args']):
        raise ProtocolError('verified opaque domain lacks its actual capture/tool color evidence binding')
    paths={a['path']:checked_artifact(path.parent,a) for a in artifacts}
    for partition,label in (('train','train_ids'),('holdout','holdout_ids'),('negative','negative_ids')):
        if [t['identity'] for t in manifest['trials'] if t['partition']==partition] != evidence[label]:
            raise ProtocolError('profile partition identities differ from actual evidence')
    if any(sum(t['partition']==part and not t['restart'] for t in manifest['trials']) != profile['repetitions']
           for part in ('train','holdout')):
        raise ProtocolError('profile repetition declaration differs from actual controls')
    return manifest,paths,path.parent


def verify_profile_evidence(profile, *, image_module=None,native_binary_sha256=None):
    validate('profile',profile)
    if profile['calibrated'] is not True:
        raise RunError(3,'calibration','uncalibrated profile cannot grant a working domain')
    manifest,paths,root=evidence_manifest(profile)
    if image_module is not None:
        site=Path(image_module.__file__).resolve().parent.parent
        files={f['path'] for f in manifest['tools']['files']}
        if any(p.relative_to(ROOT).as_posix() not in files for p in site.rglob('*')
               if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc'):
            raise ProtocolError('locked decoder installation is missing from the actual tool closure')
    if native_binary_sha256 is not None:
        files={f['path']:f for f in manifest['tools']['files']}
        if files.get(manifest['tools']['native_binary'],{}).get('sha256')!=native_binary_sha256:
            raise RunError(3,'calibration','profile was calibrated against another native binary/configuration')
    # Trial re-evaluation is mandatory. A summary, signature-shaped marker or
    # arbitrary positive_max is never sufficient to produce a threshold.
    recomputed=evaluate_trials(manifest,paths,root,image_module=image_module)
    if recomputed != profile['metrics']:
        raise RunError(3,'calibration','profile metrics do not reconstruct from actual controls')
    return {row['identity']:row['threshold'] for row in recomputed}


def preflight_profile(options):
    """Supported consumer entry: dependency/source/control recheck before probes."""
    from .comparison_dependencies import image_module
    profile=validate('profile',read(options['profile_file']))
    manifest,_,_=evidence_manifest(profile)
    sites={str((ROOT/relative_path(item['path'])).parent.parent) for item in manifest['tools']['files']
           if item['path'].endswith('/PIL/__init__.py')}
    site=options.get('comparison_site')
    if site is None:
        if len(sites)!=1:raise RunError(3,'calibration','actual locked decoder site cannot be uniquely reconstructed')
        site=next(iter(sites))
    site=output_path(site)
    decoder=image_module(site)
    verify_profile_evidence(profile,image_module=decoder,native_binary_sha256=sha(Path(options['binary'])))
    return {**options,'comparison_site':str(site)}


def evaluate_trials(manifest, paths, root, *, image_module=None):
    if image_module is None:
        raise RunError(3,'calibration','locked image decoder is required to reconstruct calibration')
    verify_bootstrap(manifest,paths,root)
    corpus=manifest['corpus']
    trials=manifest['trials']
    if not trials or len({t['identity'] for t in trials}) != len(trials):
        raise ProtocolError('missing/duplicate calibration trial')
    positive,holdout,negative={},{},{}
    nonces=set()
    fences={}
    transparent_probes={}
    partitions={'train':[],'holdout':[],'negative':[]}
    for trial in trials:
        exact_fields(trial,'identity partition board fault restart native web','calibration trial')
        partition=trial['partition']
        if partition not in partitions or trial['board'] not in ('train','holdout'):
            raise ProtocolError('unknown calibration partition/board')
        if type(trial['restart']) is not bool:
            raise ProtocolError('restart must be an observed process boundary')
        partitions[partition].append(trial)
        observations=[]
        images=[]
        for side in ('native','web'):
            exact_fields(trial[side],'stream capture request launch ordinal','calibration stream reference')
            stream_name,capture_name=trial[side]['stream'],trial[side]['capture']
            if stream_name not in paths or capture_name not in paths:
                raise RunError(4,'protocol','trial is missing its owned stream/capture artifact')
            record=read(paths[stream_name])
            exact_fields(record,'schema_version calibration_kind side nonce process_nonce process request_sha256 sample_ordinal snapshot_sha256 snapshot_json submitted_commands board fault restart provenance capture controls complete','control stream')
            verify_trial_request(manifest,trial,side,record,paths)
            if (record['schema_version'] != 1 or record['calibration_kind'] != KIND or record['side'] != side or
                    record['board'] != trial['board'] or record['fault'] != trial['fault'] or
                    record['restart'] != trial['restart'] or record['complete'] is not True):
                raise RunError(4,'protocol','control stream identity/completion differs')
            nonce=(side,record['nonce'])
            if not record['nonce'] or nonce in nonces:
                raise RunError(4,'protocol','stale/repeated control capture nonce')
            nonces.add(nonce)
            c=record['capture']
            exact_fields(c,'path sha256 raw raw_sha256 width height dpr clear_rgba readback_id fence_id fence_completed codec_fence_id codec_readback_id codec_raw_match native_receipt raw_alpha_available raw_alpha_min raw_alpha_max raw_opaque_pixels raw_total_pixels source_channel_order framebuffer_format color_profile alpha','control capture')
            prefix=Path(stream_name).parent
            if (prefix/relative_path(c['path'])).as_posix() != capture_name or sha(paths[capture_name]) != c['sha256']:
                raise ProtocolError('control capture is bound to another stream')
            raw_name=(prefix/relative_path(c['raw'])).as_posix()
            if raw_name not in paths or sha(paths[raw_name]) != c['raw_sha256']:
                raise RunError(4,'protocol','control raw RGBA artifact is absent or changed')
            key=(side,record['process_nonce'])
            previous=fences.get(key,0)
            if (not record['process_nonce'] or type(c['fence_id']) is not int or c['fence_id'] <= previous or
                    c['readback_id'] != c['fence_id'] or c['fence_completed'] is not True):
                raise RunError(4,'protocol','control has no fresh completed fence')
            if (c['codec_raw_match'] is not True or c['codec_readback_id'] != c['codec_fence_id'] or
                    type(c['codec_fence_id']) is not int or
                    c['codec_fence_id'] < c['fence_id']+(1 if side=='native' else 0)):
                raise RunError(4,'protocol','opaque codec/raw receipts are incomplete or merged')
            fences[key]=c['codec_fence_id']
            if c['dpr'] != manifest['scope']['actual_dpr']:
                raise RunError(3,'calibration','control actual DPR differs')
            raw=paths[raw_name].read_bytes()
            size=(c['width'],c['height'])
            if (any(type(v) is not int or v <= 0 for v in size) or len(raw) != size[0]*size[1]*4):
                raise RunError(4,'protocol','raw RGBA extent differs')
            alphas=raw[3::4]
            actual=(min(alphas),max(alphas),sum(a==255 for a in alphas),len(alphas))
            if (c['raw_alpha_available'] is not True or actual != (c['raw_alpha_min'],c['raw_alpha_max'],
                    c['raw_opaque_pixels'],c['raw_total_pixels']) or actual[:2] != (255,255)):
                raise RunError(3,'calibration','opaque control lacks measured all-A255 raw alpha')
            with image_module.open(paths[capture_name]) as image:
                if image.size != size or image.convert('RGBA').tobytes() != raw:
                    raise RunError(3,'calibration','control codec does not preserve opaque raw RGBA')
            expected_color=manifest['color'][side]
            if side=='native':
                receipt=exact_fields(c['native_receipt'],'renderer_owner device_epoch scene_revision pixel_width pixel_height dpr','native scene receipt')
                if (any(type(receipt[k]) is not int or receipt[k]<=0 for k in ('renderer_owner','device_epoch','scene_revision')) or
                        receipt['pixel_width']!=c['width'] or receipt['pixel_height']!=c['height'] or receipt['dpr']!=c['dpr']):
                    raise RunError(4,'protocol','native actual scene owner/epoch/revision/metrics receipt differs')
                if (c['framebuffer_format'] != expected_color['framebuffer_format'] or
                        c['source_channel_order'] != expected_color['source_channel_order'] or
                        c['color_profile'] != 'native-encoded-SDR' or c['alpha'] != 'premultiplied' or
                        c['clear_rgba']!=expected_color['clear_rgba']):
                    raise RunError(3,'calibration','native raw encoding differs')
                actual_capture=manifest['environment']['native_capture']
                if (c['framebuffer_format']!=actual_capture['framebuffer_format'] or
                        c['source_channel_order']!=actual_capture['source_channel_order'] or
                        c['clear_rgba']!=actual_capture['clear_rgba']):
                    raise RunError(3,'calibration','control storage/clear differs from the actual three-pilot probe')
                prior=transparent_probes.get(prefix)
                if prior and (prior[0]['board'],prior[0]['fault'])!=(trial['board'],trial['fault']):
                    raise ProtocolError('one producer output reused across different control inputs')
                transparent_probes[prefix]=(trial,c['codec_fence_id'],(c['width'],c['height'],c['dpr']))
            else:
                if c['color_profile'] != 'sRGB-SDR' or c['alpha'] not in ('straight','opaque'):
                    raise RunError(3,'calibration','Web raw encoding differs')
                png=png_metadata(paths[capture_name])
                if (c['source_channel_order']!='RGBA' or c['native_receipt'] is not None or
                        c['framebuffer_format']!=png['format'] or png['format'] not in expected_color['file_formats'] or
                        png['color_chunks']!=expected_color['png_color_chunks']):
                    raise RunError(3,'calibration','actual Web PNG encoding/chunks differ from the bound domain')
            if record['provenance'].get('binding') != manifest['binding']:
                raise RunError(3,'calibration','control source/environment/font/tool binding differs')
            if side=='native':
                if record['provenance'].get('source') != manifest['source']['native']:
                    raise RunError(3,'calibration','control binary embeds another source closure')
                if record['provenance'].get('backend') != manifest['environment']['native']['backend']:
                    raise RunError(3,'calibration','control actual backend differs')
                if record['provenance'].get('gpu') != manifest['environment']['native']['gpu']:
                    raise RunError(3,'calibration','control actual GPU/driver differs')
                for field in ('preset','toolchain','configuration','platform','shader'):
                    if record['provenance'].get(field)!=manifest['environment']['native'][field]:
                        raise RunError(3,'calibration','control actual '+field+' differs')
                if stable_device_metrics(record['provenance'].get('metrics'))!=manifest['environment']['metrics']:
                    raise RunError(3,'calibration','control actual stable window metrics differ')
                tools={t['path']:t for t in manifest['tools']['files']}
                if record['provenance'].get('binary_sha256')!=tools[manifest['tools']['control_binary']]['sha256']:
                    raise RunError(3,'calibration','control binary hash differs')
            else:
                if (record['provenance'].get('browser') != manifest['environment']['web'] or
                        record['provenance'].get('source_closure') != manifest['source']['closure']):
                    raise RunError(3,'calibration','control actual Web source/browser differs')
                verify_served_assets(Path(stream_name).parent,manifest,paths,trial[side]['request'])
            observations.append(record)
            images.append((raw,size))
        for control in observations[0]['controls']:
            if control['kind']=='text' and control['source'].get('raster_policy') not in manifest['environment']['native_capture']['font_raster_policies']:
                raise RunError(3,'calibration','actual native control font raster policy differs')
        values,hard_failures=measure_controls(corpus,trial,observations,images,manifest['font_priors'])
        if partition=='negative':
            definition=next((r for r in corpus['negative'] if r['identity']==trial['identity']),None)
            if not definition or definition['fault'] != trial['fault']:
                raise ProtocolError('undeclared negative control')
            expected=set(definition['must_fail'])
            if any(name not in values and name not in hard_failures for name in expected):
                raise RunError(3,'calibration','negative control lacks required detection layer')
            negative[trial['identity']]={name:values[name] for name in expected if name in POLICY and name in values}
            if any(name not in hard_failures for name in expected if name not in POLICY):
                raise RunError(3,'calibration','negative hard gate did not detect actual injected primitive/input fault')
        else:
            if trial['fault'] != 'none' or hard_failures:
                raise RunError(3,'calibration','independent positive failed geometry/material/font/presence: '+','.join(hard_failures))
            (positive if partition=='train' else holdout)[trial['identity']]=values
    for partition in ('train','holdout'):
        rows=partitions[partition]
        repeats=[r for r in rows if not r['restart']]
        restarts=[r for r in rows if r['restart']]
        if len(repeats) < 5 or len(restarts) != 1 or any(r['board'] != partition for r in rows):
            raise RunError(4,'protocol','missing five controls or independent restart: '+partition)
        for side in ('native','web'):
            processes=[read(paths[r[side]['stream']])['process_nonce'] for r in rows]
            restart_index=rows.index(restarts[0])
            if processes[restart_index] in [p for i,p in enumerate(processes) if i!=restart_index]:
                raise RunError(4,'protocol','restart reused the original producer process')
    if [r['identity'] for r in partitions['negative']] != [r['identity'] for r in corpus['negative']]:
        raise RunError(4,'protocol','missing/reordered negative controls')
    for prefix,(trial,fence,actual_metrics) in transparent_probes.items():
        verify_transparent_probe(prefix,corpus,trial,paths,fence,actual_metrics)
    return candidate_rows(positive,holdout,negative)


def verify_served_assets(prefix,manifest,paths,request_name):
    name=(prefix/'served-assets.json').as_posix()
    if name not in paths:raise RunError(4,'protocol','actual served-asset receipt absent')
    receipt=read(paths[name])
    assets=receipt.get('assets',[])
    if len({a['url'] for a in assets})!=len(assets):raise ProtocolError('duplicate served asset role')
    table={a['url']:a for a in assets}
    for url,filename in [('/request.js','served-request.js'),('/','served-index.html')]:
        artifact=(prefix/filename).as_posix()
        if artifact not in paths or table.get(url,{}).get('sha256')!=sha(paths[artifact]) or table[url].get('requests',0)<1:
            raise ProtocolError('actual served document/request bytes are absent or changed')
    javascript=paths[(prefix/'served-request.js').as_posix()].read_text(encoding='utf-8')
    if (not javascript.startswith('window.__controlRequest=') or not javascript.endswith(';') or
            json.loads(javascript[len('window.__controlRequest='):-1])!=read(paths[request_name])):
        raise ProtocolError('actual served input differs from the canonical Web request')
    for url,role in [('/app.js','app.js'),('/app.css','app.css')]:
        artifact=manifest['bootstrap']['control_bundle'].get(role)
        if artifact is None:
            if url in table:raise ProtocolError('undeclared served CSS bundle')
            continue
        a=table.get(url,{})
        if artifact not in paths or a.get('sha256')!=sha(paths[artifact]) or a.get('bytes')!=paths[artifact].stat().st_size or a.get('requests',0)<1:
            raise ProtocolError('actual served '+role+' differs from frozen build output')
    for index,font in enumerate(manifest['fonts']):
        if table.get('/font/'+str(index),{}).get('sha256') not in (None,font['sha256']):
            raise ProtocolError('actual served font differs')
    # Unused fonts need not have a network request; their exact files and actual
    # selected faces remain mandatory in preflight and per-control observations.


def verify_bootstrap(manifest,paths,root):
    """Reopen actual probe streams/build receipts before measuring any controls."""
    from . import comparison,runner_evidence,pilots
    from .runner import _probe
    bootstrap=exact_fields(manifest['bootstrap'],'native_probes browser control_bundle consumer_bundle prepared prepared_directory','bootstrap evidence')
    tools={f['path']:f for f in manifest['tools']['files']}
    binary_sha=tools[manifest['tools']['native_binary']]['sha256']
    definitions=pilots.definitions()
    probes=bootstrap['native_probes']
    if [p['identity'] for p in probes]!=CASES:raise ProtocolError('incomplete environment probe case identities')
    native_env=manifest['environment']['native']
    for name,value in [('frozen-corpus.json',manifest['corpus']),('frozen-policy.json',manifest['policy'])]:
        if name not in paths or read(paths[name])!=value:raise ProtocolError('frozen bootstrap definition/policy differs')
    for definition,probe in zip(definitions,probes):
        if probe['stream'] not in paths or probe['request'] not in paths:raise RunError(4,'protocol','raw environment probe absent')
        case=_probe(definition);request=read(paths[probe['request']])
        if request.get('definition')!=case or request.get('nonce')!=probe['nonce'] or request.get('fault')!='none':
            raise ProtocolError('actual environment probe request differs from canonical pilot source')
        observed=runner_evidence.native(paths[probe['stream']],case,probe['nonce'],manifest['source']['native'],
            binary_sha,native_env['preset'],native_env['configuration'])
        if (stable_device_metrics(runner_evidence.metrics(observed,'native'))!=manifest['environment']['metrics'] or
                comparison.capture_description(observed)!=manifest['environment']['native_capture']):
            raise RunError(3,'calibration','actual environment probe does not reconstruct stable environment')
        if {k:observed['hello']['provenance'].get(k) for k in native_env}!=native_env:
            raise RunError(3,'calibration','actual native probe shader/platform/device/contract differs')
    if bootstrap['browser'] not in paths or read(paths[bootstrap['browser']])!=manifest['environment']['web']:
        raise ProtocolError('actual browser probe receipt differs')
    web_request_name='environment-web-request.json'
    if web_request_name not in paths:raise ProtocolError('actual Web environment probe request absent')
    web_probe=read(paths[web_request_name])
    exact_fields(web_probe,'calibration_kind nonce board fault restart repetitions actual_dpr cells panels font_files binding process_launch_id','Web environment probe request')
    if (web_probe['calibration_kind']!=KIND or web_probe['board']!='train' or web_probe['fault']!='none' or
            web_probe['restart'] is not False or web_probe['repetitions']!=0 or web_probe['binding']!={} or
            web_probe['actual_dpr']!=manifest['scope']['actual_dpr'] or web_probe['cells']!=control_cells(manifest['corpus'],'train') or
            web_probe['panels']!=manifest['corpus']['panels']):
        raise ProtocolError('actual Web environment probe input differs')
    verify_served_assets(Path(bootstrap['browser']).parent,manifest,paths,web_request_name)
    for key in ('control_bundle','consumer_bundle'):
        bundle=bootstrap[key]
        if not {'app.js','closure.json','metafile.json'}<=set(bundle) or any(p not in paths for p in bundle.values()):
            raise ProtocolError('incomplete actual bundle/metafile/closure artifacts')
        if read(paths[bundle['closure.json']])!=manifest['source']['closure']:
            raise ProtocolError('control/consumer official import closures differ')
        meta=read(paths[bundle['metafile.json']])
        for name in ('app.js','app.css'):
            if name in bundle:
                outputs=[v for p,v in meta.get('outputs',{}).items() if Path(p).name==name]
                if len(outputs)!=1 or outputs[0].get('bytes')!=paths[bundle[name]].stat().st_size:
                    raise ProtocolError('bundle bytes do not match actual build metafile')
    if bootstrap['prepared'] not in paths:raise ProtocolError('prepared tool/library receipt missing')
    prepared=read(paths[bootstrap['prepared']])
    from demo_reference_web import bootstrap as web_bootstrap
    current_prepared=web_bootstrap.verify_prepared(output_path(ROOT/relative_path(bootstrap['prepared_directory'])))
    if current_prepared!=prepared:raise ProtocolError('actual locked source/package/browser preparation changed')
    for field in ('browser_sha256','node_sha256','browser_libraries'):
        if prepared.get(field)!=manifest['environment']['libraries'][field]:raise ProtocolError('prepared/library receipt differs')


def png_metadata(path):
    """Strict RGB8/RGBA8 PNG structure/CRC. Color chunks are facts, not assumptions."""
    import struct
    import zlib
    raw=Path(path).read_bytes()
    if not raw.startswith(b'\x89PNG\r\n\x1a\n'):
        raise ProtocolError('Web capture is not PNG')
    offset=8
    chunks=[]
    ihdr=None
    ended=False
    while offset < len(raw):
        if len(raw)-offset < 12:
            raise ProtocolError('truncated PNG chunk')
        length=struct.unpack('>I',raw[offset:offset+4])[0]
        kind=raw[offset+4:offset+8]
        data=raw[offset+8:offset+8+length]
        if len(data) != length or offset+length+12 > len(raw):
            raise ProtocolError('PNG chunk exceeds file')
        crc=struct.unpack('>I',raw[offset+8+length:offset+12+length])[0]
        if zlib.crc32(kind+data)&0xffffffff != crc:
            raise ProtocolError('PNG chunk CRC differs')
        chunks.append({'type':kind.decode('ascii'),'sha256':hashlib.sha256(data).hexdigest(),'bytes':length})
        if kind==b'IHDR':
            if ihdr is not None or len(chunks)!=1 or length!=13:
                raise ProtocolError('invalid PNG IHDR')
            ihdr=struct.unpack('>IIBBBBB',data)
            if ihdr[2] != 8 or ihdr[3] not in (2,6) or ihdr[4:] != (0,0,0):
                raise ProtocolError('unsupported PNG encoding')
        offset+=length+12
        if kind==b'IEND':
            ended=True
            if length or offset!=len(raw):
                raise ProtocolError('PNG trailing content')
            break
    if not ended or ihdr is None or not any(c['type']=='IDAT' for c in chunks):
        raise ProtocolError('PNG is incomplete')
    return {'format':'PNG RGB8' if ihdr[3]==2 else 'PNG RGBA8','chunks':chunks,
            'color_chunks':[c for c in chunks if c['type'] in ('sRGB','gAMA','cHRM','iCCP')]}


def control_cells(corpus, board):
    """Expand only the frozen declarative input, independent of either output."""
    if board not in ('train','holdout'):
        raise ProtocolError('unknown control board')
    cells=[]
    phase=corpus[board]['phase']
    for panel_index,panel in enumerate(corpus['panels']):
        foreground=[255,255,255] if panel_index==0 else [0,0,0]
        def add(name,kind,x,y,w,h,**fields):
            cells.append(dict(identity=f'p{panel_index}.{name}',kind=kind,
                rect=dict(x=panel['x']+x+phase,y=panel['y']+y+phase,width=w,height=h),
                background=panel['rgb'],foreground=foreground,alpha=1,**fields))
        for i,rgb in enumerate(corpus['rgb_sentinels']):
            add('rgb'+str(i),'fill',12+64*i,16,28,24,rgb=rgb)
        for i,gray in enumerate(corpus['gray_ramp']):
            add('gray'+str(i),'fill',12+34*i,62,22,24,rgb=[gray]*3)
        for i,alpha in enumerate(corpus['alpha']):
            # Distinct factor combinations include alpha-only, opacity-only and both.
            paint_opacity=(1, 1, 1, 1, .5)[i]
            add('alpha'+str(i),'alpha',12+64*i,106,28,24,rgb=corpus['alpha_rgb'],source_alpha=alpha,paint_opacity=paint_opacity)
        add('sourceover','sourceover',400,106,28,24,rgb=corpus['alpha_rgb'],source_alpha=.5,paint_opacity=1,second_rgb=[201,17,83],second_alpha=.5)
        add('alpha-both','alpha',336,106,28,24,rgb=corpus['alpha_rgb'],source_alpha=.5,paint_opacity=.5)
        add('border','border',12,156,40,24,radius=0,border=1)
        add('round4','round',90,156,40,24,radius=4,border=0)
        add('round6','round',168,156,40,24,radius=6,border=0)
        add('outline4','border',246,156,40,24,radius=4,border=1)
        add('clip','clip',324,156,40,24,radius=4,border=0,clip_width=24)
        add('round8','round',402,156,40,24,radius=8,border=0)
        add('outline8','border',480,156,40,24,radius=8,border=1)
        for i,(size,icon) in enumerate((s,n) for s in corpus['icon_sizes'] for n in corpus['icons']):
            add('icon'+str(i),'icon',12+(i%8)*64,220+(i//8)*44,size,size,content=icon,font_size=size,font_weight=400)
        index=0
        for size in corpus['font_sizes']:
            for weight in corpus['font_weights']:
                for char in corpus[board]['text']:
                    add('glyph'+str(index),'text',12+(index%12)*44,340+(index//12)*40,30,32,
                        content=char,font_size=size,font_weight=weight)
                    index+=1
        add('shadow','shadow',48,760,40,24,radius=6,shadow=corpus['shadow'],
            occluding_fill=dict(rgb=panel['rgb'],alpha=1))
        add('shadow-half-fill','shadow',208,760,40,24,radius=6,shadow=corpus['shadow'],
            occluding_fill=dict(rgb=panel['rgb'],alpha=.5))
        add('shadow-no-fill','shadow',368,760,40,24,radius=6,shadow=corpus['shadow'],
            occluding_fill=dict(rgb=panel['rgb'],alpha=0))
    return validate_control_count(cells)


def verify_native_control_geometry(cell,row,commands):
    """Actual factory geometry and final draw membership, independent of pixels."""
    source=row['source'];r=cell['rect']
    def membership(part,key='primitive_index',kind='quad'):
        index=part.get(key)
        matches=[i for i,c in enumerate(commands) if c['kind']==kind and type(index) is int and
                 c['first_instance']<=index<c['first_instance']+c['instance_count']]
        if len(matches)!=1 or part.get(key+'_commands')!=matches:raise ProtocolError('actual control command membership differs')
        translation=part.get('second_translation' if key=='second_primitive_index' else 'translation')
        if translation!=[0,0]:raise ProtocolError('actual control translation differs')
        if kind=='quad' and part.get('clip_kind')!='viewport':raise ProtocolError('actual control clip kind unavailable')
        return matches[0],index
    if cell['kind']=='border':
        if cell['radius']==0:
            expected=[dict(x=r['x'],y=r['y'],width=r['width'],height=1),
                dict(x=r['x'],y=r['y']+r['height']-1,width=r['width'],height=1),
                dict(x=r['x'],y=r['y']+1,width=1,height=r['height']-2),
                dict(x=r['x']+r['width']-1,y=r['y']+1,width=1,height=r['height']-2)]
            parts=source.get('quad_parts',[])
            if len(parts)!=4 or any(p.get('edge')!=i or p.get('rect')!=shape for i,(p,shape) in enumerate(zip(parts,expected))):
                raise ProtocolError('actual square border four nonoverlapping quads differ')
            for part in parts:
                membership(part)
                if part.get('foreground')!=cell['foreground'] or part.get('source_alpha')!=1 or part.get('paint_opacity')!=1:
                    raise ProtocolError('actual square border material differs')
        else:
            shape={**r,'x':r['x']+1,'y':r['y']+1,'width':r['width']-2,'height':r['height']-2}
            if (source.get('actual_shape')!=shape or source.get('actual_shape_radius')!=cell['radius']-1 or
                    source.get('outline_width')!=1 or source.get('outline_offset')!=0):
                raise ProtocolError('actual inset outline/canonical border mapping differs')
            membership(source,'packed_index','rounded_effect')
    if cell['kind']=='shadow':
        shadow=membership(source,'packed_index','rounded_effect')
        fill=membership(source.get('occluding_fill',{}))
        if shadow[0]>=fill[0]:raise ProtocolError('actual shadow must precede its occluding fill')
    if cell['kind']=='sourceover':
        if membership(source)>=membership(source,'second_primitive_index'):
            raise ProtocolError('actual source-over primitive order differs')


def mutated_cells(corpus,board,fault):
    from copy import deepcopy
    cells=deepcopy(control_cells(corpus,board))
    for cell in cells:
        if fault=='translate-all' or fault=='translate-child' and cell['identity']=='p1.border':
            cell['rect']['x']+=1
        if fault=='translate-shape' and cell['identity']=='p1.border' or fault=='translate-glyph' and cell['identity']=='p1.glyph0':
            cell['rect']['x']+=1
        if fault=='flat-color' and cell['identity']=='p1.rgb6':
            cell['rgb']=[90,83,201]
        if fault=='swap-rb' and cell['kind']=='fill':
            cell['rgb']=cell['rgb'][::-1]
        if fault=='linearize-input' and cell['kind']=='fill':
            from .color_metrics import srgb_to_linear
            cell['rgb']=[round(v*255) for v in srgb_to_linear([v/255 for v in cell['rgb']])]
        if fault=='alpha-squared' and cell['kind']=='alpha':
            cell['source_alpha']**=2
        if fault=='opacity-squared' and cell['kind']=='alpha':
            cell['paint_opacity']**=2
        if fault=='glyph-color' and cell['kind']=='text':
            cell['foreground']=[255,0,0]
        if fault=='glyph-size' and cell['kind']=='text':
            cell['font_size']+=1
        if fault=='font-weight' and cell['kind']=='text' and cell['font_weight']==600:
            cell['font_weight']=400
        if fault=='font-face' and cell['kind']=='text':
            cell['replace_font']=True
        if fault=='delete-border' and cell['identity']=='p1.border':
            cell['omit_right_edge']=True
        if fault=='delete-icon' and cell['identity']=='p1.icon0':
            cell['omit_draw']=True
        if fault=='delete-glyphs' and cell['kind']=='text':
            cell['omit_draw']=True
        if fault=='delete-one-glyph' and cell['identity']=='p1.glyph0':
            cell['omit_draw']=True
        if fault=='clip-glyph' and cell['kind']=='text':
            cell['clip_glyph_right_px']=1
        if cell['identity']=='p1.shadow':
            if fault=='shadow-offset':cell['shadow']['offset'][0]+=1
            if fault=='shadow-blur':cell['shadow']['blur']+=2
            if fault=='shadow-spread':cell['shadow']['spread']+=1
            if fault=='shadow-drop':cell['shadow']['alpha']=0
            if fault=='shadow-order':cell['reverse_shadow_order']=True
        if fault=='wrong-icon' and cell['identity']=='p1.icon0':
            cell['render_icon']='DownOutlined'
        if fault=='overlap-order' and cell['kind']=='sourceover':
            cell['reverse_layers']=True
    return cells


def measure_controls(corpus,trial,observations,images,priors):
    """Recompute from full raw planes and observed control geometry/material/font.

    Every control is checked on its own before entering the positive partition.
    Whole-board background/guard remains a hard independent prior.
    """
    expected=control_cells(corpus,trial['board'])
    values={}
    failures=set()
    def collect(name,value):
        if value is None or not math.isfinite(value):
            failures.add(name)
        else:
            values[name]=max(values.get(name,0),value)
    dpr=observations[0]['capture']['dpr']
    for record in observations:
        if [c['identity'] for c in record['controls']] != [c['identity'] for c in expected]:
            raise RunError(4,'protocol','control stream is missing/reordered actual controls')
    for index,cell in enumerate(expected):
        actual=[record['controls'][index] for record in observations]
        for side,row in enumerate(actual):
            required(row,('identity','rect','kind','source'),'actual control')
            if row['kind'] != cell['kind']:
                failures.add('source.material')
            source=row['source']
            if side==0:
                try:verify_native_control_geometry(cell,row,observations[side].get('submitted_commands',[]))
                except ProtocolError:failures.add('source.geometry-order-membership')
            expected_foreground=cell['shadow']['rgb'] if cell['kind']=='shadow' else cell.get('rgb',cell['foreground'])
            expected_source_alpha=cell['shadow']['alpha'] if cell['kind']=='shadow' else cell.get('source_alpha',1)
            expected_opacity=cell.get('paint_opacity',1)
            expected_alpha=expected_source_alpha*expected_opacity
            if (source.get('foreground') != expected_foreground or type(source.get('alpha')) not in (int,float) or
                    abs(source['alpha']-expected_alpha)>1e-6):
                failures.add('source.material')
            if (abs(source.get('source_alpha',-1)-expected_source_alpha)>1e-6 or
                    abs(source.get('paint_opacity',-1)-expected_opacity)>1e-6):
                failures.add('source.alpha-opacity-factors')
            if side==0:
                encoded=source.get('encoded_rgba');opacity=source.get('opacity')
                if (not isinstance(encoded,list) or len(encoded)!=4 or type(opacity) not in (int,float) or
                        any(type(v) not in (int,float) or not 0<=v<=1 for v in encoded) or not 0<=opacity<=1 or
                        any(abs(v-rgb/255)>1e-6 for v,rgb in zip(encoded[:3],expected_foreground)) or
                        abs(encoded[3]*opacity-expected_alpha)>1e-6):
                    failures.add('source.actual-retained-material')
            if 'radius' in cell and source.get('radius')!=cell['radius']:
                failures.add('source.material')
            if cell['kind']=='sourceover' and (source.get('second_foreground')!=cell['second_rgb'] or source.get('second_alpha')!=cell['second_alpha']):
                failures.add('source.material')
            if cell['kind']=='shadow' and (source.get('offset')!=cell['shadow']['offset'] or
                    source.get('blur')!=cell['shadow']['blur'] or source.get('spread')!=cell['shadow']['spread']):
                failures.add('source.material')
            if cell['kind']=='shadow':
                fill=source.get('occluding_fill',{})
                if (fill.get('foreground')!=cell['occluding_fill']['rgb'] or
                        abs(fill.get('source_alpha',-1)-cell['occluding_fill']['alpha'])>1e-6 or
                        fill.get('paint_opacity')!=1):failures.add('source.shadow-occlusion')
            if cell['kind']=='border' and source.get('border_width',source.get('outline_width'))!=cell['border']:
                failures.add('source.material')
            if cell['kind']=='clip' and source.get('clip_rect')!={**cell['rect'],'width':cell['clip_width']}:
                failures.add('source.clip')
            collect('geometry.logical_px',max(abs(row['rect'][k]-cell['rect'][k]) for k in cell['rect']))
            if cell['kind'] in ('text','icon'):
                source=row['source']
                if cell['kind']=='text' and source.get('content') != cell['content'] or source.get('font_size') != cell['font_size']:
                    failures.add('font.identity')
                if source.get('font_weight') != cell['font_weight']:
                    failures.add('font.identity')
                if cell['kind']=='icon':
                    from .formal_icon_source import verify_web_icon,verify_native_icon
                    try:
                        if side:verify_web_icon(cell['content'],source.get('svg',{}))
                        else:verify_native_icon(cell['content'],source.get('icon_layers'),observations[side].get('submitted_commands'))
                    except ProtocolError:failures.add('source.formal-icon')
                if cell['kind']=='text':
                    from .pilots import FONTS
                    allowed=[f['sha256'] for f in FONTS if f['weight']==cell['font_weight'] and
                             f['identity']==('noto-sans' if ord(cell['content'])<128 else 'noto-sans-cjk-sc')]
                    if source.get('actual_font_sha256') not in allowed:
                        failures.add('font.identity')
                    prior=priors.get(source.get('actual_font_sha256','')+':'+cell['content'])
                    if prior is None:
                        failures.add('font.identity')
                    else:
                        advance=prior['advance']*cell['font_size']/prior['units']
                        collect('text.geometry_logical_px',abs(source.get('advance',-1)-advance))
                        if side==0 and source.get('glyph_id')!=prior['glyph_id']:
                            failures.add('font.cmap')
                if not source.get('ink_boxes'):
                    failures.add('presence.glyph' if cell['kind']=='text' else 'presence.icon')
                if cell['kind']=='text':
                    if type(source.get('advance')) not in (int,float):
                        failures.add('font.identity')
                    elif side:
                        collect('text.geometry_logical_px',abs(source['advance']-actual[0]['source']['advance']))
        r=cell['rect']
        guard=20 if cell['kind']=='shadow' else 2
        box=(math.floor(r['x']*dpr)-guard,math.floor(r['y']*dpr)-guard,
             math.ceil((r['x']+r['width'])*dpr)+guard,math.ceil((r['y']+r['height'])*dpr)+guard)
        crops=[crop_rgba(data,size,box) for data,size in images]
        width,height=box[2]-box[0],box[3]-box[1]
        background=cell['background']
        foreground=cell.get('rgb',cell['foreground'])
        alpha=cell.get('source_alpha',1)*cell.get('paint_opacity',1)
        if cell['kind']=='sourceover':
            second=cell['second_alpha']
            combined=second+(1-second)*alpha
            foreground=[(second*b+(1-second)*alpha*a)/combined for a,b in zip(foreground,cell['second_rgb'])]
            alpha=combined
        if cell['kind']=='shadow':
            foreground=cell['shadow']['rgb']
            alpha=cell['shadow']['alpha']
            if foreground==background:
                # Black-on-black is visually unidentifiable; still validate source/order.
                if any(list(data[p:p+3])!=background for data in crops for p in range(0,len(data),4)):
                    failures.add('shadow.unidentifiable-flat-output')
                continue
            basis=visible_basis([v/255 for v in background],[[v/255 for v in foreground]])
            planes=[[],[]]
            for pixel in range(width*height):
                lx=(box[0]+pixel%width+.5)/dpr;ly=(box[1]+pixel//width+.5)/dpr
                distance=rounded_distance(lx,ly,r,cell['radius'])*dpr
                gaussian=gaussian_rounded_shadow(lx,ly,r,cell['shadow'],cell['radius'])
                for side,data in enumerate(crops):
                    rgb=[v/255 for v in data[pixel*4:pixel*4+3]]
                    result=solve_visible_contributions(rgb,basis,alphas=[alpha])
                    if not result['valid']:failures.add('shadow.output-material')
                    contribution=result['contributions'][0]
                    planes[side].append(min(1,max(0,contribution)))
                    # CSS outer shadow knocks out the original box. Native's
                    # full shadow is occluded by the actual fill once. A real
                    # translucent mismatch fails this independent prior.
                    if abs(distance)>1:
                        expected=alpha*gaussian*(1-cell['occluding_fill']['alpha'] if distance<0 else 1)
                        predicted=source_over([v/255 for v in foreground],[v/255 for v in background],expected)
                        if max(abs(a-b) for a,b in zip(rgb,predicted))>2/255:
                            failures.add('shadow.independent-gaussian-profile')
            shifted={**r,'x':r['x']+cell['shadow']['offset'][0],'y':r['y']+cell['shadow']['offset'][1]}
            tails=tuple(i for i in range(width*height) if rounded_distance(
                (box[0]+i%width+.5)/dpr,(box[1]+i//width+.5)/dpr,shifted,cell['radius'])>cell['shadow']['blur'])
            measured=shadow_profile_metrics(*planes,(width,height),tail_indices=tails,profile_indices=tuple(range(width*height)))
            if not measured['required_ink_present']:failures.add('presence.shadow')
            for key in ('coverage_mass_relative_error','projection_relative_error','profile_max_error','tail_max_error'):
                collect('shadow.'+key,measured[key])
            continue
        is_text=cell['kind'] in ('text','icon')
        if is_text:
            # Semantic ink boxes restrict presence; an enclosing control border
            # cannot stand in for a missing glyph in the interior.
            core_boxes=[]
            for row in actual:
                core_boxes.extend(row['source'].get('ink_boxes',[]))
            if not core_boxes:
                continue
        planes=[[],[]]
        overlap_basis=visible_basis([v/255 for v in background],[[v/255 for v in cell['rgb']],[v/255 for v in cell['second_rgb']]]) if cell['kind']=='sourceover' else None
        for pixel in range(width*height):
            x,y=box[0]+pixel%width+.5,box[1]+pixel//width+.5
            lx,ly=x/dpr,y/dpr
            inside=r['x'] <= lx < r['x']+r['width'] and r['y'] <= ly < r['y']+r['height']
            observed=[list(data[pixel*4:pixel*4+3]) for data in crops]
            if overlap_basis is not None:
                solved=[solve_visible_contributions([v/255 for v in rgb],overlap_basis,
                    alphas=[cell['source_alpha']*cell['paint_opacity'],cell['second_alpha']]) for rgb in observed]
                if any(not s['valid'] for s in solved):failures.add('overlap.output-material')
                vectors=[s['contributions'] if s['contributions'] is not None else (s['joint_contrast'],) for s in solved]
                collect('overlap.contribution_max_error',max(abs(a-b) for a,b in zip(*vectors)))
            if cell['kind'] in ('border','round','clip'):
                distance=rounded_distance(lx,ly,r,cell.get('radius',0))*dpr
                if cell['kind']=='clip':distance=max(distance,(lx-r['x']-cell['clip_width'])*dpr)
                border=cell.get('border',0)*dpr
                distance=max(distance,-distance-border) if border else distance
                if abs(distance)>1:
                    expected_rgb=foreground if distance < -1 else background
                    if any(rgb8_delta_e00(rgb,expected_rgb)>1 for rgb in observed):
                        failures.add('geometry.independent-shape-or-guard')
            if cell['kind']=='shadow':
                predicted_alpha=alpha*gaussian_rounded_shadow(lx,ly,r,cell['shadow'],cell['radius'])
                predicted=[round(v*255) for v in source_over([v/255 for v in foreground],
                           [v/255 for v in background],predicted_alpha)]
                # Fixed encoded-byte numerical bound, declared before sampling;
                # this verifies the model, never trains color from a shadow.
                if any(max(abs(a-b) for a,b in zip(rgb,predicted))>2 for rgb in observed):
                    failures.add('shadow.independent-gaussian-profile')
            if not is_text and cell['kind'] in ('fill','alpha','sourceover') and inside:
                # Keep transport zero only on integer-phase opaque flat sentinels.
                predicted=[round(v*255) for v in source_over([v/255 for v in foreground],[v/255 for v in background],alpha)]
                # Per-side correctness is independent of paired agreement.
                if min(lx-r['x'],ly-r['y'],r['x']+r['width']-lx,r['y']+r['height']-ly)*dpr >= 1:
                    collect('color.flat_delta_e00',rgb8_delta_e00(*observed))
                    if alpha==1 and cell['kind']=='fill' and cell['identity'].split('.')[1].startswith('rgb'):
                        collect('transport.max_channel_residual',max(abs(a-b) for a,b in zip(*observed)))
                    for rgb in observed:
                        if rgb8_delta_e00(rgb,predicted)>1:
                            failures.add('color.independent-source-over')
            if foreground==background or alpha==0:
                if any(rgb!=background for rgb in observed):failures.add('guard.unidentifiable-flat-output')
                continue
            for side,rgb in enumerate(observed):
                try:
                    unmixed=unmix_coverage([v/255 for v in rgb],[v/255 for v in foreground],
                                          [v/255 for v in background],alpha,byte_error=1/255)
                    k=unmixed['coverage']
                    # Only accept the proven quantization interval, then bound
                    # the mathematical integration value. It is never a color repair.
                    planes[side].append(min(1,max(0,k)))
                except ProtocolError:
                    failures.add('material.unmix')
                    planes[side].append(0)
            if is_text and planes[0][-1]>=.98:
                collect('color.text_interior_delta_e00',rgb8_delta_e00(*observed))
            if cell['kind']=='text':
                for side,row in enumerate(actual):
                    prior=priors.get(row['source'].get('actual_font_sha256','')+':'+cell['content'])
                    if prior is None:continue
                    scale=cell['font_size']/prior['units']
                    baseline=r['y']+(32-(prior['ascent']-prior['descent'])*scale)/2+prior['ascent']*scale
                    px,py=(lx-r['x'])/scale,(baseline-ly)/scale
                    inside_outline,distance=outline_point(prior['contours'],px,py)
                    if distance*scale*dpr>1:
                        k=planes[side][-1]
                        if inside_outline and k<.98 or not inside_outline and k>1/255:
                            failures.add('font.independent-outline')
            if cell['kind']=='icon':
                from .formal_icon_source import resource
                entry,_,formal_manifest,_=resource(cell['content'])
                for side in range(2):
                    inside_any=False;nearest=math.inf
                    for layer in entry['layers']:
                        prior=priors[formal_manifest['outline_container_sha256']+':'+chr(layer['codepoint'])]
                        scale=cell['font_size']/prior['units']
                        baseline=r['y']+(cell['font_size']-(prior['ascent']-prior['descent'])*scale)/2+prior['ascent']*scale
                        ink,distance=outline_point(prior['contours'],(lx-r['x'])/scale,(baseline-ly)/scale)
                        inside_any|=ink;nearest=min(nearest,distance*scale*dpr)
                    if nearest>1 and (inside_any and planes[side][-1]<.98 or not inside_any and planes[side][-1]>1/255):
                        failures.add('icon.independent-outline')
        if planes[0]:
            if cell['kind']!='shadow' and not reference_aa_is_bounded(planes[0],(width,height)):
                failures.add('geometry.reference-aa-exceeds-one-physical-pixel')
            measured=coverage_metrics(*planes,(width,height))
            prefix='text' if cell['kind']=='text' else 'pixels'
            if not measured['required_ink_present']:
                failures.add('presence.glyph' if cell['kind']=='text' else 'presence.icon' if cell['kind']=='icon' else 'presence.shape')
            for key in ('coverage_mass_relative_error','contour_distance_px'):
                collect(prefix+'.'+key,measured[key])
            key='projection_relative_error' if cell['kind']=='text' else 'coverage_local_relative_error'
            collect(prefix+'.'+key,measured[key])
    for index,declared_alpha in enumerate(corpus['alpha']):
        pair=[next(c for c in expected if c['identity']==f'p{panel}.alpha{index}') for panel in (0,1)]
        for raw,size in images:
            observed=[]
            for cell in pair:
                r=cell['rect'];x,y=int((r['x']+r['width']/2)*dpr),int((r['y']+r['height']/2)*dpr)
                start=(y*size[0]+x)*4
                observed.append([v/255 for v in raw[start:start+3]])
            try:
                solved=dual_background(*observed,(0,0,0),(1,1,1),tolerance=1/255)
                effective_alpha=pair[0]['source_alpha']*pair[0]['paint_opacity']
                if abs(solved['alpha']-effective_alpha)>2/255:
                    failures.add('alpha.independent-unmix')
                if declared_alpha and solved['foreground'] is not None and any(abs(a-b/255)>2/(255*declared_alpha) for a,b in zip(solved['foreground'],corpus['alpha_rgb'])):
                    failures.add('color.independent-unmix')
            except ProtocolError:
                failures.add('alpha.independent-unmix')
    canvas_values,canvas_failures=canvas_guard(corpus,trial,observations,images)
    for name,value in canvas_values.items():collect(name,value)
    failures.update(canvas_failures)
    return values,sorted(failures)


def canvas_guard(corpus,trial,observations,images):
    """Classify every remaining board pixel, including the retained glyph frames.

    Control rectangles are processed in measure_controls; this routine owns all
    other pixels. These are strata, never exclusion masks or learned crops.
    """
    cells=control_cells(corpus,trial['board']);dpr=observations[0]['capture']['dpr'];size=images[0][1]
    if any(s!=size for _,s in images) or size!=(round(1280*dpr),round(900*dpr)):
        raise RunError(4,'protocol','control board viewport is incomplete or was enlarged')
    rows={};frames={};by_y={}
    for cell in cells:
        r=cell['rect'];guard=20 if cell['kind']=='shadow' else 2
        l,t,rr,b=(math.floor(r['x']*dpr)-guard,math.floor(r['y']*dpr)-guard,
                  math.ceil((r['x']+r['width'])*dpr)+guard,math.ceil((r['y']+r['height'])*dpr)+guard)
        for y in range(max(0,t),min(size[1],b)):rows.setdefault(y,[]).append((l,rr))
        if cell['kind']=='text':
            frame=dict(x=r['x']-4,y=r['y']-4,width=r['width']+8,height=r['height']+8)
            box=(math.floor(frame['x']*dpr)-1,math.floor(frame['y']*dpr)-1,
                 math.ceil((frame['x']+frame['width'])*dpr)+1,math.ceil((frame['y']+frame['height'])*dpr)+1)
            w,h=box[2]-box[0],box[3]-box[1]
            record=dict(rect=frame,box=box,size=(w,h),planes=([0.]*(w*h),[0.]*(w*h)),cell=cell)
            frames[cell['identity']]=record
            for y in range(max(0,box[1]),min(size[1],box[3])):by_y.setdefault(y,[]).append(record)
    failures=set();values={};classified=0
    for y in range(size[1]):
        spans=rows.get(y,[])
        for x in range(size[0]):
            if any(l<=x<r for l,r in spans):
                classified+=1
                continue # Already classified by the control stratum.
            lx,ly=(x+.5)/dpr,(y+.5)/dpr
            panel=next((p for p in corpus['panels'] if p['x']<=lx<p['x']+p['width'] and p['y']<=ly<p['y']+p['height']),None)
            near=[]
            for frame in by_y.get(y,[]):
                distance=rounded_distance(lx,ly,frame['rect'],0)
                if max(distance,-distance-1)*dpr<=1:near.append(frame)
            if not near:
                classified+=1
                for side,(raw,_) in enumerate(images):
                    clear=observations[side]['capture']['clear_rgba']
                    if len(clear)!=4 or clear[3]!=1:raise RunError(3,'calibration','actual opaque control clear unavailable')
                    expected=bytes(panel['rgb']+[255]) if panel else bytes(round(v*255) for v in clear)
                    if raw[(y*size[0]+x)*4:(y*size[0]+x)*4+4]!=expected:failures.add('guard.complete-canvas')
            else:
                classified+=1
                near=near[:1] # Fixed source order gives touching frames one primary owner.
                for side,(raw,_) in enumerate(images):
                    pixel=raw[(y*size[0]+x)*4:(y*size[0]+x)*4+3];background=near[0]['cell']['background']
                    foreground=[255,255,255] if background[0]==0 else [0,0,0]
                    try:k=unmix_coverage([v/255 for v in pixel],[v/255 for v in foreground],[v/255 for v in background],1,byte_error=1/255)['coverage']
                    except ProtocolError:failures.add('guard.frame-material');k=0
                    for frame in near:
                        box=frame['box'];w=frame['size'][0]
                        if box[0]<=x<box[2] and box[1]<=y<box[3]:frame['planes'][side][(y-box[1])*w+x-box[0]]=min(1,max(0,k))
    if classified!=size[0]*size[1]:raise ProtocolError('control canvas classification is incomplete')
    for frame in frames.values():
        result=coverage_metrics(*frame['planes'],frame['size'])
        if not result['required_ink_present']:failures.add('guard.required-frame')
        for key in ('coverage_mass_relative_error','contour_distance_px','coverage_local_relative_error'):
            if result[key] is None:failures.add('guard.frame-contour')
            else:values['pixels.'+key]=max(values.get('pixels.'+key,0),result[key])
    return values,failures


def rounded_distance(x,y,rect,radius):
    radius=min(radius,rect['width']/2,rect['height']/2)
    qx=abs(x-rect['x']-rect['width']/2)-rect['width']/2+radius
    qy=abs(y-rect['y']-rect['height']/2)-rect['height']/2+radius
    return math.hypot(max(qx,0),max(qy,0))+min(max(qx,qy),0)-radius


def gaussian_rounded_shadow(x,y,rect,shadow,radius):
    """Independent Gaussian integral over a rounded box (fixed midpoint quadrature).

    CSS blur radius is 2 sigma. No production shader/reference function is used.
    256 subdivisions are fixed input, and never selected from captured residuals.
    """
    if shadow['blur']<=0 or shadow['spread'] != 0:
        raise ProtocolError('shadow model is outside finite pilot scope')
    left,top=rect['x']+shadow['offset'][0],rect['y']+shadow['offset'][1]
    return _gaussian_integral(x-left,y-top,rect['width'],rect['height'],shadow['blur'],radius)


from functools import lru_cache


@lru_cache(maxsize=100000)
def _gaussian_integral(x,y,width,height,blur,radius):
    sigma=blur/2
    dx=width/256
    total=0
    root2=math.sqrt(2)
    for i in range(256):
        u=(i+.5)*dx
        inset=0
        if u<radius:
            inset=radius-math.sqrt(max(0,radius*radius-(u-radius)**2))
        elif u>width-radius:
            inset=radius-math.sqrt(max(0,radius*radius-(u-(width-radius))**2))
        vertical=(math.erf((height-inset-y)/(sigma*root2))-math.erf((inset-y)/(sigma*root2)))/2
        total+=math.exp(-((u-x)/sigma)**2/2)/(sigma*math.sqrt(2*math.pi))*vertical*dx
    return total


def calibrate(options):
    """Independent entry: never calls ordinary runner.run or consumes pilot pixels."""
    import os
    import platform
    import sys
    import uuid
    from copy import deepcopy
    from .adapters import RealAdapters,process
    from . import comparison,runner_evidence,pilots
    from .runner import _probe,resolve_preset
    from .source_closure import source_manifest
    from .reporting import write_json
    from .comparison_dependencies import image_module,prepare as prepare_comparison
    from demo_reference_web import bootstrap,reference
    directory=output_path(options['output'])
    if directory.exists():
        raise ProtocolError('calibration output already exists; failed controls cannot be overwritten')
    directory.mkdir(parents=True)
    report={'schema_version':1,'calibration_kind':KIND,'status':'error','exit_code':4,'published':False}
    try:
        if options.get('suite') != 'pilot' or options.get('dpr','native') != 'native':
            raise ProtocolError('finite independent calibration requires --suite pilot --dpr native')
        repetitions=options.get('repetitions',5)
        if type(repetitions) is not int or not 5 <= repetitions <= 15:
            raise ProtocolError('independent repetitions must be between 5 and 15')
        configure,configuration=resolve_preset(options['preset'])
        binary=Path(options['binary']).resolve()
        control_binary=output_path(options['control_binary'])
        if not binary.is_file() or not control_binary.is_file():
            raise RunError(3,'environment','real Gallery/control binary is absent')
        adapters=RealAdapters()
        prepared=adapters.prepare(directory,options)
        definitions=pilots.definitions()
        for definition in definitions:
            adapters.font_preflight(definition,prepared)
        corpus=read(FIXTURES/'pilot-calibration.json')
        # Freeze exact input/policy bytes before producing any raster.
        write_json(directory/'frozen-corpus.json',corpus)
        policy={key:list(value) for key,value in POLICY.items()}
        write_json(directory/'frozen-policy.json',policy)
        native_manifest=source_manifest(ROOT)
        # Verify decoder, pure SFNT tooling, seven pinned files and both build
        # closures before starting a native or browser environment probe.
        site=options.get('comparison_site') or prepare_comparison(directory/'comparison-site',offline=options.get('offline',False))
        site=Path(site).resolve()
        library=image_module(site)
        prepared_info=bootstrap.verify_prepared(prepared['prepared'])
        admitted=[reference.admit_font(expected,actual,'pinned') for expected,actual in zip(pilots.FONTS,prepared['fonts'])]
        from demo_reference_web.font_resources import fonttools_library
        font_library=fonttools_library(ROOT/'out/demo-differential/archives','BUNDLED',offline=options.get('offline',False))
        font_descriptors=[{**font,'path':Path(font['path']).resolve().relative_to(ROOT).as_posix()} for font in prepared['fonts']]
        independent_fonts=all_font_priors(font_descriptors,corpus,font_library)
        bundle=directory/'control-bundle';consumer_bundle=directory/'consumer-bundle'
        for build,entry in ((bundle,'calibration.tsx'),(consumer_bundle,'app.tsx')):
            process([prepared_info['node'],ROOT/'tools/demo_reference_web/build.mjs',Path(prepared['prepared'])/'prepared.json',
                     build,ROOT/'tools/demo_reference_web'/entry],directory/('build-'+entry),90,failure_code=3)
        closure=read(bundle/'closure.json')
        if read(consumer_bundle/'closure.json')!=closure:
            raise RunError(3,'calibration','independent controls and ordinary consumer have different official import closures')
        write_json(directory/'prepared-receipt.json',prepared_info)
        probes=[]
        probe_case=_probe(definitions[0])
        probe_nonce=uuid.uuid4().hex
        native_stream=adapters.native(probe_case,probe_nonce,directory/'environment-native',prepared,options)
        native=runner_evidence.native(native_stream,probe_case,probe_nonce,native_manifest,sha(binary),configure,configuration)
        probes.append(dict(identity=definitions[0]['identity'],nonce=probe_nonce,stream='environment-native/observations.jsonl',request='environment-native/request.json'))
        actual_metrics=stable_device_metrics(runner_evidence.metrics(native,'native'))
        dpr=actual_metrics['dpr']
        capture_facts=comparison.capture_description(native)
        for index,other in enumerate(definitions[1:],1):
            other_case=_probe(other);other_nonce=uuid.uuid4().hex
            other_stream=adapters.native(other_case,other_nonce,directory/('environment-native-'+str(index)),prepared,options)
            observed=runner_evidence.native(other_stream,other_case,other_nonce,native_manifest,sha(binary),configure,configuration)
            probes.append(dict(identity=other['identity'],nonce=other_nonce,stream=f'environment-native-{index}/observations.jsonl',request=f'environment-native-{index}/request.json'))
            if stable_device_metrics(runner_evidence.metrics(observed,'native'))!=actual_metrics or comparison.capture_description(observed)!=capture_facts:
                raise RunError(3,'calibration','three pilot identities do not share one observed capture/font environment')
        if len(capture_facts['font_raster_policies'])!=1:
            raise RunError(3,'calibration','finite control font policy cannot uniquely reproduce the observed native policy')
        definition=deepcopy(definitions[0]);definition['fixture']['viewport']['dpr']=dpr
        icon_manifest=read(ROOT/'third_party/ant-design-icons/manifest.json')
        def web_process(request,output):
            output.mkdir(parents=True)
            for name in ('tmp','cache','config'):(output/name).mkdir()
            request_path=output.parent/(output.name+'-request.json')
            request_path.write_bytes(request_bytes(request))
            config={'prepared':prepared_info,'request_path':str(request_path),'output':str(output),'build':str(bundle),
                    'actual_font_sources':admitted}
            config_path=output/'worker.json';write_json(config_path,config)
            process([prepared_info['node'],ROOT/'tools/demo_reference_web/calibration.mjs',config_path],
                    output/'process',options.get('timeout',60)+30,failure_code=4,launch_id=request['process_launch_id'])
        probe_request=dict(calibration_kind=KIND,nonce=uuid.uuid4().hex,board='train',fault='none',restart=False,
            repetitions=0,actual_dpr=dpr,cells=control_cells(corpus,'train'),panels=corpus['panels'],
            font_files=prepared['fonts'],binding={},process_launch_id=uuid.uuid4().hex)
        web_process(probe_request,directory/'environment-web')
        browser=read(directory/'environment-web/browser.json')
        lock=bootstrap.load_lock()
        web_provenance={'source_commit':lock['source']['commit'],'source_tree_sha256':lock['source']['components_tree_sha256'],
            'source_closure':closure,'browser':browser,'environment':{'browser_libraries':prepared_info['browser_libraries'],
                'browser_sha256':prepared_info['browser_sha256'],'node_sha256':prepared_info['node_sha256']}}
        web={'hello':{'provenance':web_provenance}}
        bound=comparison.binding(definition,native,web)
        environment=comparison.environment_description(definition,native['hello']['provenance'],web_provenance,actual_metrics,capture_facts)
        bound['environment_hash']=digest(environment)
        paths={ROOT/'tools/run_demo_differential.py',binary,control_binary,Path(prepared_info['node']),Path(prepared_info['browser'])}
        paths.add(Path(prepared['prepared'])/'prepared.json')
        paths.add(ROOT/'third_party/ant-design-icons/manifest.json')
        paths.update(ROOT/'third_party/ant-design-icons'/r['file'] for r in icon_manifest['icons'] if r['name'] in corpus['icons'])
        paths.update(Path(font['path']).resolve() for font in prepared['fonts'])
        paths.update(p for p in font_library.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc')
        for area in ('tools/demo_differential','tools/demo_reference_web',str(Path(site).relative_to(ROOT))):
            paths.update(p for p in (ROOT/area).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc')
        tools={'python':sys.version,'native_binary':binary.relative_to(ROOT).as_posix(),
               'control_binary':control_binary.relative_to(ROOT).as_posix(),
               'files':[{'path':p.relative_to(ROOT).as_posix(),'sha256':sha(p),'bytes':p.stat().st_size}
                                           for p in sorted(paths,key=lambda p:p.as_posix())]}
        bound['tool_hash']=digest(tools)
        scope=dict(cases=CASES,locale='zh-CN',direction='ltr',theme='default',motion='disabled',font_policy='pinned',
            actual_dpr=dpr,origin_phase_px=[0,0],materials=corpus['materials'],
            font_sizes_weights=[dict(size=s,weight=w) for s in corpus['font_sizes'] for w in corpus['font_weights']])
        manifest=dict(schema_version=1,calibration_kind=KIND,binding=bound,
            source=comparison.source_description(native['hello']['provenance'],web_provenance),
            environment=environment,
            tools=tools,scope=scope,color=None,corpus=corpus,policy=policy,artifacts=[],trials=[],
            fonts=font_descriptors,fonttools_library=font_library.relative_to(ROOT).as_posix(),font_priors=independent_fonts,
            bootstrap=dict(native_probes=probes,browser='environment-web/browser.json',prepared='prepared-receipt.json',
                prepared_directory=Path(prepared['prepared']).resolve().relative_to(ROOT).as_posix(),
                control_bundle={p.name:p.relative_to(directory).as_posix() for p in bundle.iterdir() if p.is_file()},
                consumer_bundle={p.name:p.relative_to(directory).as_posix() for p in consumer_bundle.iterdir() if p.is_file()}))
        def controls(partition,board,fault,identity,count,restart=False):
            base=directory/identity
            base.mkdir(parents=True)
            request=control_request(manifest,side='native',partition=partition,board=board,fault=fault,identity=identity,
                nonce=uuid.uuid4().hex,launch_id=uuid.uuid4().hex,repetitions=count,restart=restart)
            request_path=base/'native-request.json';request_path.write_bytes(request_bytes(request))
            argv=[control_binary,'--request',request_path,'--output',base/'native']
            if platform.system()=='Linux':
                argv=[sys.executable,'-B',ROOT/'tools/run_background_wayland.py','--log-dir',base/'wayland',
                      '--timeout',str(options.get('timeout',60)),'--',*argv]
            process(argv,base/'native-process',options.get('timeout',60)+20,launch_id=request['process_launch_id'])
            web_request=control_request(manifest,side='web',partition=partition,board=board,fault=fault,identity=identity,
                nonce=uuid.uuid4().hex,launch_id=uuid.uuid4().hex,repetitions=count,restart=restart)
            # fault is the trial identity; the actual per-side input is preserved
            # in worker/request files, never changed after the producer exits.
            web_process(web_request,base/'web')
            for i in range(count):
                manifest['trials'].append(dict(identity=identity if partition=='negative' else identity+'.'+str(i),
                    partition=partition,board=board,fault=fault,restart=restart,
                    native={'stream':f'{identity}/native/stream-{i}.json','capture':f'{identity}/native/capture-{i}.bmp',
                        'request':f'{identity}/native-request.json','launch':f'{identity}/native-process/launch.json','ordinal':i},
                    web={'stream':f'{identity}/web/stream-{i}.json','capture':f'{identity}/web/capture-{i}.png',
                        'request':f'{identity}/web-request.json','launch':f'{identity}/web/process/launch.json','ordinal':i}))
        controls('train','train','none','train-repeats',repetitions)
        controls('train','train','none','train-restart',1,True)
        controls('holdout','holdout','none','holdout-repeats',repetitions)
        controls('holdout','holdout','none','holdout-restart',1,True)
        for negative in corpus['negative']:
            controls('negative','train',negative['fault'],negative['identity'],1)
        first=read(directory/'train-repeats/native/stream-0.json')['capture']
        color={'native':dict(framebuffer_format=first['framebuffer_format'],source_channel_order=first['source_channel_order'],
            decoded_channel_order='RGBA',encoding='native-encoded-SDR',alpha='premultiplied',clear_rgba=manifest['environment']['native_capture']['clear_rgba'],
            opaque_required=True,codec_policy='opaque-BMP-rawRGBA-v1'),
            'web':dict(file_formats=['PNG RGB8','PNG RGBA8'],decoded_channel_order='RGBA',encoding='sRGB-SDR',
                alpha_policy='RGB-opaque-or-verified-RGBA-opaque',launch_args_hash=digest(browser['launch_args']),
                png_color_chunks_policy='record-and-verify-chunks-CRC-v1',
                png_color_chunks=png_metadata(directory/'train-repeats/web/capture-0.png')['color_chunks']),
            'comparison':dict(domain='sRGB-encoded-opaque',conversion='verified-opaque-identity-v1',lab='sRGB-D65-CIEDE2000-v1'),
            'control_evidence_sha256':'0'*64}
        # Freeze actual trial/probe/build roles, not the disposable installation
        # or browser cache. Those dependencies have their independent tool lock.
        artifact_paths={directory/name for name in ('frozen-corpus.json','frozen-policy.json','prepared-receipt.json','environment-web-request.json')}
        roots=[directory/name for name in ('environment-native','environment-native-1','environment-native-2','environment-web','control-bundle','consumer-bundle',
            'train-repeats','train-restart','holdout-repeats','holdout-restart')]
        roots.extend(directory/n['identity'] for n in corpus['negative'])
        for base in roots:
            artifact_paths.update(p for p in base.rglob('*') if p.is_file() and not {'tmp','cache','config'}&set(p.relative_to(base).parts[:-1]))
        for p in sorted(artifact_paths):
            if p.is_file() and p.suffix in ('.rgba','.bmp','.png','.json','.jsonl','.js','.css','.html'):
                manifest['artifacts'].append(dict(path=p.relative_to(directory).as_posix(),sha256=sha(p),bytes=p.stat().st_size))
        color['control_evidence_sha256']=digest(manifest['artifacts'])
        manifest['color']=color
        rows=evaluate_trials(manifest,{a['path']:directory/a['path'] for a in manifest['artifacts']},directory,image_module=library)
        encoded=json.dumps(manifest,ensure_ascii=False,sort_keys=True,separators=(',',':')).encode()+b'\n'
        evidence_sha=hashlib.sha256(encoded).hexdigest();evidence_path=directory/(evidence_sha+'.json')
        evidence_path.write_bytes(encoded)
        profile=dict(schema_version=1,identity='pilot-pinned-font',**bound,corpus_hash=digest(corpus),repetitions=repetitions,
            calibration_kind=KIND,scope=scope,calibrated=True,metrics=rows,masks=[],color=color,
            evidence=dict(path=evidence_path.relative_to(ROOT).as_posix(),sha256=evidence_sha,
                train_ids=[t['identity'] for t in manifest['trials'] if t['partition']=='train'],
                holdout_ids=[t['identity'] for t in manifest['trials'] if t['partition']=='holdout'],
                negative_ids=[t['identity'] for t in manifest['trials'] if t['partition']=='negative'],
                control_definition_sha256=sha(FIXTURES/'pilot-calibration.json'),policy_sha256=digest(policy),
                observations_and_captures_manifest_sha256=digest(manifest['artifacts'])))
        verify_profile_evidence(profile,image_module=library)
        pending=directory/'.profile.pending';write_json(pending,profile)
        os.replace(pending,directory/'profile.json')
        report.update(status='pass',exit_code=0,published=True,profile='profile.json')
        return 0
    except Exception as error:
        code=(error.code if isinstance(error,RunError) else 3 if isinstance(error,(OSError,ImportError)) else
              2 if isinstance(error,(ProtocolError,ValueError,KeyError,TypeError)) else 4)
        report.update(exit_code=code,message=str(error))
        return code
    finally:
        write_json(directory/'calibration-report.json',report)


def verify_transparent_probe(prefix,corpus,trial,paths,previous_fence,actual_metrics):
    name=(prefix/'transparent-probe.json').as_posix()
    if name not in paths:
        raise RunError(4,'protocol','transparent raw probe is absent')
    probe=read(paths[name])
    exact_fields(probe,'raw raw_sha256 width height dpr readback_id fence_id fence_completed cells clear_rgba storage','transparent probe')
    raw_name=(prefix/relative_path(probe['raw'])).as_posix()
    if raw_name not in paths or sha(paths[raw_name]) != probe['raw_sha256']:
        raise ProtocolError('transparent raw probe bytes differ')
    if (probe['fence_completed'] is not True or probe['readback_id'] != probe['fence_id'] or
            probe['fence_id'] <= previous_fence or probe['clear_rgba'] != [0,0,0,0] or
            probe['storage'] != 'native-encoded-SDR/premultiplied'):
        raise RunError(4,'protocol','transparent probe has a stale or merged fence/storage receipt')
    expected=[c for c in mutated_cells(corpus,trial['board'],trial['fault']) if c['kind']=='alpha']
    if probe['cells'] != expected:
        raise ProtocolError('transparent control input was changed')
    raw=paths[raw_name].read_bytes()
    width,height=probe['width'],probe['height']
    if (width,height,probe['dpr'])!=actual_metrics or len(raw) != width*height*4 or raw[:4] != bytes(4):
        raise RunError(3,'calibration','transparent black clear/raw extent differs')
    for cell in expected:
        r=cell['rect'];dpr=probe['dpr']
        x,y=int((r['x']+r['width']/2)*dpr),int((r['y']+r['height']/2)*dpr)
        if not 0 <= x < width or not 0 <= y < height:
            raise ProtocolError('transparent control is uncaptured')
        observed=raw[(y*width+x)*4:(y*width+x)*4+4]
        alpha=cell['source_alpha']*cell.get('paint_opacity',1)
        expected_rgba=[round(v*alpha) for v in cell['rgb']]+[round(255*alpha)]
        if max(abs(a-b) for a,b in zip(observed,expected_rgba))>1:
            raise RunError(3,'calibration','transparent raw does not satisfy encoded premultiplied/source-over storage')
    # The entire raw plane is checked. Only the independently declared 1 px
    # geometric edge band may vary in coverage; it still obeys premultiplication.
    dpr=probe['dpr'];rows={}
    for cell in expected:
        r=cell['rect']
        for y in range(max(0,math.floor(r['y']*dpr)-1),min(height,math.ceil((r['y']+r['height'])*dpr)+1)):
            rows.setdefault(y,[]).append(cell)
    for y in range(height):
        candidates=rows.get(y,[])
        for x in range(width):
            observed=raw[(y*width+x)*4:(y*width+x)*4+4]
            near=[(cell,rounded_distance((x+.5)/dpr,(y+.5)/dpr,cell['rect'],0)*dpr)
                  for cell in candidates if cell['rect']['x']*dpr-1<=x+.5<=(cell['rect']['x']+cell['rect']['width'])*dpr+1]
            near=[pair for pair in near if pair[1]<=1]
            if not near:
                if observed!=bytes(4):raise RunError(3,'calibration','transparent control guard is not zero RGBA')
                continue
            if len(near)!=1:raise ProtocolError('transparent controls overlap')
            cell,distance=near[0];alpha=cell['source_alpha']*cell.get('paint_opacity',1)
            if observed[3]>round(alpha*255)+1 or any(abs(observed[i]-cell['rgb'][i]*observed[3]/255)>1.01 for i in range(3)):
                raise RunError(3,'calibration','transparent edge is not bounded premultiplied source coverage')
            if distance < -1 and abs(observed[3]-round(alpha*255))>1:
                raise RunError(3,'calibration','transparent control interior alpha differs')


def font_priors(fonts,corpus,library):
    """Read-only independent SFNT cmap/hmtx/outlines using the existing locked tool."""
    return _sfnt_priors(fonts,corpus,library)


def all_font_priors(fonts,corpus,library):
    from .formal_icon_source import resource,official_font_bytes
    result=font_priors(fonts,corpus,library)
    codepoints=[layer['codepoint'] for name in corpus['icons'] for layer in resource(name)[0]['layers']]
    result.update(font_priors_from_bytes(official_font_bytes(),codepoints,library))
    return result


def font_priors_from_bytes(data,codepoints,library,face_index=0):
    """Same oracle for immutable official_font_bytes(); no raster input or file write.

    Returns the existing SHA256 + ':' + scalar key and unchanged prior fields
    (glyph_id/units/advance/ascent/descent/bounds/contours). outline_point consumes
    these contours in SFNT coordinates. Caller still verifies the official source.
    """
    if type(data) is not bytes or not data or len(data)>32*1024*1024:
        raise ProtocolError('immutable bounded SFNT bytes required')
    if (not codepoints or len(codepoints)>4096 or any(type(cp) is not int or not 0<=cp<=0x10ffff or
            0xd800<=cp<=0xdfff for cp in codepoints)):
        raise ProtocolError('bounded actual scalar inventory required')
    fingerprint=hashlib.sha256(data).hexdigest()
    descriptor=dict(identity='official-icon',sha256=fingerprint,face_index=face_index,weight=400,path='embedded-official-icon')
    corpus=dict(train=dict(text=[chr(cp) for cp in dict.fromkeys(codepoints)]),holdout=dict(text=[]))
    return _sfnt_priors([descriptor],corpus,library,{fingerprint:data})


def _sfnt_priors(fonts,corpus,library,in_memory=None):
    import sys
    import io
    library=Path(library).resolve()
    if any(p.suffix in ('.pyc','.pyd','.so','.dll') for p in library.rglob('*') if p.is_file()):
        raise RunError(3,'calibration','font prior tool requires verified pure source without cached/compiled overrides')
    sys.path.insert(0,str(library))
    previous_bytecode=sys.dont_write_bytecode
    sys.dont_write_bytecode=True
    try:
        import fontTools
        if fontTools.__version__!='4.60.1' or not Path(fontTools.__file__).resolve().is_relative_to(library):
            raise RunError(3,'calibration','locked pure-Python fontTools 4.60.1 is required')
        from fontTools.ttLib import TTFont
        from fontTools.pens.basePen import BasePen
        from fontTools.pens.boundsPen import BoundsPen
        result={}
        for descriptor in fonts:
            exact_fields(descriptor,'identity sha256 face_index weight path','font prior source')
            if descriptor['identity']=='noto-emoji':continue
            if in_memory is None:
                path=output_path(ROOT/relative_path(descriptor['path']))
                if sha(path)!=descriptor['sha256']:
                    raise RunError(3,'calibration','SFNT prior font bytes changed')
            else:
                data=in_memory[descriptor['sha256']]
                if hashlib.sha256(data).hexdigest()!=descriptor['sha256']:
                    raise ProtocolError('in-memory SFNT source hash differs')
                path=io.BytesIO(data)
            with TTFont(path,fontNumber=descriptor['face_index'],recalcTimestamp=False) as font:
                if 'fvar' in font or font['OS/2'].usWeightClass!=descriptor['weight']:
                    raise RunError(3,'calibration','font prior is not the pinned static actual weight')
                units=font['head'].unitsPerEm
                glyphset=font.getGlyphSet()
                class OutlinePen(BasePen):
                    def __init__(self):
                        super().__init__(glyphset);self.contours=[];self.points=[];self.point=None
                    def _moveTo(self,point):self.points=[list(point)];self.point=point
                    def _lineTo(self,point):self.points.append(list(point));self.point=point
                    def _curveToOne(self,p1,p2,p3):self.curve((self.point,p1,p2,p3))
                    def _qCurveToOne(self,p1,p2):self.curve((self.point,p1,p2))
                    def _closePath(self):
                        if self.points[-1]!=self.points[0]:self.points.append(self.points[0])
                        self.contours.append(self.points);self.points=[];self.point=None
                    def _endPath(self):raise ProtocolError('open SFNT outline is outside pilot scope')
                    def curve(self,points):
                        degree=len(points)-1
                        curvature=max(math.hypot(*(degree*(degree-1)*(points[i][j]-2*points[i+1][j]+points[i+2][j]) for j in (0,1)))
                                      for i in range(len(points)-2))
                        count=max(8,math.ceil(math.sqrt(curvature*(80/units)*1024/8)))
                        if count>4096:raise ProtocolError('font outline numerical subdivision bound exceeded')
                        for step in range(1,count+1):
                            t=step/count
                            point=tuple(sum(math.comb(degree,i)*(1-t)**(degree-i)*t**i*p[j] for i,p in enumerate(points)) for j in (0,1))
                            self._lineTo(point)
                cmap=font.getBestCmap()
                chars=corpus['train']['text']+corpus['holdout']['text']
                for char in chars:
                    if (ord(char)<128)!=(descriptor['identity']=='noto-sans'):continue
                    name=cmap.get(ord(char))
                    if not name:raise RunError(3,'calibration','independent SFNT cmap has no control scalar')
                    pen=OutlinePen();glyphset[name].draw(pen)
                    bounds=BoundsPen(glyphset);glyphset[name].draw(bounds)
                    if not pen.contours or bounds.bounds is None:
                        raise RunError(3,'calibration','printable control SFNT outline is empty')
                    result[descriptor['sha256']+':'+char]=dict(glyph_id=font.getGlyphID(name),units=units,
                        advance=font['hmtx'].metrics[name][0],ascent=font['hhea'].ascent,descent=font['hhea'].descent,
                        bounds=list(bounds.bounds),contours=pen.contours)
        return result
    except ImportError as error:
        raise RunError(3,'calibration','locked font prior tool cannot be imported') from error
    finally:
        sys.dont_write_bytecode=previous_bytecode
        sys.path.pop(0)


def outline_point(contours,x,y):
    """Nonzero winding and nearest segment distance, independent of FT/renderer."""
    winding=0
    distance=math.inf
    for contour in contours:
        for a,b in zip(contour,contour[1:]):
            dx,dy=b[0]-a[0],b[1]-a[1]
            norm=dx*dx+dy*dy
            u=max(0,min(1,((x-a[0])*dx+(y-a[1])*dy)/norm)) if norm else 0
            distance=min(distance,math.hypot(x-a[0]-u*dx,y-a[1]-u*dy))
            cross=dx*(y-a[1])-dy*(x-a[0])
            if a[1]<=y<b[1] and cross>0:winding+=1
            elif b[1]<=y<a[1] and cross<0:winding-=1
    return winding!=0,distance
