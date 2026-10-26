"""Read-only R2 verification; run from this worker worktree."""
import hashlib
import struct
import subprocess
from pathlib import Path

root = Path.cwd().resolve()
assert str(root).lower().replace('\\', '/') == 'c:/users/jiang/.t3/worktrees/rynui/t3-081ff944'
evidence = root / 'openspec/changes/092-20261026-add-native-auto-complete/evidence/windows/r2'
base = '0c3ab536f4500a057ce81f84f7cd4d77887ea758'
source = 'd481487a9531ef9f32420a28de11fefd28501304'

def git(*args):
    return subprocess.check_output(['git', *args], cwd=root, text=True).strip()

assert git('rev-parse', f'{source}^') == base
subprocess.run(['git', 'merge-base', '--is-ancestor', base, 'HEAD'], cwd=root, check=True)
protected = ['src/theme', 'include/ryn/theme.hpp', 'design-tokens', 'src/renderer', 'src/graphics',
             'src/component/avatar_component.cpp', 'src/component/input_component.cpp',
             'src/component/form_component.cpp', 'src/component/date_picker_component.cpp',
             'src/component/time_picker_component.cpp', 'src/component/popover_component.cpp',
             'src/component/select_component.cpp', 'tests/auto_complete_component_tests.cpp',
             'tests/auto_complete_integration_tests.cpp', 'tests/auto_complete_review_tests.cpp',
             'README.md', 'README.en.md', 'docs/component-completion.md', 'gallery',
             'examples/token_gallery', 'tests/token_gallery_frame_tests.cpp',
             ':(glob)**/*support-overlay*', ':(glob)**/*generated*catalog*']
assert not git('diff', base, '--', *protected), 'protected baseline changed'
assert not git('diff', source, '--', 'src', 'include', 'tests', 'examples'), 'tested source changed'
print('Workspace:', root)
print('Branch:', git('branch', '--show-current'))
print('Tested source commit:', source)
print('Exact source parent:', base)
print('Protected Theme/golden/R1/Avatar/Input/Form/DateTime/Popover/Select/renderer/Gallery blobs unchanged.')

for config in ('Debug', 'Release'):
    for kind in ('model', 'theme', 'component', 'integration', 'review', 'r2'):
        path = root / f'out/build/windows-msvc-headless/tests/{config}/rynui_auto_complete_{kind}_tests.exe'
        data = path.read_bytes()
        pe = struct.unpack_from('<I', data, 0x3C)[0]
        assert data[:2] == b'MZ' and data[pe:pe+4] == b'PE\0\0'
        assert struct.unpack_from('<H', data, pe + 24)[0] == 0x20B
        assert struct.unpack_from('<Q', data, pe + 24 + 72)[0] == 1048576
        print(f'PE {config}/{kind}: stack reserve 1048576; SHA256 {hashlib.sha256(data).hexdigest()}')

for name in ('default', 'dark', 'compact', 'dark-compact', 'compact-dark'):
    path = root / f'design-tokens/ant-design/6.6.5/golden/{name}.json'
    print(f'Unchanged golden {name}: SHA256 {hashlib.sha256(path.read_bytes()).hexdigest()}')

for path in sorted(evidence.iterdir()):
    if path.is_file() and path.name != 'artifacts.log':
        print(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}')
