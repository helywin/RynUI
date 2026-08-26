#include "graphics/glyph_scene.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using ryn::String;
using ryn::font::FontIdentity;
using ryn::font::FontRuntime;
using ryn::graphics::GlyphInstance;
using ryn::graphics::GlyphInstanceRange;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.00001F;
}

struct Fixture final {
    Fixture()
        : fonts(create_runtime()),
          text_engine(*fonts) {
        const auto latin_load = fonts->load_font_file(
            RYNUI_VALIDATION_LATIN_FONT, 0, 14);
        const auto cjk_load = fonts->load_font_file(
            RYNUI_VALIDATION_CJK_FONT, 0, 14);
        require(latin_load && cjk_load, "glyph scene fonts failed to load");
        chain = {latin_load.font, cjk_load.font};
    }

    static std::unique_ptr<FontRuntime> create_runtime() {
        auto created = FontRuntime::create();
        require(static_cast<bool>(created), "Font Runtime initialization failed");
        return std::move(created.runtime);
    }

    std::unique_ptr<FontRuntime> fonts;
    ryn::text::TextEngine text_engine;
    std::array<FontIdentity, 2> chain{};
};

GlyphInstance instance(float marker) {
    GlyphInstance result;
    result.position_size[0] = marker;
    return result;
}

void test_instance_layout_and_text_positioning() {
    constexpr std::array expected_bindings{
        ryn::graphics::GlyphAttributeBinding{0, ryn::graphics::GlyphAttributeFormat::float4, 0},
        ryn::graphics::GlyphAttributeBinding{1, ryn::graphics::GlyphAttributeFormat::float4, 16},
        ryn::graphics::GlyphAttributeBinding{2, ryn::graphics::GlyphAttributeFormat::float4, 32},
        ryn::graphics::GlyphAttributeBinding{3, ryn::graphics::GlyphAttributeFormat::float4, 48},
        ryn::graphics::GlyphAttributeBinding{4, ryn::graphics::GlyphAttributeFormat::float4, 64},
    };
    require(sizeof(GlyphInstance) == 80
                && ryn::graphics::glyph_attribute_bindings == expected_bindings
                && ryn::graphics::glyph_vertex_count == 6,
            "Glyph instance layout does not match the shader contract");

    Fixture fixture;
    const String content = u8"A 中";
    const auto shaped = fixture.text_engine.shape(content.view(), fixture.chain);
    const auto measured = fixture.text_engine.measure(
        shaped.text,
        {20.0F, std::numeric_limits<float>::infinity()});
    require(shaped && measured, "glyph scene text fixture failed");

    ryn::graphics::GlyphAtlas atlas;
    ryn::graphics::GlyphScene scene;
    const ryn::graphics::GlyphPlacement placement{
        {10.0F, 20.0F},
        {400.0F, 200.0F},
        {0.0F, 0.0F, 400.0F, 200.0F},
        {4.0F, 2.0F},
        {0.2F, 0.4F, 0.6F, 0.8F},
        0.5F,
    };
    const auto result = scene.append_text(
        *fixture.fonts, atlas, shaped.text, measured.measurement, placement);
    require(result && result.primitive.instances.count == 2
                && scene.instances().size() == 2,
            "visible glyph scene instance count is incorrect");
    require(atlas.entry_count() == 3,
            "space glyph was not cached as an explicit empty entry");

    const auto& first = scene.instances().at(result.primitive.instances.first);
    const auto first_atlas = atlas.ensure(
        *fixture.fonts, shaped.text.glyphs.front().font,
        shaped.text.glyphs.front().glyph_id);
    require(first_atlas && !first_atlas.entry->empty,
            "first glyph atlas entry is unavailable");
    const float expected_left = placement.origin_pixels.x
        + shaped.text.glyphs.front().offset_x
        + static_cast<float>(first_atlas.entry->bearing_x);
    const float expected_top = placement.origin_pixels.y
        + measured.measurement.lines.front().baseline
        - shaped.text.glyphs.front().offset_y
        - static_cast<float>(first_atlas.entry->bearing_y);
    require(near(first.position_size[0], -1.0F + 2.0F * expected_left / 400.0F)
                && near(first.position_size[1], 1.0F - 2.0F * expected_top / 200.0F),
            "Glyph instance did not apply baseline, bearing, and shaping offset");
    require(first.clip_bounds == std::array<float, 4>{-1.0F, 1.0F, 1.0F, -1.0F}
                && first.color == placement.color
                && near(first.translation_opacity[0], 0.02F)
                && near(first.translation_opacity[1], -0.02F)
                && first.translation_opacity[2] == 0.5F,
            "Glyph instance lost clip, translation, color, or opacity");
    require(scene.instances().at(result.primitive.instances.first + 1).position_size[0]
                > first.position_size[0],
            "space advance did not move the following visible glyph");
    require(atlas.dirty_regions().size() == 2,
            "space glyph unexpectedly dirtied the atlas texture");

    atlas.clear_dirty_regions();
    const std::size_t rasterizations = fixture.fonts->counters().rasterizations;
    require(scene.instances().update_material(
                result.primitive.instances,
                {0.8F, 0.2F, 0.1F, 1.0F},
                0.7F) == 2,
            "Glyph Material-only update did not reach visible instances");
    require(atlas.dirty_regions().empty()
                && fixture.fonts->counters().rasterizations == rasterizations,
            "Glyph Material-only update changed atlas or raster state");
    scene.instances().clear_dirty_ranges();
    require(scene.instances().update_geometry(
                result.primitive.instances,
                {-0.5F, 0.5F, 0.5F, -0.5F},
                {0.05F, -0.05F}) == 2,
            "Glyph translation/clip update did not reach visible instances");
    require(atlas.dirty_regions().empty()
                && fixture.fonts->counters().rasterizations == rasterizations,
            "Glyph translation/clip update caused raster or atlas work");
}

