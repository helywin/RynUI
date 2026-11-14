"""Source of the version-one protocol schemas; no observations are fixtures."""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def obj(properties, required=None):
    return {'type': 'object', 'additionalProperties': False, 'properties': properties,
            'required': list(properties) if required is None else required}


def arr(items, minimum=0, maximum=100000, unique=False):
    return {'type': 'array', 'items': items, 'minItems': minimum, 'maxItems': maximum, 'uniqueItems': unique}


def enum(*values):
    return {'type': 'string', 'enum': list(values)}


def ref(name):
    return {'$ref': '#/$defs/' + name}


S = {'type': 'string', 'minLength': 1, 'maxLength': 4096}
TEXT = {'type': 'string', 'maxLength': 1048576}
ID = {'type': 'string', 'pattern': '^[a-zA-Z0-9][a-zA-Z0-9._:/-]*$', 'maxLength': 256}
SHA = {'type': 'string', 'pattern': '^[0-9a-f]{64}$'}
COMMIT = {'type': 'string', 'pattern': '^[0-9a-f]{40}$'}
PATH = {'type': 'string', 'format': 'relative-path', 'maxLength': 4096}
N = {'type': 'number'}
U = {'type': 'integer', 'minimum': 0}
BOOL = {'type': 'boolean'}
VERSION = {'type': 'integer', 'const': 1}
MAPPING = enum('paired', 'native-only', 'web-only', 'unsupported', 'pending')
CATEGORY = enum('visual', 'layout', 'typography', 'token', 'state', 'input', 'logic', 'lifecycle',
                'environment', 'reference-drift', 'protocol', 'coverage', 'calibration')
VERDICT = enum('pass', 'fail', 'error', 'not-run')
FAMILY = enum('input', 'search', 'password', 'text-area', 'otp')
KIND = enum('overview', 'introduction', 'tokens', 'component')

DEFS = {
    'json': {'anyOf': [{'type': ['string', 'number', 'boolean', 'null']},
                       {'type': 'array', 'items': ref('json')},
                       {'type': 'object', 'additionalProperties': ref('json')}]},
    'map': {'type': 'object', 'additionalProperties': ref('json')},
    'rect': obj({'x': N, 'y': N, 'width': {'type': 'number', 'minimum': 0},
                 'height': {'type': 'number', 'minimum': 0}}),
    'route': obj({'identity': ID, 'kind': KIND, 'component': TEXT, 'family': FAMILY}),
    'catalog': obj({'identity': ID, 'category': U, 'source_path': PATH, 'status': S}),
    'sample': obj({'identity': ID, 'component': TEXT, 'family': FAMILY, 'kind': KIND,
                   'source_function': ID, 'public_header': PATH}),
    'source': obj({'identity': ID, 'source_path': PATH, 'source_function': ID,
                   'public_header': PATH, 'code': TEXT}),
    'target': obj({'identity': ID, 'sample': ID, 'alias': TEXT}),
    'fixture': obj({
        'data': ref('map'), 'props': ref('map'), 'slots': ref('map'),
        'parent_policy': enum('accept', 'reject', 'scripted'),
        'clock_epoch_us': U, 'seed': U, 'timezone': S,
        'viewport': obj({'width': {'type': 'integer', 'minimum': 1, 'maximum': 16384},
                         'height': {'type': 'integer', 'minimum': 1, 'maximum': 16384},
                         'dpr': {'anyOf': [{'type': 'number', 'exclusiveMinimum': 0, 'maximum': 4},
                                          {'const': 'native'}]}}),
        'container': obj({'width': N, 'height': N, 'padding': N, 'gap': N, 'layout': S}),
        'theme': ref('map'), 'locale': S, 'format_locale': S, 'direction': enum('ltr', 'rtl'),
        'fonts': arr(obj({'identity': ID, 'sha256': SHA, 'face_index': U, 'weight': U}), 1, 16),
        'font_policy': enum('pinned', 'system'), 'motion': enum('disabled', 'controlled')}),
    'official': obj({'repository': {'const': 'https://github.com/ant-design/ant-design'},
                     'commit': {'const': '4a39f54842eade4e565ab336ef6097cd7e723cdd'},
                     'demo_path': PATH, 'demo_sha256': SHA,
                     'source_paths': arr(obj({'path': PATH, 'sha256': SHA}), 1),
                     'adapter_id': ID, 'adapter_sha256': SHA,
                     'mode': enum('official-demo', 'official-component-adapted'),
                     'adaptation': arr(obj({'field': S, 'value': ref('json'), 'reason': S}))}),
    'native': obj({'route': ID, 'family': FAMILY, 'sample_id': ID, 'targets': arr(ID, 1, unique=True),
                  'source_function': ID, 'public_header': PATH}),
    'trace': obj({'seq': U, 'step': ID, 'time_us': U, 'target': ID, 'key': TEXT, 'event': ID,
                  'payload': ref('json'), 'owner_generation': U, 'layer': S}),
    'target_observation': obj({'identity': ID, 'key': TEXT, 'bounds': ref('rect'), 'clip': ref('rect'),
                               'scroll': ref('map'), 'state': ref('map'),
                               'text': arr(ref('map')), 'icons': arr(ref('map')), 'tokens': ref('map'),
                               'owner_generation': U, 'scene_revision': U}),
    'capture': obj({'path': PATH, 'sha256': SHA, 'nonce': S, 'checkpoint': ID, 'scene_revision': U,
                    'width': U, 'height': U, 'dpr': N, 'fence_completed': BOOL,
                    'framebuffer_format': S, 'channel_order': enum('RGBA', 'BGRA'),
                    'alpha': enum('straight', 'premultiplied', 'opaque'), 'color_profile': S}),
}

