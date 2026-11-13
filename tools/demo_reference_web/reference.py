"""Official Web reference adapter API. It executes inputs; the external runner owns the oracle."""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import struct
import sys
import tempfile
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from demo_differential.protocol import ProtocolError, output_path, read, validate
from demo_reference_web.bootstrap import (PreparationError, canonical, digest, fetch, load_lock, readonly, require,
                       sha, verify_cache, verify_prepared, verify_source)

ADAPTER_ID = 'antd665.reference-web.v1'


def adapter_sha256():
    return sha(HERE / 'app.tsx')


def code_hash():
    return digest({p.name: sha(p) for p in sorted(HERE.iterdir())
                   if p.suffix in ('.py', '.mjs', '.tsx', '.json', '.yaml')})


def central_fonts():
    text = (ROOT / 'cmake/dependencies/RynUIDependencyLock.cmake').read_text()
    variables = dict(re.findall(r'set\((RYNUI_[A-Z0-9_]+)\s+"([^"]+)"\)', text))
    result = []
    for name, identity, filename in [('RYNUI_NOTO_SANS', 'noto-sans', 'NotoSans-Regular.ttf'),
                                     ('RYNUI_NOTO_SANS_CJK_SC', 'noto-sans-cjk-sc', 'NotoSansCJKsc-Regular.otf')]:
        result.append({'identity': identity, 'url': variables[name + '_SOURCE_URL'],
                       'sha256': variables[name + '_SOURCE_SHA256'], 'license': variables[name + '_LICENSE'],
                       'version': variables[name + '_VERSION'], 'filename': filename, 'face_index': 0, 'weight': 400})
    return result


def font_metadata(path):
    data = Path(path).read_bytes()
    offset = struct.unpack_from('>I', data, 12)[0] if data[:4] == b'ttcf' else 0
    count = struct.unpack_from('>H', data, offset + 4)[0]
    names = {}
    for index in range(count):
        tag, _, table_offset, length = struct.unpack_from('>4sIII', data, offset + 12 + index * 16)
        if tag != b'name':
            continue
        _, record_count, strings = struct.unpack_from('>HHH', data, table_offset)
        for row in range(record_count):
            platform_id, _, _, name_id, size, start = struct.unpack_from('>HHHHHH', data, table_offset + 6 + row * 12)
            if name_id not in (1, 2, 4, 6, 16, 17):
                continue
            raw = data[table_offset + strings + start:table_offset + strings + start + size]
            if platform_id in (0, 3):
                value = raw.decode('utf-16-be')
            elif platform_id == 1:
                value = raw.decode('mac_roman')
            else:
                continue
            names.setdefault(str(name_id), [])
            if value not in names[str(name_id)]:
                names[str(name_id)].append(value)
    require(bool(names.get('6')), 'font has no actual PostScript face identity')
    return {'sha256': sha(path), 'face_index': 0, 'names': names}


def prepare_fonts(output):
    output = output_path(output)
    require(not output.exists(), 'font output already exists')
    output.mkdir(parents=True)
    records = []
    for row in central_fonts():
        archive = fetch(row, ROOT / 'out/demo-differential/archives')
        target = output / row['filename']
        shutil.copy2(archive, target)
        records.append({key: row[key] for key in ('identity', 'sha256', 'face_index', 'weight')} | {'path': str(target)})
    (output / 'font-files.json').write_bytes(canonical(records) + b'\n')
    return records


PUBLIC_PROPS = {
    'Button': {'type', 'size', 'disabled', 'loading', 'danger', 'ghost', 'shape', 'block', 'htmlType', 'variant', 'color'},
    'Input': {'size', 'disabled', 'allowClear', 'maxLength', 'minLength', 'placeholder', 'readOnly', 'type',
              'status', 'variant', 'autoComplete', 'name'},
    'Select': {'size', 'disabled', 'loading', 'allowClear', 'placeholder', 'showSearch', 'open', 'defaultOpen',
               'virtual', 'listHeight', 'popupMatchSelectWidth', 'mode', 'status', 'variant', 'optionFilterProp'},
}


