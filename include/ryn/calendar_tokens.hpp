#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct CalendarColorToken final {
    Color full_background;
    Color panel_background;
    Color text;
    Color muted;
    Color disabled;
    Color selected;
    Color selected_text;
    Color item_active_background;
    Color hover;
    Color today;
    Color border;
    Color focus;

    [[nodiscard]] auto values() const {
        return std::array{full_background, panel_background,       text,  muted, disabled, selected,
                          selected_text,   item_active_background, hover, today, border,   focus};
    }

    friend bool operator==(const CalendarColorToken&, const CalendarColorToken&) = default;
};

struct CalendarMetricToken final {
    float panel_width{700};
    float mini_width{300};
    float mini_content_height{256};
    float padding{8};
    float header_height{48};
    float year_control_width{80};
    float month_control_width{70};
    float cell_height{116};
    float mini_cell_height{36};
    float cell_padding{8};
    float gap{4};
    float radius{8};
    float line_width{1};
    float focus_width{2};
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{
            panel_width,         mini_width,  mini_content_height, padding,      header_height, year_control_width,
            month_control_width, cell_height, mini_cell_height,    cell_padding, gap,           radius,
            line_width,          focus_width, font_size,           line_height};
    }

    friend bool operator==(const CalendarMetricToken&, const CalendarMetricToken&) = default;
};

struct CalendarThemeToken final {
    CalendarColorToken colors;
    CalendarMetricToken metrics;
    ShadowList shadow;
    friend bool operator==(const CalendarThemeToken&, const CalendarThemeToken&) = default;
};

struct CalendarTokenOverride final {
    std::optional<Color> full_background;
    std::optional<Color> panel_background;
    std::optional<Color> text;
    std::optional<Color> muted;
    std::optional<Color> disabled;
    std::optional<Color> selected;
    std::optional<Color> selected_text;
    std::optional<Color> item_active_background;
    std::optional<Color> hover;
    std::optional<Color> today;
    std::optional<Color> border;
    std::optional<Color> focus;
    std::optional<LogicalLength> panel_width;
    std::optional<LogicalLength> mini_width;
    std::optional<LogicalLength> mini_content_height;
    std::optional<LogicalLength> padding;
    std::optional<LogicalLength> header_height;
    std::optional<LogicalLength> year_control_width;
    std::optional<LogicalLength> month_control_width;
    std::optional<LogicalLength> cell_height;
    std::optional<LogicalLength> mini_cell_height;
    std::optional<LogicalLength> cell_padding;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> radius;
    std::optional<LogicalLength> line_width;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<SystemFontFamily> font_family;
    std::optional<ShadowList> shadow;
    friend bool operator==(const CalendarTokenOverride&, const CalendarTokenOverride&) = default;
};
} // namespace ryn
