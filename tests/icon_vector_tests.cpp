// This header must compile first, with no umbrella/transitive Color include.
#include <ryn/icon.hpp>
#include "support/input_fixture.hpp"
#include "font/icon_vector_font.hpp"
#include "icons/icon_vector_data.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <type_traits>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;
static_assert(std::copy_constructible<IconVector> && std::copy_constructible<IconSource>);
static_assert(std::is_same_v<decltype(std::declval<const IconVector&>().paths()), std::span<const IconPath>>);

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F action) {
    bool rejected{};
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid Icon vector accepted");
}

IconPath rectangle(IconColorRole role = IconColorRole::Primary) {
    return {role, {IconMove{{10, 10}}, IconLine{{90, 10}}, IconLine{{90, 90}}, IconLine{{10, 90}}, IconClose{}}};
}

IconVector vector_icon() {
    auto hole = rectangle();
    hole.commands.insert(hole.commands.end(),
                         {IconMove{{35, 35}}, IconLine{{35, 65}}, IconLine{{65, 65}}, IconLine{{65, 35}}, IconClose{}});
    return {
        {0, 0, 100, 100},
        {hole,
         {IconColorRole::Secondary,
          {IconMove{{10, 75}}, IconQuadratic{{50, 20}, {90, 75}}, IconLine{{90, 90}}, IconLine{{10, 90}}, IconClose{}},
          .7F},
         {IconColorRole::Primary,
          {IconMove{{20, 20}}, IconCubic{{20, 0}, {80, 0}, {80, 20}}, IconLine{{80, 25}}, IconLine{{20, 25}},
           IconClose{}}}}};
}

std::uint32_t word(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint32_t value{};
    for (std::size_t index = 0; index < 4; ++index) {
        value = (value << 8) | std::to_integer<std::uint8_t>(bytes[offset + index]);
    }
    return value;
}

void deterministic_font_and_real_coverage() {
    const auto vector = vector_icon();
    const IconSource source{vector};
    const auto& data = *detail::IconSourceAccess::vector(source);
    const auto repeated = font::build_icon_vector_font(vector.view_box(), vector.paths());
    check(repeated == data.font_bytes && repeated.size() < 2 * 1024 * 1024 && word(repeated, 0) == 0x4F54544F,
          "vector font bytes are not bounded deterministic OpenType/CFF");
    std::uint32_t sum{};
    for (std::size_t offset = 0; offset < repeated.size(); offset += 4) {
        sum += word(repeated, offset);
    }
    check(sum == 0xB1B0AFBA, "OpenType whole-file checksum differs");
    Fixture f;
    const auto reference = f.resolve(50).front();
    const auto loaded = f.scene.icon_font(source, reference, 50);
    check(f.scene.icon_font(source, reference, 50) == loaded &&
              f.scene.icon_font(IconSource{vector}, reference, 50) == loaded,
          "immutable source identity did not share the font cache");
    for (std::size_t layer = 0; layer < data.layers.size(); ++layer) {
        const auto lookup = f.fonts->glyph_index(loaded, data.layers[layer].codepoint);
        check(static_cast<bool>(lookup) && lookup.glyph.glyph_id == layer + 1, "custom cmap did not map a layer");
        const auto raster = f.fonts->rasterize(loaded, lookup.glyph.glyph_id);
        check(static_cast<bool>(raster) && raster.glyph && raster.glyph->width > 0 && raster.glyph->height > 0 &&
                  std::ranges::any_of(raster.glyph->coverage, [](auto value) { return value > 0; }),
              "Bezier layer did not produce real FreeType coverage");
        if (layer == 0) {
            const auto& bitmap = *raster.glyph;
            const auto center = (bitmap.height / 2) * bitmap.row_stride + bitmap.width / 2;
            check(bitmap.coverage[center] == 0 && bitmap.coverage[bitmap.row_stride * 3 + 3] > 0,
                  "opposite winding did not retain a transparent hole");
        }
    }
    const std::array chain{loaded};
    const auto shaped = f.engine.shape(String{u8"\uE000"}.view(), chain);
    check(static_cast<bool>(shaped) && shaped.text.glyphs.size() == 1 &&
              std::abs(shaped.text.glyphs[0].advance_x - 50) < .01F,
          "custom em/advance did not shape through HarfBuzz");
    const IconSource wide{IconVector{
        {10, -10, 200, 100},
        {{IconColorRole::Primary,
          {IconMove{{10, -10}}, IconLine{{210, -10}}, IconLine{{210, 90}}, IconLine{{10, 90}}, IconClose{}}}}}};
    const auto wide_font = f.scene.icon_font(wide, reference, 50);
    const auto lookup = f.fonts->glyph_index(wide_font, 0xE000);
    const auto raster = f.fonts->rasterize(wide_font, lookup.glyph.glyph_id);
    check(static_cast<bool>(raster) && std::abs(static_cast<int>(raster.glyph->width) - 50) <= 1 &&
              std::abs(static_cast<int>(raster.glyph->height) - 25) <= 1,
          "non-square viewBox did not preserve aspect ratio and letterbox");
    check(std::abs(raster.glyph->bearing_x + raster.glyph->width / 2.0F - 25) <= 1 &&
              std::abs(raster.glyph->bearing_y - raster.glyph->height / 2.0F - 18.75F) <= 1,
          "nonzero viewBox origin did not center the shape");
}