ACTION_COMMON = {'step': ID, 'kind': S, 'target': ID, 'time_us': U,
                 'layer': enum('browser-input', 'browser-composition-seam', 'native-core-dispatch',
                               'native-sdl-mapped', 'native-composition-seam', 'external-props', 'clock', 'observer'),
                 'preconditions': ref('map')}
ACTION_PARAMETERS = {
    'pointer-move': obj({'x_fraction': N, 'y_fraction': N}),
    'pointer-down': obj({'button': enum('primary', 'secondary'), 'x_fraction': N, 'y_fraction': N}),
    'pointer-up': obj({'button': enum('primary', 'secondary'), 'x_fraction': N, 'y_fraction': N}),
    'pointer-cancel': obj({}),
    'key-down': obj({'key': ID, 'modifiers': arr(enum('shift', 'control', 'alt', 'meta'), unique=True)}),
    'key-up': obj({'key': ID, 'modifiers': arr(enum('shift', 'control', 'alt', 'meta'), unique=True)}),
    'text-commit': obj({'text': TEXT}),
    'composition': obj({'phase': enum('start', 'update', 'commit', 'cancel'), 'text': TEXT,
                         'selection_start': U, 'selection_end': U, 'session': ID}),
    'wheel': obj({'delta_x': N, 'delta_y': N}),
    'external-props': obj({'values': ref('map')}),
    'async-complete': obj({'request': ID, 'result': ref('json')}),
    'advance-clock': obj({'to_us': U}),
    'navigate': obj({'route': ID, 'sample_id': TEXT}),
    'checkpoint': obj({}),
}
DEFS['action'] = {'oneOf': [obj({**ACTION_COMMON, 'kind': {'const': kind}, 'parameters': parameters})
                                 for kind, parameters in ACTION_PARAMETERS.items()]}
ORACLE_PARAMETERS = {
    'equals': {'path': S, 'value': ref('json')},
    'unchanged': {'path': S, 'from_step': ID},
    'count': {'stream': enum('callbacks', 'receipts'), 'target': ID, 'event': ID, 'equals': U},
    'ordered_events': {'stream': enum('callbacks', 'receipts'), 'events': arr(ID)},
    'happens_before': {'first': ID, 'second': ID},
    'focused': {'target': ID},
    'contains_keys_in_order': {'path': S, 'keys': arr(ID, unique=True)},
    'within': {'path': S, 'minimum': N, 'maximum': N},
    'consumed_by': {'target': ID},
    'retired': {'owner': ID},
    'idle_after': {'owner': ID, 'duration_us': U},
}
DEFS['predicate'] = {'oneOf': [obj({'op': {'const': op}, 'step': ID,
                                   'side': enum('both', 'web', 'native'),
                                   'origin': enum('independent-contract', 'web-observation'), **parameters})
                                     for op, parameters in ORACLE_PARAMETERS.items()]}


