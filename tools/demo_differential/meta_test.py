"""Pilot red/green orchestration; this module never injects faults into artifacts.

The runner supplies ``invoke_run(fault_id, fresh_output_dir) -> RunOutcome``.
It must enable the existing native acceptance hook only for Fault.case, leave
the Web reference untouched, and return the actual ordinary-run exit code.
See RunOutcome for the deliberately small integration contract. Unit controls
of this module prove orchestration, not real GPU fault detection (task 5.2).
"""
from __future__ import annotations

from copy import deepcopy
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
from typing import Callable, Mapping, Sequence
import xml.etree.ElementTree as ET

from .observations import read_observations
from .protocol import ProtocolError, ROOT, FIXTURES, loads, output_path, read, relative_path, validate


@dataclass(frozen=True)
class Fault:
    identity: str
    case: str
    first_categories: tuple[str, ...]


BUTTON = 'antd665.button.basic-states.v1'
INPUT = 'antd665.input.controlled-edit.v1'
SELECT = 'antd665.select.popup-lifecycle.v1'
PILOT = (BUTTON, INPUT, SELECT)
FAULTS = (
    Fault('button-padding-plus-one', BUTTON, ('layout', 'token', 'visual')),
    Fault('input-reject-commit', INPUT, ('state', 'logic')),
    Fault('duplicate-callback', INPUT, ('logic',)),
)
DIFFERENCE_CATEGORIES = frozenset(('visual', 'layout', 'typography', 'token', 'state', 'input', 'logic', 'lifecycle'))


@dataclass(frozen=True)
class RunOutcome:
    """An ordinary run, not an expected-failure result.

    report is the formal report-v1 mapping, identical to output_dir/report.json.
    exit_code is the actual runner return code, checked separately from report.
    observations maps each pilot case to {'web': <relative JSONL path>,
    'native': <relative JSONL path>}; all paths are relative to output_dir and
    listed in that case's report.artifacts. Captures remain relative to JSONL.

    report.provenance requires fault ('none' or a finite FAULTS id), boolean
    reference_cache_published, and meta_binding with SHA256 strings for
    reference/cases/profile/source. 'cases' is definitions_digest(cases);
    other values come from locked reference/profile/source manifests. This
    supplements, and never replaces, the runner's normal provenance checks.
    """
    exit_code: int
    report: Mapping
    observations: Mapping[str, Mapping[str, str]]


@dataclass(frozen=True)
class MetaResult:
    exit_code: int
    report_path: Path


def reference_cache_publish_allowed(fault_id: str) -> bool:
    if fault_id not in ('none', *(fault.identity for fault in FAULTS)):
        raise ProtocolError('unknown acceptance fault')
    return fault_id == 'none'


def fault_for_case(fault_id: str, case_id: str) -> str:
    reference_cache_publish_allowed(fault_id)
    return next((fault.identity for fault in FAULTS if fault.identity == fault_id and fault.case == case_id), 'none')


def guard_reference_publish(fault_id: str) -> None:
    """Call BEFORE any cache publication, including a prepare/cache-miss path."""
    if not reference_cache_publish_allowed(fault_id):
        raise ProtocolError('faulted ordinary run cannot publish reference cache')


def guard_ordinary_run(report: Mapping, fault_id: str) -> None:
    """Call before finalizing any ordinary run, including outside meta-test.

    An undetected fault is a policy/protocol error, never a manufactured
    difference (exit 1). The caller must propagate this error as nonzero.
    """
    allowed = reference_cache_publish_allowed(fault_id)
    validate('report', report)
    provenance = report['provenance']
    if provenance.get('fault') != fault_id or type(provenance.get('reference_cache_published')) is not bool:
        raise ProtocolError('missing/mismatched fault or cache publication metadata')
    if not allowed and provenance['reference_cache_published']:
        raise ProtocolError('faulted ordinary run published reference cache')
    if not allowed and (report['status'] == 'pass' or report['exit_code'] == 0):
        raise ProtocolError('faulted ordinary run cannot pass; fault was not detected')


