#pragma once
#include <ryn/design_token.hpp>
#include <optional>

namespace ryn {
struct FloatButtonTokenOverride final {
    std::optional<LogicalLength> size;
    std::optional<LogicalLength> icon_size;
    std::optional<LogicalLength> content_font_size;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> inset_inline;
    std::optional<LogicalLength> inset_block;
    std::optional<Color> background;
    std::optional<Color> primary_background;
    std::optional<Color> color;
    std::optional<Color> primary_color;
    std::optional<ShadowList> shadow;
    friend bool operator==(const FloatButtonTokenOverride&, const FloatButtonTokenOverride&) = default;
};

struct FloatButtonThemeConfig final {
    FloatButtonTokenOverride tokens;
    friend bool operator==(const FloatButtonThemeConfig&, const FloatButtonThemeConfig&) = default;
};

struct FloatButtonThemeToken final {
    float size{40};
    float icon_size{18};
    float content_font_size{12};
    float border_radius{8};
    float gap{16};
    float inset_inline{24};
    float inset_block{48};
    Color background;
    Color primary_background;
    Color color;
    Color primary_color;
    ShadowList shadow;
    friend bool operator==(const FloatButtonThemeToken&, const FloatButtonThemeToken&) = default;
};
} // namespace ryn
