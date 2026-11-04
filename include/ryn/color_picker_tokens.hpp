#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>

namespace ryn {
struct ColorPickerThemeToken final {
    float panel_width{234};
    float saturation_height{160};
    float rail_height{12};
    float gap{12};
    float thumb_size{8};
    Color checker_light{1, 1, 1};
    Color checker_dark{.75F, .75F, .75F};
    Color thumb{1, 1, 1};
    Color focus;

    [[nodiscard]] auto colors() const {
        return std::array{checker_light, checker_dark, thumb, focus};
    }

    [[nodiscard]] auto metrics() const {
        return std::array{panel_width, saturation_height, rail_height, gap, thumb_size};
    }

    friend bool operator==(const ColorPickerThemeToken&, const ColorPickerThemeToken&) = default;
};

struct ColorPickerTokenOverride final {
    std::optional<LogicalLength> panel_width;
    std::optional<LogicalLength> saturation_height;
    std::optional<LogicalLength> rail_height;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> thumb_size;
    std::optional<Color> checker_light;
    std::optional<Color> checker_dark;
    std::optional<Color> thumb;
    std::optional<Color> focus;
    friend bool operator==(const ColorPickerTokenOverride&, const ColorPickerTokenOverride&) = default;
};
} // namespace ryn
