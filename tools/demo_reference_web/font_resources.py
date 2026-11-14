"""Optional acceptance fonts, sourced only from the central native resource lock."""
from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile

from .bootstrap import ROOT, PreparationError, canonical, fetch, require, sha
from .font_metadata import font_metadata
from demo_differential.protocol import output_path

LOCK = ROOT / 'cmake/dependencies/RynUIDependencyLock.cmake'


def variables():
    return dict(re.findall(r'set\((RYNUI_[A-Z0-9_]+)\s+"([^"]+)"\)', LOCK.read_text()))


def central_fonts():
    values = variables()
    result = []
    # File order is the explicit ordered chain used by both adapters. Same-family
    # weights share a CSS alias; they are never separate fallback families.
    entries = [('NOTO_SANS', 'noto-sans', 'NotoSans-Regular.ttf', None),
               ('NOTO_SANS_CJK_SC', 'noto-sans-cjk-sc', 'NotoSansCJKsc-Regular.otf', None),
               ('NOTO_EMOJI_REGULAR', 'noto-emoji', 'NotoEmoji-Regular.ttf', 'NOTO_EMOJI_VARIABLE'),
               ('NOTO_SANS_SEMIBOLD', 'noto-sans', 'NotoSans-SemiBold.ttf', None),
               ('NOTO_SANS_CJK_SC_SEMIBOLD', 'noto-sans-cjk-sc', 'NotoSansCJKsc-SemiBold.ttf', 'NOTO_SANS_CJK_SC_VARIABLE'),
               ('NOTO_SANS_MEDIUM', 'noto-sans', 'NotoSans-Medium.ttf', None),
               ('NOTO_SANS_CJK_SC_MEDIUM', 'noto-sans-cjk-sc', 'NotoSansCJKsc-Medium.ttf', 'NOTO_SANS_CJK_SC_VARIABLE')]
    for name, identity, filename, source_name in entries:
        name = 'RYNUI_' + name
        source = 'RYNUI_' + source_name if source_name else name
        license_source = ('RYNUI_NOTO_SANS' if identity == 'noto-sans' else
                          'RYNUI_NOTO_SANS_CJK_SC' if identity == 'noto-sans-cjk-sc' else source)
        result.append({'identity': identity, 'filename': filename, 'url': values[source + '_SOURCE_URL'],
                       'sha256': values[name + ('_SHA256' if source_name else '_SOURCE_SHA256')],
                       'source_sha256': values[source + '_SOURCE_SHA256'], 'version': values[source + '_VERSION'],
                       'commit': values[source + '_COMMIT'], 'license': values[source + '_LICENSE'],
                       'license_url': values[license_source + '_LICENSE_URL'],
                       'license_sha256': values[license_source + '_LICENSE_SHA256'],
                       'face_index': int(values[name + '_FACE_INDEX']), 'weight': int(values[name + '_WEIGHT']),
                       'slant': values[name + '_SLANT'], 'postscript_name': values[name + '_POSTSCRIPT_NAME'],
                       'instance': source_name is not None})
    return result


def admit_font(expected, actual, policy):
    fields = ('identity', 'sha256', 'face_index', 'weight')
    require(set(actual) == set(fields) | {'path'}, 'font descriptor fields')
    require({key: actual[key] for key in fields} == expected, 'font descriptor mismatch')
    path = output_path(actual['path'])
    require(path.is_file() and sha(path) == actual['sha256'], 'explicit font bytes/hash unavailable: ' + actual['identity'])
    metadata = font_metadata(path, actual['face_index'], [0x1f642])
    require(metadata['weight'] == actual['weight'] and metadata['slant'] == 'upright' and not metadata['variable'],
            'actual font weight/slant/static face differs; regular bytes cannot be relabeled 500/600')
    if policy == 'pinned':
        locked = next((row for row in central_fonts() if all(row[key] == actual[key] for key in fields)), None)
        require(locked is not None, 'pinned font must use central native resource identity/hash/face/weight lock')
        require(metadata['names']['6'] == [locked['postscript_name']], 'actual PostScript face differs from central lock')
    elif policy != 'system':
        raise PreparationError('unknown font policy')
    return {'identity': actual['identity'], **metadata}


