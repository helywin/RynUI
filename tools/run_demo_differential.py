#!/usr/bin/env python3
"""Machine-judged official Ant Design/native Gallery differential tests."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

from demo_differential.inventory import (query_binary, source_inventory, require_retained, obligations,
                                         coverage_gaps, VECTORS)
from demo_differential.protocol import FIXTURES, ProtocolError, output_path, read
from demo_differential import pilots
from demo_differential.runner import run, select
from demo_differential.adapters import RunError
from demo_differential.meta_test import FAULTS, run_pilot_meta


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    inventory = commands.add_parser('inventory', help='Compare actual compiled registry with all source writers')
    inventory.add_argument('--binary', type=Path, required=True)
    inventory.add_argument('--output', type=Path, required=True)
    listing = commands.add_parser('list', help='List source-bound pilot definitions; does not execute')
    running = commands.add_parser('run', help='Run the finite pilot with actual native/locked official Web adapters')
    meta = commands.add_parser('meta-test', help='Require full pilot green, every real acceptance fault red, then green')
    calibrating = commands.add_parser('calibrate', help='Independently capture fixed pilot controls; atomically publish only reconstructible evidence')
    calibrating.add_argument('--suite', choices=['pilot'], required=True)
    calibrating.add_argument('--binary', type=Path, required=True)
    calibrating.add_argument('--control-binary', type=Path, required=True)
    calibrating.add_argument('--preset', required=True)
    calibrating.add_argument('--output', type=Path, required=True)
    calibrating.add_argument('--prepared', type=Path)
    calibrating.add_argument('--font-files', type=Path)
    calibrating.add_argument('--comparison-site', type=Path)
    calibrating.add_argument('--offline', action='store_true')
    calibrating.add_argument('--dpr', choices=['native'], default='native')
    calibrating.add_argument('--repetitions', type=int, default=5)
    calibrating.add_argument('--timeout', type=int, choices=range(1,121), default=60)
    for command in (listing, running, meta):
        command.add_argument('--suite', choices=['pilot'], default='pilot')
        command.add_argument('--case', dest='cases', action='append', default=[])
        command.add_argument('--component')
        command.add_argument('--family')
    for command in (running, meta):
        command.add_argument('--binary', type=Path, required=True)
        command.add_argument('--preset', required=True, help='Concrete CMake build preset, e.g. windows-msvc-release')
        command.add_argument('--output', type=Path, required=True)
        command.add_argument('--reference', choices=['locked', 'rebuild'] if command is running else ['locked'], default='locked')
        command.add_argument('--profile-file', type=Path, required=True, help='Actual independently calibrated v1 profile')
        command.add_argument('--prepared', type=Path)
        command.add_argument('--font-files', type=Path)
        command.add_argument('--comparison-site', type=Path)
        command.add_argument('--offline', action='store_true')
        command.add_argument('--dpr', default='native')
        command.add_argument('--timeout', type=int, default=30)
    running.add_argument('--fault', choices=['none', *(fault.identity for fault in FAULTS)], default='none',
                         help='Acceptance-only native pilot fault; this ordinary run cannot pass or publish cache')
    replay = commands.add_parser('replay', help='Rerun failed/not-run cases with unchanged definitions/profile/reference')
    replay.add_argument('--failure', type=Path, required=True)
    replay.add_argument('--binary', type=Path)
    replay.add_argument('--preset')
    replay.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == 'calibrate':
            from demo_differential.calibration import calibrate
            return calibrate(vars(args))
        if args.command == 'list':
            print(json.dumps([{'identity': case['identity'], 'sample': case['native']['sample_id'],
                               'targets': case['native']['targets'], 'profile': case['profile'],
                               'execution': 'not-run'} for case in select(pilots.definitions(), vars(args))], indent=2))
            return 0
        if args.command == 'run':
            from demo_differential.calibration import preflight_profile
            return run(preflight_profile(vars(args)), pilots.definitions())
        if args.command == 'meta-test':
            result = run_pilot_meta(vars(args), pilots.definitions())
            print(json.dumps({'meta_report': str(result.report_path), 'exit_code': result.exit_code}))
            return result.exit_code
        if args.command == 'replay':
            from demo_differential.protocol import validate
            from demo_differential.comparison import digest
            previous = validate('report', read(args.failure))
            options = previous['provenance'].get('options')
            if not options:
                raise ProtocolError('failure has no replayable original options')
            from demo_differential.runner_evidence import sha
            if sha(options['profile_file']) != previous['provenance'].get('profile_sha256'):
                raise ProtocolError('profile changed; replay cannot silently migrate tolerances')
            cases = [row['identity'] for row in previous['cases'] if row['verdict'] != 'pass']
            if not cases:
                raise ProtocolError('replay selection is empty')
            definitions = pilots.definitions()
            for case in definitions:
                if case['identity'] in cases and previous['provenance']['definitions'].get(case['identity']) != digest(case):
                    raise ProtocolError('case/fixture/oracle changed; replay requires an explicit definition migration')
            options.update(cases=cases, output=args.output, binary=args.binary or options['binary'],
                           preset=args.preset or options['preset'], command='run')
            from demo_differential.calibration import preflight_profile
            return run(preflight_profile(options), definitions)
        output = output_path(args.output)
        if output.exists():
            raise ProtocolError('output already exists; use a fresh directory')
        output.mkdir(parents=True)
        actual = query_binary(args.binary, output / 'inventory.json')
        delta = require_retained(actual, read(FIXTURES / 'identity-baseline.json'))
        coverage = read(FIXTURES / 'coverage.json')
        gaps = coverage_gaps(actual, coverage)
        result = {'schema_version': 1, 'source_binary_equal': True,
                  'counts': {vector: len(actual[vector]) for vector in VECTORS},
                  'baseline_delta': delta, 'obligations': len(obligations(actual)), 'coverage_gaps': gaps}
        (output / 'inventory-check.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
        # This is an inventory check, never a claim of executed visual/logic coverage.
        print(json.dumps({'source_binary_equal': True, 'counts': result['counts'],
                          'coverage_gaps': len(gaps)}, ensure_ascii=False))
        return 0
    except (ProtocolError, OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return error.code if isinstance(error, RunError) else 2


if __name__ == '__main__':
    raise SystemExit(main())
