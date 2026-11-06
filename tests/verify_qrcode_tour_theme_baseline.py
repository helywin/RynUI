#!/usr/bin/env python3
"""Append-only public Theme contracts; preserve every full baseline domain, including Image/Calendar/Carousel."""
import json
from pathlib import Path
import re
import subprocess
ROOT=Path(__file__).resolve().parents[1]
BASE='c7bf186d741046ddc18641c44e341fd151fd6026'
def old(path):
    return subprocess.check_output(['git','show',f'{BASE}:{Path(path).as_posix()}'],cwd=ROOT,text=True,encoding='utf-8')
def identities(text):
    return re.findall(r'^\s+(\w+),',text.split('enum class TokenIdentity')[1].split('};')[0],re.M)
def fields(text):
    return re.findall(r'^    (.+);$',text.split('struct ThemeConfig final {')[1].split('friend bool')[0],re.M)
def appended_tokens(before, after, additions, context):
    assert list(after) == list(before), (context, 'component fields')
    for key,value in before.items():
        if key in additions:
            assert len(after[key]) == len(value) + additions[key], (context, key, 'append count')
            assert after[key][:len(value)] == value, (context, key, 'old prefix')
        else:
            assert after[key] == value, (context, key)

def main():
    domains={'qrcode', 'watermark', 'borderBeam', 'affix'}
    for p in sorted((ROOT/'design-tokens/ant-design/6.6.5/golden').glob('*.json')):
        before=json.loads(old(p.relative_to(ROOT)))
        after=json.loads(p.read_text(encoding='utf-8'))
        assert list(after) == list(before) + ['qrcode', 'watermark', 'borderBeam', 'affix'], (p.name, 'domain order')
        assert set(before)|domains==set(after),(p.name,set(after)-set(before))
        for name,value in before.items():
            if name == 'skeleton':
                appended_tokens(value, after[name], {'colors':2}, (p.name,name))
            elif name == 'popover':
                appended_tokens(value, after[name], {'colors':1,'metrics':6}, (p.name,name))
            elif name!='identity':
                assert after[name]==value,(p.name,name)
        assert after['identity']!=before['identity']
    path='src/theme/theme_runtime_types.hpp'
    before=identities(old(path));after=identities((ROOT/path).read_text())
    assert after[:len(before)-1]==before[:-1]
    assert after[-1]=='count' and len(after)==len(set(after))
    additions=['qrcode_colors','qrcode_metrics','watermark','border_beam','affix']
    assert after[len(before)-1:-1]==additions
    path='include/ryn/theme.hpp'
    additions=['QRCodeThemeConfig qrcode','WatermarkThemeConfig watermark','BorderBeamThemeConfig border_beam','AffixThemeConfig affix']
    assert fields((ROOT/path).read_text())==fields(old(path))+additions
    print(f'5 complete goldens: all pre-QRCode main domains preserved; {len(domains)} appended domains; old identities and aggregate order unchanged')
if __name__=='__main__':main()
