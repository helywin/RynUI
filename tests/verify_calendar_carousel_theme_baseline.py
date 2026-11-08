#!/usr/bin/env python3
"""Preserve all 64 pre-Calendar/Carousel top-level fields and token ordinals."""
from pathlib import Path
import json
import re
import subprocess
ROOT=Path(__file__).resolve().parents[1]
BASE='135ba70b74673bc6d7b34d1d83aa414fd0db3607'
def original(path): return subprocess.check_output(['git','show',f'{BASE}:{Path(path).as_posix()}'],cwd=ROOT,text=True,encoding='utf-8')
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
    paths=sorted((ROOT/'design-tokens/ant-design/6.6.5/golden').glob('*.json'))
    assert len(paths)==5
    for path in paths:
        before=json.loads(original(path.relative_to(ROOT)))
        after=json.loads(path.read_text(encoding='utf-8'))
        assert len(before)==64
        assert list(after) == list(before) + ['calendar', 'carousel', 'qrcode', 'watermark', 'borderBeam', 'affix', 'tour']
        assert len(after)==71
        for field,value in before.items():
            if field == 'identity':
                continue
            # Later authorized Card/Empty additions retain every pre-107 value.
            # Check exact append sizes and old prefixes instead of exempting domains.
            if field == 'card':
                verify_appended_tokens(value, after[field], {'colors': 1, 'metrics': 2}, (path.name, field))
            elif field == 'empty':
                verify_appended_tokens(value, after[field], {'colors': 6}, (path.name, field))
            elif field == 'skeleton':
                verify_appended_tokens(value, after[field], {'colors': 2}, (path.name, field))
            elif field == 'popover':
                verify_appended_tokens(value, after[field], {'colors': 1, 'metrics': 6}, (path.name, field))
            elif field == 'modal':
                native = dict(after[field])
                assert native.pop('maskBlurSigma') == 4, (path.name, 'native Modal sigma')
                assert native == value, (path.name, 'old Modal fields')
            else:
                assert after[field] == value, (path.name, field)
        assert after['identity']!=before['identity']
    path='src/theme/theme_runtime_types.hpp'
    def identities(text): return re.findall(r'^\s+(\w+),',text.split('enum class TokenIdentity')[1].split('};')[0],re.M)
    before=identities(original(path)); after=identities((ROOT/path).read_text())
    assert after[:len(before)-1]==before[:-1]
    assert after[-1]=='count' and len(set(after))==len(after)
    assert after[len(before)-1:len(before)+2]==['calendar_colors','calendar_metrics','calendar_effects']
    assert after[len(before)+2:len(before)+5]==['carousel_colors','carousel_metrics','carousel_effects']
    path='include/ryn/theme.hpp'
    def config_fields(text): return re.findall(r'^    (.+);$',text.split('struct ThemeConfig final {')[1].split('friend bool')[0],re.M)
    assert config_fields((ROOT/path).read_text(encoding='utf-8')) == config_fields(original(path))+['CalendarThemeConfig calendar','CarouselThemeConfig carousel','QRCodeThemeConfig qrcode','WatermarkThemeConfig watermark','BorderBeamThemeConfig border_beam','AffixThemeConfig affix','TourThemeConfig tour']
    runtime=(ROOT/'src/theme/theme_runtime.cpp').read_text()
    for name in ['Calendar.colors','Calendar.metrics','Calendar.effects','Carousel.colors','Carousel.metrics','Carousel.effects']: assert '"'+name+'"' in runtime
    print('Calendar/Carousel baseline: 5 full goldens, all 64 old fields/prefixes protected; exact Card/Empty appends; old TokenIdentity names/order unchanged')
if __name__=='__main__': main()
