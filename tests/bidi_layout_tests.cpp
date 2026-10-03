#include "graphics/glyph_scene.hpp"
#include "text/text_engine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::text;

void check(bool valid, const char* message) {
    if (!valid) {
        throw std::runtime_error(message);
    }
}

bool near(float first, float second) {
    return std::abs(first - second) < 0.001F;
}

void layout_contract() {
    auto created = font::FontRuntime::create();
    check(bool(created), "font runtime");
    auto& fonts = *created.runtime;
    const auto latin = fonts.load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 24);
    const auto arabic = fonts.load_font_file(RYNUI_VALIDATION_ARABIC_FONT, 0, 24);
    const auto hebrew = fonts.load_font_file(RYNUI_VALIDATION_HEBREW_FONT, 0, 24);
    check(latin && arabic && hebrew, "bidi fonts");
    const std::vector chain{latin.font, arabic.font, hebrew.font};
    TextEngine engine{fonts};
    const String mixed{u8"A אבג 12 B"};
    const auto shaped = engine.shape(mixed.view(), chain);
    const auto measured = engine.measure(shaped.text, {32, std::numeric_limits<float>::infinity()});
    check(shaped && measured, "mixed layout");
    std::vector<std::size_t> order;
    float x{};
    for (const auto& cluster : measured.measurement.visual_clusters) {
        order.push_back(cluster.byte_start);
        check(near(cluster.x, x) && cluster.byte_end > cluster.byte_start, "visual cluster geometry");
        x += cluster.width;
    }
    check(order == std::vector<std::size_t>{0, 1, 9, 10, 8, 6, 4, 2, 11, 12}, "known mixed visual order");
    check(near(x, measured.measurement.width), "cluster and line widths differ");

    const String wrapped{u8"אבג 12 דהו 34\n\nمرحبا office مرحبا\n"};
    const auto wrap_shape = engine.shape(wrapped.view(), chain);
    const auto wrap_measure = engine.measure(wrap_shape.text, {32, 65});
    check(wrap_shape && wrap_measure && wrap_measure.measurement.lines.size() > 5, "logical wrap/hard empty lines");
    auto unique = wrap_measure.measurement.visual_glyphs;
    std::ranges::sort(unique);
    std::vector<std::size_t> expected(wrap_shape.text.glyphs.size());
    std::iota(expected.begin(), expected.end(), 0);
    check(unique == expected, "wrapped glyph coverage duplicated or lost glyphs");
    std::size_t prior_byte{};
    for (const auto& line : wrap_measure.measurement.lines) {
        check(line.byte_start >= prior_byte && line.byte_end >= line.byte_start, "line ranges are not logical");
        prior_byte = line.byte_end;
        if (line.cluster_count == 0) {
            check(line.glyph_count == 0 && line.width == 0, "empty line geometry");
            continue;
        }
        const auto runs = wrap_shape.text.bidi.line_runs(line.byte_start, line.byte_end);
        check(runs.has_value(), "line analysis");
        const auto& last = wrap_measure.measurement.visual_clusters[line.cluster_begin + line.cluster_count - 1];
        check(near(last.x + last.width, line.width), "wrapped cluster pen");
        for (auto index = line.cluster_begin; index < line.cluster_begin + line.cluster_count; ++index) {
            const auto& cluster = wrap_measure.measurement.visual_clusters[index];
            const auto run = std::ranges::find_if(*runs, [&](const auto& value) {
                return value.byte_begin <= cluster.byte_start && cluster.byte_start < value.byte_end;
            });
            check(run != runs->end() && run->level == cluster.level, "line did not apply its own L1 levels");
        }
    }
    check(wrap_measure.measurement.lines.back().glyph_count == 0, "final empty line lost");

    graphics::GlyphAtlas atlas;
    graphics::GlyphScene scene;
    const auto primitive =
        scene.append_text(fonts, atlas, shaped.text, measured.measurement, {{10, 10}, {600, 200}, {0, 0, 600, 200}});
    check(bool(primitive), "mixed glyph scene");
    std::size_t instance_index{};
    float pen{};
    for (const auto visual : measured.measurement.visual_glyphs) {
        const auto& glyph = shaped.text.glyphs[visual];
        const auto physical = std::round((10 + pen + glyph.offset_x) * 4) * 0.25F;
        const auto phase = static_cast<font::GlyphRasterPhase>(std::lround((physical - std::floor(physical)) * 4));
        const auto entry = atlas.ensure(fonts, glyph.font, glyph.glyph_id, phase);
        check(bool(entry), "scene atlas entry");
        if (!entry.entry->empty) {
            const auto& instance = scene.instances().at(primitive.primitive.instances.first +
                                                        static_cast<std::uint32_t>(instance_index++));
            check(near(instance.position_size[0],
                       std::floor(physical) + entry.entry->bearing_x - graphics::glyph_atlas_padding) &&
                      instance.uv_rect == std::array{entry.entry->uv.left, entry.entry->uv.top, entry.entry->uv.right,
                                                     entry.entry->uv.bottom},
                  "scene did not consume visual glyph order");
        }
        pen += std::abs(glyph.advance_x);
    }
    check(instance_index == primitive.primitive.instances.count && primitive.primitive.line_ranges.size() == 1,
          "scene instance coverage");

    std::size_t requests{};
    TextState state{engine, mixed, chain, 24, {32, 600}, [&] { ++requests; }};
    check(state.synchronize(), "text state initial");
    const auto shape_count = state.counters().shape_count;
    const auto* levels = state.shaped().bidi.levels().data();
    check(state.set_width_constraint(60) && state.synchronize() && state.counters().shape_count == shape_count &&
              state.shaped().bidi.levels().data() == levels,
          "width update reshaped/analyzed text");
    check(state.set_direction(TextDirection::RightToLeft) && state.synchronize() &&
              state.counters().shape_count == shape_count + 1 && state.shaped().bidi.paragraphs()[0].base_level == 1 &&
              requests == 2,
          "direction did not invalidate exactly once");
    const auto revision = state.revision();
    bool rejected{};
    try {
        static_cast<void>(state.set_direction(static_cast<TextDirection>(255)));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && state.revision() == revision && state.direction() == TextDirection::RightToLeft &&
              !state.set_direction(TextDirection::RightToLeft),
          "invalid/equal direction changed retained state");

    check(state.set_content(String{u8"אבג 123 אבג 456 office مرحبا"}) &&
              state.set_ellipsis({1, String{u8"…"}, false, 0}) && state.synchronize() && state.truncated() &&
              state.measurement().lines.size() == 1 && state.shaped().bidi.source() == state.display_content() &&
              state.shaped().bidi.direction() == TextDirection::RightToLeft,
          "ellipsis prefix lacked independent bidi analysis");
    const auto counters = state.counters();
    check(state.synchronize() && state.counters().shape_count == counters.shape_count &&
              state.counters().measure_count == counters.measure_count,
          "unchanged ellipsis rebuilt analysis");
}
} // namespace

int main() {
    try {
        layout_contract();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
