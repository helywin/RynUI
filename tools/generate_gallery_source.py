#!/usr/bin/env python3
"""Build offline Gallery source excerpts from the functions that actually compile."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path


# Braces inside comments, ordinary strings, characters and C++ raw literals do not
# delimit a function. Keep source spelling/newlines in the resulting excerpt.
LEXEMES = re.compile(
    r'(?P<raw>(?:u8|u|U|L)?R"(?P<delimiter>[^\s()\\]{0,16})\(.*?\)(?P=delimiter)")'
    r'|(?P<comment>//[^\n]*|/\*.*?\*/)'
    r'|(?P<string>(?:u8|u|U|L)?"(?:\\.|[^"\\])*")'
    r"|(?P<char>(?:u8|u|U|L)?'(?:\\.|[^'\\])*')"
    r'|(?P<brace>[{}])', re.DOTALL)


@dataclass(frozen=True)
class SampleSource:
    identity: str
    function: str
    header: str
    line: int
    props: str
    methods: str
    slots: str
    theme_path: str
    token_names: str
    code: str


def block_end(text: str, start: int) -> int:
    depth = 0
    for token in LEXEMES.finditer(text, start):
        if token.lastgroup != 'brace':
            continue
        depth += 1 if token.group() == '{' else -1
        if depth == 0:
            return token.end()
    raise ValueError('unclosed C++ source block')


def function_source(text: str, name: str) -> tuple[str, int]:
    match = re.search(r'^void\s+' + re.escape(name) + r'\s*\([^;{}]*\)\s*\{', text, re.MULTILINE)
    if not match:
        raise ValueError(f'registered source function is absent: {name}')
    opening = text.index('{', match.start(), match.end())
    return text[match.start():block_end(text, opening)], text.count('\n', 0, match.start()) + 1


def excerpt(text: str, root: str) -> tuple[str, int]:
    emitted: set[str] = set()
    parts: list[str] = []

    def visit(name: str) -> None:
        if name in emitted:
            return
        emitted.add(name)
        body, line = function_source(text, name)
        code = LEXEMES.sub(lambda token: token.group() if token.lastgroup == 'brace' else ' ', body)
        for dependency in dict.fromkeys(re.findall(r'\b(add_\w+)\s*\(', code)):
            if dependency != name:
                visit(dependency)
        parts.append(f'// token_gallery_definition.cpp:{line}\n{body}')

    visit(root)
    return '\n\n'.join(parts), function_source(text, root)[1]


def struct_source(text: str, name: str, seen: frozenset[str] = frozenset()) -> str:
    if name in seen:
        raise ValueError(f'cyclic public type alias: {name}')
    match = re.search(r'\bstruct\s+' + re.escape(name) + r'\s+final\s*\{', text)
    if not match:
        alias = re.search(r'\busing\s+' + re.escape(name) + r'\s*=\s*(\w+)\s*;', text)
        return struct_source(text, alias[1], seen | {name}) if alias else ''
    opening = text.index('{', match.start(), match.end())
    return text[opening + 1:block_end(text, opening) - 1]


def api_header(root: Path, relative: str) -> str:
    path = root / relative
    if not path.is_file() or not path.resolve().is_relative_to((root / 'include/ryn').resolve()):
        raise ValueError(f'public header is absent or outside include/ryn: {relative}')
    text = path.read_text(encoding='utf-8')
    if relative == 'include/ryn/form.hpp':
        # Form's public typed family is split into focused headers, all re-exported by form.hpp.
        for sibling in sorted(path.parent.glob('form_*.hpp')):
            text += '\n' + sibling.read_text(encoding='utf-8')
    # message.hpp/notification.hpp are honest forwarding headers. Do not collect
    # unrelated Props from every transitive dependency of a normal component.
    if not re.search(r'\b(?:class|struct)\s+\w+Props\b', text):
        forwarded = re.findall(r'^#include <(ryn/[^>]+)>', text, re.MULTILINE)
        if len(forwarded) == 1:
            text += '\n' + api_header(root, 'include/' + forwarded[0])
    return text


def generate(root: Path) -> list[SampleSource]:
    source = (root / 'examples/token_gallery/token_gallery_definition.cpp').read_text(encoding='utf-8')
    registry = source[source.index('constexpr auto sample_factories'):source.index('constexpr auto sample_descriptors')]
    declarations = re.findall(
        r'\{\{"(gallery\.sample\.[^"]+)"(.*?)"(add_[^"]+)",\s*"(include/ryn/[^"]+)"',
        registry, re.DOTALL)
    if not declarations:
        raise ValueError('no registered sample descriptors found')
    public = '\n'.join(p.read_text(encoding='utf-8') for p in sorted((root / 'include/ryn').glob('*.hpp')))
    config = struct_source(public, 'ThemeConfig')
    config_types = {field: type_name for type_name, field in re.findall(r'\b(\w+ThemeConfig)\s+(\w+)\s*;', config)}
    result: list[SampleSource] = []
    identities: set[str] = set()
    for identity, fields, function, header in declarations:
        if identity in identities:
            raise ValueError(f'duplicate sample: {identity}')
        identities.add(identity)
        code, line = excerpt(source, function)
        api = api_header(root, header)
        props = list(dict.fromkeys(re.findall(r'\bclass\s+(\w+Props)\b', api)))
        slots = list(dict.fromkeys(re.findall(r'\busing\s+(\w+)\s*=\s*(?:ryn::)?SlotContent<', api)))
        methods = list(dict.fromkeys(re.findall(r'\b(?:\w+Props(?:Base)?(?:<[^>]+>)?|Derived)\s*&\s*(\w+)\s*\(', api)))
        component = re.search(r'"ant\.component\.([^"]+)"', fields)
        key = component[1].replace('-', '_') if component else ''
        key = {'switch': 'switch_', 'icon': 'text'}.get(key, key)
        theme_path = ''
        token_names = ''
        if key in config_types:
            theme_path = 'ThemeConfig.' + key + '.tokens'
            body = struct_source(public, config_types[key])
            override = re.search(r'\b(\w+)\s+tokens\s*;', body)
            if override:
                body = struct_source(public, override[1])
                names = re.findall(r'^\s*(?:std::optional<[^;\n]+>|\w+)\s+(\w+)\s*;', body, re.MULTILINE)
                token_names = ' · '.join(dict.fromkeys(names))
        elif not component:
            theme_path = 'ThemeConfig.seed / ThemeConfig.alias'
        if not props and not slots and header != 'include/ryn/design_token.hpp':
            raise ValueError(f'no real public Props or slots in {header}')
        result.append(SampleSource(identity, function, header, line, ' · '.join(props),
                                   ' · '.join(methods[:14]), ' · '.join(slots), theme_path, token_names, code))
    return result


def literal(value: str) -> str:
    delimiter = 'ryn_' + hashlib.sha256(value.encode()).hexdigest()[:10]
    return f'R"{delimiter}({value}){delimiter}"'


def render(records: list[SampleSource]) -> str:
    lines = ['// Generated from compiled Gallery functions and public headers. Do not edit.',
             'constexpr auto sample_sources = std::to_array<GallerySampleSource>({']
    for row in records:
        values = [row.identity, 'examples/token_gallery/token_gallery_definition.cpp', row.function,
                  row.header, row.props, row.methods, row.slots, row.theme_path, row.token_names, row.code]
        lines.append('    {' + ', '.join(literal(value) for value in values) + ', ' + str(row.line) + '},')
    lines.append('});\n')
    return '\n'.join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output.resolve()
    if not output.is_relative_to(root / 'out'):
        parser.error('generated source excerpts must stay under out/')
    records = generate(root)
    data = render(records)
    if args.check:
        if not output.is_file() or output.read_text(encoding='utf-8') != data:
            raise SystemExit('generated Gallery source metadata is stale')
    else:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(data, encoding='utf-8', newline='\n')
    print(json.dumps({'samples': len(records), 'output': str(output)}, ensure_ascii=False))


if __name__ == '__main__':
    main()
