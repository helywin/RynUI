"""Fingerprint actual native build inputs; Git HEAD alone cannot prove a dirty build."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

AREAS = ('include', 'src', 'examples/token_gallery', 'shaders', 'cmake', 'gallery', 'design-tokens')
SUFFIXES = {'.cpp', '.c', '.h', '.hpp', '.hlsl', '.hlsli', '.inc', '.cmake', '.json', '.yaml', '.yml'}
EXPLICIT = ('CMakeLists.txt', 'CMakePresets.json', 'examples/CMakeLists.txt',
            'tools/generate_gallery_source.py',
            'tools/demo_differential/source_closure.py')


def source_manifest(root):
    root = Path(root).resolve()
    paths = {root / relative for relative in EXPLICIT}
    for area in AREAS:
        paths.update(path for path in (root / area).rglob('*') if path.is_file() and path.suffix in SUFFIXES)
    files = [{'path': path.relative_to(root).as_posix(), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
             for path in sorted(paths, key=lambda p: p.relative_to(root).as_posix())]
    encoded = json.dumps(files, sort_keys=True, separators=(',', ':')).encode()
    return {'schema_version': 1, 'source_hash': hashlib.sha256(encoded).hexdigest(), 'files': files}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output.resolve()
    if not output.is_relative_to(root / 'out'):
        parser.error('generated native manifest must remain in this checkout out/')
    manifest = source_manifest(root)
    encoded = json.dumps(manifest, ensure_ascii=True, separators=(',', ':'))
    text = 'constexpr char gallery_demo_build_manifest[] = R"rynmanifest(' + encoded + ')rynmanifest";\n'
    if not output.is_file() or output.read_text(encoding='utf-8') != text:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding='utf-8', newline='\n')


if __name__ == '__main__':
    main()
