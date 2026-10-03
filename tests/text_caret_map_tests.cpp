#include "text/text_caret_map.hpp"
#include "text/text_engine.hpp"
#include "input/text_boundary.hpp"
#include "support/allocation_probe.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::text;
namespace allocation_probe = ryn_test::allocation;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void real_fonts() {
    auto runtime = font::FontRuntime::create();
    require(bool(runtime), "font runtime failed");
    const auto latin = runtime.runtime->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 18);
    const auto cjk = runtime.runtime->load_font_file(RYNUI_VALIDATION_CJK_FONT, 0, 18);
    require(bool(latin) && bool(cjk), "caret fonts unavailable");
    const std::array chain{latin.font, cjk.font};
    TextEngine engine{*runtime.runtime};
    TextCaretMap map;
    std::uint64_t revision{};
    for (const auto source : {String{}, String{u8"ffi"}, String{u8"a\u0301b"}, String{u8"中文 abc"},
                              String{u8"\U0001F469\u200D\U0001F4BB"}, String{u8"\U0010FFFF"}, String{u8"a\u200Bb"}}) {
        const auto shaped = engine.shape(source.view(), chain);
        require(bool(shaped), "caret shape failed");
        const auto measured = engine.measure(shaped.text, {28, std::numeric_limits<float>::infinity()});
        require(bool(measured), "caret measure failed");
        require(engine.map_carets(shaped.text, source.view(), ++revision, measured.measurement.first_baseline, map),
                "caret map failed");
        input::TextBoundaryMap boundaries;
        require(boundaries.assign(source.bytes()), "Unicode map failed");
        require(map.stops().size() == boundaries.grapheme_bytes().size(), "glyph clusters replaced legal graphemes");
        float previous = -1;
        for (std::size_t index = 0; index < map.stops().size(); ++index) {
            const auto stop = map.stops()[index];
            require(stop.byte == boundaries.grapheme_bytes()[index] && stop.x >= previous &&
                        stop.baseline == measured.measurement.first_baseline,
                    "invalid caret geometry");
            require(stop.glyph_begin <= shaped.text.glyphs.size() &&
                        stop.glyph_count <= shaped.text.glyphs.size() - stop.glyph_begin,
                    "caret glyph range escaped shape");
            require(map.at(stop.byte, revision) == stop, "exact legal caret not found");
            previous = stop.x;
        }
        require(std::abs(map.stops().back().x - measured.measurement.width) < 0.001F,
                "caret end differs from shaped advance");
        require(!map.nearest(0, revision + 1) && !map.at(0, revision + 1), "stale shape revision accepted");
        if (source == String{u8"ffi"}) {
            require(shaped.text.glyphs.size() < 3 && map.stops().size() == 4,
                    "validation font did not exercise ligature");
            const auto width = map.stops().back().x;
            require(std::abs(map.stops()[1].x - width / 3) < 0.001F &&
                        std::abs(map.stops()[2].x - width * 2 / 3) < 0.001F,
                    "ligature fallback not equally distributed");
        }
        if (source == String{u8"a\u0301b"}) {
            require(!map.at(1, revision) && !map.at(2, revision), "combining grapheme split");
        }
    }
    const auto source = String{u8"abc"};
    const auto shaped = engine.shape(source.view(), chain);
    require(!engine.map_carets(shaped.text, String{u8"xyz"}.view(), revision + 1, 18, map) &&
                map.revision() == revision,
            "unrelated same-length source accepted");
}

ShapedText synthetic(std::size_t count, float advance = 10) {
    ShapedText text;
    text.normalized_size_bytes = count;
    for (std::size_t i = 0; i < count; ++i) {
        ShapedGlyph glyph;
        glyph.cluster = i;
        glyph.advance_x = advance;
        text.glyphs.push_back(glyph);
    }
    return text;
}

