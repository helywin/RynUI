#!/usr/bin/env python3
"""Check that offline excerpts are real, bounded and backed by the current DSL."""
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location('gallery_source', ROOT / 'tools/generate_gallery_source.py')
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def check(value: bool, message: str) -> None:
    if not value:
        raise AssertionError(message)


def main() -> None:
    source = r'''void add_example() {
    // } add_fake();
    Text(u8"braces { } and quote \\\";");
    Text(R"cpp( } " add_missing(); { )cpp");
    auto cb = [] { return '}'; };
    /* } add_comment(); */
}
void add_next() {}'''
    body, line = MODULE.function_source(source, 'add_example')
    check(line == 1 and body.endswith('}') and 'add_next' not in body, 'C++ lexical boundary is wrong')
    code, _ = MODULE.excerpt(source, 'add_example')
    check(code.endswith(body), 'source spelling changed or a string/comment became a dependency')
    try:
        MODULE.function_source('void add_bad() {', 'add_bad')
    except ValueError:
        pass
    else:
        raise AssertionError('truncated source was accepted')
    records = MODULE.generate(ROOT)
    check(len(records) == 69 and len({r.identity for r in records}) == len(records), 'sample/source inventory drifted')
    for row in records:
        check((ROOT / row.header).is_file() and row.line > 0 and row.function in row.code,
              'missing source or public header')
        check(row.code.count('// token_gallery_definition.cpp:') >= 1, 'excerpt lost its source location')
        if row.theme_path.startswith('ThemeConfig.') and row.theme_path.endswith('.tokens'):
            check(bool(row.token_names), f'component token mapping is empty: {row.identity}')
    by_id = {r.identity: r for r in records}
    check('InputProps' in by_id['gallery.sample.input.basics'].props and
          'InputPrefix' in by_id['gallery.sample.input.basics'].slots and
          'defaultValue' in by_id['gallery.sample.input.basics'].methods, 'Input API is fabricated or incomplete')
    check('MessageProps' in by_id['gallery.sample.message.workflow'].props and
          by_id['gallery.sample.message.workflow'].theme_path == 'ThemeConfig.message.tokens',
          'forwarding header or shared NoticeThemeConfig mapping failed')
    check(not by_id['gallery.sample.flex.layouts'].theme_path, 'invented a component token for Flex')
    check('RowProps' in by_id['gallery.sample.grid.layouts'].props and
          'ColProps' in by_id['gallery.sample.grid.layouts'].props and
          'ColContent' in by_id['gallery.sample.grid.layouts'].slots,
          'Grid source omitted its real typed Row/Col API')
    check('MasonryProps' in by_id['gallery.sample.masonry.layouts'].props and
          'MasonryContent' in by_id['gallery.sample.masonry.layouts'].slots and
          'onLayoutChange' in by_id['gallery.sample.masonry.layouts'].methods,
          'Masonry source omitted its real typed keyed layout API')
    check('add_bidi_input(' in by_id['gallery.sample.input.bidi'].code, 'wrapper excerpt omitted its actual sample')
    check(MODULE.render(records) == MODULE.render(MODULE.generate(ROOT)), 'generation is nondeterministic')
    print(f'Gallery source/API/token contracts: {len(records)} samples passed')


if __name__ == '__main__':
    main()
