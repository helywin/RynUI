#include "text/text_caret_map.hpp"
#include "text/text_engine.hpp"
#include "input/text_boundary.hpp"
#include "support/allocation_probe.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

#if defined(_MSC_VER) && !defined(NDEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace ryn;
using namespace ryn::text;

void check(bool valid, const char* message) {
    if (!valid) {
        throw std::runtime_error(message);
    }
}

void caret_contract() {
    auto created = font::FontRuntime::create();
    check(bool(created), "caret font runtime");
    auto& fonts = *created.runtime;
    const auto latin = fonts.load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 24);
    const auto arabic = fonts.load_font_file(RYNUI_VALIDATION_ARABIC_FONT, 0, 24);
    const auto hebrew = fonts.load_font_file(RYNUI_VALIDATION_HEBREW_FONT, 0, 24);
    check(latin && arabic && hebrew, "caret fonts");
    const std::array chain{latin.font, arabic.font, hebrew.font};
    TextEngine engine{fonts};
    TextCaretMap map;
    const String rtl{u8"אבג"};
    const auto rtl_shape = engine.shape(rtl.view(), chain);
    const auto rtl_measure = engine.measure(rtl_shape.text, {32, 600});
    check(rtl_shape && rtl_measure && engine.map_carets(rtl_shape.text, rtl.view(), 1, rtl_measure.measurement, map),
          "RTL measured caret map");
    check(map.at(0, 1)->x == rtl_measure.measurement.width && map.at(rtl.size_bytes(), 1)->x == 0 &&
              map.line_edge(0, false, 1)->byte == rtl.size_bytes() && map.line_edge(0, true, 1)->byte == 0,
          "RTL caret endpoints are not visual edges");
    check(map.adjacent_visual(map.at(0, 1).value(), -1, 1)->byte == 2 &&
              map.adjacent_visual(map.at(2, 1).value(), 1, 1)->byte == 0,
          "RTL arrow movement is logical rather than visual");
    check(engine.map_carets(rtl_shape.text, rtl.view(), 2, 27, map) && map.at(0, 2)->baseline == 27 &&
              map.at(rtl.size_bytes(), 2)->x == 0,
          "unwrapped RTL caret geometry");

    const String mixed{u8"A אבג 12 B"};
    const auto shaped = engine.shape(mixed.view(), chain);
    const auto measured = engine.measure(shaped.text, {32, 600});
    check(shaped && measured && engine.map_carets(shaped.text, mixed.view(), 3, measured.measurement, map),
          "mixed caret map");
    const auto upstream = map.at(2, 3, TextCaretAffinity::Upstream).value();
    const auto downstream = map.at(2, 3, TextCaretAffinity::Downstream).value();
    check(upstream.x < downstream.x && upstream.line == downstream.line && upstream.affinity != downstream.affinity,
          "mixed run boundary lost dual affinity");
    std::vector<TextCoverageSegment> segments;
    check(map.visit_coverage(0, 4, 3, [&](auto piece) { segments.push_back(piece); }) && segments.size() == 2 &&
              segments[0].x + segments[0].width < segments[1].x,
          "logical selection did not preserve visual gap");
    input::TextBoundaryMap boundaries;
    check(boundaries.assign(mixed.bytes()), "mixed boundaries");
    for (auto boundary : boundaries.grapheme_bytes()) {
        check(map.at(boundary, 3).has_value(), "logical byte query lost a grapheme boundary");
    }
    const auto visual = map.line_stops(0);
    for (std::size_t index = 0; index < visual.size(); ++index) {
        if (index > 0) {
            check(visual[index].x >= visual[index - 1].x, "visual index is not ordered by x");
            check(map.adjacent_visual(visual[index], -1, 3) == visual[index - 1], "left visual adjacency");
        }
        if (index + 1 < visual.size()) {
            check(map.adjacent_visual(visual[index], 1, 3) == visual[index + 1], "right visual adjacency");
        }
    }
    check(!map.at(3, 3) && !map.nearest(0, 4) && !map.visit_coverage(3, 4, 3, [](auto) {}) &&
              !map.visit_coverage(4, 0, 3, [](auto) {}),
          "illegal byte/range/revision accepted");
    auto invalid = measured.measurement;
    invalid.visual_glyphs[1] = invalid.visual_glyphs[0];
    check(!map.assign(shaped.text, invalid, boundaries.grapheme_bytes(), 4) && map.revision() == 3 &&
              map.at(2, 3, TextCaretAffinity::Downstream) == downstream,
          "bad glyph order replaced published map");
    invalid = measured.measurement;
    invalid.visual_clusters[0].x = std::numeric_limits<float>::quiet_NaN();
    check(!map.assign(shaped.text, invalid, boundaries.grapheme_bytes(), 4) && map.revision() == 3,
          "NaN cluster published");
    ryn_test::allocation::begin();
    std::size_t visited{};
    for (int index = 0; index < 20000; ++index) {
        static_cast<void>(map.at(2, 3, TextCaretAffinity::Upstream));
        static_cast<void>(map.nearest(static_cast<float>(index % 100), 3));
        static_cast<void>(map.adjacent_visual(downstream, -1, 3));
        static_cast<void>(map.line_bytes(0, 3));
        check(map.visit_coverage(0, 4, 3, [&](auto) { ++visited; }), "coverage query failed");
    }
    const auto allocations = ryn_test::allocation::end();
    check(allocations == 0 && visited == 40000, "bidi caret/coverage queries allocate");

    const String wrapped{u8"אבג דהו זחט\n\nمرحبا مرحبا\n"};
    const auto wrap_shape = engine.shape(wrapped.view(), chain);
    const auto wrap_measure = engine.measure(wrap_shape.text, {32, 65});
    check(wrap_shape && wrap_measure &&
              engine.map_carets(wrap_shape.text, wrapped.view(), 4, wrap_measure.measurement, map),
          "wrapped mixed caret map");
    check(boundaries.assign(wrapped.bytes()), "wrapped boundaries");
    bool soft_affinity{};
    for (const auto boundary : boundaries.grapheme_bytes()) {
        const auto before = map.at(boundary, 4, TextCaretAffinity::Upstream).value();
        const auto after = map.at(boundary, 4, TextCaretAffinity::Downstream).value();
        if (before.line != after.line) {
            check(after.line == before.line + 1 && before.affinity == TextCaretAffinity::Upstream &&
                      after.affinity == TextCaretAffinity::Downstream,
                  "soft wrap affinity contract");
            soft_affinity = true;
        }
    }
    const auto left_edge = map.line_edge(0, false, 4).value();
    check(soft_affinity && map.adjacent_visual(left_edge, -1, 4) == map.line_edge(1, true, 4),
          "RTL soft-wrap arrow did not enter next line from its right edge");
    check(map.nearest(0, -100, 4)->line == 0 && map.nearest(0, 100000, 4)->line == map.line_count() - 1,
          "two dimensional hit line clamp");
    const auto current = map.line_edge(0, true, 4).value();
    check(map.adjacent_line(current, 1, current.x, 4)->line == 1, "preferred x vertical navigation");

    const String ligature_source{u8"لا"};
    const auto ligature = engine.shape(ligature_source.view(), chain);
    const auto ligature_measure = engine.measure(ligature.text, {32, 600});
    check(ligature && ligature.text.glyphs.size() == 1 &&
              engine.map_carets(ligature.text, ligature_source.view(), 5, ligature_measure.measurement, map) &&
              std::abs(map.at(2, 5)->x - ligature_measure.measurement.width / 2) < .001F,
          "RTL ligature grapheme division");
    const String zero_source{u8"א\u200Fב"};
    const auto zero = engine.shape(zero_source.view(), chain);
    const auto zero_measure = engine.measure(zero.text, {32, 600});
    check(zero && engine.map_carets(zero.text, zero_source.view(), 6, zero_measure.measurement, map),
          "zero-width RTL map");
    const auto zero_hit = map.nearest(map.at(2, 6)->x, 6).value();
    check(zero_hit.byte == 2, "duplicate visual x did not choose earliest logical byte");

    const String atomic_source{u8"A\U0001F469\u200D\U0001F4BBא"};
    const auto atomic_shape = engine.shape(atomic_source.view(), chain);
    const auto atomic_measure = engine.measure(atomic_shape.text, {32, 20});
    check(atomic_shape && atomic_measure &&
              engine.map_carets(atomic_shape.text, atomic_source.view(), 7, atomic_measure.measurement, map),
          "emergency wrap split a grapheme across fallback/control clusters");

    std::string longer;
    for (int index = 0; index < 20; ++index) {
        longer += mixed.bytes();
        longer += '\n';
    }
    const auto long_source = String::from_utf8(longer).value();
    const auto long_shaped = engine.shape(long_source.view(), chain);
    const auto long_measured = engine.measure(long_shaped.text, {32, 65});
    check(boundaries.assign(longer), "long boundaries");
    std::size_t failures{};
    for (std::size_t fault = 0; fault < 64; ++fault) {
        TextCaretMap candidate;
        check(engine.map_carets(shaped.text, mixed.view(), 1, measured.measurement, candidate), "fault baseline");
        ryn_test::allocation::begin(fault);
        bool threw{};
        try {
            static_cast<void>(
                candidate.assign(long_shaped.text, long_measured.measurement, boundaries.grapheme_bytes(), 2));
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        static_cast<void>(ryn_test::allocation::end());
        if (!threw) {
            break;
        }
        ++failures;
        std::size_t pieces{};
        check(candidate.revision() == 1 && candidate.visit_coverage(0, 4, 1, [&](auto) { ++pieces; }) && pieces == 2,
              "allocation failure replaced published coverage/map");
    }
    check(failures > 0, "allocation fault probe missed preparation");
    std::cout << "bidi query cycles=20000 allocations=" << allocations << " atomic_failures=" << failures << '\n';
}
} // namespace

int main() {
#if defined(_MSC_VER) && !defined(NDEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    try {
        caret_contract();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
