#!/usr/bin/env python3
"""Protect the pre-Mentions snapshot leaves and TokenIdentity ordinals."""
from pathlib import Path
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = 'a0e7c7ed858385e6d4469c20d60719775a8b28ba'


def original(path):
    return subprocess.check_output(['git', 'show', f'{BASE}:{Path(path).as_posix()}'], cwd=ROOT, text=True, encoding='utf-8')


def verify_appended_tokens(before, after, appended, context):
    assert list(after) == list(before), (context, 'component field order changed')
    for key, value in before.items():
        extra = appended.get(key, 0)
        if extra:
            assert len(after[key]) == len(value) + extra, (context, key, 'unexpected appended length')
            assert after[key][:len(value)] == value, (context, key, 'old values changed')
        else:
            assert after[key] == value, (context, key, 'unowned value changed')


def main():
    paths = sorted((ROOT / 'design-tokens/ant-design/6.6.5/golden').glob('*.json'))
    assert len(paths) == 5
    for path in paths:
        before = json.loads(original(path.relative_to(ROOT)))
        after = json.loads(path.read_text(encoding='utf-8'))
        assert len(before) == 61
        expected_fields = list(before)
        for anchor, additions in [('transfer', ['image', 'upload']), ('autoComplete', ['mentions'])]:
            position = expected_fields.index(anchor) + 1
            expected_fields[position:position] = additions
        expected_fields += ['calendar', 'carousel', 'qrcode', 'watermark', 'borderBeam', 'affix', 'tour']
        assert list(after) == expected_fields
        for field, value in before.items():
            if field == 'identity':
                continue
            if field == 'card':
                verify_appended_tokens(value, after[field], {'colors': 1, 'metrics': 2}, (path.name, field))
            elif field == 'empty':
                verify_appended_tokens(value, after[field], {'colors': 6}, (path.name, field))
            elif field == 'skeleton':
                verify_appended_tokens(value, after[field], {'colors': 2}, (path.name, field))
            elif field == 'popover':
                verify_appended_tokens(value, after[field], {'colors': 1, 'metrics': 6}, (path.name, field))
            else:
                assert after[field] == value, (path.name, field)
        assert after['identity'] != before['identity']
    path = 'src/theme/theme_runtime_types.hpp'
    def identities(text):
        body = text.split('enum class TokenIdentity')[1].split('};')[0]
        return re.findall(r'^\s+(\w+),', body, re.M)
    before = identities(original(path))
    after = identities((ROOT / path).read_text(encoding='utf-8'))
    assert after[:len(before) - 1] == before[:-1]
    assert after[-1] == 'count'
    names = [f'mentions_{part}' for part in ('colors', 'metrics', 'typography', 'effects', 'order', 'input')]
    assert [name for name in after if name in names] == names
    print('Mentions baseline: 5 goldens; all 61 old top-level fields preserved except identity; old token ordinals unchanged')


if __name__ == '__main__':
    main()