def validate_composition(tree, controls, slots):
    seen = []
    count = 0
    properties = {'Flex': {'vertical', 'gap', 'wrap', 'align', 'justify'},
                  'Space': {'size', 'direction', 'align', 'wrap'},
                  'Typography.Text': {'type', 'strong', 'italic', 'underline', 'delete', 'code', 'mark', 'disabled'}}
    def visit(node, depth=0):
        nonlocal count
        count += 1
        require(depth <= 24 and count <= 512, 'composition depth/node limit exceeded')
        require(isinstance(node, dict), 'composition node must be an object')
        component = node.get('component')
        if component == 'control':
            require(set(node) == {'component', 'target'} and node['target'] in controls, 'invalid control composition leaf')
            seen.append(node['target'])
            return
        require(component in properties, 'unsupported composition component/DOM')
        allowed = {'component', 'props', 'content', 'target', 'slot'} if component == 'Typography.Text' else {'component', 'props', 'children'}
        require(set(node) <= allowed, 'unknown composition field')
        props = node.get('props', {})
        require(isinstance(props, dict) and set(props) <= properties[component], 'unknown composition public prop/CSS')
        for key, value in props.items():
            if key in ('vertical', 'strong', 'italic', 'underline', 'delete', 'code', 'mark', 'disabled') or (key == 'wrap' and component == 'Space'):
                require(type(value) is bool, 'composition boolean prop')
            elif key in ('gap', 'size'):
                values = value if isinstance(value, list) and component == 'Space' else [value]
                require(1 <= len(values) <= 2 and all(type(v) in (int, float) and 0 <= v <= 4096 or
                        isinstance(v, str) and v in ('small', 'middle', 'large') for v in values), 'composition gap/size')
            elif key == 'wrap':
                require(type(value) is bool or value in ('wrap', 'nowrap', 'wrap-reverse'), 'composition wrap')
            elif key == 'direction':
                require(value in ('horizontal', 'vertical'), 'composition direction')
            elif key == 'align':
                require(value in ('start', 'end', 'center', 'baseline', 'stretch', 'flex-start', 'flex-end', 'normal'), 'composition align')
            elif key == 'justify':
                require(value in ('start', 'end', 'center', 'flex-start', 'flex-end', 'space-between', 'space-around', 'space-evenly', 'normal'), 'composition justify')
            elif key == 'type':
                require(value in ('secondary', 'success', 'warning', 'danger'), 'composition Typography type')
        if component == 'Typography.Text':
            require(('content' in node) != ('slot' in node), 'Typography needs literal content or caption slot')
            if 'slot' in node:
                require(node.get('target') in controls and node['slot'] == 'caption' and
                        node['slot'] in slots.get(node['target'], {}), 'unknown composition caption slot')
            else:
                require('target' not in node and isinstance(node['content'], str), 'Typography literal must be text')
        else:
            require(isinstance(node.get('children'), list) and node['children'], 'composition requires children')
            for child in node['children']:
                visit(child, depth + 1)
    visit(tree)
    require(len(seen) == len(set(seen)) and set(seen) == set(controls), 'composition must consume each control exactly once')


