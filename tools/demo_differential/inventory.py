"""Join independent source writers with the actual compiled Gallery registry."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import subprocess

from .protocol import ROOT, FIXTURES, ProtocolError, read, validate, unique
from generate_gallery_source import generate
from update_ant_design_gallery_catalog import canonical_json, input_paths

VECTORS = ('catalog', 'routes', 'samples', 'sources', 'targets')
FAMILIES = ('input', 'search', 'password', 'text-area', 'otp')
DEFINITION = 'examples/token_gallery/token_gallery_definition.cpp'


def source_inventory(root=ROOT):
    root = Path(root)
    paths = input_paths(root)
    manifest, overlay, source_schema, overlay_schema = [read(path) for path in paths]
    statuses = {row['identity']: row['status'] for row in overlay['entries']}
    catalog = [{'identity': row['identity'], 'category': category, 'source_path': row['source_path'],
                'status': statuses[row['identity']]}
               for category, group in enumerate(manifest['categories']) for row in group['components']]
    routes = [{'identity': kind, 'kind': kind, 'component': '', 'family': 'input'}
              for kind in ('overview', 'introduction', 'tokens')]
    for entry in catalog:
        slug = entry['identity'].removeprefix('ant.component.')
        for family in FAMILIES if slug == 'input' else ('input',):
            routes.append({'identity': f'input/{family}' if slug == 'input' else slug,
                           'kind': 'component', 'component': entry['identity'], 'family': family})
    text = (root / DEFINITION).read_text(encoding='utf-8')
    registry = text[text.index('constexpr auto sample_factories'):text.index('constexpr auto sample_descriptors')]
    blocks = re.split(r'(?=\{\{"gallery\.sample\.)', registry)[1:]
    records = generate(root)
    if len(blocks) != len(records):
        raise ProtocolError('sample descriptor parser/writer identity count differs')
    samples = []
    for block, record in zip(blocks, records):
        identity = re.search(r'\{\{"([^"]+)"', block)[1]
        if identity != record.identity:
            raise ProtocolError('sample descriptor parser/writer order differs')
        component = re.search(r'\{\{"[^"]+",\s*"([^"]*)"', block)[1]
        family = re.search(r'GalleryInputFamily::(\w+)', block)
        samples.append({'identity': identity, 'component': component,
                        'family': family[1].replace('_', '-') if family else 'input',
                        'kind': 'tokens' if 'GalleryPageKind::tokens' in block else 'component',
                        'source_function': record.function, 'public_header': record.header})
    target_text = (root / 'examples/token_gallery/gallery_test_inventory.cpp').read_text(encoding='utf-8')
    target_text = target_text[target_text.index('constexpr auto targets'):target_text.index('});')]
    targets = [{'identity': identity, 'sample': owner, 'alias': alias} for identity, owner, alias in
               re.findall(r'\{"([^"]+)",\s*"([^"]+)",\s*"([^"]*)"\}', target_text)]
    result = {'schema_version': 1, 'reference_commit': manifest['upstream']['commit'],
              'catalog_hash': hashlib.sha256(canonical_json(dict(zip(
                  ('manifest', 'overlay', 'source_schema', 'overlay_schema'),
                  (manifest, overlay, source_schema, overlay_schema))))).hexdigest(),
              'catalog': catalog, 'routes': routes, 'samples': samples,
              'sources': [{'identity': row.identity, 'source_path': DEFINITION, 'source_function': row.function,
                           'public_header': row.header, 'code': row.code} for row in records], 'targets': targets}
    validate('inventory', result)
    validate_relations(result)
    return result


def validate_relations(value):
    samples = {row['identity']: row for row in value['samples']}
    sources = {row['identity']: row for row in value['sources']}
    if list(samples) != list(sources):
        raise ProtocolError('source/sample ordered sets differ')
    for identity, sample in samples.items():
        if any(sample[field] != sources[identity][field] for field in ('source_function', 'public_header')):
            raise ProtocolError(f'source/sample provenance differs: {identity}')
    targets = {row['identity']: row for row in value['targets']}
    for identity, target in targets.items():
        if target['sample'] not in samples:
            raise ProtocolError(f'target has no live sample owner: {identity}')
        chain = {identity}
        alias = target['alias']
        while alias:
            if alias in chain:
                raise ProtocolError(f'target alias cycle: {identity}')
            chain.add(alias)
            if alias in targets:
                if targets[alias]['sample'] != target['sample']:
                    raise ProtocolError(f'alias changes sample owner: {identity}')
                alias = targets[alias]['alias']
            else:
                # Some legacy aliases point at a newer live target rather than
                # another legacy identity. Prove the real factory consumes it.
                if json.dumps(alias) not in sources[target['sample']]['code']:
                    raise ProtocolError(f'alias has no actual source consumer: {identity} -> {alias}')
                alias = ''


def differences(expected, actual):
    """Ordered, owner-aware delta; same-count replacement is never invisible."""
    result = {}
    for vector in VECTORS:
        left = expected[vector]
        right = actual[vector]
        unique(left)
        unique(right)
        before = {row['identity']: row for row in left}
        after = {row['identity']: row for row in right}
        common_before = [row['identity'] for row in left if row['identity'] in after]
        common_after = [row['identity'] for row in right if row['identity'] in before]
        result[vector] = {
            'added': [row['identity'] for row in right if row['identity'] not in before],
            'deleted': [row['identity'] for row in left if row['identity'] not in after],
            'reordered': common_before != common_after,
            'changed': [key for key in before if key in after and before[key] != after[key]],
        }
    return result


def require_exact(expected, actual):
    validate('inventory', actual)
    validate_relations(actual)
    delta = differences(expected, actual)
    if any(row['added'] or row['deleted'] or row['reordered'] or row['changed'] for row in delta.values()):
        raise ProtocolError('compiled/source inventory differs: ' + json.dumps(delta, ensure_ascii=False))
    if any(expected[key] != actual[key] for key in ('reference_commit', 'catalog_hash')):
        raise ProtocolError('compiled catalog source fingerprint differs')
    return delta


def baseline_projection(inventory):
    return {vector: [{key: value for key, value in row.items() if key not in ('code', 'status')}
                     for row in inventory[vector]] for vector in VECTORS}


def require_retained(inventory, baseline):
    current = baseline_projection(inventory)
    delta = differences(baseline['vectors'], current)
    if any(row['deleted'] or row['reordered'] or row['changed'] for row in delta.values()):
        raise ProtocolError('declared identity baseline requires an explicit migration: ' + json.dumps(delta))
    return delta


def obligations(inventory):
    """Every catalog/route/sample/target remains in its own denominator."""
    result = []
    by_sample = {row['identity']: row for row in inventory['samples']}
    def add(identity, component, family, sample, target, feature, role, paths):
        result.append({'identity': identity, 'component_id': component, 'family': family,
                       'sample_id': sample, 'target_id': target, 'feature_id': feature,
                       'role': role, 'source_paths': paths})
    for row in inventory['catalog']:
        add('catalog/' + row['identity'], row['identity'], 'input', '', '', 'catalog-presence', 'catalog',
            [row['source_path']])
    for row in inventory['routes']:
        add('route/' + row['identity'], row['component'], row['family'], '', '', 'route-presence', 'route',
            ['examples/token_gallery/gallery_page_model.cpp'])
    for row in inventory['samples']:
        add('sample/' + row['identity'], row['component'], row['family'], row['identity'], '',
            'default-visible-context', 'sample', [DEFINITION, row['public_header']])
    for row in inventory['targets']:
        sample = by_sample[row['sample']]
        add('target/' + row['identity'], sample['component'], sample['family'], row['sample'], row['identity'],
            'target-alias' if row['alias'] else 'target-observation', 'alias' if row['alias'] else 'observation',
            [DEFINITION, 'examples/token_gallery/gallery_test_inventory.cpp'])
    unique(result)
    return result


def coverage_gaps(inventory, coverage, case_ids=(), execution=None):
    validate('coverage', coverage)
    declared = {row['identity']: row for row in coverage['rows']}
    expected = obligations(inventory)
    expected_ids = {row['identity'] for row in expected}
    gaps = []
    execution = execution or {}
    for row in expected:
        actual = declared.get(row['identity'])
        if not actual:
            gaps.append({'identity': row['identity'], 'reason': 'unmapped'})
            continue
        for field in ('component_id', 'family', 'sample_id', 'target_id', 'feature_id'):
            if actual[field] != row[field]:
                raise ProtocolError(f'coverage changed obligation {field}: {row["identity"]}')
        if actual['mapping'] == 'pending' or actual['implementation'] != 'ready':
            gaps.append({'identity': row['identity'], 'reason': 'planned'})
        elif actual['mapping'] in ('unsupported', 'web-only'):
            if not actual['reason'] or not actual['source_paths']:
                raise ProtocolError('coverage boundary lacks source/reason')
            if actual['component_id'] not in ('ant.component.list', 'ant.component.util'):
                raise ProtocolError('whole existing native obligation cannot be exempted')
        elif not actual['cases'] or any(case not in case_ids for case in actual['cases']):
            gaps.append({'identity': row['identity'], 'reason': 'missing-case'})
        elif any(execution.get(case) != 'pass' for case in actual['cases']):
            gaps.append({'identity': row['identity'], 'reason': 'not-pass'})
    if set(declared) - expected_ids:
        raise ProtocolError('coverage contains removed/unknown obligations: ' + str(sorted(set(declared) - expected_ids)))
    return gaps


def query_binary(binary, output, root=ROOT):
    binary = Path(binary).resolve()
    command = [str(binary), '--page-inventory=' + str(output)]
    result = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=30)
    if result.returncode:
        raise ProtocolError(f'no-window inventory failed ({result.returncode}): {result.stderr}')
    actual = read(output)
    expected = source_inventory(root)
    require_exact(expected, actual)
    return actual