void duplicate_and_failure() {
    auto shaped = synthetic(3);
    shaped.glyphs[1].advance_x = 0;
    const std::array<std::size_t, 4> boundaries{0, 1, 2, 3};
    TextCaretMap map;
    require(map.assign(shaped, boundaries, 1, 18), "synthetic map failed");
    require(map.nearest(10, 1)->byte == 1 && map.nearest(15, 1)->byte == 1 && map.nearest(-100, 1)->byte == 0 &&
                map.nearest(100, 1)->byte == 3,
            "nearest/tie policy unstable");
    require(!map.nearest(std::numeric_limits<float>::quiet_NaN(), 1), "NaN hit accepted");
    shaped.glyphs[1].cluster = 100;
    require(!map.assign(shaped, boundaries, 2, 18) && map.revision() == 1, "bad cluster published partial map");
    shaped.glyphs[1].cluster = 1;
    shaped.glyphs[1].advance_x = -1;
    require(!map.assign(shaped, boundaries, 2, 18), "negative advance accepted");
    shaped.glyphs[1].advance_x = 0;
    shaped.runs.push_back({{}, 0, 3, 0, 3, true});
    require(!map.assign(shaped, boundaries, 2, 18), "unsupported RTL navigation accepted");
    const auto bigger = synthetic(128);
    std::array<std::size_t, 129> many{};
    for (std::size_t i = 0; i < many.size(); ++i) {
        many[i] = i;
    }
    std::size_t failures{};
    for (std::size_t fail = 0; fail < 32; ++fail) {
        TextCaretMap candidate;
        const auto small = synthetic(3);
        require(candidate.assign(small, boundaries, 1, 18), "initial fault map failed");
        allocation_probe::begin(fail);
        bool threw{};
        try {
            static_cast<void>(candidate.assign(bigger, many, 2, 18));
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        static_cast<void>(allocation_probe::end());
        if (!threw) {
            break;
        }
        ++failures;
        require(candidate.revision() == 1 && candidate.stops().size() == 4 && candidate.at(3, 1)->x == 30,
                "allocation failure replaced published caret map");
    }
    require(failures > 0, "allocation fault probe missed map preparation");
    require(map.assign(bigger, many, 2, 18), "lookup benchmark map failed");
    allocation_probe::begin();
    std::size_t sum{};
    for (std::size_t i = 0; i < 20000; ++i) {
        sum += map.nearest(float(i % 128) * 10, 2)->byte;
        sum += map.at(i % 129, 2)->byte;
    }
    const auto allocations = allocation_probe::end();
    require(allocations == 0 && sum > 0, "caret lookup allocated");
    std::cout << "TextCaretMap lookup cycles=20000 allocations=" << allocations << " atomic_failures=" << failures
              << '\n';
}

void multiline_maps() {
    auto runtime = font::FontRuntime::create();
    require(bool(runtime), "multiline font runtime failed");
    const auto latin = runtime.runtime->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 18);
    const auto cjk = runtime.runtime->load_font_file(RYNUI_VALIDATION_CJK_FONT, 0, 18);
    require(bool(latin) && bool(cjk), "multiline fonts failed");
    const std::array chain{latin.font, cjk.font};
    TextEngine engine{*runtime.runtime};
    TextCaretMap map;
    std::uint64_t revision{};
    for (const auto source :
         {String{}, String{u8"\n\n"}, String{u8"abc\n"}, String{u8"ffi\n中\n\nabc"}, String{u8"é中🙂\nabc xyz"}}) {
        const auto shaped = engine.shape(source.view(), chain);
        require(bool(shaped), "multiline shape failed");
        input::TextBoundaryMap boundaries;
        require(boundaries.assign(source.bytes()), "multiline Unicode map failed");
        for (const auto width : {26.0F, 50.0F, std::numeric_limits<float>::infinity()}) {
            const auto measured = engine.measure(shaped.text, {28, width});
            require(bool(measured) &&
                        engine.map_carets(shaped.text, source.view(), ++revision, measured.measurement, map),
                    "multiline caret assignment failed");
            require(map.line_count() == measured.measurement.lines.size(), "caret lines differ from measurement");
            for (std::size_t line = 0; line < map.line_count(); ++line) {
                const auto& geometry = measured.measurement.lines[line];
                const auto stops = map.line_stops(line);
                require(!stops.empty() && stops.front().byte == geometry.byte_start &&
                            stops.back().byte == geometry.byte_end && std::abs(stops.back().x - geometry.width) < .001F,
                        "multiline line edges differ from shaping");
                float previous = -1;
                for (const auto stop : stops) {
                    require(boundaries.is_boundary(stop.byte) && stop.line == line && stop.x >= previous &&
                                stop.baseline == geometry.baseline,
                            "multiline invalid grapheme/geometry");
                    require(map.nearest(stop.x, static_cast<float>(line) * 28 + 14, revision)->line == line,
                            "two-dimensional caret hit changed line");
                    previous = stop.x;
                }
                require(map.line_edge(line, false, revision) == stops.front() &&
                            map.line_edge(line, true, revision) == stops.back(),
                        "line Home/End lookup failed");
            }
            for (const auto boundary : boundaries.grapheme_bytes()) {
                const auto before = map.at(boundary, revision, TextCaretAffinity::Upstream);
                const auto after = map.at(boundary, revision);
                require(before && after && before->line <= after->line, "missing multiline legal stop/affinity");
                if (before->line != after->line) {
                    require(after->line == before->line + 1 && after->x == 0 &&
                                before->x == measured.measurement.lines[before->line].width,
                            "soft-wrap affinity unstable");
                }
            }
            require(map.nearest(-100, -100, revision)->line == 0 &&
                        map.nearest(1000, 100000, revision)->line == map.line_count() - 1,
                    "outside multiline hit did not clamp");
            require(!map.nearest(0, 0, revision + 1) && !map.line_edge(map.line_count(), true, revision) &&
                        !map.nearest_on_line(0, map.line_count(), revision),
                    "stale/invalid multiline map lookup accepted");
        }
    }
    const auto source = String{u8"abcdef\nx\nabcdef"};
    const auto shaped = engine.shape(source.view(), chain);
    auto measured = engine.measure(shaped.text, {28, std::numeric_limits<float>::infinity()});
    require(engine.map_carets(shaped.text, source.view(), ++revision, measured.measurement, map),
            "vertical navigation map failed");
    const auto current = map.at(4, revision).value();
    const auto short_line = map.adjacent_line(current, 1, current.x, revision).value();
    const auto long_line = map.adjacent_line(short_line, 1, current.x, revision).value();
    require(short_line.byte == 8 && long_line.byte == 13, "vertical move did not preserve preferred x");
    allocation_probe::begin();
    std::size_t sum{};
    for (int i = 0; i < 20000; ++i) {
        sum += map.nearest(float(i % 50), float(i % 84), revision)->byte;
        sum += map.adjacent_line(current, i % 3, current.x, revision)->byte;
    }
    const auto allocations = allocation_probe::end();
    require(allocations == 0 && sum > 0, "multiline lookup allocated");
    const auto prior = std::vector<TextCaretStop>(map.stops().begin(), map.stops().end());
    input::TextBoundaryMap boundaries;
    require(boundaries.assign(source.bytes()), "vertical boundaries failed");
    measured.measurement.lines[1].byte_start = 1;
    require(!map.assign(shaped.text, measured.measurement, boundaries.grapheme_bytes(), revision + 1),
            "invalid overlapping line published");
    require(map.revision() == revision && std::ranges::equal(map.stops(), prior), "failed line map replaced state");
    measured = engine.measure(shaped.text, {28, std::numeric_limits<float>::infinity()});
    std::string long_text;
    for (int line = 0; line < 64; ++line) {
        long_text += "abcdefgh\n";
    }
    const auto bigger_source = String::from_utf8(long_text).value();
    const auto bigger = engine.shape(bigger_source.view(), chain);
    const auto bigger_measured = engine.measure(bigger.text, {28, 50});
    input::TextBoundaryMap bigger_boundaries;
    require(bigger_boundaries.assign(long_text), "larger boundaries failed");
    std::size_t failures{};
    for (std::size_t fail = 0; fail < 64; ++fail) {
        TextCaretMap candidate;
        require(candidate.assign(shaped.text, measured.measurement, boundaries.grapheme_bytes(), 1),
                "initial multiline fault map failed");
        allocation_probe::begin(fail);
        bool threw{};
        try {
            static_cast<void>(
                candidate.assign(bigger.text, bigger_measured.measurement, bigger_boundaries.grapheme_bytes(), 2));
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        static_cast<void>(allocation_probe::end());
        if (!threw) {
            break;
        }
        ++failures;
        require(candidate.revision() == 1 && candidate.line_count() == 3 && candidate.at(13, 1)->line == 2 &&
                    candidate.line_edge(1, true, 1)->byte == 8,
                "allocation failure replaced published multiline map");
    }
    require(failures > 0, "multiline preparation allocation faults missed");
}
} // namespace

int main() {
    try {
        real_fonts();
        duplicate_and_failure();
        multiline_maps();
        std::cout << "Unicode-to-glyph caret map passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
