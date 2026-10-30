import hashlib
import json
import shutil
import struct
import subprocess
import sys
from pathlib import Path

root = Path('D:/code/RynUI')
label = sys.argv[1]
dest = root / 'openspec/changes/103-20261030-measure-reactive-effect-scaling/evidence' / ('windows-' + label)
if dest.exists():
    raise SystemExit(f'Refusing to overwrite frozen evidence: {dest}')
dest.mkdir(parents=True)

def record(path):
    raw = path.read_bytes()
    result = {'path': path.relative_to(root).as_posix(), 'size': len(raw), 'sha256': hashlib.sha256(raw).hexdigest(), 'mtime_ns': path.stat().st_mtime_ns}
    if path.suffix == '.exe':
        pe = struct.unpack_from('<I', raw, 0x3c)[0]
        assert raw[pe:pe + 4] == b'PE\0\0'
        optional = pe + 24
        assert struct.unpack_from('<H', raw, optional)[0] == 0x20b
        result['stack_reserve'] = struct.unpack_from('<Q', raw, optional + 72)[0]
        assert result['stack_reserve'] == 1048576, path
    return result

def freeze_file(source, target):
    raw = source.read_bytes()
    # Keep binary/UTF-16 logs exact; normalize UTF-8 text snapshots to Git's LF form.
    if b'\0' not in raw:
        raw = raw.replace(b'\r\n', b'\n')
    target.write_bytes(raw)

patterns = [f'103-{label}-*', '103-msvc.ps1', '103-summarize.py', '103-environment.json', '103-summary.json']
if label == 'after':
    patterns += ['103-resume-*', '103-before-effect-*', '103-before-effect-fixed-*', '103-before-fix-*',
                 '103-comparison.json', '103-compare.py', '103-freeze.py']
if label == 'baseline':
    patterns += ['103-sizing-*', '103-initial-target-error-*', '103-source-check-*', '103-initial-signal-*', '103-initial-subscription-*']
selected = sorted({p for pattern in patterns for p in (root / 'out').glob(pattern) if p.is_file()})
records = []
for path in selected:
    target = dest / path.name
    freeze_file(path, target)
    records.append({'source': record(path), 'frozen': record(target)})
source_paths = ['include/ryn/detail/reactive_runtime.hpp', 'src/reactive/reactive.cpp', 'src/graphics/rounded_effect.cpp', 'src/graphics/rounded_effect.hpp', 'src/renderer/common/rounded_effect_gpu_resources.cpp', 'src/renderer/common/rounded_effect_gpu_resources.hpp', 'tests/CMakeLists.txt', 'tests/reactive_fanout_benchmark.cpp', 'tests/effect_range_benchmark.cpp', 'tests/support/scaling_benchmark.hpp', 'tests/support/allocation_probe.hpp']
if (root / 'tests/reactive_subscription_tests.cpp').exists():
    source_paths.append('tests/reactive_subscription_tests.cpp')
if label == 'after':
    source_paths.append('tests/effect_dirty_retry_tests.cpp')
sources = [record(root / name) for name in source_paths]
for name in source_paths:
    target = dest / 'source' / (name + '.txt')
    target.parent.mkdir(parents=True, exist_ok=True)
    freeze_file(root / name, target)
    records.append({'source': record(root / name), 'frozen': record(target)})
binaries = []
for config in ['Debug', 'Release']:
    for name in ['reactive_fanout_benchmark', 'effect_range_benchmark', 'signal_tests', 'memo_tests', 'batch_tests', 'scope_effect_tests', 'reactive_subscription_tests'] + (['effect_dirty_retry_tests'] if label == 'after' else []):
        path = root / 'out/build/windows-msvc-headless/tests' / config / f'rynui_{name}.exe'
        if path.exists():
            binaries.append(record(path))
    for name in ['reactive_source_lifecycle', 'rounded_effect_math', 'rounded_effect_store', 'rounded_effect_scene', 'rounded_effect_gpu', 'rounded_effect_gpu_resources', 'scene_backend']:
        path = root / 'out/build/windows-msvc-headless/tests/portable' / config / f'rynui_portable_{name}.exe'
        binaries.append(record(path))
index = {'label': label, 'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(), 'records': records, 'sources': sources, 'binaries': binaries, 'limits': ['MSVC HEADLESS/RECORDING only', 'subscriber scans/sort comparisons/planning time unobserved', 'C++ allocation requests only; not peak memory', 'all samples retained; no native/GPU/present acceptance']}
(dest / 'index.json').write_bytes((json.dumps(index, ensure_ascii=False, indent=2) + '\n').encode('utf-8'))
print(f'Frozen {len(records)} records, {len(sources)} sources, {len(binaries)} PE fingerprints in {dest}')
