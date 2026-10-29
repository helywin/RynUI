from pathlib import Path
import csv, hashlib, io, json, platform, struct, subprocess
from datetime import datetime, timezone

root = Path('D:/code/RynUI')
out = root / 'out'
dest = root / 'openspec/changes/102-20261029-isolate-tree-table-state-projection/evidence/windows-table'
dest.mkdir(parents=True, exist_ok=True)
def digest(data):
    return hashlib.sha256(data).hexdigest()
def decode(path):
    data = path.read_bytes()
    return data.decode('utf-16') if data[:2] in (b'\xff\xfe', b'\xfe\xff') else data.decode('utf-8-sig')
def dump(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n', encoding='utf-8', newline='\n')

records = []
candidates = sorted(set(out.glob('102-table-*.log')) | set(out.glob('102-table-*.json')) |
                    set(out.glob('102-table-*.xml')) | set(out.glob('102-table-*.diff')) |
                    {out / '102-msvc.ps1', out / '102-table-evidence.py', out / '102-table-preservation.py'})
for source in candidates:
    raw = source.read_bytes()
    frozen = raw
    normalization = 'none'
    if raw[:2] not in (b'\xff\xfe', b'\xfe\xff'):
        frozen = raw.decode('utf-8-sig').replace('\r\n', '\n').encode('utf-8')
        normalization = 'UTF-8 without BOM, LF'
    target = dest / source.name
    target.write_bytes(frozen)
    records.append(dict(path=target.name, sha256=digest(frozen), bytes=len(frozen),
                        original_path=str(source), original_sha256=digest(raw), original_bytes=len(raw),
                        normalization=normalization))

measurements = []
baseline_records = []
for label in ('before', 'table', 'table-page-final'):
    for count in (1000, 5000, 10000):
        for round_number in (1, 2, 3):
            filename = f'102-{label}-bench-Release-table-{count}-r{round_number}.log'
            source = (dest.parent / 'windows-tree' / filename) if label == 'before' else out / filename
            if label == 'before':
                baseline_records.append(dict(path='../windows-tree/' + filename, sha256=digest(source.read_bytes())))
            lines = decode(source).splitlines()
            rows = list(csv.DictReader(io.StringIO('\n'.join(line for line in lines if line and not line.startswith('#')))))
            assert len(rows) == 192, (source, len(rows))
            for operation in ('selection', 'bordered', 'page'):
                samples = [row for row in rows if row['operation'] == operation]
                assert len(samples) == 64
                prefix = f'# p95_ns,table,{count},{operation},'
                p95 = int(next(line for line in lines if line.startswith(prefix)).split(',')[-1])
                total_times = [int(row['dispatch_ns']) + int(row['sync_ns']) for row in samples]
                metrics = {}
                for key in ('dispatch_allocations', 'sync_allocations', 'comparator', 'visited_rows', 'copied_records', 'sort_passes', 'composer_rebuilds'):
                    values = [int(row[key]) for row in samples]
                    metrics[key] = dict(min=min(values), max=max(values), total=sum(values))
                if label != 'before':
                    for key in ('comparator', 'copied_records', 'sort_passes'):
                        assert metrics[key]['max'] == 0, (source, operation, key, metrics[key])
                measurements.append(dict(label=label, rows=count, operation=operation, round=round_number,
                                         samples=len(samples), mean_ns=sum(total_times)/len(samples), p95_ns=p95, metrics=metrics))

source_files = ['src/component/table_component.cpp', 'src/component/table_component.hpp', 'src/component/text_component.cpp',
                'tests/CMakeLists.txt', 'tests/state_projection_tests.cpp', 'tests/state_projection_benchmark.cpp',
                'tests/support/state_projection_fixture.hpp', 'tests/support/allocation_probe.hpp',
                'src/component/tree_component.cpp', 'src/component/tree_component.hpp', 'src/component/tree_model.cpp']
sources = [dict(path=p, sha256=digest((root/p).read_bytes())) for p in source_files]
exe_names = ['state_projection_tests', 'state_projection_benchmark', 'table_component_tests', 'table_theme_tests',
             'table_public_api_tests', 'pagination_component_tests', 'tree_component_tests', 'tree_review_tests',
             'tree_r2_tests', 'tree_select_component_tests', 'tree_select_integration_tests']
binaries = []
build_artifacts = []
for config in ('Debug', 'Release'):
    paths = [root/f'out/build/windows-msvc-headless/tests/{config}/rynui_{name}.exe' for name in exe_names]
    paths += [root/f'out/build/windows-msvc-headless/tests/portable/{config}/rynui_portable_{name}.exe'
              for name in ('focus_order', 'focus_lifecycle', 'input_component', 'text_component', 'text_component_frame', 'text_scene_service')]
    for path in paths:
        raw = path.read_bytes()
        pe = struct.unpack_from('<I', raw, 60)[0]
        optional = pe + 24
        assert raw[pe:pe+4] == b'PE\0\0' and struct.unpack_from('<H', raw, optional)[0] == 0x20b
        stack = struct.unpack_from('<Q', raw, optional+72)[0]
        assert stack == 1048576, (path, stack)
        binaries.append(dict(path=str(path.relative_to(root)), sha256=digest(raw), bytes=len(raw),
                             mtime_utc=datetime.fromtimestamp(path.stat().st_mtime, timezone.utc).isoformat(), stack_reserve=stack))
    objects = [root/f'out/build/windows-msvc-headless/src/CMakeFiles/rynui_components.dir/{config}/component/{name}.cpp.obj'
               for name in ('table_component', 'text_component')]
    objects += [root/f'out/build/windows-msvc-headless/tests/CMakeFiles/rynui_state_projection_{name}.dir/{config}/state_projection_{name}.cpp.obj'
                for name in ('tests', 'benchmark')]
    objects.append(root/f'out/build/windows-msvc-headless/src/{config}/rynui_components.lib')
    for path in objects:
        raw = path.read_bytes()
        build_artifacts.append(dict(path=str(path.relative_to(root)), sha256=digest(raw), bytes=len(raw),
                                    mtime_utc=datetime.fromtimestamp(path.stat().st_mtime, timezone.utc).isoformat()))
dump(dest/'summary.json', dict(measurements=measurements))
records.append(dict(path='summary.json', sha256=digest((dest/'summary.json').read_bytes()),
                    bytes=(dest/'summary.json').stat().st_size, normalization='generated UTF-8 LF'))
dump(dest/'index.json', dict(recorded_utc=datetime.now(timezone.utc).isoformat(),
     base_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
     platform=platform.platform(), processor=platform.processor(), preset='windows-msvc-headless', max_jobs=2,
     sources=sources, binaries=binaries, build_artifacts=build_artifacts, records=records, baseline_records=baseline_records,
     boundary='Table stage with new MSVC HEADLESS D/R builds and tests; benchmarks Release only, serial. No GUI/GPU measurement.'))
print(json.dumps(dict(records=len(records), binaries=len(binaries), samples=len(measurements)*64), ensure_ascii=False))
for count in (1000, 5000, 10000):
    for operation in ('selection', 'bordered', 'page'):
        summary = dict(rows=count, operation=operation)
        for label in ('before', 'table', 'table-page-final'):
            selected = [m for m in measurements if m['label'] == label and m['rows'] == count and m['operation'] == operation]
            summary[label] = dict(mean_ms=sum(m['mean_ns'] for m in selected)/3e6,
                                 p95_ms=[min(m['p95_ns'] for m in selected)/1e6, max(m['p95_ns'] for m in selected)/1e6],
                                 comparator=[min(m['metrics']['comparator']['min'] for m in selected), max(m['metrics']['comparator']['max'] for m in selected)])
        print(json.dumps(summary))
