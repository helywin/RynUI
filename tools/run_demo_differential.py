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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    inventory = commands.add_parser('inventory', help='Compare actual compiled registry with all source writers')
    inventory.add_argument('--binary', type=Path, required=True)
    inventory.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
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
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
