#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct TagPalette final {
    Color background;
    Color border;
    Color text;
    friend bool operator==(const TagPalette&, const TagPalette&) = default;
};

struct TagColorToken final {
    TagPalette normal;
    std::array<TagPalette, 13> presets;
    std::array<TagPalette, 6> statuses;
    Color primary;
    Color primary_hover;
    Color primary_active;
    Color checked_text;
    Color hover_background;
    Color disabled_background;
    Color disabled_text;
    Color close;
    Color close_hover;
    Color focus;
    friend bool operator==(const TagColorToken&, const TagColorToken&) = default;
};

struct TagMetricToken final {
    float padding_inline{7};
    float icon_gap{7};
    float close_gap{3};
    float close_size{10};
    float border_width{1};
    float border_radius{4};
    float font_size{12};
    float line_height{20};
    float focus_width{3};
    float focus_offset{1};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{padding_inline, icon_gap,  close_gap,   close_size,  border_width,
                          border_radius,  font_size, line_height, focus_width, focus_offset};
    }

    friend bool operator==(const TagMetricToken&, const TagMetricToken&) = default;
};

struct TagThemeToken final {
    TagColorToken colors;
    TagMetricToken metrics;
    friend bool operator==(const TagThemeToken&, const TagThemeToken&) = default;
};

struct TagTokenOverride final {
    std::optional<Color> default_background;
    std::optional<Color> default_color;
    std::optional<Color> default_border;
    std::optional<Color> checked_background;
    std::optional<Color> checked_color;
    std::optional<Color> close_color;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> icon_gap;
    std::optional<LogicalLength> close_size;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    friend bool operator==(const TagTokenOverride&, const TagTokenOverride&) = default;
};

struct BadgeColorToken final {
    Color background;
    Color count_text;
    Color text;
    Color outline;
    std::array<Color, 6> statuses;
    friend bool operator==(const BadgeColorToken&, const BadgeColorToken&) = default;
};

struct BadgeMetricToken final {
    float indicator_height{20};
    float indicator_height_small{14};
    float dot_size{6};
    float status_size{6};
    float padding_inline{8};
    float text_gap{8};
    float font_size{12};
    float text_font_size{14};
    float text_line_height{22};
    float outline_width{1};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    std::uint32_t font_weight{400};

    [[nodiscard]] auto values() const {
        return std::array{
            indicator_height, indicator_height_small, dot_size,         status_size,  padding_inline, text_gap,
            font_size,        text_font_size,         text_line_height, outline_width};
    }

    friend bool operator==(const BadgeMetricToken&, const BadgeMetricToken&) = default;
};

struct BadgeThemeToken final {
    BadgeColorToken colors;
    BadgeMetricToken metrics;
    friend bool operator==(const BadgeThemeToken&, const BadgeThemeToken&) = default;
};

struct BadgeTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> count_color;
    std::optional<Color> text_color;
    std::optional<Color> outline_color;
    std::optional<LogicalLength> indicator_height;
    std::optional<LogicalLength> indicator_height_small;
    std::optional<LogicalLength> dot_size;
    std::optional<LogicalLength> status_size;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> font_size;
    std::optional<std::uint32_t> font_weight;
    friend bool operator==(const BadgeTokenOverride&, const BadgeTokenOverride&) = default;
};
} // namespace ryn
