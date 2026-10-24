import json
import subprocess
from pathlib import Path

BASE = '4efa310a256019192e01f0738ee226d16ad44396'
ROOT = 'openspec/changes/073-20261024-add-native-empty-and-result/'


def original(path):
    return subprocess.check_output(['git', 'show', f'{BASE}:{path}'])


def text(path):
    return Path(path).read_text(encoding='utf-8')


source = 'src/theme/theme_snapshot.cpp'
expected = original(source).decode('utf-8')
for anchor, guard in [
    ('        fixed_length(o.font_size, token.metrics.font_size, "Empty font_size must be positive", true);',
     '\n    if (token.metrics.font_size < 0.5F) {\n'
     '        throw std::invalid_argument("Empty font_size must round to at least one logical pixel (>= 0.5)");\n    }'),
    ('        fixed_length(o.font_size, token.metrics.font_size, "Result font_size must be positive", true);',
     '\n    if (token.metrics.title_font_size < 0.5F || token.metrics.font_size < 0.5F) {\n'
     '        throw std::invalid_argument("Result font sizes must round to at least one logical pixel (>= 0.5)");\n    }'),
]:
    assert expected.count(anchor) == 1
    expected = expected.replace(anchor, anchor + guard, 1)
assert text(source) == expected, 'Theme source changed beyond the two localized guards'

unchanged = ['src/component/text_component.cpp', 'src/font/font_runtime.cpp', 'src/theme/theme_runtime.cpp',
             'src/component/empty_result_component.cpp', 'src/component/popconfirm_component.cpp',
             'src/component/window_component_services.cpp', 'tests/empty_result_theme_tests.cpp']
for path in unchanged:
    assert text(path) == original(path).decode('utf-8'), path

goldens = sorted(Path('design-tokens/ant-design/6.6.5/golden').glob('*.json'))
assert len(goldens) == 5
for path in goldens:
    assert path.read_bytes().replace(b'\r\n', b'\n') == original(path.as_posix()).replace(b'\r\n', b'\n'), path

allowed = {'src/theme/theme_snapshot.cpp', 'tests/empty_result_component_tests.cpp', 'docs/empty-result.md',
           ROOT + 'design.md', ROOT + 'tasks.md', ROOT + 'specs/native-empty-result/spec.md'}
changed = subprocess.check_output(['git', 'diff', '--name-only', BASE], text=True).splitlines()
assert all(path in allowed or path.startswith(ROOT + 'evidence/review-r3-4efa310/') for path in changed), changed
untracked = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard'], text=True).splitlines()
assert all(path.startswith(ROOT + 'evidence/review-r3-4efa310/') for path in untracked), untracked
print(json.dumps({'baseline': BASE, 'theme_change': 'two guards, six added lines only',
                  'unchanged_sources': unchanged, 'five_goldens_unchanged_including_identity': [p.name for p in goldens],
                  'only_073_scope_changed': True}, indent=2))