def validate_request(request, prepared):
    require(set(request) == {'schema_version', 'nonce', 'definition', 'font_files'}, 'unknown/missing Web envelope field')
    require(request['schema_version'] == 1 and isinstance(request['nonce'], str) and request['nonce'], 'request version/nonce')
    definition = validate('case', request['definition'])
    lock = load_lock()
    official = definition['official']
    require(official['commit'] == lock['source']['commit'], 'official commit/tag mismatch')
    require(official['demo_path'] in lock['source']['demos'], 'unregistered official demo')
    require(official['demo_sha256'] == lock['source']['demos'][official['demo_path']], 'official demo source mismatch')
    require(official['adapter_id'] == ADAPTER_ID and official['adapter_sha256'] == adapter_sha256(), 'adapter identity/hash mismatch')
    source = Path(prepared['source'])
    for item in official['source_paths']:
        require(sha(source / item['path']) == item['sha256'], 'declared source closure mismatch: ' + item['path'])
    fixture = definition['fixture']
    require(type(fixture['viewport']['dpr']) in (float, int), 'resolve actual native DPR before Web execution')
    require(fixture['theme'].get('algorithm', 'default') in ('default', 'dark', 'compact'), 'unsupported theme algorithm')
    require(set(fixture['theme']) <= {'algorithm', 'token', 'components', 'inherit', 'hashed', 'cssVar'}, 'unknown theme input')
    require(fixture['container']['layout'] in ('row', 'column', 'wrap', 'composition-v1'), 'unsupported fixture layout')
    require(fixture['parent_policy'] in ('accept', 'reject'), 'scripted parent policy is not implemented')
    require(fixture['locale'] in ('zh-CN', 'en-US'), 'unsupported component locale')
    require(fixture['format_locale'] == fixture['locale'], 'independent format locale adapter is not implemented')
    require(fixture['fonts'] and len(request['font_files']) == len(fixture['fonts']), 'font chain missing')
    fonts = []
    pinned = {r['sha256'] for r in central_fonts()}
    for expected, actual in zip(fixture['fonts'], request['font_files']):
        require(set(actual) == {'identity', 'path', 'sha256', 'face_index', 'weight'}, 'font descriptor fields')
        require({k: actual[k] for k in expected} == expected, 'font descriptor mismatch')
        path = output_path(actual['path'])
        require(path.is_file() and sha(path) == expected['sha256'], 'font bytes mismatch')
        require(actual['face_index'] == 0, 'nonzero font collection face not implemented')
        require(actual['weight'] == 400, 'unlocked font weight/synthesis policy')
        if fixture['font_policy'] == 'pinned':
            require(actual['sha256'] in pinned, 'pinned font must use central native resource lock')
        fonts.append(actual)
    if official['mode'] == 'official-component-adapted':
        require(set(fixture['props']) == ({'controls', 'composition'} if fixture['container']['layout'] == 'composition-v1' else {'controls'}),
                'adapted props require declared controls/composition map')
        controls = fixture['props']['controls']
        require(isinstance(controls, dict) and controls, 'no controls declared')
        require(set(fixture['data']) <= set(controls) and set(fixture['slots']) <= set(controls), 'unconsumed fixture data/slots')
        slot_fields = {'Button': {'content', 'prefix', 'caption'}, 'Input': {'prefix', 'suffix', 'caption'}, 'Select': {'caption'}}
        data_fields = {'Button': set(), 'Input': {'value', 'defaultValue'}, 'Select': {'value', 'defaultValue', 'options'}}
        for target, entry in controls.items():
            require(entry.get('component') in PUBLIC_PROPS, 'unsupported official component')
            require(set(entry) <= PUBLIC_PROPS[entry['component']] | {'component', 'width'}, 'unknown/unconsumed public props: ' + target)
            if 'width' in entry:
                require(type(entry['width']) in (int, float) and 0 < entry['width'] <= 16384, 'width must be a finite external layout number')
            for key in ('disabled', 'loading', 'danger', 'ghost', 'block', 'allowClear', 'readOnly', 'open', 'defaultOpen', 'virtual'):
                if key in entry:
                    require(type(entry[key]) is bool, 'unsupported non-boolean public prop: ' + key)
            if 'size' in entry:
                require(entry['size'] in ('small', 'middle', 'large'), 'unsupported public size')
            require(set(fixture['data'].get(target, {})) <= data_fields[entry['component']], 'unknown/unconsumed initial data')
            data = fixture['data'].get(target, {})
            require(not ('value' in data and 'defaultValue' in data), 'controlled and uncontrolled initial data conflict')
            require(set(fixture['slots'].get(target, {})) <= slot_fields[entry['component']], 'unknown/unconsumed slot')
            for key, value in fixture['slots'].get(target, {}).items():
                scalar = type(value) in (str, int, float)
                icon = (key in ('prefix', 'suffix') and isinstance(value, dict) and set(value) == {'icon'} and
                        isinstance(value['icon'], str) and re.fullmatch(r'[A-Z][A-Za-z0-9]*(Outlined|Filled|TwoTone)', value['icon']))
                require(scalar or icon, 'unsupported slot content/CSS/JS: ' + key)
            if entry['component'] == 'Select':
                require(isinstance(data.get('options'), list) and data['options'], 'Select options required')
                for option in data['options']:
                    require(set(option) <= {'value', 'label', 'disabled'} and {'value', 'label'} <= set(option), 'invalid option')
                require(len({x['value'] for x in data['options']}) == len(data['options']), 'duplicate option key')
        if fixture['container']['layout'] == 'composition-v1':
            validate_composition(fixture['props']['composition'], controls, fixture['slots'])
        # Every changed input is declared, with an independent reason, rather than a silent adaptation.
        fields = ('props', 'slots', 'data', 'container', 'theme', 'parent_policy')
        require(len(official['adaptation']) == len(fields) and all(row['reason'].strip() for row in official['adaptation']),
                'adaptation requires six unique input fields with nonempty source reasons')
        changes = {row['field']: row['value'] for row in official['adaptation']}
        require(set(changes) == set(fields), 'unknown/duplicate adaptation field')
        for key in fields:
            require(key in changes and changes[key] == fixture[key], 'undeclared/mismatched adaptation: ' + key)
    else:
        require(not fixture['props'] and not fixture['slots'] and not fixture['data'], 'official-demo cannot replace internal input')
        require(not official['adaptation'], 'official-demo must not declare internal adaptation')
    allowed = {'pointer-move', 'pointer-down', 'pointer-up', 'key-down', 'key-up', 'text-commit', 'wheel',
               'external-props', 'advance-clock', 'navigate', 'checkpoint'}
    for action in definition['actions']:
        require(action['kind'] in allowed, 'unsupported Web action: ' + action['kind'])
        if action['kind'] == 'external-props':
            require(action['parameters']['values'].keys() == {'parent_policy'} and
                    action['parameters']['values']['parent_policy'] in ('accept', 'reject'), 'unsupported parent policy update')
        if action['kind'].startswith('pointer-'):
            for key in ('x_fraction', 'y_fraction'):
                require(0 <= action['parameters'][key] <= 1, 'pointer fraction outside semantic target')
    return definition


