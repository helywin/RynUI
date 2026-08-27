#include "graphics/rounded_effect.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ryn::ShadowLayer shadow(ryn::ShadowKind kind, std::uint8_t marker) {
    return {kind, {}, 2.0F, 0.0F, ryn::Color::rgba8(marker, marker, marker, 80)};
}

void test_shadow_list_order_and_cross_surface_composition() {
    ryn::graphics::RoundedEffectScene scene;
    const ryn::graphics::LogicalRoundedRect shape{{10.0F, 10.0F, 80.0F, 32.0F}, 6.0F};
    const auto first = scene.append_shadow_list(
        shape,
        {shadow(ryn::ShadowKind::outer, 10),
         shadow(ryn::ShadowKind::inset, 20),
         shadow(ryn::ShadowKind::outer, 30)});
    const auto second = scene.append_outline(
        {{110.0F, 10.0F, 80.0F, 32.0F}, 6.0F},
        2.0F,
        2.0F,
        ryn::Color::rgba8(22, 119, 255));
    require(scene.store().compact({0.0F, 0.0F, 240.0F, 80.0F}),
            "rounded-effect Scene failed to compact");

    std::vector<ryn::graphics::SceneDrawCommand> commands;
    scene.compose_surface(first, {
        ryn::graphics::SceneDrawKind::quad,
        10,
        1,
        ryn::graphics::invalid_glyph_atlas_page,
    }, commands);
    scene.compose_surface(second, {
        ryn::graphics::SceneDrawKind::quad,
        11,
        1,
        ryn::graphics::invalid_glyph_atlas_page,
    }, commands);
    require(commands.size() == 6
                && commands[0] == ryn::graphics::SceneDrawCommand{
                    ryn::graphics::SceneDrawKind::rounded_effect, 0, 1,
                    ryn::graphics::invalid_glyph_atlas_page}
                && commands[1] == ryn::graphics::SceneDrawCommand{
                    ryn::graphics::SceneDrawKind::rounded_effect, 2, 1,
                    ryn::graphics::invalid_glyph_atlas_page}
                && commands[2].kind == ryn::graphics::SceneDrawKind::quad
                && commands[3] == ryn::graphics::SceneDrawCommand{
                    ryn::graphics::SceneDrawKind::rounded_effect, 1, 1,
                    ryn::graphics::invalid_glyph_atlas_page}
                && commands[4] == ryn::graphics::SceneDrawCommand{
                    ryn::graphics::SceneDrawKind::rounded_effect, 3, 1,
                    ryn::graphics::invalid_glyph_atlas_page}
                && commands[5].kind == ryn::graphics::SceneDrawKind::quad,
            "outer/fill/inset or cross-component paint order changed");
}

void test_clip_empty_destroy_reuse_and_button_like_outline() {
    ryn::graphics::RoundedEffectScene scene;
    const auto empty = scene.append_shadow_list(
        {{0.0F, 0.0F, 20.0F, 20.0F}, 4.0F}, {});
    require(empty.before_fill.empty() && empty.after_fill.empty()
                && scene.store().live_count() == 0,
            "empty ShadowList allocated a retained effect");

    const auto clipped = scene.append_shadow_list(
        {{0.0F, 0.0F, 20.0F, 20.0F}, 4.0F},
        {shadow(ryn::ShadowKind::outer, 10)},
        {},
        ryn::graphics::EffectClip{4, {100.0F, 100.0F, 10.0F, 10.0F}});
    const auto outline = scene.append_outline(
        {{30.0F, 10.0F, 80.0F, 32.0F}, 6.0F},
        2.0F,
        2.0F,
        ryn::Color::rgba8(22, 119, 255));
    require(scene.store().compact({0.0F, 0.0F, 160.0F, 80.0F})
                && !scene.store().packed_index(clipped.before_fill.front()).has_value(),
            "ancestor-clipped Scene effect remained packed");

    std::vector<ryn::graphics::SceneDrawCommand> commands;
    scene.compose_surface(outline, {
        ryn::graphics::SceneDrawKind::quad,
        7,
        1,
        ryn::graphics::invalid_glyph_atlas_page,
    }, commands);
    require(commands.size() == 2
                && commands.front().kind == ryn::graphics::SceneDrawKind::rounded_effect
                && commands.back().kind == ryn::graphics::SceneDrawKind::quad,
            "Button-like focus outline was not painted before its surface fill");

    const auto old_id = outline.before_fill.front();
    require(scene.remove(outline) && !scene.store().contains(old_id),
            "destroying a rounded-effect primitive left retained state");
    const auto replacement = scene.append_outline(
        {{30.0F, 10.0F, 80.0F, 32.0F}, 6.0F},
        1.0F,
        1.0F,
        ryn::Color::rgba8(22, 119, 255));
    require(replacement.before_fill.front().index == old_id.index
                && replacement.before_fill.front().generation != old_id.generation,
            "destroyed rounded-effect slot did not safely reuse its identity");
    static_cast<void>(scene.store().compact({0.0F, 0.0F, 160.0F, 80.0F}));
    commands.clear();
    scene.compose_surface(outline, {
        ryn::graphics::SceneDrawKind::quad,
        8,
        1,
        ryn::graphics::invalid_glyph_atlas_page,
    }, commands);
    require(commands.size() == 1 && commands.front().kind == ryn::graphics::SceneDrawKind::quad,
            "stale primitive drew a reused rounded-effect identity");
}

} // namespace

int main() {
    try {
        test_shadow_list_order_and_cross_surface_composition();
        test_clip_empty_destroy_reuse_and_button_like_outline();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
