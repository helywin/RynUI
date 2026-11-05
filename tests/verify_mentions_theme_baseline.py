#!/usr/bin/env python3
"""Protect the pre-Mentions snapshot leaves and TokenIdentity ordinals."""
from pathlib import Path
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = 'a0e7c7ed858385e6d4469c20d60719775a8b28ba'


def original(path):
    return subprocess.check_output(['git', 'show', f'{BASE}:{path}'], cwd=ROOT, text=True)


def main():
    paths = sorted((ROOT / 'design-tokens/ant-design/6.6.5/golden').glob('*.json'))
    assert len(paths) == 5
    for path in paths:
        before = json.loads(original(path.relative_to(ROOT)))
        after = json.loads(path.read_text())
        assert len(before) == 61
        assert set(before) | {'mentions'} <= set(after)
        for field, value in before.items():
            if field != 'identity':
                assert after[field] == value, (path.name, field)
        assert after['identity'] != before['identity']
    path = 'src/theme/theme_runtime_types.hpp'
    def identities(text):
        body = text.split('enum class TokenIdentity')[1].split('};')[0]
        return re.findall(r'^\s+(\w+),', body, re.M)
    before = identities(original(path))
    after = identities((ROOT / path).read_text())
    assert after[:len(before) - 1] == before[:-1]
    assert after[-1] == 'count'
    names = [f'mentions_{part}' for part in ('colors', 'metrics', 'typography', 'effects', 'order', 'input')]
    assert [name for name in after if name in names] == names
    print('Mentions baseline: 5 goldens; all 61 old top-level fields preserved except identity; old token ordinals unchanged')


if __name__ == '__main__':
    main()