def fonttools_library(cache, mode, library=None, offline=False):
    require(mode in ('BUNDLED', 'SYSTEM'), 'font tool mode must be BUNDLED or SYSTEM')
    require((mode == 'SYSTEM') == (library is not None), 'SYSTEM requires explicit fonttools library; BUNDLED forbids system paths')
    values = variables()
    prefix = 'RYNUI_ICON_FONTTOOLS'
    archive = fetch({'url': values[prefix + '_SOURCE_URL'], 'sha256': values[prefix + '_SOURCE_SHA256']}, cache, offline)
    expected = {}
    with tarfile.open(archive) as source:
        for member in source.getmembers():
            parts = member.name.split('/')[1:]
            if member.isfile() and len(parts) >= 3 and parts[:2] == ['Lib', 'fontTools']:
                require(all(part not in ('', '.', '..') for part in parts), 'font tool archive path escape')
                expected['/'.join(parts[1:])] = source.extractfile(member).read()
    require('fontTools/__init__.py' in expected, 'font tool source missing')
    destination = Path(library).resolve() if mode == 'SYSTEM' else output_path(cache) / ('fonttools-' + values[prefix + '_SOURCE_SHA256']) / 'Lib'
    for name, content in expected.items():
        target = destination / name
        if mode == 'BUNDLED' and not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
        require(target.is_file() and target.read_bytes() == content, 'font tool source/version/hash mismatch: ' + name)
    require(not any(p.suffix in ('.pyd', '.so', '.dll') for p in (destination / 'fontTools').rglob('*')),
            'font tool compiled extensions are not part of the locked pure Python recipe')
    return destination


def prepare_fonts(output, *, weights=(400, 500, 600), emoji=True, mode='BUNDLED', fonttools_lib=None, offline=False):
    require(weights and all(type(weight) is int for weight in weights) and len(set(weights)) == len(weights) and
            set(weights) <= {400, 500, 600}, 'only distinct locked weights 400/500/600 may be prepared')
    output = output_path(output)
    require(not output.exists(), 'font output already exists')
    rows = [row for row in central_fonts() if row['weight'] in weights and (emoji or row['identity'] != 'noto-emoji')]
    cache = ROOT / 'out/demo-differential/archives'
    tool = fonttools_library(cache, mode, fonttools_lib, offline) if any(row['instance'] for row in rows) else None
    output.mkdir(parents=True)
    records = []
    derivations = []
    for row in rows:
        source = fetch({'url': row['url'], 'sha256': row['source_sha256']}, cache, offline)
        license_path = fetch({'url': row['license_url'], 'sha256': row['license_sha256']}, cache, offline)
        shutil.copyfile(license_path, output / (row['identity'] + '-OFL.txt'))
        target = output / row['filename']
        if row['instance']:
            process = subprocess.run([sys.executable, '-I', '-B', str(Path(__file__).with_name('font_instance.py')),
                                      str(tool), str(source), str(target), row['identity'], str(row['weight'])],
                                     capture_output=True, text=True, timeout=300)
            require(process.returncode == 0, 'font instance failed: ' + process.stderr)
        else:
            shutil.copyfile(source, target)
        require(sha(target) == row['sha256'], 'font instance/source result SHA256 mismatch: ' + row['identity'])
        record = {key: row[key] for key in ('identity', 'sha256', 'face_index', 'weight')} | {'path': str(target)}
        admit_font({key: record[key] for key in record if key != 'path'}, record, 'pinned')
        records.append(record)
        derivations.append(row | {'tool_mode': mode if row['instance'] else None,
                                  'tool_version': variables()['RYNUI_ICON_FONTTOOLS_VERSION'] if row['instance'] else None,
                                  'tool_license': variables()['RYNUI_ICON_FONTTOOLS_LICENSE'] if row['instance'] else None,
                                  'tool_source_sha256': variables()['RYNUI_ICON_FONTTOOLS_SOURCE_SHA256'] if row['instance'] else None,
                                  'recipe_sha256': sha(Path(__file__).with_name('font_instance.py')) if row['instance'] else None})
    (output / 'font-files.json').write_bytes(canonical(records) + b'\n')
    (output / 'font-sources.json').write_bytes(canonical(derivations) + b'\n')
    return records
