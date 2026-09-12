#include "component/input_display.hpp"
#include "text/text_engine.hpp"
#include "support/allocation_probe.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::input;
namespace probe = ryn_test::allocation;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
void mapping() {
    TextEditorStore editors; const auto id = editors.create("A\xE4\xB8\xAD\xE6\x96\x87Z");
    auto& editor = editors.require(id); detail::InputDisplayState display;
    const String hint{u8"hint"};
    static_cast<void>(display.update(editor, hint.view()));
    require(bool(editor.select({1, 7})), "replacement selection failed");
    require(bool(editor.update_composition({String{u8"ni"}, {1, 1}, {id, 1}})), "preedit failed");
    auto changed = display.update(editor, hint.view()); auto state = display.snapshot();
    require(changed.text_changed && state.text == "AniZ" && state.composing && !state.placeholder
        && state.selection == TextSelection{2, 3} && state.composition == TextSelection{1, 3}
        && state.caret == 3, "composition replacement display incorrect");
    require(display.committed_to_display(0) == 0 && display.committed_to_display(7) == 3
        && display.committed_to_display(4) == 1 && display.committed_to_display(4, true) == 3
        && display.display_to_committed(2) == 1 && display.display_to_committed(2, true) == 7
        && display.display_to_committed(4) == 8, "replacement byte mapping incorrect");
    const auto revision = display.revision();
    require(bool(editor.update_composition({String{u8"ni"}, {0, 0}, {id, 1}})), "preedit caret update failed");
    changed = display.update(editor, hint.view());
    require(!changed.text_changed && changed.geometry_changed && display.revision() == revision
        && display.snapshot().caret == 1 && editor.revision() == 0, "range update changed committed text/display revision");
    editor.cancel_composition(); static_cast<void>(display.update(editor, hint.view()));
    require(display.snapshot().text == editor.value() && !display.snapshot().composing, "cancel retained preedit");
    require(bool(editor.set_value("")), "empty value failed"); static_cast<void>(display.update(editor, hint.view()));
    require(display.snapshot().placeholder && display.snapshot().text == "hint" && display.snapshot().caret == 0, "placeholder incorrect");
    require(bool(editor.update_composition({String{u8"a\u0301"}, {1, 0}, {id, 2}})), "combining preedit failed");
    static_cast<void>(display.update(editor, hint.view()));
    require(!display.snapshot().placeholder && display.snapshot().caret == 3
        && display.snapshot().selection.empty(), "preedit caret split grapheme");
    require(display.committed_to_display(0) == 0 && display.committed_to_display(0, true) == 3,
        "collapsed replacement affinity incorrect");
    require(bool(editor.update_composition({String{u8"a\u0301"}, {0, 0, false}, {id, 2}})), "unknown preedit range failed");
    static_cast<void>(display.update(editor, hint.view())); require(display.snapshot().caret == 3, "unknown range did not use end");
}
text::TextCaretMap make_map(const detail::InputDisplayState& display) {
    text::ShapedText shaped; const auto source = display.snapshot().text;
    shaped.normalized_size_bytes = source.size();
    Utf8ScalarIterator scalars{source};
    while(const auto scalar = scalars.next()) {
        text::ShapedGlyph glyph; glyph.cluster = scalar->byte_begin; glyph.advance_x = 10;
        shaped.glyphs.push_back(glyph);
    }
    TextBoundaryMap boundaries; require(boundaries.assign(source), "display boundaries failed");
    text::TextCaretMap map; require(map.assign(shaped, boundaries.grapheme_bytes(), display.revision(), 16), "display caret map failed");
    return map;
}
void scrolling() {
    TextEditorStore editors; const auto id = editors.create("abcdefghij"); auto& editor = editors.require(id);
    detail::InputDisplayState display; const String hint;
    require(bool(editor.move(TextCaretMove::end)), "End failed"); static_cast<void>(display.update(editor, hint.view()));
    auto map = make_map(display);
    require(display.scroll_for_caret(map, map.revision(), 30, 0).value() == 71, "End caret not fully visible");
    require(display.scroll_for_caret(map, map.revision(), 120, 71).value() == 0, "wide viewport did not clamp scroll");
    require(!display.scroll_for_caret(map, map.revision() + 1, 30, 0), "stale map used for scrolling");
    require(bool(editor.move(TextCaretMove::home)), "Home failed"); static_cast<void>(display.update(editor, hint.view()));
    require(display.scroll_for_caret(map, map.revision(), 30, 71).value() == 0, "Home did not scroll to start");
    require(bool(editor.set_value("a")), "short value failed"); static_cast<void>(display.update(editor, hint.view())); map = make_map(display);
    require(display.scroll_for_caret(map, map.revision(), 30, 71).value() == 0, "shorter value kept stale offset");
    require(display.scroll_for_caret(map, map.revision(), 0, 71).value() == 0, "zero viewport has unstable scroll");
}
void allocation_paths() {
    TextEditorStore editors; const auto id = editors.create("prefix"); auto& editor = editors.require(id);
    const String hint; const std::string large(1024, 'x');
    std::size_t failures{};
    for(std::size_t fail = 0; fail < 64; ++fail) {
        require(bool(editor.set_value("prefix")), "fault initial value failed");
        detail::InputDisplayState display; static_cast<void>(display.update(editor, hint.view()));
        const auto revision = display.revision(); require(bool(editor.set_value(large)), "fault larger value failed");
        probe::begin(fail); bool threw{};
        try { static_cast<void>(display.update(editor, hint.view())); } catch(const std::bad_alloc&) { threw = true; }
        static_cast<void>(probe::end());
        if(!threw) break;
        ++failures; require(display.snapshot().text == "prefix" && display.revision() == revision,
            "allocation failure published partial display");
    }
    require(failures > 0, "display fault injection missed allocations");
    require(bool(editor.set_value("")), "benchmark value failed"); editor.reserve(128);
    detail::InputDisplayState display; display.reserve(128);
    const std::array events{CompositionChanged{String{u8"ni"}, {0, 0}, {id, 1}},
        CompositionChanged{String{u8"ni"}, {2, 0}, {id, 1}}};
    for(const auto& event : events) { require(bool(editor.update_composition(event)), "warmup preedit failed"); static_cast<void>(display.update(editor, hint.view())); }
    const auto revision = display.revision();
    probe::begin(); bool changed{};
    for(std::size_t i = 0; i < 20000; ++i) {
        if(!editor.update_composition(events[i % 2])) changed = true;
        if(display.update(editor, hint.view()).text_changed) changed = true;
    }
    const auto allocations = probe::end();
    require(allocations == 0 && !changed && display.revision() == revision, "composition range hot path changed text or allocated");
    std::cout << "InputDisplay composition-range cycles=20000 allocations=" << allocations << " atomic_failures=" << failures << '\n';
}
}
int main() {
    try { mapping(); scrolling(); allocation_paths(); std::cout << "Composition display and caret scrolling passed\n"; }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