def definitions_digest(cases: Sequence[Mapping]) -> str:
    return hashlib.sha256(json.dumps(cases, sort_keys=True, ensure_ascii=True,
                                     separators=(',', ':'), allow_nan=False).encode('utf-8')).hexdigest()


def _no_links(path: Path) -> None:
    # Include Windows junction/reparse points, also on Python 3.10.
    for item in (path, *path.parents):
        if item.exists() or item.is_symlink():
            info = item.lstat()
            if item.is_symlink() or getattr(info, 'st_file_attributes', 0) & 0x400:
                raise ProtocolError(f'linked input/output path is not allowed: {item}')


def _snapshot(path: Path) -> dict[str, str]:
    _no_links(path)
    if not path.exists():
        raise ProtocolError(f'missing protected input or run directory: {path}')
    entries = [path]
    result = {}
    while entries:
        item = entries.pop()
        _no_links(item)
        name = '.' if item == path else item.relative_to(path).as_posix()
        if item.is_file():
            result[name] = hashlib.sha256(item.read_bytes()).hexdigest()
        elif item.is_dir():
            result[name + '/'] = 'directory'
            entries.extend(sorted(item.iterdir(), reverse=True))
        else:
            raise ProtocolError(f'non-regular input/artifact: {item}')
    return result


def _artifact(directory: Path, name: str, *, nonempty=True) -> Path:
    relative_path(name)
    candidate = directory / name
    _no_links(candidate)
    candidate = candidate.resolve()
    if (not candidate.is_relative_to(directory) or not candidate.is_file() or
            nonempty and not candidate.stat().st_size):
        raise ProtocolError(f'artifact missing, empty or outside run directory: {name}')
    return candidate


def _binding(report: Mapping, digest: str) -> dict:
    binding = report['provenance'].get('meta_binding')
    if not isinstance(binding, dict) or set(binding) != {'reference', 'cases', 'profile', 'source'}:
        raise ProtocolError('missing complete meta input binding')
    if any(not isinstance(value, str) or not re.fullmatch('[0-9a-f]{64}', value) for value in binding.values()):
        raise ProtocolError('meta input binding requires SHA256 identities')
    if binding['cases'] != digest:
        raise ProtocolError('case/fixture/input definitions changed')
    return binding


