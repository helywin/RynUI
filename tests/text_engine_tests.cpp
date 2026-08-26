#include "text/text_engine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>

namespace {

using ryn::String;
using ryn::font::FontIdentity;
using ryn::font::FontRuntime;
using ryn::text::ShapedText;
using ryn::text::TextEngine;
using ryn::text::TextErrorKind;
using ryn::text::TextLayoutConfig;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct Fixture final {
    Fixture()
        : fonts(create_runtime()),
          engine(*fonts) {
        const auto latin_load = fonts->load_font_file(
            RYNUI_VALIDATION_LATIN_FONT, 0, 14);
        const auto cjk_load = fonts->load_font_file(
            RYNUI_VALIDATION_CJK_FONT, 0, 14);
        require(latin_load && cjk_load, "text validation fonts failed to load");
        latin = latin_load.font;
        cjk = cjk_load.font;
    }

    [[nodiscard]] std::array<FontIdentity, 2> chain() const noexcept {
        return {latin, cjk};
    }

    [[nodiscard]] ryn::text::TextShapeResult shape(const String& value) {
        const auto fonts_chain = chain();
        return engine.shape(value.view(), fonts_chain);
    }

    static std::unique_ptr<FontRuntime> create_runtime() {
        auto created = FontRuntime::create();
        require(static_cast<bool>(created), "Font Runtime initialization failed");
        return std::move(created.runtime);
    }

    std::unique_ptr<FontRuntime> fonts;
    FontIdentity latin{};
    FontIdentity cjk{};
    TextEngine engine;
};

void test_utf8_decoder_and_lossy_boundary() {
    Fixture fixture;
    const String valid = u8"A中!";
    const auto scalars = ryn::text::decode_utf8(valid.view());
    require(scalars.size() == 3, "UTF-8 decoder returned the wrong scalar count");
    require(scalars[0].value == U'A' && scalars[0].byte_start == 0
                && scalars[0].byte_end == 1,
            "ASCII byte range is incorrect");
    require(scalars[1].value == U'中' && scalars[1].byte_start == 1
                && scalars[1].byte_end == 4 && scalars[1].cjk,
            "CJK byte range is incorrect");
    require(scalars[2].value == U'!' && scalars[2].byte_start == 4
                && scalars[2].byte_end == 5,
            "trailing ASCII byte range is incorrect");

    const std::array<char, 4> malformed{
        static_cast<char>(0xE2), '(', static_cast<char>(0xA1), 'A'};
    const auto fonts_chain = fixture.chain();
    const auto repaired = fixture.engine.shape_utf8_lossy(
        std::string_view{malformed.data(), malformed.size()}, fonts_chain);
    require(static_cast<bool>(repaired), "lossy UTF-8 text did not shape");
    require(repaired.text.replacement_count == 2,
            "lossy UTF-8 text reported the wrong replacement count");
    require(repaired.text.normalized_size_bytes == 8,
            "lossy UTF-8 text reported the wrong normalized byte size");
    require(std::ranges::all_of(repaired.text.glyphs, [&](const auto& glyph) {
                return glyph.cluster < repaired.text.normalized_size_bytes;
            }),
            "shaping emitted a cluster outside normalized UTF-8");
    require(std::ranges::any_of(repaired.text.glyphs, [](const auto& glyph) {
                return glyph.cluster == 7;
            }),
            "valid text following malformed UTF-8 was not shaped");
}

void test_fallback_runs_clusters_and_ligatures() {
    Fixture fixture;
    const String mixed = u8"office 中文!";
    const auto shaped = fixture.shape(mixed);
    const auto repeated = fixture.shape(mixed);
    require(shaped && repeated, "Latin/CJK text shaping failed");
    require(shaped.text == repeated.text, "repeated shaping was not deterministic");
    require(shaped.text.runs.size() >= 3,
            "Latin/CJK/punctuation fallback did not create ordered runs");
    require(shaped.text.runs.front().font == fixture.latin,
            "Latin run did not select the preferred font");
    require(std::ranges::any_of(shaped.text.runs, [&](const auto& run) {
                return run.font == fixture.cjk;
            }),
            "CJK run did not select the fallback font");
    require(shaped.text.runs.back().font == fixture.latin,
            "neutral punctuation did not return to the preferred font");

    std::size_t previous_run_end = 0;
    for (const auto& run : shaped.text.runs) {
        require(run.byte_start >= previous_run_end && run.byte_end > run.byte_start,
                "fallback runs are not ordered byte ranges");
        previous_run_end = run.byte_end;
        for (std::size_t index = run.glyph_begin;
                index < run.glyph_begin + run.glyph_count; ++index) {
            require(shaped.text.glyphs[index].font == run.font,
                    "glyph lost its fallback font identity");
        }
    }
    require(previous_run_end == mixed.size_bytes(),
            "fallback runs did not cover the complete input");

    for (const auto& run : shaped.text.runs) {
        std::size_t previous_cluster = run.byte_start;
        for (std::size_t index = run.glyph_begin;
                index < run.glyph_begin + run.glyph_count; ++index) {
            const std::size_t cluster = shaped.text.glyphs[index].cluster;
            require(cluster >= previous_cluster && cluster < run.byte_end,
                    "LTR HarfBuzz clusters are not monotonically traceable");
            require(std::ranges::any_of(shaped.text.scalars, [&](const auto& scalar) {
                        return scalar.byte_start == cluster;
                    }),
                    "glyph cluster does not map to an input scalar byte offset");
            previous_cluster = cluster;
        }
    }

    const String ligature_text = u8"office";
    const auto ligature = fixture.shape(ligature_text);
    const auto ligature_repeat = fixture.shape(ligature_text);
    require(ligature && ligature_repeat, "ligature shaping failed");
    require(ligature.text == ligature_repeat.text,
            "ligature shaping was not deterministic");
    require(ligature.text.glyphs.size() < ligature.text.scalars.size(),
            "locked Latin font did not exercise a ligature cluster");

    const String mixed_direction = u8"Aא";
    const auto unsupported = fixture.shape(mixed_direction);
    require(!unsupported
                && unsupported.error.kind == TextErrorKind::mixed_direction_unsupported
                && unsupported.error.byte_offset == 1,
            "mixed-direction paragraph did not return a capability diagnostic");
}

void test_measurement_uses_shaped_metrics() {
    Fixture fixture;
    const auto narrow = fixture.shape(String{u8"iiii"});
    const auto wide = fixture.shape(String{u8"WWWW"});
    require(narrow && wide, "measurement fixture shaping failed");
    require(narrow.text.scalars.size() == wide.text.scalars.size(),
            "equal-scalar measurement fixture is invalid");

    const TextLayoutConfig config{24.0F, std::numeric_limits<float>::infinity()};
    const auto narrow_measurement = fixture.engine.measure(narrow.text, config);
    const auto wide_measurement = fixture.engine.measure(wide.text, config);
    require(narrow_measurement && wide_measurement,
            "shaped text measurement failed");
    require(std::abs(narrow_measurement.measurement.width
                     - wide_measurement.measurement.width) > 1.0F,
            "measurement estimated width from scalar count");
    require(wide_measurement.measurement.content_bounds.right
                > wide_measurement.measurement.content_bounds.left
                && wide_measurement.measurement.content_bounds.bottom
                    > wide_measurement.measurement.content_bounds.top,
            "glyph extents did not produce visible content bounds");

    const auto repeated = fixture.engine.measure(wide.text, config);
    require(repeated && repeated.measurement == wide_measurement.measurement,
            "repeated measurement was not deterministic");

    const auto empty_shape = fixture.shape(String{u8""});
    const auto empty = fixture.engine.measure(empty_shape.text, config);
    require(empty_shape && empty, "empty text measurement failed");
    require(empty.measurement.lines.size() == 1
                && empty.measurement.lines.front().glyph_count == 0
                && empty.measurement.width == 0.0F
                && empty.measurement.height == config.line_height,
            "empty text did not preserve a deterministic line box");
}

void test_wrapping_respects_cluster_boundaries() {
    Fixture fixture;
    constexpr float line_height = 20.0F;
    const float infinity = std::numeric_limits<float>::infinity();

    const auto single_cjk = fixture.shape(String{u8"中"});
    const auto cjk = fixture.shape(String{u8"中文测试"});
    require(single_cjk && cjk, "CJK wrapping fixture shaping failed");
    const auto single_cjk_measure = fixture.engine.measure(
        single_cjk.text, {line_height, infinity});
    const auto cjk_wrapped = fixture.engine.measure(
        cjk.text, {line_height, single_cjk_measure.measurement.width * 2.1F});
    require(single_cjk_measure && cjk_wrapped
                && cjk_wrapped.measurement.lines.size() >= 2,
            "finite CJK width did not produce multiple lines");
    std::size_t cjk_glyph_count = 0;
    for (const auto& line : cjk_wrapped.measurement.lines) {
        cjk_glyph_count += line.glyph_count;
    }
    require(cjk_glyph_count == cjk.text.glyphs.size(),
            "CJK wrapping lost or duplicated glyphs");

    const auto prefix = fixture.shape(String{u8"hello "});
    const auto words = fixture.shape(String{u8"hello world"});
    require(prefix && words, "Latin wrapping fixture shaping failed");
    const auto prefix_measure = fixture.engine.measure(prefix.text, {line_height, infinity});
    const auto word_wrapped = fixture.engine.measure(
        words.text, {line_height, prefix_measure.measurement.width + 0.01F});
    require(prefix_measure && word_wrapped
                && word_wrapped.measurement.lines.size() >= 2
                && word_wrapped.measurement.lines.front().byte_end == 6,
            "Latin wrapping did not prefer the whitespace boundary");

    const auto newline_shape = fixture.shape(String{u8"A\nB\n"});
    const auto newline = fixture.engine.measure(
        newline_shape.text, {line_height, infinity});
    require(newline_shape && newline && newline.measurement.lines.size() == 3,
            "explicit newline did not create deterministic paragraphs");

    const auto ligature = fixture.shape(String{u8"office office"});
    const auto ligature_wrapped = fixture.engine.measure(
        ligature.text, {line_height, prefix_measure.measurement.width * 0.55F});
    require(ligature && ligature_wrapped, "ligature wrapping failed");
    std::set<std::size_t> prior_line_clusters;
    for (const auto& line : ligature_wrapped.measurement.lines) {
        std::set<std::size_t> line_clusters;
        for (std::size_t glyph = line.glyph_begin;
                glyph < line.glyph_begin + line.glyph_count; ++glyph) {
            line_clusters.insert(ligature.text.glyphs[glyph].cluster);
        }
        for (const std::size_t cluster : line_clusters) {
            require(!prior_line_clusters.contains(cluster),
                    "wrapping split one HarfBuzz cluster across lines");
        }
        prior_line_clusters.insert(line_clusters.begin(), line_clusters.end());
    }

    const auto overwide_shape = fixture.shape(String{u8"W"});
    const auto overwide_full = fixture.engine.measure(
        overwide_shape.text, {line_height, infinity});
    const auto overwide = fixture.engine.measure(
        overwide_shape.text, {line_height, overwide_full.measurement.width * 0.5F});
    require(overwide_shape && overwide_full && overwide
                && overwide.measurement.lines.size() == 1
                && overwide.measurement.lines.front().glyph_count == 1
                && overwide.measurement.lines.front().overflow
                && overwide.measurement.overflow,
            "overwide cluster was split or not reported");
}

} // namespace

int main() {
    try {
        test_utf8_decoder_and_lossy_boundary();
        test_fallback_runs_clusters_and_ligatures();
        test_measurement_uses_shaped_metrics();
        test_wrapping_respects_cluster_boundaries();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
