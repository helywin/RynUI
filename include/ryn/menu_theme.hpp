#pragma once

#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>
#include <optional>

namespace ryn {
struct MenuColorToken final {
    Color background;
    Color text;
    Color disabled;
    Color selected;
    Color selected_background;
    Color hover_background;
    Color danger;
    Color divider;
    Color group;
    Color focus;

    [[nodiscard]] auto values() const {
        return std::array{background,       text,   disabled, selected, selected_background,
                          hover_background, danger, divider,  group,    focus};
    }

    friend bool operator==(const MenuColorToken&, const MenuColorToken&) = default;
};

struct MenuMetricToken final {
    float item_height{40};
    float item_margin{4};
    float padding_inline{16};
    float inline_indent{24};
    float border_radius{8};
    float group_height{32};
    float divider_height{9};

    [[nodiscard]] auto values() const {
        return std::array{item_height,   item_margin,  padding_inline, inline_indent,
                          border_radius, group_height, divider_height};
    }

    friend bool operator==(const MenuMetricToken&, const MenuMetricToken&) = default;
};

struct MenuTypographyToken final {
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    float font_size{14};
    float line_height{22};
    std::uint32_t font_weight{400};
    friend bool operator==(const MenuTypographyToken&, const MenuTypographyToken&) = default;
};

struct MenuThemeToken final {
    MenuColorToken colors;
    MenuMetricToken metrics;
    MenuTypographyToken typography;
    friend bool operator==(const MenuThemeToken&, const MenuThemeToken&) = default;
};

struct MenuTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> text;
    std::optional<Color> disabled;
    std::optional<Color> selected;
    std::optional<Color> selected_background;
    std::optional<Color> hover_background;
    std::optional<Color> danger;
    std::optional<Color> divider;
    std::optional<Color> group;
    std::optional<Color> focus;
    std::optional<LogicalLength> item_height;
    std::optional<LogicalLength> item_margin;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> inline_indent;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> group_height;
    std::optional<LogicalLength> divider_height;
    std::optional<SystemFontFamily> font_family;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<std::uint32_t> font_weight;
    friend bool operator==(const MenuTokenOverride&, const MenuTokenOverride&) = default;
};

struct MenuThemeConfig final {
    MenuTokenOverride tokens;
    friend bool operator==(const MenuThemeConfig&, const MenuThemeConfig&) = default;
};

struct DropdownMetricToken final {
    float padding{4};
    float item_height{32};
    float min_width{160};
    float max_height{320};
    float gap{4};
    float border_radius{8};

    [[nodiscard]] auto values() const {
        return std::array{padding, item_height, min_width, max_height, gap, border_radius};
    }

    friend bool operator==(const DropdownMetricToken&, const DropdownMetricToken&) = default;
};

struct DropdownThemeToken final {
    Color background;
    DropdownMetricToken metrics;
    ShadowList shadow;
    std::int32_t z_index_popup{1050};
    friend bool operator==(const DropdownThemeToken&, const DropdownThemeToken&) = default;
};

struct DropdownTokenOverride final {
    std::optional<Color> background;
    std::optional<LogicalLength> padding;
    std::optional<LogicalLength> item_height;
    std::optional<LogicalLength> min_width;
    std::optional<LogicalLength> max_height;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> border_radius;
    std::optional<ShadowList> shadow;
    std::optional<std::int32_t> z_index_popup;
    friend bool operator==(const DropdownTokenOverride&, const DropdownTokenOverride&) = default;
};

struct DropdownThemeConfig final {
    DropdownTokenOverride tokens;
    friend bool operator==(const DropdownThemeConfig&, const DropdownThemeConfig&) = default;
};
} // namespace ryn