void bounds_and_invalid_definitions() {
    const IconViewBox box{0, 0, 100, 100};
    const auto valid = rectangle();
    rejects([&] { IconVector{{0, 0, 0, 100}, {valid}}; });
    rejects([&] { IconVector{{0, 0, 100, -1}, {valid}}; });
    rejects([&] { IconVector{{std::numeric_limits<float>::infinity(), 0, 100, 100}, {valid}}; });
    rejects([&] { IconVector{box, {}}; });
    rejects([&] { IconVector{box, std::vector<IconPath>(65, valid)}; });
    rejects([&] { IconVector{box, {{IconColorRole::Primary, {IconLine{{10, 10}}, IconClose{}}}}}; });
    rejects([&] { IconVector{box, {{IconColorRole::Primary, {IconMove{{10, 10}}, IconClose{}}}}}; });
    rejects([&] { IconVector{box, {{IconColorRole::Primary, {IconMove{{10, 10}}, IconLine{{20, 20}}}}}}; });
    rejects([&] {
        IconVector{
            box, {{IconColorRole::Primary, {IconMove{{10, 10}}, IconLine{{20, 20}}, IconMove{{30, 30}}, IconClose{}}}}};
    });
    auto invalid = valid;
    invalid.commands[1] = IconCubic{{std::numeric_limits<float>::quiet_NaN(), 0}, {1, 1}, {2, 2}};
    rejects([&] { IconVector{box, {invalid}}; });
    invalid = valid;
    invalid.opacity = -1;
    rejects([&] { IconVector{box, {invalid}}; });
    invalid = valid;
    invalid.color = static_cast<IconColorRole>(99);
    rejects([&] { IconVector{box, {invalid}}; });
    invalid = valid;
    invalid.commands[0] = IconMove{{std::numeric_limits<float>::max(), 10}};
    rejects([&] { IconVector{box, {invalid}}; });
    auto longest = IconPath{IconColorRole::Primary, {IconMove{{50, 0}}}};
    for (std::size_t index = 0; index < 4094; ++index) {
        const float offset = static_cast<float>(index % 70) + .12345F;
        longest.commands.push_back(IconCubic{{offset, 5.678F}, {95.432F, offset}, {50.123F, offset}});
    }
    longest.commands.push_back(IconClose{});
    const IconSource large{IconVector{box, {longest}}};
    Fixture f;
    const auto loaded = f.scene.icon_font(large, f.resolve(48).front(), 48);
    const auto lookup = f.fonts->glyph_index(loaded, 0xE000);
    const auto raster = f.fonts->rasterize(loaded, lookup.glyph.glyph_id);
    check(static_cast<bool>(raster) && !raster.glyph->coverage.empty(), "4096-command contour exceeded Type2 limits");
    longest.commands.push_back(IconClose{});
    rejects([&] { IconVector{box, {longest}}; });
    const IconSource many{IconVector{box, std::vector<IconPath>(64, valid)}};
    const auto many_font = f.scene.icon_font(many, f.resolve(48).front(), 48);
    const auto last = f.fonts->glyph_index(many_font, 0xE03F);
    check(static_cast<bool>(last) && last.glyph.glyph_id == 64, "64-layer source did not expose the final layer");
}

