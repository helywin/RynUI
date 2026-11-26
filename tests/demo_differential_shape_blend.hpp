#pragma once

#include "renderer/sdl/locked_shape_coverage.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace rynui::example {

struct ShapeBlendSource {
    ryn::detail::LockedShapeGeometry caster;
    ryn::detail::LockedShapeGeometry fill;
    std::optional<ryn::detail::LockedShapeGeometry> border;
    std::array<std::uint8_t, 3> shadow_color;
    std::array<std::uint8_t, 3> fill_color;
    std::array<std::uint8_t, 3> border_color;
    std::uint32_t width;
    std::uint32_t height;
    bool split_border;
    bool split_fill{};
    float isolated_opacity{};
};

struct ShapeBlendSample {
    std::uint32_t x;
    std::uint32_t y;
    std::array<std::uint8_t, 3> web;
    std::array<std::uint8_t, 3> native;
};

struct ShadowGroupSource {
    ryn::runtime::Rect caster;
    float radius{};
    float caster_inset{};

    struct Shadow {
        ryn::runtime::Point offset;
        float blur{};
        float spread{};
        float alpha{};
    };

    std::vector<Shadow> shadows;
    std::uint32_t width{};
    std::uint32_t height{};
};

// Reconstruct RGB8 source-over on white independently for the two raster paths.
[[nodiscard]] std::vector<ShapeBlendSample> shape_blend_samples(const ShapeBlendSource& source);

// Replay a verified isolated Chromium layer's black shadows into transparent N32.
[[nodiscard]] std::vector<std::uint8_t> shadow_group_alpha(const ShadowGroupSource& source);

} // namespace rynui::example
