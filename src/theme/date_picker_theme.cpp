#include "theme/date_picker_theme.hpp"
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
DatePickerThemeToken derive_date_picker(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                                        const ThemeAliasToken& alias) {
    DatePickerThemeToken t;
    // Locked 6.6.5 date-picker/style/token.ts and panel.ts single-date recipes.
    const auto base = map.color_text_base;
    t.colors = {alias.color_background_elevated,
                alias.color_text,
                alias.color_text_secondary,
                alias.color_text_disabled,
                map.color_primary,
                Color::rgba8(255, 255, 255),
                Color(base.red(), base.green(), base.blue(), 0.06F),
                alias.color_background_container,
                map.color_primary_hover,
                map.color_primary};
    t.metrics.panel_width = map.control_height_small * 10.5F + map.size_small * 3;
    t.metrics.padding = map.size_small;
    t.metrics.header_height = map.control_height_large;
    t.metrics.row_height = map.control_height_small + map.size_xs * 2;
    t.metrics.cell_height = map.control_height_small;
    t.metrics.radius = map.border_radius_small;
    t.metrics.gap = map.size_xs;
    t.metrics.font_size = map.font_size;
    t.metrics.line_height = map.font_size * map.line_height;
    t.metrics.font_family = seed.font_family;
    t.shadow = alias.box_shadow_secondary;
    t.z_index_popup = seed.z_index_popup_base + 50;
    return t;
}

void apply_date_picker_override(DatePickerThemeToken& t, const DatePickerTokenOverride& o) {
    if (o.background) {
        t.colors.background = *o.background;
    }
    if (o.text) {
        t.colors.text = *o.text;
    }
    if (o.muted) {
        t.colors.muted = *o.muted;
    }
    if (o.disabled) {
        t.colors.disabled = *o.disabled;
    }
    if (o.selected) {
        t.colors.selected = *o.selected;
    }
    if (o.selected_text) {
        t.colors.selected_text = *o.selected_text;
    }
    if (o.hover) {
        t.colors.hover = *o.hover;
    }
    if (o.selector_background) {
        t.colors.selector_background = *o.selector_background;
    }
    if (o.hover_border) {
        t.colors.hover_border = *o.hover_border;
    }
    if (o.active_border) {
        t.colors.active_border = *o.active_border;
    }
    const auto length = [](const std::optional<LogicalLength>& v, float fallback, float minimum = 0) {
        if (!v) {
            return fallback;
        }
        if (v->is_auto() || !std::isfinite(v->value()) || v->value() < minimum) {
            throw std::invalid_argument("DatePicker token length is invalid");
        }
        return v->value();
    };
    t.metrics.panel_width = length(o.panel_width, t.metrics.panel_width, 1);
    t.metrics.padding = length(o.padding, t.metrics.padding);
    t.metrics.header_height = length(o.header_height, t.metrics.header_height, 1);
    t.metrics.row_height = length(o.row_height, t.metrics.row_height, 1);
    t.metrics.cell_height = length(o.cell_height, t.metrics.cell_height, 1);
    t.metrics.radius = length(o.radius, t.metrics.radius);
    t.metrics.gap = length(o.gap, t.metrics.gap);
    t.metrics.font_size = length(o.font_size, t.metrics.font_size, 1);
    t.metrics.line_height = length(o.line_height, t.metrics.line_height, 1);
    if (o.font_family) {
        t.metrics.font_family = *o.font_family;
    }
    if (o.shadow) {
        t.shadow = *o.shadow;
    }
    if (o.z_index_popup) {
        t.z_index_popup = *o.z_index_popup;
    }
    float total = 0;
    for (auto v : t.metrics.values()) {
        total += v * 16;
    }
    if (!std::isfinite(total)) {
        throw std::invalid_argument("DatePicker combined extents must be finite");
    }
}
} // namespace ryn::detail