def validate_observations(directory, request):
    directory = output_path(directory)
    path = directory / 'observations.jsonl'
    require(path.is_file() and path.stat().st_size > 0, 'missing observations')
    text = path.read_text()
    require(text.endswith('\n'), 'truncated JSONL')
    from demo_differential.protocol import loads
    records = [validate('observation', loads(line)) for line in text.splitlines()]
    definition = request['definition']
    require(records[0]['type'] == 'hello' and records[-1]['type'] == 'complete', 'missing hello/complete')
    require(sum(r['type'] == 'hello' for r in records) == sum(r['type'] == 'complete' for r in records) == 1, 'duplicate envelope')
    expected_steps = [a['step'] for a in definition['actions']]
    receipts = [r for r in records if r['type'] == 'receipt']
    require([r['step'] for r in receipts] == expected_steps, 'missing/reordered action receipt')
    checkpoints = [r for r in records if r['type'] == 'checkpoint']
    require([r['step'] for r in checkpoints] == [c['step'] for c in definition['checkpoints']], 'missing checkpoint')
    for record in records:
        require(record['nonce'] == request['nonce'] and record['case'] == definition['identity'], 'stale nonce/case')
        require(record['adapter'] == ADAPTER_ID, 'wrong observation adapter')
    for receipt in receipts:
        require(receipt['receipt']['dispatched'], 'action not executed: ' + receipt['step'])
    capture_names = set()
    for record, declaration in zip(checkpoints, definition['checkpoints']):
        require([t['identity'] for t in record['targets']] == declaration['targets'], 'missing checkpoint target')
        require(len(record['captures']) == 1, 'missing/duplicate fresh frame')
        capture = record['captures'][0]
        require(capture['path'] not in capture_names, 'reused capture path')
        capture_names.add(capture['path'])
        require(capture['nonce'] == request['nonce'] and capture['checkpoint'] == record['step'], 'stale frame')
        require(capture['fence_completed'], 'incomplete browser compositor readback')
        require(sha(directory / capture['path']) == capture['sha256'], 'capture truncated/hash mismatch')
        paint = record['resources'].get('paint_receipt', {})
        require(paint.get('fresh_readback') and paint.get('nonce') == request['nonce'] and paint.get('step') == record['step'],
                'missing new paint receipt')
    require(records[-1]['completed_steps'] == expected_steps and
            records[-1]['completed_checkpoints'] == [c['step'] for c in definition['checkpoints']], 'incomplete completion sets')
    return records


