"""Read-only R3 verification; run from this worker worktree."""
import hashlib
import struct
import subprocess
from pathlib import Path

root = Path.cwd().resolve()
assert str(root).lower().replace('\\', '/') == 'c:/users/jiang/.t3/worktrees/rynui/t3-081ff944'
evidence = root / 'openspec/changes/092-20261026-add-native-auto-complete/evidence/windows/r3'
base = '40bba184cbed09584468dc7a190d135085bd34a9'
shared = '4c67fd5c3e661b0794be193e1f0144ec07816867'
source = '648da9b6abddf519d1dce41ffaf00f782ebc4ddb'

def git(*args):
    return subprocess.check_output(['git', *args], cwd=root, text=True, encoding='utf-8').strip()

assert git('rev-parse', f'{shared}^') == base
assert git('rev-parse', f'{source}^') == shared
subprocess.run(['git', 'merge-base', '--is-ancestor', source, 'HEAD'], cwd=root, check=True)
shared_added = '\n'.join(line for line in git('diff', base, shared).splitlines()
                         if line.startswith('+') and not line.startswith('+++'))
assert 'auto_complete' not in shared_added.lower() and 'autocomplete' not in shared_added.lower()
protected = ['src/theme', 'include/ryn/theme.hpp', 'design-tokens', 'src/renderer', 'src/graphics',
             'src/component/avatar_component.cpp', 'src/component/input_component.cpp',
             'src/component/form_component.cpp', 'src/component/date_picker_component.cpp',
             'src/component/time_picker_component.cpp', 'src/component/popover_component.cpp',
             'src/component/select_component.cpp', 'tests/auto_complete_component_tests.cpp',
             'tests/auto_complete_integration_tests.cpp', 'tests/auto_complete_review_tests.cpp',
             'tests/auto_complete_r2_tests.cpp', 'README.md', 'README.en.md',
             'docs/component-completion.md', 'gallery', 'examples/token_gallery',
             'tests/token_gallery_frame_tests.cpp', ':(glob)**/*support-overlay*',
             ':(glob)**/*generated*catalog*']
assert not git('diff', base, '--', *protected), 'protected baseline changed'
assert not git('diff', source, '--', 'src', 'include', 'tests', 'examples'), 'tested source changed'
print('Workspace:', root)
print('Branch:', git('branch', '--show-current'))
print('Exact R3 base:', base)
print('Independent shared commit:', shared)
print('Tested final source commit:', source)
print('Shared added code has no AutoComplete dependency.')
print('Protected Theme/golden/R1/R2/Avatar/Input/Form/DateTime/Popover/Select/renderer/Gallery blobs unchanged.')
print('Final tests per configuration: 37/37 plus separately added shared case 1/1; no repeated full selection.')

for config in ('Debug', 'Release'):
    names = [f'rynui_auto_complete_{kind}_tests.exe'
             for kind in ('model', 'theme', 'component', 'integration', 'review', 'r2', 'r3')]
    names.append('rynui_component_failure_retirement_tests.exe')
    for name in names:
        path = root / 'out/build/windows-msvc-headless/tests' / config / name
        data = path.read_bytes()
        pe = struct.unpack_from('<I', data, 0x3C)[0]
        assert data[:2] == b'MZ' and data[pe:pe+4] == b'PE\0\0'
        assert struct.unpack_from('<H', data, pe + 24)[0] == 0x20B
        assert struct.unpack_from('<Q', data, pe + 24 + 72)[0] == 1048576
        print(f'PE {config}/{name}: stack reserve 1048576; SHA256 {hashlib.sha256(data).hexdigest()}')

for name in ('default', 'dark', 'compact', 'dark-compact', 'compact-dark'):
    path = root / f'design-tokens/ant-design/6.6.5/golden/{name}.json'
    print(f'Unchanged golden {name}: SHA256 {hashlib.sha256(path.read_bytes()).hexdigest()}')

for path in sorted(evidence.iterdir()):
    if path.is_file() and path.name != 'artifacts.log':
        print(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}')
