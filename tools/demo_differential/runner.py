"""Finite select/prepare/preflight/execute/validate/compare/report/cleanup runner."""
from __future__ import annotations

from copy import deepcopy
import json
import platform
from pathlib import Path
import subprocess
import sys
import time
import uuid

from .adapters import RealAdapters, RunError
from . import comparison, runner_evidence
from .inventory import VECTORS, coverage_gaps, require_exact, require_retained, source_inventory
from .observations import IncompleteRun
from .oracle import evaluate, projected_trace
from .protocol import ROOT, FIXTURES, ProtocolError, output_path, read, unique, validate
from .reporting import publish, write_json
from .source_closure import source_manifest
from .font_requirements import required_faces
from .meta_test import definitions_digest, fault_for_case, guard_ordinary_run


def resolve_preset(name):
    presets = read(ROOT / 'CMakePresets.json')
    row = next((p for p in presets['buildPresets'] if p['name'] == name), None)
    if row is None or not row.get('configurePreset') or row.get('configuration') not in ('Debug', 'Release'):
        raise ProtocolError('a concrete CMake build preset is required')
    configure = row['configurePreset']
    if 'headless' in configure:
        raise RunError(3, 'environment', 'paired native run requires the real platform/GPU Gallery preset')
    host = {'Windows': 'windows-', 'Linux': 'linux-'}.get(platform.system())
    if not host or not configure.startswith(host):
        raise RunError(3, 'environment', 'build preset is not for this actual operating system')
    return configure, row['configuration']


def select(definitions, options):
    for case in definitions:
        validate('case', case)
    unique(definitions)
    requested = options.get('cases', [])
    missing = set(requested) - {c['identity'] for c in definitions}
    if missing:
        raise ProtocolError('unknown selected case identities: ' + str(sorted(missing)))
    if options.get('suite', 'pilot') != 'pilot':
        raise ProtocolError('this phase implements the explicit pilot denominator only')
    selected = [c for c in definitions if (not requested or c['identity'] in requested) and
                (not options.get('component') or c['native']['route'].split('/')[0] == options['component']) and
                (not options.get('family') or c['native']['family'] == options['family'])]
    if not selected:
        raise ProtocolError('empty case selection is not execution success')
    return selected


def case_coverage(case, inventory):
    sample = next((s for s in inventory['samples'] if s['identity'] == case['native']['sample_id']), None)
    if not sample or any(sample[k] != case['native'][k] for k in ('family', 'source_function', 'public_header')):
        raise ProtocolError('case does not bind a real sample/source identity: ' + case['identity'])
    routes = {r['identity'] for r in inventory['routes']}
    if case['native']['route'] not in routes:
        raise ProtocolError('case route/family is not registered')
    targets = [r['identity'] for r in inventory['targets'] if r['sample'] == sample['identity']]
    if case['native']['targets'] != targets:
        raise ProtocolError('pilot must preserve all ordered original sample target identities')
    for declaration in case['checkpoints']:
        if not set(declaration['targets']) <= set(targets) | {sample['identity']}:
            raise ProtocolError('pilot checkpoint substitutes an unregistered target')
        if not {'geometry', 'token', 'pixels', 'text'} <= set(declaration['layers']):
            raise ProtocolError('pilot checkpoint omitted a required comparison layer')
        kinds = {r['kind'] for r in declaration['rois']}
        if not {'sample-context', 'control', 'guard-band'} <= kinds:
            raise ProtocolError('pilot omitted context/control/guard coverage')
        unique(declaration['rois'])
        controls = {r['target'] for r in declaration['rois'] if r['kind'] == 'control'}
        if controls != set(declaration['targets']) - {sample['identity']}:
            raise ProtocolError('pilot omitted an observed control ROI')
    return targets


def _probe(case):
    result = deepcopy(case)
    checkpoint = case['checkpoints'][0]
    step = checkpoint['step']
    action = next(a for a in case['actions'] if a['step'] == step)
    result['actions'] = [{**action, 'time_us': 0}]
    result['checkpoints'] = [deepcopy(checkpoint)]
    result['oracle'] = [{'op': 'count', 'side': 'both', 'origin': 'independent-contract', 'step': step,
                         'stream': 'callbacks', 'target': case['native']['targets'][0], 'event': 'activate', 'equals': 0}]
    return validate('case', result)


def _artifacts(directory, case_dir):
    # Index raw logs/streams/images directly; no output is copied to fixtures.
    return sorted(p.relative_to(directory).as_posix() for p in case_dir.rglob('*') if p.is_file())