void test_dirty_ranges_remain_layered_and_sparse() {
    ryn::graphics::GlyphInstanceStore store;
    const std::array initial{
        instance(0.0F), instance(1.0F), instance(2.0F),
        instance(3.0F), instance(4.0F), instance(5.0F),
    };
    const auto all = store.append(initial);
    require(all == GlyphInstanceRange{0, 6}, "Glyph instance append range is incorrect");

    const auto original_geometry = store.at(1).position_size;
    require(store.update_material({1, 2}, {0.1F, 0.2F, 0.3F, 0.4F}, 0.6F) == 2
                && store.update_material({4, 1}, {0.1F, 0.2F, 0.3F, 0.4F}, 0.6F) == 1,
            "sparse Glyph Material update count is incorrect");
    require(store.material_dirty_ranges().size() == 2
                && store.material_dirty_ranges()[0] == GlyphInstanceRange{1, 2}
                && store.material_dirty_ranges()[1] == GlyphInstanceRange{4, 1},
            "non-adjacent Material updates expanded into a broad range");
    require(store.at(1).position_size == original_geometry,
            "Material update changed Glyph geometry");

    const auto original_color = store.at(2).color;
    require(store.update_geometry(
                {2, 3},
                {-0.5F, 0.5F, 0.5F, -0.5F},
                {0.1F, -0.2F}) == 3,
            "Glyph geometry update count is incorrect");
    require(store.geometry_dirty_ranges().size() == 1
                && store.geometry_dirty_ranges().front() == GlyphInstanceRange{2, 3},
            "contiguous Glyph geometry update was not merged");
    require(store.at(2).color == original_color,
            "geometry/clip update changed Glyph Material");
    require(store.bytes({1, 2}).size() == 2 * sizeof(GlyphInstance),
            "Glyph dirty byte range has the wrong size");

    store.clear_dirty_ranges();
    require(store.material_dirty_ranges().empty()
                && store.geometry_dirty_ranges().empty(),
            "Glyph dirty ranges did not clear");
}

void test_ordered_scene_preserves_quad_glyph_z_order() {
    ryn::graphics::OrderedScene scene;
    scene.append_quad(0, 1);
    scene.append_glyph({0, {0, 2}});
    scene.append_glyph({0, {2, 1}});
    scene.append_glyph({1, {3, 2}});
    scene.append_quad(1, 1);

    const auto commands = scene.commands();
    require(commands.size() == 4,
            "ordered Scene merged across pipeline or atlas boundaries");
    require(commands[0] == ryn::graphics::SceneDrawCommand{
                ryn::graphics::SceneDrawKind::quad,
                0,
                1,
                ryn::graphics::invalid_glyph_atlas_page}
                && commands[1] == ryn::graphics::SceneDrawCommand{
                    ryn::graphics::SceneDrawKind::glyph, 0, 3, 0}
                && commands[2] == ryn::graphics::SceneDrawCommand{
                    ryn::graphics::SceneDrawKind::glyph, 3, 2, 1}
                && commands[3] == ryn::graphics::SceneDrawCommand{
                    ryn::graphics::SceneDrawKind::quad,
                    1,
                    1,
                    ryn::graphics::invalid_glyph_atlas_page},
            "ordered Scene changed Quad/Glyph/page Z order");
}

} // namespace

int main() {
    try {
        test_instance_layout_and_text_positioning();
        test_dirty_ranges_remain_layered_and_sparse();
        test_ordered_scene_preserves_quad_glyph_z_order();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
