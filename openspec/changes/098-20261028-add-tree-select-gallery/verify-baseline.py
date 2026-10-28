"""Verify the frozen published implementation while appending TreeSelect Gallery."""
from pathlib import Path
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]
BASE = 'c9cc6c2aab75f03739e0540b47195af1cc897f67'


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)


def old(path):
    return git('show', BASE + ':' + path).decode().replace('\r\n', '\n')


path = 'examples/token_gallery/token_gallery_definition.cpp'
before = old(path)
after = (ROOT / path).read_text(encoding='utf-8')
ids = lambda source: re.findall(r'"([^"]+)"', source.split('constexpr auto stable_test_ids =', 1)[1].split('});', 1)[0])
assert len(ids(before)) == 441
assert ids(after)[:441] == ids(before) and len(ids(after)) == len(set(ids(after)))
functions = lambda source: re.findall(r'\nvoid (add_\w+)\([^\n]*\) \{.*?\n\}', source, re.S)
names = functions(before)
assert len(names) == 41
assert functions(after) == names + ['add_tree_select_destination_samples']
for name in names + ['themed_button', 'navigation_button']:
    body = lambda source: re.search(r'\nvoid ' + name + r'\(.*?\n\}', source, re.S).group()
    assert body(before) == body(after), name
calls = lambda source: re.findall(r'^\s+(add_\w+)\(state\);', source, re.M)
assert calls(after) == calls(before) + ['add_tree_select_destination_samples']
assert 'void set_theme(const ryn::ThemeConfig& config' in after
print(f'441 old IDs and 41 sample bodies/call order retained; actual new IDs={len(ids(after))}; low-stack helpers retained')

path = 'gallery/ant-design/6.6.5/support-overlay.json'
a = json.loads(old(path))['entries']
b = json.loads((ROOT / path).read_text(encoding='utf-8'))['entries']
assert [x['identity'] for x in a] == [x['identity'] for x in b]
assert len(a) - 1 == 72
for x, y in zip(a, b):
    if x['identity'] != 'ant.component.tree-select':
        assert x == y, x['identity']
print('72 non-target support entries retained')

frozen = []
for line in git('ls-tree', '-r', BASE).decode().splitlines():
    metadata, path = line.split('\t', 1)
    if (path.startswith(('src/', 'include/', 'cmake/', 'design-tokens/', 'renderer/'))
            or 'golden' in path.lower() or Path(path).name.startswith('CMake')
            or path.endswith(('.cmake', '.cmake.in')) or path in {'README.md', 'README.en.md'}):
        frozen.append((path, metadata.split()[2]))
actual = subprocess.check_output(['git', 'hash-object', '--stdin-paths'], cwd=ROOT,
                                input=('\n'.join(path for path, _ in frozen) + '\n').encode()).decode().splitlines()
assert len(actual) == len(frozen)
for (path, digest), current in zip(frozen, actual):
    assert current == digest, path
print(f'{len(frozen)} frozen Core/API/Theme/golden/renderer/RC/CMake/README blobs retained')

allowed = {'examples/token_gallery/token_gallery_definition.cpp', 'examples/token_gallery/gallery_document_viewport.cpp',
           'examples/token_gallery/generated_ant_design_reference_catalog.inc', 'tests/token_gallery_frame_tests.cpp',
           'gallery/ant-design/6.6.5/support-overlay.json', 'docs/tree-select.md', 'docs/component-completion.md'}
changed = git('diff', '--name-only', BASE).decode().splitlines() + git('ls-files', '--others', '--exclude-standard').decode().splitlines()
assert all(path in allowed or path.startswith('openspec/changes/098-20261028-add-tree-select-gallery/') for path in changed)
print('All current changes are within the Gallery phase scope')