def owned_stream(path, directory):
    path, directory = Path(path), Path(directory)
    if (path.absolute() != (directory / 'observations.jsonl').absolute() or
            not path.resolve().is_relative_to(directory.absolute())):
        raise IncompleteRun('adapter observation stream escapes its fresh case directory')
    return path


def oracle_view(observed, side):
    if side == 'native':
        return observed
    view = deepcopy(observed)
    # Only translate the existing actual DOM focus field to the shared oracle
    # key; raw streams stay intact and callback projections remain lossless.
    for row in view['checkpoints'].values():
        if 'focused' in row['state']:
            row['state']['focused_target'] = row['state']['focused']
    return view


def run(options, definitions, *, adapters=None, comparator=None, coverage_document=None):
    directory = output_path(options['output'])
    if directory.exists():
        raise ProtocolError('output already exists; old out directories cannot be reused')
    directory.mkdir(parents=True)
    nonce = uuid.uuid4().hex
    adapters = adapters or RealAdapters()
    compare = comparator or comparison.compare
    started = time.monotonic()
    argv = options.get('repro_argv') or [sys.executable, '-B', str(ROOT / 'tools/run_demo_differential.py'), *sys.argv[1:]]
    report = {'schema_version': 1, 'nonce': nonce, 'status': 'error', 'exit_code': 4,
              'selected': [], 'expected': [], 'completed': [], 'passed': [], 'cases': [],
              'errors': [{'category': 'protocol', 'message': 'run did not complete'}],
              'provenance': {'fault': options.get('fault', 'none'), 'reference_cache_published': False},
              'coverage': {'scope': 'pilot', 'paired_pass': 0, 'all_coverage_claimed': False}, 'timings': {},
              'repro_argv': argv, 'replay_argv': [sys.executable, '-B', str(ROOT / 'tools/run_demo_differential.py'),
                                               'replay', '--failure', str(directory / 'report.json'),
                                               '--output', str(directory) + '-replay'], 'desktop_fallback': False}
    publish(directory, report)  # Fail state exists before any adapter starts.
    codes = []
    report['errors'] = []
    current_stage = None
    stage_start = started
    def stage(name):
        nonlocal current_stage, stage_start
        now = time.monotonic()
        if current_stage:
            report['timings'][current_stage] = report['timings'].get(current_stage, 0) + now - stage_start
        current_stage, stage_start = name, now
    def error_info(error):
        if isinstance(error, RunError):
            return error.code, error.category
        if isinstance(error, (IncompleteRun, subprocess.SubprocessError)):
            return 4, 'protocol'
        if isinstance(error, OSError):
            return 3, 'environment'
        return 2, 'protocol'
    try:
        stage('select')
        selected = select(definitions, options)
        fault_id = options.get('fault', 'none')
        if fault_id != 'none' and not any(fault_for_case(fault_id, c['identity']) != 'none' for c in selected):
            raise ProtocolError('acceptance fault does not target a selected pilot case')
        report['selected'] = report['expected'] = [c['identity'] for c in selected]
        report['cases'] = [{'identity': c['identity'], 'verdict': 'not-run', 'categories': [], 'first_step': '',
                            'failures': [], 'artifacts': []} for c in selected]
        ledger = coverage_document if coverage_document is not None else read(FIXTURES / 'pilot-coverage.json')
        validate('coverage', ledger)
        report['coverage']['definition_ledger_sha256'] = comparison.digest(ledger)
        report['provenance']['mechanism_controls'] = coverage_document is not None or comparator is not None
        configure, configuration = resolve_preset(options['preset'])
        binary = Path(options['binary']).resolve()
        if not binary.is_file() or not 1 <= options['timeout'] <= 120:
            raise ProtocolError('a built binary and timeout in 1..120 seconds are required')
        binary_hash = runner_evidence.sha(binary)
        manifest = source_manifest(ROOT)
        actual = adapters.inventory(binary, directory / 'inventory.json')
        require_exact(source_inventory(), actual)
        delta = require_retained(actual, read(FIXTURES / 'identity-baseline.json'))
        for case in selected:
            case_coverage(case, actual)
        # The pilot ledger is a scoped subset, but every declared row must still
        # join the original obligation fields. Planned rows never become passes.
        coverage_gaps(actual, ledger, [c['identity'] for c in selected])
        report['coverage'].update(counts={v: len(actual[v]) for v in VECTORS}, baseline_delta=delta,
                                  gaps=coverage_gaps(actual, read(FIXTURES / 'coverage.json')))
        write_json(directory / 'source-manifest.json', manifest)
        report['provenance'].update({'source': manifest, 'binary_sha256': binary_hash, 'configure_preset': configure,
                                'configuration': configuration, 'python': sys.version, 'os': platform.platform(),
                                'definitions': {c['identity']: comparison.digest(c) for c in selected}})
        report['provenance']['required_font_faces'] = {c['identity']: required_faces(c) for c in selected}
        report['provenance']['profile_sha256'] = runner_evidence.sha(options['profile_file'])
        report['provenance']['meta_binding'] = {'reference': comparison.digest({}),
            'cases': definitions_digest(selected), 'profile': report['provenance']['profile_sha256'],
            'source': manifest['source_hash']}
        report['provenance']['reference_cache_keys'] = {}
        stage('prepare')
        prepared = adapters.prepare(directory, options)
        for case, result in zip(selected, report['cases']):
            case_dir = directory / 'cases' / case['identity']
            case_dir.mkdir(parents=True)
            try:
                stage('preflight')
                definition = deepcopy(case)
                write_json(case_dir / 'definition.json', definition)
                adapters.font_preflight(definition, prepared)
                rows = [r for r in ledger['rows'] if case['identity'] in r['cases']]
                required = {'sample/' + case['native']['sample_id']} | {'target/' + t for t in case['native']['targets']}
                if not required <= {r['identity'] for r in rows} or any(
                        r['mapping'] != 'paired' or r['implementation'] != 'ready' for r in rows):
                    raise RunError(2, 'coverage', 'selected pilot obligations are missing/planned; they cannot be counted pass')
                fonts = [{k: f[k] for k in ('identity', 'sha256', 'face_index', 'weight')} for f in prepared['fonts']]
                if fonts != definition['fixture']['fonts']:
                    raise RunError(3, 'environment', 'prepared font chain does not match declared input')
                profile = read(options['profile_file'])
                if runner_evidence.sha(options['profile_file']) != report['provenance']['profile_sha256']:
                    raise ProtocolError('profile changed during this run')
                validate('profile', profile)
                if not profile['calibrated']:
                    raise RunError(3, 'calibration', 'profile is uncalibrated; actual platform controls are required')
                probe_case = _probe(definition)
                probe_nonce = nonce + '-probe-' + str(len(report['completed']))
                probe_path = adapters.native(probe_case, probe_nonce, case_dir / 'preflight-native', prepared, options)
                probe = runner_evidence.native(owned_stream(probe_path, case_dir / 'preflight-native'), probe_case,
                                                probe_nonce, manifest, binary_hash, configure, configuration,
                                                expected_fault=fault_for_case(fault_id, case['identity']))
                observed = runner_evidence.metrics(probe, 'native')
                dpr = definition['fixture']['viewport']['dpr']
                requested_dpr = options.get('dpr', dpr)
                if requested_dpr != 'native' and observed['dpr'] != float(requested_dpr):
                    raise RunError(3, 'environment', 'requested DPR differs from actual native window')
                if dpr != 'native' and dpr != observed['dpr']:
                    raise RunError(3, 'environment', 'declared DPR differs from actual native window')
                definition['fixture']['viewport']['dpr'] = observed['dpr']
                # Only resolve the declared DPR input. Never read native measured
                # bounds into Web styles, expected geometry, data or oracle.
                for item in definition['official']['adaptation']:
                    item['value'] = deepcopy(definition['fixture'][item['field']])
                adapters.web_preflight(definition, nonce, prepared)
                write_json(case_dir / 'resolved-definition.json', definition)
                stage('execute')
                native_path = adapters.native(definition, nonce, case_dir / 'native', prepared, options)
                # Faults still execute the real comparison. Cache publication
                # is independently prohibited in the Web adapter for the run.
                stage('validate')
                n = runner_evidence.native(owned_stream(native_path, case_dir / 'native'), definition, nonce,
                                          manifest, binary_hash, configure, configuration,
                                          expected_fault=fault_for_case(fault_id, case['identity']))
                if runner_evidence.sha(binary) != binary_hash:
                    raise ProtocolError('binary changed during execution')
                if runner_evidence.metrics(n, 'native') != observed:
                    raise RunError(3, 'environment', 'native metrics changed after preflight')
                stage('execute')
                web_path = adapters.web(definition, nonce, case_dir / 'web', prepared, options)
                stage('validate')
                w = runner_evidence.web(owned_stream(web_path, case_dir / 'web'), definition, nonce)
                report['provenance']['reference_cache_keys'][case['identity']] = w['hello']['provenance'].get('cache_key')
                report['provenance']['meta_binding']['reference'] = comparison.digest(report['provenance']['reference_cache_keys'])
                runner_evidence.effective_inputs(definition, n, w)
                report['completed'].append(case['identity'])
                stage('compare')
                failures = evaluate(definition, oracle_view(n, 'native'), 'native') + evaluate(definition, oracle_view(w, 'web'), 'web')
                if projected_trace(n) != projected_trace(w):
                    nt, wt = projected_trace(n), projected_trace(w)
                    index = next((i for i in range(min(len(nt), len(wt))) if nt[i] != wt[i]), min(len(nt), len(wt)))
                    first = nt[index] if index < len(nt) else wt[index]
                    failures.append({'category': 'logic', 'step': first['step'], 'message': 'projected callback trace differs',
                                     'native': projected_trace(n), 'web': projected_trace(w)})
                if comparator is None:
                    from .comparison_dependencies import image_module, prepare
                    library = options.get('comparison_site')
                    if not library:
                        try:
                            library = prepare(directory / 'comparison-dependencies', offline=options.get('offline', False))
                        except ProtocolError as error:
                            raise RunError(3, 'environment', str(error)) from error
                        options['comparison_site'] = library
                    try:
                        images = image_module(library)
                    except ProtocolError as error:
                        raise RunError(3, 'environment', str(error)) from error
                else:
                    images = None  # Pure unit tests may inject mechanism controls.
                errors, measurements = compare(definition, n, w, {'native': Path(native_path).parent,
                                                                  'web': Path(web_path).parent}, profile, images)
                failures.extend(errors)
                write_json(case_dir / 'comparison.json', measurements)
                write_json(case_dir / 'trace.json', {'native': projected_trace(n), 'web': projected_trace(w)})
                write_json(case_dir / 'geometry.json', {side: {step: row['targets'] for step, row in observed_side['checkpoints'].items()}
                                                       for side, observed_side in (('native', n), ('web', w))})
                order = {a['step']: i for i, a in enumerate(definition['actions'])}
                failures.sort(key=lambda f: order.get(f.get('step', ''), -1))
                result.update(verdict='fail' if failures else 'pass', failures=failures,
                              categories=list(dict.fromkeys(f['category'] for f in failures)),
                              first_step=failures[0].get('step', '') if failures else '')
                codes.append(1 if failures else 0)
                if not failures:
                    report['passed'].append(case['identity'])
            except (ProtocolError, OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
                code, category = error_info(error)
                codes.append(code)
                result.update(verdict='error', categories=[category], failures=[{'category': category,
                              'stage': current_stage, 'message': str(error), 'exit_code': code,
                              'details': getattr(error, 'details', None)}])
            finally:
                result['artifacts'] = _artifacts(directory, case_dir)
    except (ProtocolError, OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        code, category = error_info(error)
        codes.append(code)
        report['errors'].append({'category': category, 'message': str(error)})
    except KeyboardInterrupt:
        codes.append(4)
        report['errors'].append({'category': 'protocol', 'message': 'run cancelled; remaining cases not-run'})
    finally:
        stage('cleanup')
        try:
            adapters.cleanup()
        except Exception as error:
            codes.append(4)
            report['errors'].append({'category': 'protocol', 'message': 'cleanup failed: ' + str(error)})
        stage('report')
        report['exit_code'] = max(codes, default=4)
        report['status'] = 'error' if report['exit_code'] >= 2 else 'fail' if report['exit_code'] else 'pass'
        report['provenance']['reference_cache_published'] = getattr(adapters, 'reference_cache_published', False)
        try:
            guard_ordinary_run(report, options.get('fault', 'none'))
        except ProtocolError as error:
            report['exit_code'] = max(2, report['exit_code'])
            report['status'] = 'error'
            report['errors'].append({'category': 'protocol', 'message': str(error)})
        report['coverage']['paired_pass'] = len(report['passed'])
        report['timings']['total'] = time.monotonic() - started
        report['provenance']['options'] = {k: str(v) if isinstance(v, Path) else v for k, v in options.items()}
        publish(directory, report)
    return report['exit_code']
