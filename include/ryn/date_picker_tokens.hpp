#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct DatePickerColorToken final {
    Color background;
    Color text;
    Color muted;
    Color disabled;
    Color selected;
    Color selected_text;
    Color hover;
    Color selector_background;
    Color hover_border;
    Color active_border;

    [[nodiscard]] auto values() const {
        return std::array{background,          text,         muted,        disabled, selected, selected_text, hover,
                          selector_background, hover_border, active_border};
    }

    friend bool operator==(const DatePickerColorToken&, const DatePickerColorToken&) = default;
};

struct DatePickerMetricToken final {
    float panel_width{288};
    float padding{8};
    float header_height{40};
    float row_height{36};
    float cell_height{24};
    float radius{4};
    float gap{4};
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{panel_width, padding, header_height, row_height, cell_height,
                          radius,      gap,     font_size,     line_height};
    }

    friend bool operator==(const DatePickerMetricToken&, const DatePickerMetricToken&) = default;
};

struct DatePickerThemeToken final {
    DatePickerColorToken colors;
    DatePickerMetricToken metrics;
    ShadowList shadow;
    int z_index_popup{1050};
    friend bool operator==(const DatePickerThemeToken&, const DatePickerThemeToken&) = default;
};

struct DatePickerTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> text;
    std::optional<Color> muted;
    std::optional<Color> disabled;
    std::optional<Color> selected;
    std::optional<Color> selected_text;
    std::optional<Color> hover;
    std::optional<Color> selector_background;
    std::optional<Color> hover_border;
    std::optional<Color> active_border;
    std::optional<LogicalLength> panel_width;
    std::optional<LogicalLength> padding;
    std::optional<LogicalLength> header_height;
    std::optional<LogicalLength> row_height;
    std::optional<LogicalLength> cell_height;
    std::optional<LogicalLength> radius;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<SystemFontFamily> font_family;
    std::optional<ShadowList> shadow;
    std::optional<int> z_index_popup;
    friend bool operator==(const DatePickerTokenOverride&, const DatePickerTokenOverride&) = default;
};
} // namespace ryn
