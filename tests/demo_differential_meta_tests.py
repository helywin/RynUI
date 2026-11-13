"""Synthetic runner controls for meta orchestration, NOT task 5.2 GPU evidence."""
from copy import deepcopy
from dataclasses import replace
import json
import io
from contextlib import redirect_stdout, redirect_stderr
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_differential.meta_test import (
    BUTTON, INPUT, SELECT, FAULTS, PILOT, RunOutcome, definitions_digest,
    MetaResult, fault_for_case, guard_ordinary_run, guard_reference_publish,
    reference_cache_publish_allowed, run_meta_test,
)
from demo_differential.protocol import ProtocolError, read
from demo_differential_observation_tests import records_for
from demo_differential_protocol_tests import valid_case


def pilot_controls():
    """Schema-valid mechanism inputs; these are not the real pilot definitions."""
    cases = []
    for identity, target in zip(PILOT, ('gallery.state.default', 'gallery.input.controlled', 'gallery.select.controlled-search')):
        case = valid_case()
        case['identity'] = identity
        case['fixture']['viewport']['dpr'] = 1
        case['native']['targets'] = [target]
        case['checkpoints'][0]['targets'] = [target]
        case['actions'].append({**deepcopy(case['actions'][0]), 'step': 'after', 'time_us': 1})
        case['checkpoints'].append({**deepcopy(case['checkpoints'][0]), 'step': 'after'})
        cases.append(case)
    return cases


