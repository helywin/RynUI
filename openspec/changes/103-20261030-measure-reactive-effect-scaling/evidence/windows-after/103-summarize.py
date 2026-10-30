import csv
import hashlib
import json
import math
from pathlib import Path

root = Path('D:/code/RynUI')
out = root / 'out'

def decoded(path):
    raw = path.read_bytes()
    return raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')

groups = []
for path in sorted(out.glob('103-*-bench-Release-*-r*.csv')):
    metadata = json.loads(decoded(path.with_suffix('.json')))
    if metadata['exit'] != 0:
        continue
    rows = list(csv.DictReader(decoded(path).splitlines()))
    kind = metadata['component']
    keys = ['mode', 'k', 'phase', 'operation'] if kind == 'reactive' else ['effects', 'requested_ranges', 'mode', 'phase']
    times = ['total_ns'] if kind == 'reactive' else ['update_total_ns', 'sync_total_ns', 'effect_sync_total_ns', 'commit_total_ns']
    counts = ['source_updates', 'observer_runs', 'source_reads', 'allocations', 'requested_bytes'] if kind == 'reactive' else ['geometry_updates', 'material_updates', 'upload_ranges', 'uploaded_bytes', 'update_allocations', 'sync_allocations']
    buckets = {}
    summaries = {}
    for row in rows:
        assert row['schema'] == '1'
        key = tuple(row[k] for k in keys)
        if row['record'] == 'sample':
            buckets.setdefault(key, []).append(row)
        else:
            assert key not in summaries
            summaries[key] = row
    assert set(buckets) == set(summaries)
    for key, samples in buckets.items():
        summary = summaries[key]
        n = len(samples)
        assert n == int(summary['samples'])
        item = dict(zip(keys, key)) | {'file': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'label': metadata['label'], 'component': kind, 'round': metadata['round'], 'samples': n}
        for field in times:
            values = [int(x[field]) for x in samples]
            assert sum(values) == int(summary[field]), (path, key, field)
            pfield = 'p95_ns' if field == 'total_ns' else field.replace('total_ns', 'p95_ns')
            percentile = sorted(values)[math.ceil(.95 * n) - 1]
            assert percentile == int(summary[pfield]), (path, key, pfield)
            item[field.replace('total_ns', 'mean_ns')] = sum(values) / n
            item[pfield] = percentile
        for field in counts:
            value = sum(int(x[field]) for x in samples)
            assert value == int(summary[field]), (path, key, field)
            item[field] = value
        if kind == 'reactive':
            assert all(not x['subscriber_scans'] for x in samples)
            if item['phase'] == 'hot':
                k = int(item['k'])
                expected = 0 if item['operation'] == 'inactive_update' else k
                assert all(int(x['observer_runs']) == expected and int(x['source_updates']) == 1 for x in samples)
        else:
            assert all(not x['planning_ns'] and not x['sort_comparisons'] for x in samples)
            expected = int(item['requested_ranges']) if item['phase'] != 'cold_sync' else 1
            assert all(int(x['upload_ranges']) == expected and len(x['actual_ranges_first_count'].split(';')) == expected for x in samples)
        groups.append(item)
result = {'groups': groups, 'raw_samples': sum(x['samples'] for x in groups), 'hot_samples': sum(x['samples'] for x in groups if x['phase'] == 'hot')}
(out / '103-summary.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(f'Verified {len(groups)} groups and {result["raw_samples"]} samples ({result["hot_samples"]} hot).')
for label in sorted({x['label'] for x in groups}):
    for x in groups:
        if x['label'] != label or x['phase'] != 'hot' or x['round'] != 1:
            continue
        if x['component'] == 'reactive':
            print(label, 'reactive', x['mode'], x['k'], x['operation'], 'mean_ms=', round(x['mean_ns'] / 1e6, 6))
        else:
            print(label, 'effect', x['effects'], x['requested_ranges'], x['mode'], 'update_ms=', round(x['update_mean_ns'] / 1e6, 6), 'sync_ms=', round(x['sync_mean_ns'] / 1e6, 6))
