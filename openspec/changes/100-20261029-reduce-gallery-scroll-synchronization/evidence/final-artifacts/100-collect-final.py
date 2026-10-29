from pathlib import Path
import hashlib
import json
import shutil
import struct
import subprocess

root = Path('D:/code/RynUI')
stage = root / 'openspec/changes/100-20261029-reduce-gallery-scroll-synchronization/evidence/final-artifacts'
native = root / 'openspec/changes/099-20261028-replace-native-control-symbol-placeholders/evidence/native-artifacts'
for target in (stage, native):
    target.mkdir(parents=True, exist_ok=True)
    (target / '.gitattributes').write_text('* -text whitespace=cr-at-eol\n*.log -diff\n', encoding='ascii')

def fingerprint(path):
    data = path.read_bytes()
    return {'path': path.relative_to(root).as_posix(), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(), 'mtime_ns': path.stat().st_mtime_ns}

names = ['gallery-scroll-before-release.log', '100-msvc.ps1', '100-collect-final.py', '100-final-apply.json', '099-final-apply.json', '100-final-format.log', '100-final-doctor.json', '100-final-strict.log', '100-glyph-growth-diagnostic.log', '100-profile-cast-diagnostic.log', '100-final-full-build-Release.log', '100-final-full-build-Release.json']
for prefix in ('build-Debug', 'build-Release', 'test-Debug', 'test-Release', 'gallery-build-Release', 'inventory-Release', 'contracts-Release', 'common-Release', 'data-Release', 'panels-Release', 'forms-Release', 'date-picker-Release', 'time-picker-Release', 'tree-select-Release'):
    names += ['100-' + prefix + ext for ext in ('.log', '.json', '.xml') if (root / ('out/100-' + prefix + ext)).exists()]
for run in ('first-pass-diagnostic', 'profile', 'inheritance', 'spinner', 'batched', 'final'):
    names += ['100-scroll-' + run + '-release' + ext for ext in ('.log', '.json') if (root / ('out/100-scroll-' + run + '-release' + ext)).exists()]
for name in names:
    shutil.copyfile(root / 'out' / name, stage / name)

sources = ['src/component/input_component.cpp', 'tests/input_component_tests.cpp', 'src/component/button_component.cpp', 'src/component/window_component_services.hpp', 'src/renderer/common/glyph_gpu_resources.cpp', 'src/renderer/common/quad_gpu_resources.cpp', 'src/renderer/common/quad_gpu_resources.hpp', 'examples/token_gallery/token_gallery_runtime.cpp', 'tests/button_component_tests.cpp', 'tests/quad_primitive_tests.cpp', 'tests/glyph_gpu_resources_tests.cpp', 'tests/token_gallery_frame_tests.cpp']
portable = ['button_component','quad_primitive','glyph_gpu_resources','logical_scene_packing','scene_backend','button_scene_service','input_component','input_gpu','input_caret_blink','input_display','input_actions','text_scene_service','text_area','text_input_session','select_component','focus_lifecycle']
binaries = []
for config in ('Debug', 'Release'):
    build = root / 'out/build/windows-msvc-headless/tests'
    binaries += [build / 'portable' / config / ('rynui_portable_' + name + '.exe') for name in portable]
    binaries += [build / config / ('rynui_' + name + '_tests.exe') for name in ('form_component','date_picker_review','time_picker_review')]
binaries += [root / 'out/build/windows-msvc/examples/Release/rynui_token_gallery.exe', root / 'out/build/windows-msvc/tests/Release/rynui_token_gallery_frame_tests.exe', root / 'out/build/windows-msvc/tests/Release/rynui_ant_design_reference_catalog_tests.exe']
binary_records = []
for path in binaries:
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe+4] == b'PE\0\0'
    assert struct.unpack_from('<H', data, pe + 24)[0] == 0x20b
    reserve = struct.unpack_from('<Q', data, pe + 24 + 72)[0]
    assert reserve == 1048576, (path, reserve)
    binary_records.append(dict(fingerprint(path), stack_reserve=reserve))
source_records = [dict(fingerprint(root / name), git_blob_sha256=hashlib.sha256(subprocess.check_output(['git','show', ':' + name], cwd=root)).hexdigest()) for name in sources]
manifest = {'source_head_before_commit': subprocess.check_output(['git','rev-parse','HEAD'], cwd=root, text=True).strip(), 'sources': source_records, 'binaries': binary_records, 'files': [fingerprint(path) for path in sorted(stage.iterdir()) if path.name != 'index.json']}
(stage / 'index.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

images = []
for control in ('menu','table','pagination','date-picker'):
    for suffix in ('.log', '.json'):
        shutil.copyfile(root / 'out' / ('099-native-' + control + suffix), native / ('099-native-' + control + suffix))
    images.append(fingerprint(root / 'out' / ('099-native-' + control + '.bmp')))
for name in ('099-native-menu-scale15-diagnostic.log','099-native-date-focus-diagnostic.log','099-native-date-passive-focus-diagnostic.log'):
    shutil.copyfile(root / 'out' / name, native / name)
(native / 'index.json').write_text(json.dumps({'exe': fingerprint(binaries[-3]), 'images_local_only': images, 'files': [fingerprint(path) for path in sorted(native.iterdir()) if path.name != 'index.json']}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(f'Indexed {len(sources)} source files, {len(binaries)} PE files with 1 MiB stacks, {len(manifest["files"])} raw artifacts and four local BMPs')