class RunnerControl:
    """Write bounded protocol/image controls and formal ordinary-run reports."""
    def __init__(self, cases):
        self.cases = cases
        self.calls = []
        self.outcomes = []

    def __call__(self, fault_id, directory):
        self.calls.append((fault_id, directory))
        directory.mkdir()
        observations = {}
        results = []
        nonce = f'unit-run-{len(self.calls)}'
        fault = next((fault for fault in FAULTS if fault.identity == fault_id), None)
        for index, case in enumerate(self.cases):
            streams = {}
            artifacts = []
            for side in ('web', 'native'):
                folder = directory / 'cases' / case['identity'] / side
                folder.mkdir(parents=True)
                records = records_for(case, folder, nonce)
                actual_fault = fault_id if side == 'native' and fault is not None and fault.case == case['identity'] else 'none'
                for record in records:
                    record['provenance']['fault'] = actual_fault
                if side == 'web':
                    from demo_differential import comparison
                    from demo_differential.adapters import web_definition
                    from demo_reference_web import bootstrap, reference
                    for record in records:
                        record['adapter'] = case['official']['adapter_id']
                        record['provenance'].update(source_commit=case['official']['commit'],
                            source_tree_sha256=bootstrap.load_lock()['source']['components_tree_sha256'],
                            adapter_sha256=case['official']['adapter_sha256'], adapter_closure_sha256=reference.code_hash(),
                            fixture_sha256=comparison.digest(case['fixture']), case_sha256=comparison.digest(web_definition(case)),
                            source_closure={item['path']: item['sha256'] for item in case['official']['source_paths']})
                        if record['type'] == 'checkpoint':
                            capture = record['captures'][0]
                            capture['nonce'] = nonce
                            record['resources']['paint_receipt'] = {'nonce': nonce, 'step': record['step'],
                                'sequence': records.index(record) // 2, 'sha256': capture['sha256'], 'fresh_readback': True}
                    for i in range(1, len(records) - 1, 2):
                        records[i:i + 2] = reversed(records[i:i + 2])
                stream = folder / 'observations.jsonl'
                stream.write_text(''.join(json.dumps(record) + '\n' for record in records), encoding='utf-8')
                (folder / 'stdout.log').write_bytes(b'')
                streams[side] = stream.relative_to(directory).as_posix()
                artifacts.extend(path.relative_to(directory).as_posix() for path in sorted(folder.iterdir()))
            observations[case['identity']] = streams
            failed = fault is not None and case['identity'] == fault.case
            failures = [{'category': fault.first_categories[0], 'step': 'after', 'message': 'unit difference control'}] if failed else []
            results.append({'identity': case['identity'], 'verdict': 'fail' if failed else 'pass',
                            'categories': [failures[0]['category']] if failed else [], 'first_step': 'after' if failed else '',
                            'failures': failures, 'artifacts': artifacts})
        report = {'schema_version': 1, 'nonce': nonce, 'status': 'fail' if fault else 'pass',
                  'exit_code': 1 if fault else 0, 'selected': list(PILOT), 'expected': list(PILOT),
                  'completed': list(PILOT), 'passed': [row['identity'] for row in results if row['verdict'] == 'pass'],
                  'cases': results, 'errors': [], 'coverage': {}, 'timings': {},
                  'provenance': {'fault': fault_id, 'reference_cache_published': False,
                                 'meta_binding': {'reference': 'a' * 64, 'cases': definitions_digest(self.cases),
                                                  'profile': 'b' * 64, 'source': 'c' * 64}},
                  'repro_argv': ['unit-control'], 'replay_argv': ['unit-control'], 'desktop_fallback': False}
        (directory / 'report.json').write_text(json.dumps(report) + '\n', encoding='utf-8')
        self.write_junit(report, directory)
        (directory / 'index.html').write_text('<!doctype html><title>Unit control</title>\n', encoding='utf-8')
        outcome = RunOutcome(report['exit_code'], report, observations)
        self.outcomes.append(outcome)
        return outcome

    @staticmethod
    def write_junit(report, directory):
        suite = ET.Element('testsuite', tests='3')
        for result in report['cases']:
            test = ET.SubElement(suite, 'testcase', name=result['identity'])
            if result['verdict'] == 'fail':
                ET.SubElement(test, 'failure', message='unit difference control')
        ET.ElementTree(suite).write(directory / 'junit.xml', encoding='utf-8', xml_declaration=True)


class MetaTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(dir=ROOT / 'out', prefix='meta-unit-')
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.cases = pilot_controls()
        self.runner = RunnerControl(self.cases)
        self.protected = []
        for name in ('reference', 'fixture-input', 'profile', 'source'):
            path = self.directory / name
            path.mkdir()
            (path / 'locked.txt').write_text(name, encoding='utf-8')
            self.protected.append(path)
        self.count = 0

    def run_meta(self, invoke=None, **options):
        self.count += 1
        return run_meta_test(self.directory / f'meta-{self.count}', invoke or self.runner,
                             cases=options.pop('cases', self.cases),
                             protected_inputs=options.pop('protected_inputs', self.protected), **options)

    def altered(self, mutation, when=lambda fault, count: fault != 'none'):
        def invoke(fault, directory):
            outcome = self.runner(fault, directory)
            if when(fault, len(self.runner.calls)):
                mutation(outcome.report, directory, outcome)
                (directory / 'report.json').write_text(json.dumps(outcome.report) + '\n', encoding='utf-8')
            return outcome
        return invoke

    def assert_rejected(self, invoke, *, message=None):
        result = self.run_meta(invoke)
        report = read(result.report_path)
        self.assertNotEqual(result.exit_code, 0)
        self.assertEqual(report['status'], 'fail')
        self.assertTrue(report['errors'])
        if message:
            self.assertIn(message, report['errors'][0])
        return report

    @staticmethod
    def green(report):
        report.update(status='pass', exit_code=0, passed=list(PILOT))
        for result in report['cases']:
            result.update(verdict='pass', categories=[], failures=[], first_step='')

    def test_positive_red_green_for_every_finite_real_hook_id(self):
        result = self.run_meta()
        self.assertEqual(result.exit_code, 0)
        report = read(result.report_path)
        self.assertEqual([fault for fault, _ in self.runner.calls],
                         ['none', 'button-padding-plus-one', 'none', 'input-reject-commit', 'none', 'duplicate-callback', 'none'])
        self.assertEqual(len({path for _, path in self.runner.calls}), 7)
        self.assertTrue(all(row['verified'] and row['artifacts'] for row in report['runs']))
        self.assertEqual([row['exit_code'] for row in report['runs']], [0, 1, 0, 1, 0, 1, 0])
        self.assertEqual(report['platform_acceptance'], 'not-inferred')
        self.assertEqual(FAULTS[2].case, INPUT)
        self.assertEqual(FAULTS[2].first_categories, ('logic',))

    def test_fault_still_green_is_not_expected_failure(self):
        def invoke(fault, directory):
            outcome = self.runner(fault, directory)
            if fault != 'none':
                self.green(outcome.report)
                (directory / 'report.json').write_text(json.dumps(outcome.report), encoding='utf-8')
                return replace(outcome, exit_code=0)
            return outcome
        self.assert_rejected(invoke, message='cannot pass')

    def test_wrong_first_layer_even_with_a_later_correct_layer_is_red(self):
        def mutate(report, directory, outcome):
            row = next(row for row in report['cases'] if row['verdict'] == 'fail')
            row['failures'].insert(0, {'category': 'logic', 'step': 'after'})
            row['categories'].insert(0, 'logic')
        self.assert_rejected(self.altered(mutate), message='wrong first failure layer')

    def test_late_correct_layer_cannot_hide_an_earlier_wrong_step(self):
        def mutate(report, directory, outcome):
            row = report['cases'][0]
            row['failures'].append({'category': 'logic', 'step': 'initial'})
            row['categories'].append('logic')
        self.assert_rejected(self.altered(mutate), message='earlier executed-step')

    def test_red_in_an_unrelated_case_does_not_prove_detection(self):
        def mutate(report, directory, outcome):
            failed = report['cases'][0]
            unrelated = report['cases'][2]
            unrelated.update(verdict='fail', failures=deepcopy(failed['failures']),
                             categories=failed['categories'][:], first_step=failed['first_step'])
            failed.update(verdict='pass', failures=[], categories=[], first_step='')
            report['passed'] = [BUTTON, INPUT]
            self.runner.write_junit(report, directory)
        self.assert_rejected(self.altered(mutate), message='exactly its pilot case')

    def test_environment_nonzero_is_not_detection(self):
        for code in (2, 3, 4, -9):
            with self.subTest(code=code):
                runner = RunnerControl(self.cases)
                def invoke(fault, directory):
                    outcome = runner(fault, directory)
                    if fault == 'none':
                        return outcome
                    outcome.report.update(status='error', exit_code=max(code, 0),
                                          errors=[{'category': 'environment', 'message': 'unit startup failure'}])
                    (directory / 'report.json').write_text(json.dumps(outcome.report), encoding='utf-8')
                    return replace(outcome, exit_code=code)
                self.assert_rejected(invoke)

    def test_exit_code_must_be_real_difference_one_and_match_report(self):
        def invoke(fault, directory):
            outcome = self.runner(fault, directory)
            return replace(outcome, exit_code=3) if fault != 'none' else outcome
        self.assert_rejected(invoke, message='exit code differs')

    def test_missing_case_checkpoint_action_complete_and_truncated_protocol(self):
        for damage in ('case', 'checkpoint', 'action', 'complete', 'truncated', 'no-dispatch', 'target'):
            with self.subTest(damage=damage):
                def mutate(report, directory, outcome):
                    if damage == 'case':
                        report['completed'].pop()
                        return
                    stream = directory / outcome.observations[BUTTON]['native']
                    records = [json.loads(line) for line in stream.read_text().splitlines()]
                    if damage in ('checkpoint', 'action', 'complete'):
                        kind = {'checkpoint': 'checkpoint', 'action': 'receipt', 'complete': 'complete'}[damage]
                        records.pop(next(index for index, row in enumerate(records) if row['type'] == kind))
                    elif damage == 'no-dispatch':
                        records[1]['receipt']['dispatched'] = False
                    elif damage == 'target':
                        next(row for row in records if row['type'] == 'checkpoint')['targets'] = []
                    text = ''.join(json.dumps(row) + '\n' for row in records)
                    stream.write_text(text[:-1] if damage == 'truncated' else text, encoding='utf-8')
                self.assert_rejected(self.altered(mutate))

    def test_error_not_run_missing_first_failure_and_inconsistent_results(self):
        for damage in ('not-run', 'error', 'no-failure', 'first-step', 'categories', 'passed', 'unrelated-case', 'status'):
            with self.subTest(damage=damage):
                def mutate(report, directory, outcome):
                    row = report['cases'][0]
                    if damage in ('not-run', 'error'):
                        row['verdict'] = damage
                    elif damage == 'no-failure':
                        row['failures'] = []
                    elif damage == 'first-step':
                        row['first_step'] = 'never-executed'
                    elif damage == 'categories':
                        row['categories'] = ['logic']
                    elif damage == 'passed':
                        report['passed'] = list(PILOT)
                    elif damage == 'unrelated-case':
                        row['identity'] = INPUT
                    else:
                        report['status'] = 'error'
                self.assert_rejected(self.altered(mutate))

    def test_each_binding_identity_is_immutable(self):
        for identity in ('reference', 'cases', 'profile', 'source'):
            with self.subTest(identity=identity):
                def mutate(report, directory, outcome):
                    report['provenance']['meta_binding'][identity] = 'd' * 64
                self.assert_rejected(self.altered(mutate), message='changed')

    def test_reference_fixture_profile_input_and_source_bytes_cannot_be_rewritten(self):
        for path in self.protected:
            with self.subTest(path=path.name):
                def mutate(report, directory, outcome):
                    (path / 'locked.txt').write_text('fault or native output', encoding='utf-8')
                self.assert_rejected(self.altered(mutate), message='changed')
                (path / 'locked.txt').write_text(path.name, encoding='utf-8')

    def test_new_file_in_protected_reference_is_a_change(self):
        def mutate(report, directory, outcome):
            (self.protected[0] / 'new-cache-publish').write_text('forbidden', encoding='utf-8')
        self.assert_rejected(self.altered(mutate), message='changed')

    def test_faulted_cache_publish_and_ordinary_green_are_rejected(self):
        self.assertTrue(reference_cache_publish_allowed('none'))
        guard_reference_publish('none')
        for fault in FAULTS:
            self.assertFalse(reference_cache_publish_allowed(fault.identity))
            with self.assertRaises(ProtocolError):
                guard_reference_publish(fault.identity)
            folder = self.directory / f'ordinary-{fault.identity}'
            outcome = self.runner(fault.identity, folder)
            guard_ordinary_run(outcome.report, fault.identity)
            self.green(outcome.report)
            with self.assertRaisesRegex(ProtocolError, 'cannot pass'):
                guard_ordinary_run(outcome.report, fault.identity)
        def mutate(report, directory, outcome):
            report['provenance']['reference_cache_published'] = True
        self.assert_rejected(self.altered(mutate), message='published reference cache')
        with self.assertRaises(ProtocolError):
            reference_cache_publish_allowed('invented-fault')

    def test_native_fault_metadata_and_untouched_web_reference(self):
        for side in ('web', 'native'):
            with self.subTest(side=side):
                def mutate(report, directory, outcome):
                    stream = directory / outcome.observations[BUTTON][side]
                    records = [json.loads(line) for line in stream.read_text().splitlines()]
                    for row in records:
                        row['provenance']['fault'] = 'none' if side == 'native' else report['provenance']['fault']
                    stream.write_text(''.join(json.dumps(row) + '\n' for row in records), encoding='utf-8')
                self.assert_rejected(self.altered(mutate), message='hook fingerprint')

    def test_recovery_still_red_is_meta_failure(self):
        def mutate(report, directory, outcome):
            report.update(status='fail', exit_code=1, passed=[INPUT, SELECT])
            report['cases'][0].update(verdict='fail', categories=['token'], first_step='after',
                                      failures=[{'category': 'token', 'step': 'after'}])
            self.runner.write_junit(report, directory)
        invoke = self.altered(mutate, when=lambda fault, count: count == 3)
        def real_exit(fault, directory):
            outcome = invoke(fault, directory)
            return replace(outcome, exit_code=outcome.report['exit_code'])
        self.assert_rejected(real_exit, message='still red')

    def test_report_schema_truncation_and_persisted_mapping_consistency(self):
        for damage in ('truncated', 'mapping-mismatch', 'unknown-field', 'missing-binding'):
            with self.subTest(damage=damage):
                def invoke(fault, directory):
                    outcome = self.runner(fault, directory)
                    if fault != 'none':
                        if damage == 'truncated':
                            (directory / 'report.json').write_text('{"schema_version":', encoding='utf-8')
                        elif damage == 'mapping-mismatch':
                            outcome.report['nonce'] = 'not-on-disk'
                        else:
                            if damage == 'unknown-field':
                                outcome.report['expected_failure'] = True
                            else:
                                del outcome.report['provenance']['meta_binding']
                            (directory / 'report.json').write_text(json.dumps(outcome.report), encoding='utf-8')
                    return outcome
                self.assert_rejected(invoke)

    def test_missing_empty_corrupted_or_unlisted_artifacts_fail(self):
        for damage in ('missing-image', 'empty-image', 'hash', 'unlisted', 'missing-index', 'junit',
                       'empty-junit', 'wrong-junit-verdict', 'stream-path'):
            with self.subTest(damage=damage):
                def mutate(report, directory, outcome):
                    image_name = next(name for name in report['cases'][0]['artifacts'] if name.endswith('.bmp'))
                    if damage == 'missing-image':
                        (directory / image_name).unlink()
                    elif damage in ('empty-image', 'hash'):
                        (directory / image_name).write_bytes(b'' if damage == 'empty-image' else b'corrupt')
                    elif damage == 'unlisted':
                        report['cases'][0]['artifacts'].remove(image_name)
                    elif damage == 'missing-index':
                        (directory / 'index.html').unlink()
                    elif damage == 'junit':
                        (directory / 'junit.xml').write_text('<testsuites>', encoding='utf-8')
                    elif damage == 'empty-junit':
                        (directory / 'junit.xml').write_text('<testsuite/>', encoding='utf-8')
                    elif damage == 'wrong-junit-verdict':
                        document = ET.parse(directory / 'junit.xml')
                        row = document.getroot().find('testcase')
                        row.remove(row.find('failure'))
                        document.write(directory / 'junit.xml', encoding='utf-8')
                    else:
                        outcome.observations[BUTTON]['native'] = '../outside.jsonl'
                self.assert_rejected(self.altered(mutate))

    def test_artifact_path_escape_absolute_and_backslash_are_rejected(self):
        for name in ('../outside.txt', '/absolute.txt', 'C:/absolute.txt', 'case/../../outside.txt', 'case\\image.bmp'):
            with self.subTest(name=name):
                def mutate(report, directory, outcome):
                    report['cases'][0]['artifacts'].append(name)
                self.assert_rejected(self.altered(mutate))

    def test_output_freshness_and_out_ownership_before_invocation(self):
        old = self.directory / 'existing'
        old.mkdir()
        for path in (old, ROOT / 'tools/not-an-output', ROOT / 'out/../not-an-output'):
            with self.subTest(path=path), self.assertRaises((ProtocolError, FileExistsError)):
                run_meta_test(path, self.runner, cases=self.cases, protected_inputs=self.protected)
        self.assertEqual(self.runner.calls, [])

    def test_later_run_cannot_overwrite_prior_artifacts_or_reuse_nonce(self):
        for damage in ('old-artifact', 'nonce'):
            with self.subTest(damage=damage):
                def mutate(report, directory, outcome):
                    if damage == 'old-artifact':
                        old_path = self.runner.calls[-2][1] / 'index.html'
                        old_path.write_text('overwritten', encoding='utf-8')
                    else:
                        report['nonce'] = self.runner.outcomes[-2].report['nonce']
                        for streams in outcome.observations.values():
                            for name in streams.values():
                                stream = directory / name
                                records = [json.loads(line) for line in stream.read_text().splitlines()]
                                for row in records:
                                    row['nonce'] = report['nonce']
                                stream.write_text(''.join(json.dumps(row) + '\n' for row in records), encoding='utf-8')
                self.assert_rejected(self.altered(mutate))

    def test_future_child_directory_cannot_be_prepopulated(self):
        def invoke(fault, directory):
            outcome = self.runner(fault, directory)
            if len(self.runner.calls) == 1:
                (directory.parent / '01-fault-button-padding-plus-one').mkdir()
            return outcome
        self.assert_rejected(invoke, message='child output is stale')

    def test_bad_adapter_outcome_exception_or_missing_inputs_is_nonzero(self):
        self.assert_rejected(lambda fault, directory: {})
        def crash(fault, directory):
            raise RuntimeError('unit crash')
        self.assert_rejected(crash, message='invoke_run failed')
        self.assertNotEqual(self.run_meta(protected_inputs=[]).exit_code, 0)
        self.assertNotEqual(self.run_meta(cases=self.cases[:2]).exit_code, 0)

    def test_symlink_escape_is_rejected_when_platform_allows_symlinks(self):
        link = self.directory / 'linked-output'
        try:
            link.symlink_to(self.protected[0], target_is_directory=True)
        except OSError as error:
            self.skipTest(f'Windows symlink privilege unavailable: {error}')
        with self.assertRaisesRegex(ProtocolError, 'linked'):
            run_meta_test(link / 'child', self.runner, cases=self.cases, protected_inputs=self.protected)
        self.assertEqual(self.runner.calls, [])

    def test_cli_dispatches_real_meta_and_acceptance_only_fault_option(self):
        import run_demo_differential as cli
        arguments = ['tools/run_demo_differential.py', 'meta-test', '--suite', 'pilot',
                     '--binary', 'unit-binary', '--preset', 'windows-msvc-release',
                     '--profile-file', 'unit-profile', '--output', str(self.directory / 'cli')]
        with patch.object(sys, 'argv', arguments), patch.object(cli, 'run_pilot_meta',
                return_value=MetaResult(1, self.directory / 'unit-meta.json')) as meta, redirect_stdout(io.StringIO()):
            self.assertEqual(cli.main(), 1)
            self.assertEqual(meta.call_args.args[0]['suite'], 'pilot')
        arguments[1] = 'run'
        arguments += ['--fault', 'duplicate-callback']
        with patch.object(sys, 'argv', arguments), patch.object(cli, 'run', return_value=1) as ordinary:
            self.assertEqual(cli.main(), 1)
            self.assertEqual(ordinary.call_args.args[0]['fault'], 'duplicate-callback')
        arguments[-1] = 'invented-hook'
        with patch.object(sys, 'argv', arguments), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            cli.main()

    def test_real_adapters_map_fault_to_only_native_target_case_and_disable_web_publish(self):
        from demo_differential.adapters import RealAdapters
        from demo_differential import adapters
        prepared = {'fonts': [], 'prepared': str(self.directory / 'unit-prepared')}
        adapter = RealAdapters()
        options = {'binary': 'unit-binary', 'timeout': 1, 'reference': 'locked', 'fault': 'duplicate-callback'}
        for case in self.cases:
            destination = self.directory / case['identity']
            with patch.object(adapters, 'process'):
                adapter.native(case, 'unit', destination, prepared, options)
            self.assertEqual(read(destination / 'request.json')['fault'],
                             'duplicate-callback' if case['identity'] == INPUT else 'none')
        destination = self.directory / 'web'
        destination.mkdir()
        (destination / 'result.json').write_text(json.dumps({'cache_published': False}), encoding='utf-8')
        with patch.object(adapters, 'process') as process:
            adapter.web(self.cases[1], 'unit', destination, prepared, options)
            self.assertIn('--no-cache-publish', process.call_args.args[0])
        request = read(destination.parent / 'web-request.json')
        self.assertNotIn('fault', request)
        self.assertEqual(request['definition']['fixture'], self.cases[1]['fixture'])
        for fault in FAULTS:
            self.assertEqual(fault_for_case(fault.identity, SELECT), 'none')
        (destination / 'result.json').write_text(json.dumps({'cache_published': True}), encoding='utf-8')
        with patch.object(adapters, 'process'), self.assertRaisesRegex(ProtocolError, 'publication violates'):
            adapter.web(self.cases[1], 'unit', destination, prepared, options)

    def test_production_meta_wrapper_prepares_once_and_calls_ordinary_runner_seven_times(self):
        from demo_differential import meta_test, runner, source_closure
        from demo_differential.adapters import RealAdapters
        from demo_reference_web import bootstrap
        prepared = self.directory / 'prepared'
        prepared.mkdir()
        (prepared / 'prepared.json').write_text('{}', encoding='utf-8')
        font = self.directory / 'font'
        font.write_bytes(b'unit font bytes')
        for name in ('tools/demo_reference_web', 'tools/demo_differential'):
            (self.directory / name).mkdir(parents=True)
        (self.directory / 'tools/run_demo_differential.py').write_text('unit source', encoding='utf-8')
        profile = self.directory / 'unit-profile'
        profile.write_text('unit profile', encoding='utf-8')
        calls = []
        def ordinary(options, cases):
            calls.append(options)
            self.assertIs(options['reference_publish'], False)
            self.assertEqual(options['reference'], 'locked')
            self.assertEqual(options['prepared'], str(prepared))
            return self.runner(options['fault'], options['output']).exit_code
        options = {'output': self.directory / 'production-meta', 'profile_file': profile, 'reference': 'locked'}
        with patch.object(meta_test, 'ROOT', self.directory), patch.object(meta_test, 'FIXTURES', self.protected[1]), \
                patch.object(source_closure, 'source_manifest', return_value={'files': []}), \
                patch.object(bootstrap, 'verify_prepared', return_value={'source': str(self.protected[0])}), \
                patch.object(RealAdapters, 'prepare', return_value={'prepared': str(prepared), 'fonts': [{'path': str(font)}]}) as prepare, \
                patch.object(RealAdapters, 'cleanup'), patch.object(runner, 'run', side_effect=ordinary):
            result = meta_test.run_pilot_meta(options, self.cases)
        self.assertEqual(result.exit_code, 0, read(result.report_path))
        self.assertEqual(prepare.call_count, 1)
        self.assertEqual(len(calls), 7)


class OrdinaryFaultPlumbingTests(unittest.TestCase):
    """Execute the ordinary state machine/oracle with explicit protocol substitutes."""
    @classmethod
    def setUpClass(cls):
        import demo_differential_runner_tests as controls
        controls.RunnerTests.setUpClass()

    def setUp(self):
        import demo_differential_runner_tests as controls
        # Reuse Runner18's bounded binary/profile/inventory control setup.
        self.fixture = controls.RunnerTests('test_complete_mechanism_zero_and_strict_artifact_reports')
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)

    def execute(self, fault_id, *, detect=True):
        import demo_differential_runner_tests as controls
        from demo_differential import runner, pilots
        from demo_differential.inventory import obligations
        case = controls.control_case()
        fault_case = next(f.case for f in FAULTS if f.identity == fault_id)
        if fault_case == INPUT:
            original = pilots.input_case()
            case.update(identity=INPUT, native=original['native'])
            case['fixture']['props'] = deepcopy(original['fixture']['props'])
            case['fixture']['props'].pop('composition')
            case['fixture']['data'] = deepcopy(original['fixture']['data'])
            case['checkpoints'] = [{**deepcopy(original['checkpoints'][0]), 'step': 'capture'}]
            case['actions'][0]['target'] = case['native']['targets'][0]
            case['actions'][1]['target'] = case['native']['sample_id']
            case['oracle'][0]['target'] = case['native']['targets'][0]
            case['oracle'][0]['event'] = 'change'
        class FaultRecords(controls.AdapterControl):
            def native(self, definition, nonce, directory, prepared, options):
                path = super().native(definition, nonce, directory, prepared, options)
                rows = [json.loads(line) for line in path.read_text().splitlines()]
                active = fault_for_case(options.get('fault', 'none'), definition['identity'])
                # These generated records test protocol plumbing only. They
                # never claim the native C++ hook or a GPU was executed.
                for row in rows:
                    row['provenance']['fault'] = active
                    if detect and active == 'button-padding-plus-one':
                        for target in row['targets']:
                            target['tokens']['padding_inline'] = 1
                    if detect and active in ('duplicate-callback', 'input-reject-commit') and 'send' in row['completed_steps']:
                        row['callbacks'] = [{'seq': i, 'step': 'send', 'time_us': 0,
                            'target': definition['native']['targets'][0], 'key': '', 'event': 'change',
                            'payload': {'proposed': 'unit', 'committed': 'unit', 'accepted': True},
                            'owner_generation': 1, 'layer': 'native-core-dispatch'} for i in range(2)]
                path.write_text(''.join(json.dumps(row) + '\n' for row in rows), encoding='utf-8')
                return path
        compared = []
        def compare(definition, native, web, *args):
            compared.append(True)
            wrong = any(target['tokens'].get('padding_inline', 0) != 0
                        for row in native['checkpoints'].values() for target in row['targets'])
            return ([{'category': 'token', 'step': 'capture', 'message': 'unit observed padding differs'}] if wrong else [], {})
        options = dict(self.fixture.options, fault=fault_id)
        ledger = {'schema_version': 1, 'baseline_commit': '94ad9bf0cd0a383265d37e253d83625ea611e692',
            'rows': [{**row, 'mapping': 'paired', 'implementation': 'ready', 'official_demo_ids': [],
                      'cases': [case['identity']], 'reason': 'unit protocol control'}
                     for row in obligations(controls.CONTROL_INVENTORY) if row['sample_id'] == case['native']['sample_id']]}
        code = runner.run(options, [case], adapters=FaultRecords(), comparator=compare, coverage_document=ledger)
        return code, read(options['output'] / 'report.json'), compared

    def test_actual_oracle_or_observed_token_difference_is_exit_one(self):
        for fault in FAULTS:
            with self.subTest(fault=fault.identity):
                self.fixture.options['output'] = self.fixture.directory / fault.identity
                code, report, compared = self.execute(fault.identity)
                self.assertEqual(code, 1, report)
                self.assertTrue(compared)
                self.assertEqual(report['cases'][0]['verdict'], 'fail')
                self.assertIn(report['cases'][0]['failures'][0]['category'], fault.first_categories)
                self.assertFalse(report['errors'])

    def test_only_fault_marker_is_policy_error_two_never_difference_one(self):
        code, report, compared = self.execute('button-padding-plus-one', detect=False)
        self.assertEqual(code, 2, report)
        self.assertTrue(compared)
        self.assertEqual(report['cases'][0]['failures'], [])
        self.assertEqual(report['status'], 'error')
        self.assertIn('not detected', report['errors'][0]['message'])


