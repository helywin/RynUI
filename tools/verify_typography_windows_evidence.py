"""Verify native Windows evidence; headless results cannot satisfy this contract."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct


def validate(data, directory, check_files=True):
    assert data['scope'] == 'windows-native' and data['status'] == 'passed'
    assert data['preset'] == 'windows-msvc' and data['toolchain'] == 'MSVC x64'
    assert data['generator'] == 'Ninja Multi-Config'
    assert data['clean_configure_exit_code'] == 0
    assert data['build_exit_codes'] == {'Debug': 0, 'Release': 0}
    runs = {run['id']: run for run in data['runs']}
    assert len(runs) == len(data['runs'])
    for scale in ('1.0', '1.25', '1.5', '2.0'):
        assert float(runs['scale-' + scale]['fields']['render_scale']) == float(scale)
    assert runs['copy-only']['fields']['copy_only'] == 'true'
    assert float(runs['system-scale']['fields']['render_scale']) == float(
        runs['system-scale']['fields']['system_display_scale'])
    assert runs['dark-1.0']['fields']['theme'] == 'dark'
    assert runs['dark-2.0']['fields']['theme'] == 'dark'
    assert runs['release']['configuration'] == 'Release'
    for run in runs.values():
        fields = run['fields']
        assert run['exit_code'] == 0 and fields['typography_acceptance'] == 'passed'
        assert fields['gpu_driver'] == 'direct3d12' and fields['shader_format'] == 'DXIL'
        assert all(fields[name] == 'passed' for name in ('clipboard', 'ellipsis', 'link', 'divider'))
        assert fields['edit'] == ('not-applicable' if fields['copy_only'] == 'true' else 'passed')
        assert all(int(fields[name]) > 0 for name in ('submits', 'quad_draws', 'glyph_draws', 'effect_draws'))
        fonts = run['fonts']
        regular = fonts['regular']
        for name in ('strong', 'italic'):
            face = fonts[name]
            assert (face['path'], face['face_index']) != (regular['path'], regular['face_index'])
        assert int(fonts['strong']['weight']) >= 500 and fonts['italic']['italic'] == '1'
        expected = {'headings', 'semantics', 'copied', 'expanded', 'multiline-expanded', 'dividers', 'link-focus'}
        if fields['copy_only'] == 'false':
            expected |= {'editing', 'committed'}
        assert {Path(item['path']).stem for item in run['screenshots']} == expected
        if check_files:
            for item in run['screenshots']:
                path = (directory / item['path']).resolve()
                assert path.is_relative_to(directory.resolve())
                blob = path.read_bytes()
                assert blob[:8] == b'\x89PNG\r\n\x1a\n'
                assert struct.unpack('>II', blob[16:24]) == (1280, 1000)
                assert hashlib.sha256(blob).hexdigest() == item['sha256']


def self_test(data, directory):
    for mutation in ('scope', 'driver', 'scale', 'copy', 'font', 'screenshots'):
        broken = copy.deepcopy(data)
        runs = {run['id']: run for run in broken['runs']}
        if mutation == 'scope':
            broken['scope'] = 'platform-generic'
        elif mutation == 'driver':
            broken['runs'][0]['fields']['gpu_driver'] = 'headless'
        elif mutation == 'scale':
            runs['scale-2.0']['fields']['render_scale'] = '1'
        elif mutation == 'copy':
            runs['copy-only']['fields']['copy_only'] = 'false'
        elif mutation == 'font':
            broken['runs'][0]['fonts']['strong'] = broken['runs'][0]['fonts']['regular']
        else:
            broken['runs'][0]['screenshots'] = []
        try:
            validate(broken, directory, False)
        except AssertionError:
            continue
        raise AssertionError('invalid native evidence accepted: ' + mutation)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--evidence', required=True, type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    evidence = json.loads(args.evidence.read_text(encoding='utf-8'))
    validate(evidence, args.evidence.parent)
    if args.self_test:
        self_test(evidence, args.evidence.parent)
    print('Windows Typography/Divider native evidence passed')
