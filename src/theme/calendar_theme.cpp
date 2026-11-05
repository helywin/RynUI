#include "theme/calendar_theme.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
CalendarThemeToken derive_calendar(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                                   const ThemeAliasToken& alias) {
    CalendarThemeToken t;
    t.colors = {alias.color_background_container,
                alias.color_background_container,
                alias.color_text,
                alias.color_text_secondary,
                alias.color_text_disabled,
                map.color_primary,
                Color::rgba8(255, 255, 255),
                Color(map.color_primary.red(), map.color_primary.green(), map.color_primary.blue(), 0.1F),
                Color(map.color_text_base.red(), map.color_text_base.green(), map.color_text_base.blue(), 0.04F),
                map.color_primary,
                alias.color_border_secondary,
                alias.color_focus_outline};
    t.metrics.font_family = seed.font_family;
    t.metrics.font_size = map.font_size;
    t.metrics.line_height = map.font_size * map.line_height;
    t.metrics.header_height = map.control_height_large + map.size_small;
    t.metrics.padding = map.size_small;
    t.metrics.cell_padding = map.size_small;
    t.metrics.mini_cell_height = map.control_height_small;
    t.metrics.radius = map.border_radius_large;
    t.metrics.gap = map.size_xs;
    t.metrics.line_width = seed.line_width;
    return t;
}

void apply_calendar_override(CalendarThemeToken& t, const CalendarTokenOverride& o) {
    if (o.full_background) {
        t.colors.full_background = *o.full_background;
    }
    if (o.panel_background) {
        t.colors.panel_background = *o.panel_background;
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
    if (o.item_active_background) {
        t.colors.item_active_background = *o.item_active_background;
    }
    if (o.hover) {
        t.colors.hover = *o.hover;
    }
    if (o.today) {
        t.colors.today = *o.today;
    }
    if (o.border) {
        t.colors.border = *o.border;
    }
    if (o.focus) {
        t.colors.focus = *o.focus;
    }
    if (o.panel_width) {
        const auto v = *o.panel_width;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar panel_width is invalid");
        }
        t.metrics.panel_width = v.value();
    }
    if (o.mini_width) {
        const auto v = *o.mini_width;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar mini_width is invalid");
        }
        t.metrics.mini_width = v.value();
    }
    if (o.mini_content_height) {
        const auto v = *o.mini_content_height;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar mini_content_height is invalid");
        }
        t.metrics.mini_content_height = v.value();
    }
    if (o.padding) {
        const auto v = *o.padding;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar padding is invalid");
        }
        t.metrics.padding = v.value();
    }
    if (o.header_height) {
        const auto v = *o.header_height;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar header_height is invalid");
        }
        t.metrics.header_height = v.value();
    }
    if (o.year_control_width) {
        const auto v = *o.year_control_width;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar year_control_width is invalid");
        }
        t.metrics.year_control_width = v.value();
    }
    if (o.month_control_width) {
        const auto v = *o.month_control_width;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar month_control_width is invalid");
        }
        t.metrics.month_control_width = v.value();
    }
    if (o.cell_height) {
        const auto v = *o.cell_height;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar cell_height is invalid");
        }
        t.metrics.cell_height = v.value();
    }
    if (o.mini_cell_height) {
        const auto v = *o.mini_cell_height;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar mini_cell_height is invalid");
        }
        t.metrics.mini_cell_height = v.value();
    }
    if (o.cell_padding) {
        const auto v = *o.cell_padding;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar cell_padding is invalid");
        }
        t.metrics.cell_padding = v.value();
    }
    if (o.gap) {
        const auto v = *o.gap;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar gap is invalid");
        }
        t.metrics.gap = v.value();
    }
    if (o.radius) {
        const auto v = *o.radius;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar radius is invalid");
        }
        t.metrics.radius = v.value();
    }
    if (o.line_width) {
        const auto v = *o.line_width;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar line_width is invalid");
        }
        t.metrics.line_width = v.value();
    }
    if (o.focus_width) {
        const auto v = *o.focus_width;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar focus_width is invalid");
        }
        t.metrics.focus_width = v.value();
    }
    if (o.font_size) {
        const auto v = *o.font_size;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar font_size is invalid");
        }
        t.metrics.font_size = v.value();
    }
    if (o.line_height) {
        const auto v = *o.line_height;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Calendar line_height is invalid");
        }
        t.metrics.line_height = v.value();
    }
    if (o.font_family) {
        t.metrics.font_family = *o.font_family;
    }
    if (o.shadow) {
        t.shadow = *o.shadow;
    }
    double extent{};
    for (auto v : t.metrics.values()) {
        extent += static_cast<double>(v) * 10000;
    }
    if (extent > std::numeric_limits<float>::max() || t.metrics.font_size < 1 || t.metrics.line_height < 1 ||
        t.metrics.font_size > std::numeric_limits<int>::max() / 64 ||
        (t.metrics.font_family != SystemFontFamily::ui_sans &&
         t.metrics.font_family != SystemFontFamily::ui_monospace)) {
        throw std::invalid_argument("Calendar final metrics are not renderable");
    }
}
} // namespace ryn::detail
