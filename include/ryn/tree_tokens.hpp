#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct TreeColorToken final {
    Color background;
    Color text;
    Color disabled;
    Color node_hover_background;
    Color node_hover_text;
    Color node_selected_background;
    Color node_selected_text;
    Color checkbox_border;
    Color checkbox_background;
    Color checkbox_checked;
    Color checkbox_mark;
    Color focus;

    [[nodiscard]] auto values() const {
        return std::array{background,
                          text,
                          disabled,
                          node_hover_background,
                          node_hover_text,
                          node_selected_background,
                          node_selected_text,
                          checkbox_border,
                          checkbox_background,
                          checkbox_checked,
                          checkbox_mark,
                          focus};
    }

    friend bool operator==(const TreeColorToken&, const TreeColorToken&) = default;
};

struct TreeMetricToken final {
    float title_height{};
    float switcher_size{};
    float indent_size{};
    float row_gap{};
    float icon_size{};
    float icon_gap{};
    float padding_inline{};
    float border_radius{};
    float checkbox_size{};
    float border_width{};
    float focus_width{};
    float focus_offset{};
    float font_size{};
    float line_height{};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{title_height, switcher_size,  indent_size,   row_gap,       icon_size,
                          icon_gap,     padding_inline, border_radius, checkbox_size, border_width,
                          focus_width,  focus_offset,   font_size,     line_height};
    }

    friend bool operator==(const TreeMetricToken&, const TreeMetricToken&) = default;
};

struct TreeThemeToken final {
    TreeColorToken colors;
    TreeMetricToken metrics;
    friend bool operator==(const TreeThemeToken&, const TreeThemeToken&) = default;
};

struct TreeTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> text;
    std::optional<Color> disabled;
    std::optional<Color> node_hover_background;
    std::optional<Color> node_hover_text;
    std::optional<Color> node_selected_background;
    std::optional<Color> node_selected_text;
    std::optional<Color> checkbox_border;
    std::optional<Color> checkbox_background;
    std::optional<Color> checkbox_checked;
    std::optional<Color> checkbox_mark;
    std::optional<Color> focus;
    std::optional<LogicalLength> title_height;
    std::optional<LogicalLength> switcher_size;
    std::optional<LogicalLength> indent_size;
    std::optional<LogicalLength> row_gap;
    std::optional<LogicalLength> icon_size;
    std::optional<LogicalLength> icon_gap;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> checkbox_size;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<SystemFontFamily> font_family;
    friend bool operator==(const TreeTokenOverride&, const TreeTokenOverride&) = default;
};
} // namespace ryn
