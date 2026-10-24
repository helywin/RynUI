#include "theme/steps_theme.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
StepsThemeToken derive_steps(const AntDesignDefaultSeed& seed, const ThemeMapToken& map, const ThemeAliasToken& alias) {
    StepsThemeToken t;
    // Locked Ant Design 6.6.5 steps/style/{index,status,small}.ts filled recipes.
    const auto base = map.color_text_base;
    const auto tint = [](Color c) {
        return Color(c.red(), c.green(), c.blue(), .1F);
    };
    t.colors = {alias.color_text,           alias.color_text_secondary,
                alias.color_text_secondary, Color::rgba8(255, 255, 255),
                map.color_primary,          map.color_error,
                alias.color_text_disabled,  Color(base.red(), base.green(), base.blue(), .06F),
                map.color_primary,          tint(map.color_primary),
                tint(map.color_error),      alias.color_split,
                map.color_primary,          alias.color_focus_outline,
                map.color_primary_hover};
    auto& m = t.metrics;
    m.font_size = map.font_size;
    m.title_font_size = map.font_size_large;
    m.title_font_size_small = map.font_size;
    m.line_height = map.font_size * map.line_height;
    m.title_line_height = map.font_size_large * map.line_height_large;
    m.title_line_height_small = m.line_height;
    m.icon_size = map.control_height;
    m.icon_size_small = map.control_height_small;
    m.icon_font_size = map.font_size;
    m.icon_gap = map.size_xs;
    m.subtitle_gap = map.size_xs;
    m.description_gap = map.size_xs / 2;
    m.item_gap = map.size;
    m.vertical_gap = map.size;
    m.line_width = seed.line_width;
    m.font_family = seed.font_family;
    t.effects = {alias.line_width_focus, alias.focus_outline_offset};
    return t;
}

void apply_steps_override(StepsThemeToken& t, const StepsTokenOverride& o) {
    if (o.title) {
        t.colors.title = *o.title;
    }
    if (o.description) {
        t.colors.description = *o.description;
    }
    if (o.wait_text) {
        t.colors.wait_text = *o.wait_text;
    }
    if (o.process_text) {
        t.colors.process_text = *o.process_text;
    }
    if (o.finish_text) {
        t.colors.finish_text = *o.finish_text;
    }
    if (o.error) {
        t.colors.error = *o.error;
    }
    if (o.disabled) {
        t.colors.disabled = *o.disabled;
    }
    if (o.wait_background) {
        t.colors.wait_background = *o.wait_background;
    }
    if (o.process_background) {
        t.colors.process_background = *o.process_background;
    }
    if (o.finish_background) {
        t.colors.finish_background = *o.finish_background;
    }
    if (o.error_background) {
        t.colors.error_background = *o.error_background;
    }
    if (o.connector) {
        t.colors.connector = *o.connector;
    }
    if (o.finished_connector) {
        t.colors.finished_connector = *o.finished_connector;
    }
    if (o.focus) {
        t.colors.focus = *o.focus;
    }
    if (o.hover) {
        t.colors.hover = *o.hover;
    }
    const auto length = [](float& target, const std::optional<LogicalLength>& value) {
        if (!value) {
            return;
        }
        if (value->is_auto() || !std::isfinite(value->value()) || value->value() < 0) {
            throw std::invalid_argument("Steps token length is invalid");
        }
        target = value->value();
    };
    length(t.metrics.font_size, o.font_size);
    length(t.metrics.title_font_size, o.title_font_size);
    length(t.metrics.title_font_size_small, o.title_font_size_small);
    length(t.metrics.line_height, o.line_height);
    length(t.metrics.title_line_height, o.title_line_height);
    length(t.metrics.title_line_height_small, o.title_line_height_small);
    length(t.metrics.icon_size, o.icon_size);
    length(t.metrics.icon_size_small, o.icon_size_small);
    length(t.metrics.icon_font_size, o.icon_font_size);
    length(t.metrics.icon_gap, o.icon_gap);
    length(t.metrics.subtitle_gap, o.subtitle_gap);
    length(t.metrics.description_gap, o.description_gap);
    length(t.metrics.item_gap, o.item_gap);
    length(t.metrics.vertical_gap, o.vertical_gap);
    length(t.metrics.line_width, o.line_width);
    length(t.effects.focus_width, o.focus_width);
    length(t.effects.focus_offset, o.focus_offset);
    if (o.font_family) {
        t.metrics.font_family = *o.font_family;
    }
    float extent = 0;
    for (float value : t.metrics.values()) {
        if (!std::isfinite(value) || value < 0) {
            throw std::invalid_argument("Steps final metric is invalid");
        }
        extent += value * 4;
    }
    for (float value : {t.effects.focus_width, t.effects.focus_offset}) {
        if (!std::isfinite(value) || value < 0) {
            throw std::invalid_argument("Steps final effect is invalid");
        }
        extent += value * 4;
    }
    const auto renderable_font = [](float value) {
        return value >= .5F && value < static_cast<float>(std::numeric_limits<std::int32_t>::max());
    };
    if (!std::isfinite(extent) || !renderable_font(t.metrics.font_size) ||
        !renderable_font(t.metrics.title_font_size) || !renderable_font(t.metrics.title_font_size_small) ||
        !renderable_font(t.metrics.icon_font_size) || t.metrics.line_height <= 0 || t.metrics.title_line_height <= 0 ||
        t.metrics.title_line_height_small <= 0) {
        throw std::invalid_argument("Steps final typography or extent is invalid");
    }
}
} // namespace ryn::detail
