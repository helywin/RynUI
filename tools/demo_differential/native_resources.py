"""Consume the frozen native resource maps; observation v1 remains unchanged.

Interface provenance: native source-only patch
a26daf7737857fa94496e9c8e74939960e560230bec150265f27285acbdcb80d.
These checks validate observations, never generate a semantic oracle.
"""
from __future__ import annotations

from .observations import IncompleteRun
from .oracle import exact


def require(condition, message):
    if not condition:
        raise IncompleteRun(message)


def integer(value, minimum=0):
    return type(value) is int and value >= minimum


def time_value(value):
    return value is None or type(value) is int


def lifecycle(value):
    required = {'owner_generation', 'owners', 'animation_scopes', 'auxiliary_deadlines', 'live_components',
        'live_scopes', 'live_interactions', 'focus_owned', 'capture_owned', 'session_owned', 'retired',
        'idle_observation', 'deadline_coverage', 'window_deadline_us', 'shell_owner_present', 'shared_font_cache_policy'}
    require(isinstance(value, dict) and required <= set(value), 'missing flat native sample lifecycle map')
    require(all(integer(value[k]) for k in ('owner_generation', 'live_components', 'live_scopes', 'live_interactions')),
            'lifecycle generation/count is not an actual nonnegative integer')
    require(all(type(value[k]) is bool for k in ('focus_owned', 'capture_owned', 'session_owned', 'shell_owner_present')),
            'lifecycle ownership flags are unknown or not booleans')
    require(value['deadline_coverage'] == 'sample-animation-scopes-and-pilot-auxiliary', 'unknown lifecycle deadline coverage')
    require(value['idle_observation'] == 'not-sampled', 'snapshot cannot claim passive idle was sampled')
    require(value['shared_font_cache_policy'] == 'window-owned-retained-not-page-leak', 'unknown shared font ownership policy')
    require(time_value(value['window_deadline_us']), 'window deadline must retain its actual int/null value')
    require(all(isinstance(value[k], list) for k in ('owners', 'animation_scopes', 'auxiliary_deadlines')),
            'lifecycle retained owner/scope/deadline collections are missing')
    identities = set()
    present = set()
    for row in value['owners']:
        require(isinstance(row, dict) and {'identity', 'present', 'branch_active', 'generation', 'page_generation'} <= set(row),
                'incomplete retained owner identity/generation')
        require(isinstance(row['identity'], str) and row['identity'] and row['identity'] not in identities,
                'duplicate or absent retained owner identity')
        identities.add(row['identity'])
        require(type(row['present']) is bool and integer(row['generation']) and integer(row['page_generation']),
                'invalid retained owner generation or presence')
        require(type(row['branch_active']) is bool if row['present'] else row['branch_active'] is None,
                'absent owner branch must be unknown/null')
        if row['present']:
            present.add(row['identity'])
    require(value['live_components'] == len(present), 'sample live component count contradicts retained generations')
    scopes = set()
    live_scopes = 0
    for row in value['animation_scopes']:
        require(isinstance(row, dict) and {'identity', 'present', 'active', 'deadline_us', 'generation', 'page_generation'} <= set(row),
                'incomplete animation scope generation')
        require(isinstance(row['identity'], str) and row['identity'] and row['identity'] not in scopes,
                'duplicate animation scope identity')
        scopes.add(row['identity'])
        require(type(row['present']) is bool and integer(row['generation']) and integer(row['page_generation']),
                'invalid animation scope presence/generation')
        if row['present']:
            require(integer(row['active']) and time_value(row['deadline_us']), 'scope active is an integer, never a bool')
            live_scopes += 1
        else:
            require(row['active'] is None and row['deadline_us'] is None, 'absent scope activity/deadline must remain null')
    require(value['live_scopes'] == live_scopes, 'sample live scope count contradicts exact scope generations')
    for row in value['auxiliary_deadlines']:
        require(isinstance(row, dict) and {'kind', 'owner', 'time_us'} <= set(row) and
                row['kind'] in ('button-loading', 'input-caret', 'select-motion') and row['owner'] in present and
                type(row['time_us']) is int, 'auxiliary deadline has no live sample owner or actual time')
    retired = (all(value[k] == 0 for k in ('live_components', 'live_scopes', 'live_interactions')) and
               all(value[k] is False for k in ('focus_owned', 'capture_owned', 'session_owned')))
    require(value['retired'] is (retired if value['owner_generation'] else None), 'sample retired contradicts owner-local facts')
    # Non-null window deadlines and retained shell/font caches are legal. Do not
    # invent cache_count fields or infer passive idle from these snapshots.
    return value


def validate(observed, manifest, binary_hash):
    last_readback = last_fence = 0
    renderer = None
    owner_generations = {}
    scope_generations = {}
    for row in observed['records']:
        resources = row['resources']
        value = lifecycle(resources.get('lifecycle'))
        for key, previous in (('owners', owner_generations), ('animation_scopes', scope_generations)):
            for item in value[key]:
                identity = item['identity']
                generation = (item['generation'], item['page_generation'])
                require(identity not in previous or previous[identity] == generation, 'retained semantic identity reused a stale generation')
                previous[identity] = generation
        require(resources.get('compositor_callback') is None and resources.get('presented') is None and
                resources.get('presentation_evidence') == 'not-observed-by-checkpoint-readback',
                'checkpoint must not manufacture presentation evidence')
        readback = resources.get('readback')
        if row['type'] != 'checkpoint':
            require('readback' in resources and readback is None, 'non-checkpoint readback must be null')
            continue
        fields = {'nonce', 'checkpoint', 'readback_id', 'fence_id', 'scene_revision', 'renderer_owner',
                  'device_epoch', 'metrics', 'source_channel_order', 'source_hash', 'binary_sha256'}
        require(isinstance(readback, dict) and fields <= set(readback), 'checkpoint lacks real readback/fence evidence')
        require(readback['nonce'] == row['nonce'] and readback['checkpoint'] == row['step'], 'readback belongs to another run/checkpoint')
        require(integer(readback['readback_id'], 1) and readback['readback_id'] > last_readback and
                integer(readback['fence_id'], 1) and readback['fence_id'] > last_fence,
                'old readback/fence reused even though the scene may be unchanged')
        last_readback, last_fence = readback['readback_id'], readback['fence_id']
        owner = readback['renderer_owner']
        require((integer(owner, 1) or type(owner) is str and bool(owner)) and integer(readback['device_epoch'], 1),
                'readback has no renderer owner/device epoch')
        current_renderer = (readback['renderer_owner'], readback['device_epoch'])
        require(renderer is None or renderer == current_renderer, 'readback renderer owner/device epoch changed')
        renderer = current_renderer
        require(exact(readback['metrics'], resources['metrics']), 'readback metrics differ from actual window metrics')
        require(readback['source_hash'] == manifest['source_hash'] and readback['binary_sha256'] == binary_hash,
                'checkpoint readback source/binary differs from requested build')
        require(readback['source_channel_order'] in ('RGBA', 'BGRA'), 'unknown actual readback source channel order')
        for capture in row['captures']:
            require(readback['scene_revision'] == capture['scene_revision'], 'readback scene revision differs from saved current capture')
            require(capture['channel_order'] == 'RGBA' and capture['framebuffer_format'] ==
                    readback['source_channel_order'] + '8_UNORM', 'source format or normalized RGBA channel contract differs')
    return observed
