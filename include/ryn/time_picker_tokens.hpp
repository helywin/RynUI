#pragma once

#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct TimePickerColorToken final {
    Color background;
    Color text;
    Color disabled;
    Color selected;
    Color selected_background;
    Color hover_background;
    Color border;
    Color input_background;
    Color hover_border;
    Color active_border;
    Color active_outline;

    [[nodiscard]] auto values() const {
        return std::array{background,       text,   disabled,         selected,     selected_background,
                          hover_background, border, input_background, hover_border, active_border,
                          active_outline};
    }

    friend bool operator==(const TimePickerColorToken&, const TimePickerColorToken&) = default;
};

struct TimePickerMetricToken final {
    float column_width{56};
    float column_height{224};
    float cell_height{28};
    float padding{4};
    float footer_padding{8};
    float gap{4};
    float border_radius{8};
    float border_width{1};
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{column_width, column_height, cell_height,  padding,   footer_padding,
                          gap,          border_radius, border_width, font_size, line_height};
    }

    friend bool operator==(const TimePickerMetricToken&, const TimePickerMetricToken&) = default;
};

struct TimePickerEffectToken final {
    ShadowList shadow;
    friend bool operator==(const TimePickerEffectToken&, const TimePickerEffectToken&) = default;
};

struct TimePickerThemeToken final {
    TimePickerColorToken colors;
    TimePickerMetricToken metrics;
    TimePickerEffectToken effects;
    std::int32_t z_index_popup{1050};
    friend bool operator==(const TimePickerThemeToken&, const TimePickerThemeToken&) = default;
};

struct TimePickerTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> text;
    std::optional<Color> disabled;
    std::optional<Color> selected;
    std::optional<Color> selected_background;
    std::optional<Color> hover_background;
    std::optional<Color> border;
    std::optional<Color> input_background;
    std::optional<Color> hover_border;
    std::optional<Color> active_border;
    std::optional<Color> active_outline;
    std::optional<LogicalLength> column_width;
    std::optional<LogicalLength> column_height;
    std::optional<LogicalLength> cell_height;
    std::optional<LogicalLength> padding;
    std::optional<LogicalLength> footer_padding;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<SystemFontFamily> font_family;
    std::optional<ShadowList> shadow;
    std::optional<std::int32_t> z_index_popup;
    friend bool operator==(const TimePickerTokenOverride&, const TimePickerTokenOverride&) = default;
};
} // namespace ryn
