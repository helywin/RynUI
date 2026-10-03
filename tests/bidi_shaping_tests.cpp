#include "text/text_engine.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::text;
using namespace ryn::font;

void check(bool result, const char* message) {
    if (!result) {
        throw std::runtime_error(message);
    }
}

void shaping_contract() {
    auto created = FontRuntime::create();
    check(bool(created), "font runtime creation");
    auto& fonts = *created.runtime;
    const auto latin = fonts.load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 24);
    const auto cjk = fonts.load_font_file(RYNUI_VALIDATION_CJK_FONT, 0, 24);
    const auto arabic = fonts.load_font_file(RYNUI_VALIDATION_ARABIC_FONT, 0, 24);
    const auto hebrew = fonts.load_font_file(RYNUI_VALIDATION_HEBREW_FONT, 0, 24);
    check(latin && cjk && arabic && hebrew, "locked shaping fixtures load");
    const std::array chain{latin.font, cjk.font, arabic.font, hebrew.font};
    TextEngine engine{fonts};
    const String mixed{u8"office مرحبا (אבג 123) 中文"};
    const auto shaped = engine.shape(mixed.view(), chain);
    const auto repeated = engine.shape(mixed.view(), chain);
    check(shaped && repeated && shaped.text == repeated.text, "mixed shaping determinism");
    check(shaped.text.bidi.assigned() && shaped.text.bidi.source() == mixed.view(), "analysis owns logical source");
    std::size_t previous_end{};
    bool arabic_run{};
    bool hebrew_run{};
    bool digits_run{};
    for (const auto& run : shaped.text.runs) {
        check(run.byte_start == previous_end && run.byte_end > run.byte_start, "logical runs do not cover source");
        check(run.right_to_left == bool(run.level & 1), "explicit shaping direction differs from embedding level");
        previous_end = run.byte_end;
        arabic_run |= run.font == arabic.font && run.script == 0x41726162 && run.right_to_left;
        hebrew_run |= run.font == hebrew.font && run.script == 0x48656272 && run.right_to_left;
        digits_run |= run.level == 2 && !run.right_to_left;
        std::optional<std::size_t> previous_cluster;
        for (auto index = run.glyph_begin; index < run.glyph_begin + run.glyph_count; ++index) {
            const auto& glyph = shaped.text.glyphs[index];
            check(glyph.glyph_id != 0 && glyph.cluster >= run.byte_start && glyph.cluster < run.byte_end &&
                      shaped.text.bidi.scalar_boundary(glyph.cluster),
                  "missing glyph or invalid original cluster");
            if (previous_cluster) {
                check(run.right_to_left ? glyph.cluster <= *previous_cluster : glyph.cluster >= *previous_cluster,
                      "shaper direction cluster order");
            }
            previous_cluster = glyph.cluster;
        }
    }
    check(previous_end == mixed.size_bytes() && arabic_run && hebrew_run && digits_run,
          "Arabic/Hebrew/fallback/numbers were not analyzed and shaped independently");

    const String joining{u8"ببب"};
    const auto joined = engine.shape(joining.view(), chain);
    const auto whole = fonts.shape_utf8_segment(arabic.font, joining.bytes(), 0, joining.size_bytes(),
                                                {FontShapeDirection::right_to_left, 0x41726162});
    const auto middle =
        fonts.shape_utf8_segment(arabic.font, joining.bytes(), 2, 2, {FontShapeDirection::right_to_left, 0x41726162});
    const auto isolated = fonts.shape_utf8_segment(arabic.font, joining.bytes().substr(2, 2), 0, 2,
                                                   {FontShapeDirection::right_to_left, 0x41726162});
    check(joined && whole && middle && isolated && joined.text.runs.size() == 1 && whole.glyphs.size() == 3 &&
              middle.glyphs.size() == 1 && middle.glyphs[0].cluster == 2 &&
              middle.glyphs[0].glyph_id == whole.glyphs[1].glyph_id &&
              middle.glyphs[0].glyph_id != isolated.glyphs[0].glyph_id,
          "Arabic contextual segment lost neighbors");
    for (std::size_t index = 0; index < whole.glyphs.size(); ++index) {
        check(joined.text.glyphs[index].glyph_id == whole.glyphs[index].glyph_id, "engine Arabic joining changed");
    }
    const auto ligature = engine.shape(String{u8"لا"}.view(), chain);
    check(ligature && ligature.text.glyphs.size() < 2, "Arabic lam-alef ligature missing");

    const auto mirrored = engine.shape(String{u8"(א)"}.view(), chain);
    const auto close_glyph = fonts.glyph_index(latin.font, U')');
    check(mirrored && close_glyph &&
              std::ranges::any_of(mirrored.text.glyphs,
                                  [&](const auto& glyph) {
                                      return glyph.cluster == 0 && glyph.font == latin.font &&
                                             glyph.glyph_id == close_glyph.glyph.glyph_id;
                                  }),
          "RTL bracket was not mirrored by shaping");

    const String controls{u8"A\u2067אב\u2069\u200F\u202B1\u202C\uFEFF"};
    const auto controlled = engine.shape(controls.view(), chain);
    check(bool(controlled), "format controls triggered replacement failure");
    const auto replacement = fonts.glyph_index(latin.font, U'\uFFFD');
    check(bool(replacement), "replacement fixture");
    for (const auto& glyph : controlled.text.glyphs) {
        check(glyph.glyph_id != replacement.glyph.glyph_id || glyph.font != latin.font,
              "default ignorable became replacement glyph");
        const auto scalar = std::ranges::find_if(controlled.text.scalars,
                                                 [&](const auto& value) { return value.byte_start == glyph.cluster; });
        if (scalar != controlled.text.scalars.end() && scalar->value >= 0x200B && scalar->value <= 0xFEFF) {
            check(glyph.advance_x == 0 && glyph.extent_width == 0, "format control contributed visible width");
        }
    }
    const String missing{u8"אב\U0010FFFFA"};
    const auto repaired = engine.shape(missing.view(), chain);
    check(repaired && std::ranges::any_of(repaired.text.glyphs,
                                          [&](const auto& glyph) {
                                              return glyph.cluster == 4 && glyph.font == latin.font &&
                                                     glyph.glyph_id == replacement.glyph.glyph_id;
                                          }),
          "missing glyph lost original byte cluster");

    const auto rtl_digits = engine.shape(String{u8"123"}.view(), chain, TextDirection::RightToLeft);
    check(rtl_digits && rtl_digits.text.bidi.paragraphs()[0].base_level == 1 && rtl_digits.text.runs[0].level == 2 &&
              !rtl_digits.text.runs[0].right_to_left,
          "explicit paragraph direction incorrectly reversed numbers");
    const auto invalid = engine.shape(String{u8"A"}.view(), chain, static_cast<TextDirection>(255));
    check(!invalid && invalid.error.kind == TextErrorKind::invalid_direction, "invalid text direction accepted");
    check(!fonts.shape_utf8_segment(latin.font, "A", 0, 1, {static_cast<FontShapeDirection>(255), 0}),
          "invalid font direction accepted");
    const auto paragraphs = engine.shape(String{u8"A\r\nאב\u2029مرحبا\n"}.view(), chain);
    check(paragraphs && paragraphs.text.paragraphs.size() == 4 && paragraphs.text.paragraphs.back().glyph_count == 0,
          "Unicode hard paragraph boundaries or final empty paragraph");
}
} // namespace

int main() {
    try {
        shaping_contract();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