def _check_outcome(outcome: RunOutcome, directory: Path, cases: Sequence[Mapping], fault_id: str,
                   digest: str) -> dict:
    if not isinstance(outcome, RunOutcome) or type(outcome.exit_code) is not int:
        raise ProtocolError('invoke_run must return RunOutcome with an actual integer exit code')
    report = validate('report', read(_artifact(directory, 'report.json')))
    if report != outcome.report:
        raise ProtocolError('returned report differs from persisted formal report')
    guard_ordinary_run(report, fault_id)
    if outcome.exit_code != report['exit_code']:
        raise ProtocolError('ordinary run exit code differs from report')
    if report['exit_code'] not in (0, 1) or report['status'] not in ('pass', 'fail') or report['errors']:
        raise ProtocolError('environment, protocol, reference or incomplete run is not a detected difference')
    if any(report[field] != list(PILOT) for field in ('selected', 'expected', 'completed')):
        raise ProtocolError('pilot selected/expected/completed identities differ or are incomplete')
    if [row['identity'] for row in report['cases']] != list(PILOT):
        raise ProtocolError('pilot case results omitted, replaced, duplicated or reordered')
    if not isinstance(outcome.observations, Mapping) or set(outcome.observations) != set(PILOT):
        raise ProtocolError('missing pilot observation streams')
    passed = []
    artifact_owners = {}
    for case, result in zip(cases, report['cases']):
        if result['verdict'] not in ('pass', 'fail'):
            raise ProtocolError('not-run/error case cannot satisfy meta-test')
        if result['verdict'] == 'pass':
            passed.append(case['identity'])
            if result['failures'] or result['categories'] or result['first_step']:
                raise ProtocolError('pass case contains failures')
        else:
            failures = result['failures']
            steps = {action['step']: index for index, action in enumerate(case['actions'])}
            if not failures or result['first_step'] != failures[0].get('step'):
                raise ProtocolError('missing or inconsistent first failure step')
            categories = []
            for failure in failures:
                if failure.get('step') not in steps or failure.get('category') not in DIFFERENCE_CATEGORIES:
                    raise ProtocolError('failure has no executed step or has a non-difference category')
                if failure['category'] not in categories:
                    categories.append(failure['category'])
            if result['categories'] != categories:
                raise ProtocolError('case categories differ from ordered failure layers')
            if steps[failures[0]['step']] != min(steps[failure['step']] for failure in failures):
                raise ProtocolError('first failure hides an earlier executed-step failure')
        listed = set(result['artifacts'])
        for name in listed:
            path = _artifact(directory, name, nonempty=False)  # An ordinary stdout/stderr log may be empty.
            if path in artifact_owners:
                raise ProtocolError('artifact reused across cases')
            artifact_owners[path] = case['identity']
        streams = outcome.observations[case['identity']]
        if not isinstance(streams, Mapping) or set(streams) != {'web', 'native'}:
            raise ProtocolError('both Web/native streams are required')
        if streams['web'] == streams['native']:
            raise ProtocolError('Web/native cannot share an observation stream')
        side_captures = set()
        for side in ('web', 'native'):
            name = streams[side]
            if name not in listed:
                raise ProtocolError('observation stream is absent from report artifacts')
            stream = _artifact(directory, name)
            # Validate referenced captures before the existing protocol reader
            # opens them, including symlink/reparse rejection and enumeration.
            for line in stream.read_text(encoding='utf-8').splitlines():
                record = validate('observation', loads(line))
                for capture in record['captures']:
                    image = _artifact(stream.parent, capture['path'])
                    if image.relative_to(directory).as_posix() not in listed:
                        raise ProtocolError('capture is absent from report artifacts')
                    if image in side_captures:
                        raise ProtocolError('Web/native or checkpoints reused a capture artifact')
                    side_captures.add(image)
                wanted_fault = fault_for_case(fault_id, case['identity']) if side == 'native' else 'none'
                if record['provenance'].get('fault', 'none' if side == 'web' else None) != wanted_fault:
                    raise ProtocolError('native hook fingerprint or untouched Web reference differs')
            if side == 'native':
                read_observations(stream, case, report['nonce'])
            else:
                # The real Web v1 uses checkpoint-before-receipt and a run
                # nonce + paint sequence, unlike the native v1 stream.
                from .runner_evidence import web
                resolved_path = directory / 'cases' / case['identity'] / 'resolved-definition.json'
                if resolved_path.is_file():
                    name = resolved_path.relative_to(directory).as_posix()
                    if name not in listed:
                        raise ProtocolError('resolved definition is absent from report artifacts')
                    resolved = read(_artifact(directory, name))
                else:
                    resolved = case
                validate('case', resolved)
                expected = deepcopy(case)
                if expected['fixture']['viewport']['dpr'] == 'native':
                    ratio = resolved['fixture']['viewport']['dpr']
                    if type(ratio) not in (int, float) or ratio <= 0:
                        raise ProtocolError('missing actual resolved DPR definition')
                    expected['fixture']['viewport']['dpr'] = ratio
                    for item in expected['official']['adaptation']:
                        item['value'] = deepcopy(expected['fixture'][item['field']])
                if resolved != expected:
                    raise ProtocolError('resolved case changed inputs beyond the declared DPR')
                web(stream, resolved, report['nonce'])
    if report['passed'] != passed:
        raise ProtocolError('passed identity set differs from actual case verdicts')
    expected_status, expected_exit = ('pass', 0) if len(passed) == len(PILOT) else ('fail', 1)
    if (report['status'], report['exit_code']) != (expected_status, expected_exit):
        raise ProtocolError('ordinary run status/exit code does not match case verdicts')
    try:
        document = ET.parse(_artifact(directory, 'junit.xml')).getroot()
    except ET.ParseError as error:
        raise ProtocolError('truncated/invalid JUnit artifact') from error
    if document.tag not in ('testsuite', 'testsuites'):
        raise ProtocolError('invalid JUnit artifact root')
    testcases = list(document.iter('testcase'))
    if [item.get('name') for item in testcases] != list(PILOT):
        raise ProtocolError('JUnit omitted, replaced or reordered a pilot case')
    for item, result in zip(testcases, report['cases']):
        if item.find('error') is not None or item.find('skipped') is not None or (
                (item.find('failure') is not None) != (result['verdict'] == 'fail')):
            raise ProtocolError('JUnit verdict differs from formal case verdict')
    _artifact(directory, 'index.html')
    _binding(report, digest)
    return report