class WebCachePolicyTests(unittest.TestCase):
    def test_cache_miss_without_publication_still_executes_and_cache_hit_still_validates(self):
        from demo_reference_web import reference
        from demo_differential_observation_tests import bmp
        from types import SimpleNamespace
        with tempfile.TemporaryDirectory(dir=ROOT / 'out', prefix='meta-cache-unit-') as temporary:
            base = Path(temporary)
            case = pilot_controls()[0]
            lock = reference.load_lock()
            for name in (lock['font_lock'], 'tests/fixtures/demo-differential/case.schema.json',
                         'tests/fixtures/demo-differential/observation.schema.json', 'tools/demo_differential/protocol.py'):
                path = base / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('unit input', encoding='utf-8')
            prepared = {'browser_sha256': 'a' * 64, 'node_sha256': 'b' * 64,
                        'browser_libraries': [], 'node': str(base / 'unit-node')}
            request = {'schema_version': 1, 'nonce': 'unit', 'definition': case, 'font_files': []}
            def build(argv, **kwargs):
                folder = Path(argv[3])
                folder.mkdir()
                (folder / 'closure.json').write_text(json.dumps({item['path']: item['sha256']
                    for item in case['official']['source_paths']}), encoding='utf-8')
                return SimpleNamespace(returncode=0)
            def observations(directory, request):
                (Path(directory) / 'image.bmp').write_bytes(bmp())
                return [{'provenance': {'browser': {'unit': True}},
                         'captures': [{'path': 'image.bmp', 'sha256': reference.sha(Path(directory) / 'image.bmp')}]}]
            worker = SimpleNamespace(wait=lambda **kwargs: 0)
            with patch.object(reference, 'ROOT', base), patch.object(reference, 'verify_prepared', return_value=prepared), \
                    patch.object(reference, 'validate_request', return_value=case), \
                    patch.object(reference.subprocess, 'run', side_effect=build) as build_call, \
                    patch.object(reference.subprocess, 'Popen', return_value=worker) as execution, \
                    patch.object(reference, 'validate_observations', side_effect=observations):
                result = reference.execute(request, base / 'prepared', base / 'miss', publish_cache=False)
                self.assertFalse(result['cache_published'])
                self.assertFalse((base / 'out/demo-differential/reference-cache').exists())
                cache = base / 'out/demo-differential/reference-cache' / result['cache_key']
                cache.mkdir(parents=True)
                manifest = {'browser_environment': {'unit': True}, 'capture_hashes': {'image.bmp': reference.sha(base / 'miss/image.bmp')}}
                with patch.object(reference, 'verify_cache', return_value=manifest) as verify:
                    hit = reference.execute(request, base / 'prepared', base / 'hit', publish_cache=False)
                    self.assertTrue(hit['cache_hit'])
                    self.assertFalse(hit['cache_published'])
                    self.assertEqual(verify.call_count, 2)
                with patch.object(reference, 'verify_cache', return_value={**manifest, 'capture_hashes': {}}), \
                        self.assertRaisesRegex(reference.PreparationError, 'capture drift'):
                    reference.execute(request, base / 'prepared', base / 'drift', publish_cache=False)
                self.assertEqual(build_call.call_count, 3)
                self.assertEqual(execution.call_count, 3)


if __name__ == '__main__':
    unittest.main()