def schemas():
    case = obj({'schema_version': VERSION,
        'identity': {'type': 'string', 'pattern': '^antd665\\.[a-z0-9-]+\\.[a-z0-9-]+\\.v1$'},
        'revision': {'type': 'integer', 'minimum': 1}, 'official': ref('official'), 'native': ref('native'),
        'mapping': MAPPING, 'fixture': ref('fixture'), 'profile': ID,
        'actions': arr(ref('action'), 1, 4096),
        'checkpoints': arr(obj({'step': ID, 'targets': arr(ID, 1, unique=True), 'layers': arr(ID, 1, unique=True),
                                 'rois': arr(obj({'identity': ID, 'kind': enum('sample-context', 'control', 'popup', 'guard-band'),
                                                  'target': ID, 'guard_px': N}), 1)}), 1, 256),
        'oracle': arr(ref('predicate'), 1, 16384), 'features': arr(ID, 1, unique=True),
        'dependencies': arr(PATH, 1, unique=True)})
    inventory = obj({'schema_version': VERSION, 'reference_commit': COMMIT, 'catalog_hash': SHA,
                     'catalog': arr(ref('catalog'), 1), 'routes': arr(ref('route'), 1),
                     'samples': arr(ref('sample'), 1), 'sources': arr(ref('source'), 1),
                     'targets': arr(ref('target'), 1)})
    coverage = obj({'schema_version': VERSION, 'baseline_commit': COMMIT,
        'rows': arr(obj({'identity': ID, 'component_id': TEXT, 'family': FAMILY, 'sample_id': TEXT,
                          'target_id': TEXT, 'feature_id': ID, 'mapping': MAPPING,
                          'implementation': enum('ready', 'planned'),
                          'official_demo_ids': arr(PATH, unique=True), 'cases': arr(ID, unique=True),
                          'role': enum('action', 'observation', 'alias', 'sample', 'route', 'catalog'),
                          'reason': TEXT, 'source_paths': arr(PATH, 1, unique=True)}), 1)})
    receipt = obj({'dispatched': BOOL, 'hit': TEXT, 'consumed': BOOL, 'layer': S})
    observation = obj({'schema_version': VERSION, 'type': enum('hello', 'receipt', 'checkpoint', 'complete'),
        'nonce': S, 'case': ID, 'step': TEXT, 'adapter': ID, 'provenance': ref('map'),
        'effective_inputs': ref('map'), 'receipt': receipt, 'state': ref('map'),
        'callbacks': arr(ref('trace')), 'targets': arr(ref('target_observation')),
        'captures': arr(ref('capture')), 'resources': ref('map'),
        'completed_steps': arr(ID, unique=True), 'completed_checkpoints': arr(ID, unique=True)})
    nullable = lambda rule: {'anyOf': [rule, {'type': 'null'}]}
    profile = obj({'schema_version': VERSION, 'identity': ID, 'environment_hash': SHA,
        'algorithm_hash': SHA, 'corpus_hash': SHA, 'source_hash': SHA, 'font_hashes': arr(SHA, 7, 7, unique=True),
        'calibration_kind': {'const': 'pilot-independent-v1'}, 'tool_hash': SHA,
        'scope': obj({'cases': arr(ID,3,3,unique=True), 'locale': {'const':'zh-CN'},
            'direction': {'const':'ltr'}, 'theme': {'const':'default'}, 'motion': {'const':'disabled'},
            'font_policy': {'const':'pinned'}, 'actual_dpr': nullable({'type':'number','exclusiveMinimum':0,'maximum':4}),
            'origin_phase_px': arr(N,2,2), 'materials': arr(ID,1,unique=True),
            'font_sizes_weights': arr(obj({'size':N,'weight':U}),12,12,unique=True)}),
        'repetitions': {'type': 'integer', 'minimum': 5, 'maximum':15}, 'calibrated': BOOL,
        'metrics': arr(obj({'identity': ID, 'threshold': N, 'hard_cap': N, 'quantization_floor': N,
                            'positive_max': N, 'margin': N, 'negative_min': N,
                            'unit':ID, 'domain':ID, 'holdout_max':N, 'safety_gap':N,
                            'positive_ids':arr(ID,1,unique=True), 'holdout_ids':arr(ID,1,unique=True),
                            'negative_ids':arr(ID,1,26,unique=True)}), 0,16),
        'masks': arr(obj({'identity': ID, 'reason': S, 'area_fraction': {'type': 'number', 'minimum': 0, 'maximum': .01},
                          'rect': ref('rect'), 'protected_intersection': {'const': False}}),0,0),
        'color': nullable(obj({
            'native':obj({'framebuffer_format':enum('RGBA8_UNORM','BGRA8_UNORM'),
                'source_channel_order':enum('RGBA','BGRA'), 'decoded_channel_order':{'const':'RGBA'},
                'encoding':{'const':'native-encoded-SDR'}, 'alpha':{'const':'premultiplied'},
                'clear_rgba':arr(N,4,4), 'opaque_required':{'const':True}, 'codec_policy':{'const':'opaque-BMP-rawRGBA-v1'}}),
            'web':obj({'file_formats':arr(enum('PNG RGB8','PNG RGBA8'),1,2,unique=True),
                'decoded_channel_order':{'const':'RGBA'}, 'encoding':{'const':'sRGB-SDR'},
                'alpha_policy':{'const':'RGB-opaque-or-verified-RGBA-opaque'},
                'launch_args_hash':SHA, 'png_color_chunks_policy':{'const':'record-and-verify-chunks-CRC-v1'},
                'png_color_chunks':arr(obj({'type':enum('sRGB','gAMA','cHRM','iCCP'),'sha256':SHA,'bytes':U}),0,4)}),
            'comparison':obj({'domain':{'const':'sRGB-encoded-opaque'},
                'conversion':{'const':'verified-opaque-identity-v1'}, 'lab':{'const':'sRGB-D65-CIEDE2000-v1'}}),
            'control_evidence_sha256':SHA})),
        'evidence':nullable(obj({'path':PATH,'sha256':SHA,'train_ids':arr(ID,6,16,unique=True),
            'holdout_ids':arr(ID,6,16,unique=True),'negative_ids':arr(ID,26,26,unique=True),
            'control_definition_sha256':SHA,'policy_sha256':SHA,'observations_and_captures_manifest_sha256':SHA})),
        'font_policy': {'const':'pinned'}})
    report = obj({'schema_version': VERSION, 'nonce': S, 'status': VERDICT,
        'exit_code': {'type': 'integer', 'minimum': 0, 'maximum': 4},
        'selected': arr(ID, unique=True), 'expected': arr(ID, unique=True),
        'completed': arr(ID, unique=True), 'passed': arr(ID, unique=True),
        'cases': arr(obj({'identity': ID, 'verdict': VERDICT, 'categories': arr(CATEGORY, unique=True),
                          'first_step': TEXT, 'failures': arr(ref('map')), 'artifacts': arr(PATH, unique=True)})),
        'errors': arr(obj({'category': CATEGORY, 'message': S})), 'provenance': ref('map'),
        'coverage': ref('map'), 'timings': ref('map'), 'repro_argv': arr(S), 'replay_argv': arr(S),
        'desktop_fallback': {'const': False}})
    result = {}
    for name, body in {'case': case, 'inventory': inventory, 'coverage': coverage,
                       'observation': observation, 'profile': profile, 'report': report}.items():
        needed = set()
        def collect(value):
            if isinstance(value, dict):
                if '$ref' in value:
                    key = value['$ref'][8:]
                    if key not in needed:
                        needed.add(key)
                        collect(DEFS[key])
                for item in value.values():
                    collect(item)
            elif isinstance(value, list):
                for item in value:
                    collect(item)
        collect(body)
        definitions = {key: value for key, value in DEFS.items() if key in needed}
        result[name] = {'$schema': 'https://json-schema.org/draft/2020-12/schema',
                        '$id': f'urn:rynui:demo-differential:{name}:v1', '$defs': definitions, **body}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2] / 'tests/fixtures/demo-differential'
    for name, schema in schemas().items():
        path = root / f'{name}.schema.json'
        data = json.dumps(schema, ensure_ascii=False, indent=2) + '\n'
        if args.check:
            if path.read_text(encoding='utf-8') != data:
                raise SystemExit(f'stale schema: {path}')
        else:
            path.write_bytes(data.encode('utf-8'))


if __name__ == '__main__':
    main()