def execute(request, prepared_dir, output, *, reference='locked', publish_cache=True):
    prepared_dir = output_path(prepared_dir)
    prepared = verify_prepared(prepared_dir)
    definition = validate_request(request, prepared)
    output = output_path(output)
    require(not output.exists(), 'execution output already exists')
    require(reference in ('locked', 'rebuild'), 'unknown reference cache mode')
    require(type(publish_cache) is bool, 'cache publication policy must be boolean')
    output.mkdir(parents=True)
    for name in ('tmp', 'cache', 'config'):
        (output / name).mkdir()
    lock = load_lock()
    environment = {'platform': sys.platform, 'machine': os.uname().machine if hasattr(os, 'uname') else os.name,
                   'os_release': os.uname().release if hasattr(os, 'uname') else sys.getwindowsversion().build,
                   'browser_sha256': prepared['browser_sha256'], 'node_sha256': prepared['node_sha256'],
                   'browser_libraries': prepared['browser_libraries'], 'graphics_mode': 'Chromium headless-shell default; runtime GPU identity checked separately',
                   'fixture': definition['fixture'], 'font_files': [{k: x[k] for k in x if k != 'path'} for x in request['font_files']]}
    key_inputs = {'definition': definition, 'environment': environment, 'code_hash': code_hash(),
                  'dependency_lock': sha(HERE / 'dependencies.lock.json'), 'source': lock['source']['components_tree_sha256'],
                  'font_lock': sha(ROOT / lock['font_lock']),
                  'protocol': {name: sha(ROOT / path) for name, path in {
                      'case': 'tests/fixtures/demo-differential/case.schema.json',
                      'observation': 'tests/fixtures/demo-differential/observation.schema.json',
                      'validator': 'tools/demo_differential/protocol.py'}.items()}}
    key = digest(key_inputs)
    cache = ROOT / 'out/demo-differential/reference-cache' / key
    cache_hit = cache.exists()
    if cache_hit:
        verify_cache(cache, key)
    bundle = output / 'bundle'
    environment_vars = os.environ.copy()
    environment_vars['PATH'] = str(Path(prepared['node']).parent) + os.pathsep + environment_vars.get('PATH', '')
    environment_vars['TMPDIR'] = str(output / 'tmp')
    with (output / 'build.log').open('wb') as log:
        result = subprocess.run([prepared['node'], str(HERE / 'build.mjs'), str(prepared_dir / 'prepared.json'),
                                 str(bundle), str(HERE / 'app.tsx')], env=environment_vars,
                                stdout=log, stderr=subprocess.STDOUT, timeout=90, cwd=ROOT)
    require(result.returncode == 0, 'official bundle failed; see ' + str(output / 'build.log'))
    closure = read(bundle / 'closure.json')
    for item in definition['official']['source_paths']:
        require(closure.get(item['path']) == item['sha256'], 'declared source was not imported: ' + item['path'])
    provenance = {'source_commit': lock['source']['commit'], 'source_archive_sha256': lock['source']['sha256'],
                  'source_closure': closure, 'source_tree_sha256': lock['source']['components_tree_sha256'],
                  'dependency_lock_sha256': sha(HERE / 'dependencies.lock.json'), 'pnpm_lock_sha256': lock['pnpm_lock_sha256'],
                  'adapter_sha256': adapter_sha256(), 'adapter_closure_sha256': code_hash(),
                  'fixture_sha256': digest(definition['fixture']), 'case_sha256': digest(definition), 'protocol': key_inputs['protocol'],
                  'font_lock_sha256': sha(ROOT / lock['font_lock']),
                  'font_files': environment['font_files'], 'actual_font_sources': [font_metadata(x['path']) for x in request['font_files']],
                  'environment': environment, 'cache_key': key,
                  'python': sys.version, 'python_libraries': []}
    config = {'prepared': prepared, 'request': request, 'output': str(output), 'build': str(bundle), 'provenance': provenance}
    (output / 'worker.json').write_bytes(canonical(config) + b'\n')
    (output / 'request.json').write_bytes(canonical(request) + b'\n')
    with (output / 'execute.log').open('wb') as log:
        process = subprocess.Popen([prepared['node'], str(HERE / 'execute.mjs'), str(output / 'worker.json')],
                                   stdout=log, stderr=subprocess.STDOUT, env=environment_vars, cwd=ROOT,
                                   start_new_session=os.name != 'nt')
        try:
            result = process.wait(timeout=120)
        except subprocess.TimeoutExpired:
            if os.name == 'nt':
                subprocess.run(['taskkill', '/T', '/F', '/PID', str(process.pid)], capture_output=True)
            else:
                import signal
                os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            raise PreparationError('Web browser execution timeout')
    require(result == 0, 'Web browser execution failed; see ' + str(output / 'execute.log'))
    observations = validate_observations(output, request)
    captures = {c['path']: c['sha256'] for r in observations for c in r['captures']}
    browser_environment = observations[0]['provenance']['browser']
    if cache_hit:
        old = verify_cache(cache, key)
        require(old['browser_environment'] == browser_environment, 'reference cache browser/GPU environment mismatch')
        require(old['capture_hashes'] == captures, 'same-input reference capture drift')
    elif publish_cache:
        cache.parent.mkdir(parents=True, exist_ok=True)
        temporary = Path(tempfile.mkdtemp(prefix='.pending-', dir=cache.parent))
        try:
            for name in captures:
                shutil.copy2(output / name, temporary / name)
            (temporary / 'inputs.json').write_bytes(canonical(key_inputs) + b'\n')
            manifest = {'schema_version': 1, 'key': key, 'capture_hashes': captures, 'browser_environment': browser_environment,
                        'files': {p.name: sha(p) for p in temporary.iterdir() if p.is_file()}}
            (temporary / 'manifest.json').write_bytes(canonical(manifest) + b'\n')
            readonly(temporary)
            os.rename(temporary, cache)
        finally:
            if temporary.exists():
                temporary.chmod(0o755)
                shutil.rmtree(temporary)
    result = {'executed': True, 'paired': False, 'cache_key': key, 'cache_hit': cache_hit,
              'cache_published': not cache_hit and publish_cache,
              'steps': len(definition['actions']), 'checkpoints': len(definition['checkpoints']),
              'observations': str(output / 'observations.jsonl')}
    (output / 'result.json').write_bytes(canonical(result) + b'\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('identity')
    fonts = sub.add_parser('fonts')
    fonts.add_argument('--output', required=True)
    run = sub.add_parser('run')
    run.add_argument('--request', required=True)
    run.add_argument('--prepared', required=True)
    run.add_argument('--output', required=True)
    run.add_argument('--reference', choices=['locked', 'rebuild'], default='locked')
    run.add_argument('--no-cache-publish', action='store_true', help='Execute and validate normally without publishing a cache miss')
    args = parser.parse_args()
    try:
        if args.command == 'identity':
            result = {'adapter_id': ADAPTER_ID, 'adapter_sha256': adapter_sha256(), 'code_sha256': code_hash()}
        elif args.command == 'fonts':
            result = prepare_fonts(args.output)
        else:
            result = execute(read(args.request), args.prepared, args.output, reference=args.reference,
                             publish_cache=not args.no_cache_publish)
        print(json.dumps(result))
    except (PreparationError, ProtocolError, OSError, ValueError, subprocess.SubprocessError) as error:
        print('Web reference failed: ' + str(error), file=sys.stderr)
        raise SystemExit(4 if args.command == 'run' else 3)


if __name__ == '__main__':
    main()
