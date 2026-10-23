#include "theme/segmented_theme.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
SegmentedThemeToken derive_segmented(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                                     const ThemeAliasToken& alias, std::span<const ThemeAlgorithm> algorithms) {
    const bool dark = std::find(algorithms.begin(), algorithms.end(), ThemeAlgorithm::Dark) != algorithms.end();
    const auto base = map.color_text_base;
    SegmentedThemeToken t;
    // Locked 6.6.5 segmented/style/index.ts: prepareComponentToken and size recipes.
    t.colors = {dark ? Color::rgba8(0, 0, 0) : Color::rgba8(245, 245, 245),
                alias.color_text_secondary,
                alias.color_text,
                Color(base.red(), base.green(), base.blue(), dark ? 0.12F : 0.06F),
                Color(base.red(), base.green(), base.blue(), dark ? 0.18F : 0.15F),
                alias.color_text,
                alias.color_background_elevated,
                alias.color_text_disabled,
                alias.color_focus_outline};
    auto& m = t.metrics;
    m.track_padding = seed.line_width * 2;
    m.height_small = map.control_height_small;
    m.height = map.control_height;
    m.height_large = map.control_height_large;
    m.padding_small = std::max(0.0F, map.size_xs - seed.line_width);
    m.padding = std::max(0.0F, map.size_small - seed.line_width);
    m.icon_gap = map.size_small / 2;
    m.radius_small = map.border_radius_small;
    m.radius = map.border_radius;
    m.radius_large = map.border_radius_large;
    m.item_radius_small = std::max(0.0F, map.border_radius_small - 2);
    m.item_radius = map.border_radius_small;
    m.item_radius_large = map.border_radius;
    m.font_size = map.font_size;
    m.font_size_large = map.font_size_large;
    m.line_height = map.font_size * map.line_height;
    m.line_height_large = map.font_size_large * map.line_height_large;
    m.font_family = seed.font_family;
    t.effects = {alias.box_shadow_tertiary, alias.line_width_focus, alias.focus_outline_offset};
    return t;
}

void apply_segmented_override(SegmentedThemeToken& t, const SegmentedTokenOverride& o) {
    if (o.track) {
        t.colors.track = *o.track;
    }
    if (o.item) {
        t.colors.item = *o.item;
    }
    if (o.hover) {
        t.colors.hover = *o.hover;
    }
    if (o.hover_background) {
        t.colors.hover_background = *o.hover_background;
    }
    if (o.active_background) {
        t.colors.active_background = *o.active_background;
    }
    if (o.selected) {
        t.colors.selected = *o.selected;
    }
    if (o.selected_background) {
        t.colors.selected_background = *o.selected_background;
    }
    if (o.disabled) {
        t.colors.disabled = *o.disabled;
    }
    if (o.focus) {
        t.colors.focus = *o.focus;
    }
    const auto length = [](const std::optional<LogicalLength>& value, float fallback, bool positive = false) {
        if (!value) {
            return fallback;
        }
        const float result = value->value();
        if (value->is_auto() || !std::isfinite(result) || result < 0 || (positive && result == 0)) {
            throw std::invalid_argument("Segmented token length is invalid");
        }
        return result;
    };
    t.metrics.track_padding = length(o.track_padding, t.metrics.track_padding);
    t.metrics.height_small = length(o.height_small, t.metrics.height_small, true);
    t.metrics.height = length(o.height, t.metrics.height, true);
    t.metrics.height_large = length(o.height_large, t.metrics.height_large, true);
    t.metrics.padding_small = length(o.padding_small, t.metrics.padding_small);
    t.metrics.padding = length(o.padding, t.metrics.padding);
    t.metrics.icon_gap = length(o.icon_gap, t.metrics.icon_gap);
    t.metrics.radius_small = length(o.radius_small, t.metrics.radius_small);
    t.metrics.radius = length(o.radius, t.metrics.radius);
    t.metrics.radius_large = length(o.radius_large, t.metrics.radius_large);
    t.metrics.item_radius_small = length(o.item_radius_small, t.metrics.item_radius_small);
    t.metrics.item_radius = length(o.item_radius, t.metrics.item_radius);
    t.metrics.item_radius_large = length(o.item_radius_large, t.metrics.item_radius_large);
    t.metrics.font_size = length(o.font_size, t.metrics.font_size, true);
    t.metrics.font_size_large = length(o.font_size_large, t.metrics.font_size_large, true);
    t.metrics.line_height = length(o.line_height, t.metrics.line_height, true);
    t.metrics.line_height_large = length(o.line_height_large, t.metrics.line_height_large, true);
    if (o.font_family) {
        t.metrics.font_family = *o.font_family;
    }
    if (o.selected_shadow) {
        t.effects.selected_shadow = *o.selected_shadow;
    }
    t.effects.focus_width = length(o.focus_width, t.effects.focus_width);
    t.effects.focus_offset = length(o.focus_offset, t.effects.focus_offset);
    float extent = t.effects.focus_width + t.effects.focus_offset;
    for (auto value : t.metrics.values()) {
        extent += value * 2;
    }
    if (!std::isfinite(extent)) {
        throw std::invalid_argument("Segmented combined extents must be finite");
    }
}
} // namespace ryn::detail
