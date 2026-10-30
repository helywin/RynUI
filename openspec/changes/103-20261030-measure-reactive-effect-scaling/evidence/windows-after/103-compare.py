import csv
import hashlib
import json
import math
from collections import defaultdict
from pathlib import Path

root = Path('D:/code/RynUI')
out = root / 'out'
frozen = root / 'openspec/changes/103-20261030-measure-reactive-effect-scaling/evidence/windows-baseline'

def decode(path):
    raw = path.read_bytes()
    return raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')

source_index = {item['path']: item for item in json.loads((frozen / 'index.json').read_text(encoding='utf-8'))['sources']}
for name in ['tests/reactive_fanout_benchmark.cpp', 'tests/effect_range_benchmark.cpp',
             'tests/support/scaling_benchmark.hpp', 'tests/support/allocation_probe.hpp']:
    assert hashlib.sha256((root / name).read_bytes()).hexdigest() == source_index[name]['sha256'], name

result = {'matched_work_samples': 0, 'groups': []}
for component in ['reactive', 'effect']:
    keys = ['mode', 'k', 'operation'] if component == 'reactive' else ['effects', 'requested_ranges', 'mode']
    work = ['source_updates', 'observer_runs', 'source_reads', 'scheduler_epochs', 'result_checksum'] if component == 'reactive' else [
        'geometry_updates', 'material_updates', 'geometry_input_ranges', 'material_input_ranges',
        'upload_ranges', 'uploaded_bytes', 'packed_hash', 'actual_ranges_first_count']
    metrics = ['total_ns'] if component == 'reactive' else ['update_total_ns', 'sync_total_ns', 'effect_sync_total_ns', 'commit_total_ns']
    buckets = {'baseline': defaultdict(list), 'after': defaultdict(list)}
    for round_number in range(1, 4):
        records = {}
        for label in buckets:
            path = out / f'103-{label}-bench-Release-{component}-r{round_number}.csv'
            metadata = json.loads(decode(path.with_suffix('.json')))
            assert metadata['exit'] == 0 and metadata['iterations'] == 64 and metadata['warmup'] == 8
            records[label] = [r for r in csv.DictReader(decode(path).splitlines()) if r['record'] == 'sample']
            for row in records[label]:
                if row['phase'] == 'hot':
                    buckets[label][tuple(row[k] for k in keys)].append(row)
        assert len(records['baseline']) == len(records['after'])
        for before, after in zip(records['baseline'], records['after']):
            assert all(before[k] == after[k] for k in keys + ['phase', 'sample'] + work), (component, before, after)
            result['matched_work_samples'] += 1
    assert buckets['baseline'].keys() == buckets['after'].keys()
    for key, before in buckets['baseline'].items():
        after = buckets['after'][key]
        assert len(before) == len(after) == 192
        item = {'component': component, **dict(zip(keys, key)), 'samples_per_side': len(before)}
        for metric in metrics:
            data = {}
            for label, samples in [('baseline', before), ('after', after)]:
                values = sorted(int(r[metric]) for r in samples)
                data[label] = {'mean_ms': sum(values) / len(values) / 1e6,
                               'p95_ms': values[math.ceil(.95 * len(values)) - 1] / 1e6}
            data['mean_ratio'] = data['baseline']['mean_ms'] / data['after']['mean_ms'] if data['after']['mean_ms'] else None
            item[metric] = data
        result['groups'].append(item)
(out / '103-comparison.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(f'Matched {result["matched_work_samples"]} actual-work samples; compared {len(result["groups"])} hot groups, 192 samples per side/group.')
for group in result['groups']:
    if group['component'] == 'reactive' and group['k'] == '4096':
        print(group['mode'], group['operation'], group['total_ns'])
    if group['component'] == 'effect' and group['effects'] == '10000' and group['requested_ranges'] == '1000':
        print(group['mode'], 'update', group['update_total_ns'], 'sync', group['sync_total_ns'])