def run_meta_test(output_dir: Path, invoke_run: Callable[[str, Path], RunOutcome], *,
                  cases: Sequence[Mapping], protected_inputs: Sequence[Path]) -> MetaResult:
    """Run full pilot green, then one red/full green recovery for every fault.

    Reference preparation happens before this call. protected_inputs must name
    the locked reference, fixture/input, profile and relevant source manifests
    or trees; they are byte-checked around every child. This routine never
    builds, launches a GPU itself, updates reference inputs or rewrites a child
    report/image. It owns only a fresh checkout out/ directory. Invalid output
    ownership raises ProtocolError before invoking anything; owned runs always
    get a nonzero meta report on failure.
    """
    raw_output = Path(output_dir).absolute()
    _no_links(raw_output)
    directory = output_path(raw_output)
    directory.mkdir(parents=True, exist_ok=False)
    summary = {'schema_version': 1, 'status': 'fail', 'exit_code': 1, 'runs': [],
               'errors': [], 'scope': 'pilot-meta', 'platform_acceptance': 'not-inferred'}
    try:
        definitions = deepcopy(list(cases))
        for case in definitions:
            validate('case', case)
        if [case['identity'] for case in definitions] != list(PILOT):
            raise ProtocolError('meta-test requires the complete ordered pilot definitions')
        digest = definitions_digest(definitions)
        if not protected_inputs:
            raise ProtocolError('declare immutable reference/fixture/profile/input/source paths')
        protected = {}
        for path in protected_inputs:
            raw = Path(path).absolute()
            _no_links(raw)
            path = raw.resolve()
            if directory.is_relative_to(path) or path.is_relative_to(directory):
                raise ProtocolError('protected input overlaps meta output')
            protected[path] = _snapshot(path)
        frozen_runs = {}
        binding = None
        nonces = set()
        plan = [('positive', 'none', None)]
        for fault in FAULTS:
            plan.extend([('fault', fault.identity, fault), ('recovery', 'none', None)])
        for index, (phase, fault_id, fault) in enumerate(plan):
            child = directory / f'{index:02d}-{phase}-{fault_id}'
            if child.exists():
                raise ProtocolError('child output is stale')
            attempt = {'phase': phase, 'fault': fault_id, 'output': child.name, 'verified': False}
            summary['runs'].append(attempt)
            try:
                outcome = invoke_run(fault_id, child)
            except Exception as error:
                raise ProtocolError(f'invoke_run failed: {type(error).__name__}: {error}') from error
            _no_links(child)
            for path, before in {**protected, **frozen_runs}.items():
                if _snapshot(path) != before:
                    raise ProtocolError(f'protected input or earlier run artifacts changed: {path}')
            report = _check_outcome(outcome, child, definitions, fault_id, digest)
            attempt.update(exit_code=outcome.exit_code, status=report['status'], nonce=report['nonce'])
            if report['nonce'] in nonces:
                raise ProtocolError('child reused an earlier run nonce')
            nonces.add(report['nonce'])
            current_binding = _binding(report, digest)
            if binding is None:
                binding = deepcopy(current_binding)
                summary['meta_binding'] = binding
            elif current_binding != binding:
                raise ProtocolError('reference/case/fixture/profile/source identity changed within meta-test')
            if fault is None:
                if report['status'] != 'pass' or outcome.exit_code != 0:
                    raise ProtocolError('unfaulted positive/recovery run is still red')
            else:
                failed = [row for row in report['cases'] if row['verdict'] == 'fail']
                if outcome.exit_code != 1 or [row['identity'] for row in failed] != [fault.case]:
                    raise ProtocolError('fault must cause a complete difference in exactly its pilot case')
                first = failed[0]['failures'][0]
                attempt['first_failure'] = deepcopy(first)
                if first['category'] not in fault.first_categories:
                    raise ProtocolError('fault was red at the wrong first failure layer')
            frozen_runs[child] = _snapshot(child)
            attempt.update(verified=True, artifacts=frozen_runs[child])
        summary.update(status='pass', exit_code=0)
    except (ProtocolError, OSError, ValueError, TypeError, KeyError) as error:
        summary['errors'].append(f'{type(error).__name__}: {error}')
    report_path = directory / 'meta-report.json'
    temporary = directory / 'meta-report.json.tmp'
    temporary.write_text(json.dumps(summary, indent=2, ensure_ascii=True, allow_nan=False) + '\n', encoding='utf-8')
    temporary.replace(report_path)
    return MetaResult(summary['exit_code'], report_path)


