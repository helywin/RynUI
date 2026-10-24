#include "theme/time_picker_theme.hpp"
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
TimePickerThemeToken derive_time_picker(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                                        const ThemeAliasToken& alias) {
    TimePickerThemeToken t;
    const auto base = map.color_text_base;
    t.colors = {alias.color_background_elevated,
                alias.color_text,
                alias.color_text_disabled,
                map.color_primary,
                Color(map.color_primary.red(), map.color_primary.green(), map.color_primary.blue(), 0.1F),
                Color(base.red(), base.green(), base.blue(), 0.04F),
                alias.color_border_secondary,
                alias.color_background_container,
                map.color_primary_hover,
                map.color_primary,
                alias.color_focus_outline};
    // Locked 6.6.5 date-picker/style/token.ts: initPanelComponentToken.
    t.metrics.column_width = map.control_height_large * 1.4F;
    t.metrics.column_height = 28 * 8;
    t.metrics.cell_height = 28;
    t.metrics.padding = map.size_xs / 2;
    t.metrics.footer_padding = map.size_xs;
    t.metrics.gap = map.size_xs / 2;
    t.metrics.border_radius = map.border_radius_large;
    t.metrics.border_width = seed.line_width;
    t.metrics.font_size = map.font_size;
    t.metrics.line_height = map.font_size * map.line_height;
    t.metrics.font_family = seed.font_family;
    t.effects.shadow = alias.box_shadow_secondary;
    if (seed.z_index_popup_base > 2147483597) {
        throw std::invalid_argument("TimePicker popup order overflows");
    }
    t.z_index_popup = seed.z_index_popup_base + 50;
    return t;
}

void apply_time_picker_override(TimePickerThemeToken& t, const TimePickerTokenOverride& o) {
    if (o.background) {
        t.colors.background = *o.background;
    }
    if (o.text) {
        t.colors.text = *o.text;
    }
    if (o.disabled) {
        t.colors.disabled = *o.disabled;
    }
    if (o.selected) {
        t.colors.selected = *o.selected;
    }
    if (o.selected_background) {
        t.colors.selected_background = *o.selected_background;
    }
    if (o.hover_background) {
        t.colors.hover_background = *o.hover_background;
    }
    if (o.border) {
        t.colors.border = *o.border;
    }
    if (o.input_background) {
        t.colors.input_background = *o.input_background;
    }
    if (o.hover_border) {
        t.colors.hover_border = *o.hover_border;
    }
    if (o.active_border) {
        t.colors.active_border = *o.active_border;
    }
    if (o.active_outline) {
        t.colors.active_outline = *o.active_outline;
    }
    const auto length = [](const std::optional<LogicalLength>& input, float fallback, bool positive = false) {
        const auto result = input ? input->value() : fallback;
        if ((input && input->is_auto()) || !std::isfinite(result) || result < 0 || (positive && result == 0)) {
            throw std::invalid_argument("TimePicker final token length is invalid");
        }
        return result;
    };
    // Validate every final derived value, including values with no explicit override.
    t.metrics.column_width = length(o.column_width, t.metrics.column_width, true);
    t.metrics.column_height = length(o.column_height, t.metrics.column_height, true);
    t.metrics.cell_height = length(o.cell_height, t.metrics.cell_height, true);
    t.metrics.padding = length(o.padding, t.metrics.padding);
    t.metrics.footer_padding = length(o.footer_padding, t.metrics.footer_padding);
    t.metrics.gap = length(o.gap, t.metrics.gap);
    t.metrics.border_radius = length(o.border_radius, t.metrics.border_radius);
    t.metrics.border_width = length(o.border_width, t.metrics.border_width);
    t.metrics.font_size = length(o.font_size, t.metrics.font_size, true);
    t.metrics.line_height = length(o.line_height, t.metrics.line_height, true);
    if (t.metrics.font_size < 0.5F || t.metrics.font_size > 65535) {
        throw std::invalid_argument("TimePicker font_size must round to a renderable pixel size");
    }
    if (o.font_family) {
        t.metrics.font_family = *o.font_family;
    }
    if (t.metrics.font_family < SystemFontFamily::ui_sans || t.metrics.font_family > SystemFontFamily::ui_monospace) {
        throw std::invalid_argument("TimePicker font_family is invalid");
    }
    if (o.shadow) {
        t.effects.shadow = *o.shadow;
    }
    if (o.z_index_popup) {
        t.z_index_popup = *o.z_index_popup;
    }
    float extent = 0;
    for (const auto value : t.metrics.values()) {
        extent += value * 144;
    }
    if (!std::isfinite(extent)) {
        throw std::invalid_argument("TimePicker combined extents must be finite");
    }
}
} // namespace ryn::detail