void retained_source_switch_and_cleanup() {
    Fixture f;
    const IconSource custom{vector_icon()};
    Signal<IconSource> source{IconSource{IconName::EyeOutlined}};
    int runs{};
    f.services.mount(Content{[&] {
        ++runs;
        Icon(IconProps{}.source(source));
        Text(u8"sibling");
    }});
    f.synchronize();
    const auto texts = f.services.text().mounted_texts();
    const auto component = texts[0].component;
    const auto node = f.services.components().root(component);
    const auto primary = texts[0].scene;
    const auto sibling_shapes = f.scene.text_state(texts[1].scene).counters().shape_count;
    source.set(custom);
    f.synchronize();
    const auto layers = f.services.text().icon_snapshot(component).layers;
    check(layers.size() == 3 && layers[0] == primary && runs == 1 &&
              f.scene.text_state(texts[1].scene).counters().shape_count == sibling_shapes,
          "custom source switch replaced the Component or reshaped the sibling");
    const auto custom_font = f.scene.text_state(primary).shaped().glyphs[0].font;
    for (const auto layer : layers) {
        check(f.scene.node(layer) == node && f.scene.text_state(layer).shaped().glyphs[0].font == custom_font,
              "custom source layers diverged in node/font ownership");
    }
    const auto second = f.scene.primitive(layers[1]).instances;
    check(std::abs(f.scene.glyph_scene().instances().at(second.first).translation_opacity[2] - .7F) < .001F,
          "custom path opacity was lost");
    source.set(IconSource{IconName::WalletTwoTone});
    f.synchronize();
    check(f.scene.text_state(primary).shaped().glyphs[0].font != custom_font,
          "built-in source retained custom font/codepoints");
    source.set(custom);
    f.synchronize();
    check(f.scene.text_state(primary).shaped().glyphs[0].font == custom_font, "source reuse missed font cache");
    rejects([&] { source.set(IconSource{IconVector{{0, 0, -1, 1}, {rectangle()}}}); });
    check(f.services.text().icon_snapshot(component).source == custom, "invalid definition replaced retained source");
    f.font_scale = 2;
    f.chains.clear();
    static_cast<void>(f.services.text().set_font_resolver(
        [&](SystemFontFamily, std::uint32_t, bool, std::uint32_t pixels) { return f.resolve(pixels); }));
    f.synchronize();
    const auto scaled_font = f.scene.text_state(primary).shaped().glyphs[0].font;
    check(scaled_font != custom_font && f.fonts->metrics(scaled_font).metrics.display_scale == 2,
          "custom source did not receive DPI-specific raster identity");
    check(f.services.destroy(component) && f.scene.size() == 1, "custom Icon destroy leaked layer scenes");

    auto fonts = std::move(font::FontRuntime::create().runtime);
    const auto reference = fonts->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, font::FontRasterConfig{24, 1});
    text::TextEngine engine{*fonts};
    runtime::FrameRequestState frames;
    font::FontIdentity owned;
    font::FontIdentity bundled;
    {
        detail::TextSceneService scene{*fonts, engine, frames};
        owned = scene.icon_font(custom, reference.font, 24);
        bundled = scene.icon_font(reference.font, 24);
        check(static_cast<bool>(fonts->metrics(owned)) && static_cast<bool>(fonts->metrics(bundled)),
              "window-owned icon fonts were not live");
    }
    check(!fonts->metrics(owned) && !fonts->metrics(bundled) && fonts->metrics(reference.font) &&
              fonts->counters().faces_released == 4 && fonts->counters().byte_resources_released == 2,
          "TextSceneService teardown retained cached icon font bytes/faces or removed an external font");
}
} // namespace

int main() {
    try {
        deterministic_font_and_real_coverage();
        bounds_and_invalid_definitions();
        retained_source_switch_and_cleanup();
        std::cout
            << "Typed vectors: deterministic CFF, FreeType/HarfBuzz, holes, bounds, retained source, DPI, cleanup\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
