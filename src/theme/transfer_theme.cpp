#include "theme/transfer_theme.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
TransferThemeToken derive_transfer(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                                   const ThemeAliasToken& alias) {
    TransferThemeToken t;
    t.colors = {alias.color_background_container,
                alias.color_background_container,
                alias.color_background_container,
                alias.color_border,
                alias.color_split,
                map.color_primary_border,
                alias.color_text,
                alias.color_text_secondary,
                alias.color_text_disabled,
                alias.color_background_container_disabled,
                map.color_error,
                map.color_warning,
                alias.color_focus_outline,
                map.color_primary,
                Color::rgba8(255, 255, 255),
                map.color_primary_hover,
                map.color_primary_active};
    auto& m = t.metrics;
    m.header_height = map.control_height_large;
    m.item_height = map.control_height;
    m.item_padding_block = (map.control_height - std::round(map.font_size * map.line_height)) / 2;
    m.footer_height = map.control_height_large;
    m.padding_inline = map.size_small;
    m.control_height = map.control_height;
    m.gap = map.size_xs;
    m.actions_width = map.control_height_large;
    m.radius = map.border_radius;
    m.border_width = seed.line_width;
    t.typography = {map.font_size, map.font_size * map.line_height, seed.font_family};
    t.effects = {alias.line_width_focus, alias.focus_outline_offset};
    return t;
}

void apply_transfer_override(TransferThemeToken& t, const TransferTokenOverride& o) {
    if (o.background) {
        t.colors.background = *o.background;
    }
    if (o.header) {
        t.colors.header = *o.header;
    }
    if (o.footer) {
        t.colors.footer = *o.footer;
    }
    if (o.border) {
        t.colors.border = *o.border;
    }
    if (o.hover) {
        t.colors.hover = *o.hover;
    }
    if (o.selected) {
        t.colors.selected = *o.selected;
    }
    if (o.text) {
        t.colors.text = *o.text;
    }
    if (o.secondary) {
        t.colors.secondary = *o.secondary;
    }
    if (o.disabled) {
        t.colors.disabled = *o.disabled;
    }
    if (o.disabled_background) {
        t.colors.disabled_background = *o.disabled_background;
    }
    if (o.error) {
        t.colors.error = *o.error;
    }
    if (o.warning) {
        t.colors.warning = *o.warning;
    }
    if (o.focus) {
        t.colors.focus = *o.focus;
    }
    if (o.primary) {
        t.colors.primary = *o.primary;
    }
    if (o.primary_text) {
        t.colors.primary_text = *o.primary_text;
    }
    if (o.primary_hover) {
        t.colors.primary_hover = *o.primary_hover;
    }
    if (o.primary_active) {
        t.colors.primary_active = *o.primary_active;
    }
    const auto length = [](float& target, const std::optional<LogicalLength>& v) {
        if (v) {
            if (v->is_auto() || !std::isfinite(v->value()) || v->value() < 0) {
                throw std::invalid_argument("Transfer token length must be finite and nonnegative");
            }
            target = v->value();
        }
    };
    length(t.metrics.header_height, o.header_height);
    length(t.metrics.item_height, o.item_height);
    length(t.metrics.item_padding_block, o.item_padding_block);
    length(t.metrics.list_width, o.list_width);
    length(t.metrics.list_height, o.list_height);
    length(t.metrics.list_width_large, o.list_width_large);
    length(t.metrics.footer_height, o.footer_height);
    length(t.metrics.padding_inline, o.padding_inline);
    length(t.metrics.control_height, o.control_height);
    length(t.metrics.gap, o.gap);
    length(t.metrics.actions_width, o.actions_width);
    length(t.metrics.radius, o.radius);
    length(t.metrics.border_width, o.border_width);
    length(t.typography.font_size, o.font_size);
    length(t.typography.line_height, o.line_height);
    length(t.effects.focus_width, o.focus_width);
    length(t.effects.focus_offset, o.focus_offset);
    if (o.font_family) {
        t.typography.font_family = *o.font_family;
    }
    if (o.order) {
        t.order = *o.order;
    }
    double extent = t.typography.font_size + t.typography.line_height + t.effects.focus_width + t.effects.focus_offset;
    for (float v : t.metrics.values()) {
        if (!std::isfinite(v) || v < 0) {
            throw std::invalid_argument("Transfer final metric is invalid");
        }
        extent += v;
    }
    if (!std::isfinite(extent) || extent * 65536 > std::numeric_limits<float>::max() || t.metrics.item_height <= 0 ||
        t.metrics.control_height <= 0 || t.typography.font_size < .5F ||
        t.typography.font_size >= static_cast<float>(std::numeric_limits<int>::max()) ||
        t.typography.line_height <= 0 || t.order < 0 || t.order > 1000000) {
        throw std::invalid_argument("Transfer final typography, geometry or order is invalid");
    }
}
} // namespace ryn::detail