def run_pilot_meta(options: Mapping, definitions: Sequence[Mapping]) -> MetaResult:
    """Production CLI plumbing using the frozen ordinary runner and real hooks."""
    from .adapters import RealAdapters
    from .runner import run, select
    from .reporting import write_json
    from .source_closure import source_manifest
    from demo_reference_web import bootstrap

    raw_output = Path(options['output']).absolute()
    _no_links(raw_output)
    directory = output_path(raw_output)
    directory.mkdir(parents=True, exist_ok=False)
    try:
        from .calibration import preflight_profile
        options=preflight_profile(options)
        cases = select(definitions, options)
        if [case['identity'] for case in cases] != list(PILOT) or options.get('reference', 'locked') != 'locked':
            raise ProtocolError('meta-test requires the whole pilot and the same locked reference')
        preparation = directory / 'preparation'
        preparation.mkdir()
        adapter = RealAdapters()
        try:
            prepared = adapter.prepare(preparation, dict(options))
        finally:
            adapter.cleanup()
        prepared_info = bootstrap.verify_prepared(prepared['prepared'])
        fonts = preparation / 'font-files.json'
        write_json(fonts, prepared['fonts'])
        cache_root = ROOT / 'out/demo-differential/reference-cache'
        cache_root.mkdir(parents=True, exist_ok=True)
        # All children execute Web actions. Even the positive controls avoid
        # publication, so cache misses and hits leave this tree byte-identical.
        protected = [Path(options['profile_file']), FIXTURES, Path(prepared_info['source']),
                     Path(prepared['prepared']) / 'prepared.json', fonts, cache_root,
                     ROOT / 'tools/demo_reference_web', ROOT / 'tools/demo_differential',
                     ROOT / 'tools/run_demo_differential.py']
        protected.extend(ROOT / row['path'] for row in source_manifest(ROOT)['files'])
        protected.extend(Path(font['path']) for font in prepared['fonts'])
        calibration_profile = read(options['profile_file'])
        if calibration_profile.get('evidence'):
            from .calibration import evidence_manifest
            calibration_manifest, _, calibration_root = evidence_manifest(calibration_profile)
            protected.append(calibration_root)
            protected.extend(ROOT / item['path'] for item in calibration_manifest['tools']['files'])
        def invoke_run(fault_id, child):
            child_options = dict(options, output=child, fault=fault_id, prepared=prepared['prepared'],
                                 font_files=str(fonts), reference='locked', reference_publish=False, command='run')
            code = run(child_options, cases)
            report = read(child / 'report.json')
            streams = {case['identity']: {side: f"cases/{case['identity']}/{side}/observations.jsonl"
                                          for side in ('web', 'native')} for case in cases}
            return RunOutcome(code, report, streams)
        return run_meta_test(directory / 'runs', invoke_run, cases=cases, protected_inputs=protected)
    except (ProtocolError, OSError, ValueError, KeyError, TypeError) as error:
        path = directory / 'meta-report.json'
        write_json(path, {'schema_version': 1, 'status': 'fail', 'exit_code': 1, 'runs': [],
                          'errors': [f'{type(error).__name__}: {error}'], 'platform_acceptance': 'not-inferred'})
        return MetaResult(1, path)
