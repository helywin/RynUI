from pathlib import Path
import hashlib, json, subprocess

root = Path('D:/code/RynUI')
baseline = 'f595211e753f8d117dd72f1390183515a9577b31'
protected = ['include', 'src', 'tests', 'design-tokens', 'scripts', 'cmake', 'examples', 'CMakeLists.txt', 'CMakePresets.json']
allowed = {'src/component/table_component.cpp', 'src/component/table_component.hpp', 'src/component/text_component.cpp',
           'tests/CMakeLists.txt', 'tests/state_projection_tests.cpp'}
changed = set(subprocess.check_output(['git', 'diff', '--name-only', baseline, '--', *protected], cwd=root, text=True).splitlines())
assert changed == allowed, changed
entries = subprocess.check_output(['git', 'ls-tree', '-r', baseline, '--', *protected], cwd=root, text=True).splitlines()
frozen = []
for entry in entries:
    identity, name = entry.split('\t', 1)
    if name not in allowed:
        frozen.append({'path': name, 'git_blob': identity.split()[2]})
report = {'baseline': baseline, 'allowed': sorted(allowed), 'frozen': frozen,
          'text_guard': hashlib.sha256((root/'src/component/text_component.cpp').read_bytes()).hexdigest()}
(root/'out/102-table-preservation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print(f'Preserved {len(frozen)} tracked source/build/test/asset files; exactly five allowed edits.')
