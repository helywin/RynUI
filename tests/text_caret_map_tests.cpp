#include "text/text_caret_map.hpp"
#include "text/text_engine.hpp"
#include "input/text_boundary.hpp"
#include "support/allocation_probe.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::text;
namespace allocation_probe = ryn_test::allocation;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
void real_fonts() {
    auto runtime = font::FontRuntime::create(); require(bool(runtime), "font runtime failed");
    const auto latin = runtime.runtime->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 18);
    const auto cjk = runtime.runtime->load_font_file(RYNUI_VALIDATION_CJK_FONT, 0, 18);
    require(bool(latin) && bool(cjk), "caret fonts unavailable");
    const std::array chain{latin.font, cjk.font};
    TextEngine engine{*runtime.runtime};
    TextCaretMap map;
    std::uint64_t revision{};
    for(const auto source : {String{}, String{u8"ffi"}, String{u8"a\u0301b"}, String{u8"中文 abc"},
        String{u8"\U0001F469\u200D\U0001F4BB"}, String{u8"\U0010FFFF"}, String{u8"a\u200Bb"}}) {
        const auto shaped = engine.shape(source.view(), chain); require(bool(shaped), "caret shape failed");
        const auto measured = engine.measure(shaped.text, {28, std::numeric_limits<float>::infinity()});
        require(bool(measured), "caret measure failed");
        require(engine.map_carets(shaped.text, source.view(), ++revision, measured.measurement.first_baseline, map), "caret map failed");
        input::TextBoundaryMap boundaries; require(boundaries.assign(source.bytes()), "Unicode map failed");
        require(map.stops().size() == boundaries.grapheme_bytes().size(), "glyph clusters replaced legal graphemes");
        float previous = -1;
        for(std::size_t index = 0; index < map.stops().size(); ++index) {
            const auto stop = map.stops()[index];
            require(stop.byte == boundaries.grapheme_bytes()[index] && stop.x >= previous
                && stop.baseline == measured.measurement.first_baseline, "invalid caret geometry");
            require(stop.glyph_begin <= shaped.text.glyphs.size()
                && stop.glyph_count <= shaped.text.glyphs.size() - stop.glyph_begin, "caret glyph range escaped shape");
            require(map.at(stop.byte, revision) == stop, "exact legal caret not found"); previous = stop.x;
        }
        require(std::abs(map.stops().back().x - measured.measurement.width) < 0.001F, "caret end differs from shaped advance");
        require(!map.nearest(0, revision + 1) && !map.at(0, revision + 1), "stale shape revision accepted");
        if(source == String{u8"ffi"}) {
            require(shaped.text.glyphs.size() < 3 && map.stops().size() == 4, "validation font did not exercise ligature");
            const auto width = map.stops().back().x;
            require(std::abs(map.stops()[1].x - width / 3) < 0.001F
                && std::abs(map.stops()[2].x - width * 2 / 3) < 0.001F, "ligature fallback not equally distributed");
        }
        if(source == String{u8"a\u0301b"}) require(!map.at(1, revision) && !map.at(2, revision), "combining grapheme split");
    }
    const auto source = String{u8"abc"}; const auto shaped = engine.shape(source.view(), chain);
    require(!engine.map_carets(shaped.text, String{u8"xyz"}.view(), revision + 1, 18, map)
        && map.revision() == revision, "unrelated same-length source accepted");
}
ShapedText synthetic(std::size_t count, float advance = 10) {
    ShapedText text; text.normalized_size_bytes = count;
    for(std::size_t i = 0; i < count; ++i) {
        ShapedGlyph glyph; glyph.cluster = i; glyph.advance_x = advance;
        text.glyphs.push_back(glyph);
    }
    return text;
}
void duplicate_and_failure() {
    auto shaped = synthetic(3); shaped.glyphs[1].advance_x = 0;
    const std::array<std::size_t, 4> boundaries{0, 1, 2, 3};
    TextCaretMap map; require(map.assign(shaped, boundaries, 1, 18), "synthetic map failed");
    require(map.nearest(10, 1)->byte == 1 && map.nearest(15, 1)->byte == 1
        && map.nearest(-100, 1)->byte == 0 && map.nearest(100, 1)->byte == 3, "nearest/tie policy unstable");
    require(!map.nearest(std::numeric_limits<float>::quiet_NaN(), 1), "NaN hit accepted");
    shaped.glyphs[1].cluster = 100;
    require(!map.assign(shaped, boundaries, 2, 18) && map.revision() == 1, "bad cluster published partial map");
    shaped.glyphs[1].cluster = 1; shaped.glyphs[1].advance_x = -1;
    require(!map.assign(shaped, boundaries, 2, 18), "negative advance accepted");
    shaped.glyphs[1].advance_x = 0;
    shaped.runs.push_back({{}, 0, 3, 0, 3, true});
    require(!map.assign(shaped, boundaries, 2, 18), "unsupported RTL navigation accepted");
    const auto bigger = synthetic(128);
    std::array<std::size_t, 129> many{}; for(std::size_t i = 0; i < many.size(); ++i) many[i] = i;
    std::size_t failures{};
    for(std::size_t fail = 0; fail < 32; ++fail) {
        TextCaretMap candidate; const auto small = synthetic(3);
        require(candidate.assign(small, boundaries, 1, 18), "initial fault map failed");
        allocation_probe::begin(fail);
        bool threw{};
        try { static_cast<void>(candidate.assign(bigger, many, 2, 18)); }
        catch(const std::bad_alloc&) { threw = true; }
        static_cast<void>(allocation_probe::end());
        if(!threw) break;
        ++failures; require(candidate.revision() == 1 && candidate.stops().size() == 4
            && candidate.at(3, 1)->x == 30, "allocation failure replaced published caret map");
    }
    require(failures > 0, "allocation fault probe missed map preparation");
    require(map.assign(bigger, many, 2, 18), "lookup benchmark map failed");
    allocation_probe::begin();
    std::size_t sum{};
    for(std::size_t i = 0; i < 20000; ++i) { sum += map.nearest(float(i % 128) * 10, 2)->byte; sum += map.at(i % 129, 2)->byte; }
    const auto allocations = allocation_probe::end();
    require(allocations == 0 && sum > 0, "caret lookup allocated");
    std::cout << "TextCaretMap lookup cycles=20000 allocations=" << allocations << " atomic_failures=" << failures << '\n';
}
}
int main() {
    try { real_fonts(); duplicate_and_failure(); std::cout << "Unicode-to-glyph caret map passed\n"; }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
