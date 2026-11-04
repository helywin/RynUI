#include "theme/anchor_theme.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
AnchorThemeToken derive_anchor(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                               const ThemeAliasToken& alias) {
    AnchorThemeToken t;
    // Locked Ant Design 6.6.5 anchor/style/index.ts.
    t.colors = {alias.color_text,          alias.color_text,          alias.color_text, alias.color_split,
                alias.color_text_disabled, alias.color_focus_outline, Color(0, 0, 0, 0)};
    auto& m = t.metrics;
    m.font_size = map.font_size;
    m.line_height = map.font_size * map.line_height;
    m.padding_block = map.size_xs / 2;
    m.indent = map.size;
    m.padding = map.size;
    m.radius = map.border_radius_small;
    m.font_family = seed.font_family;
    t.colors.current = map.color_primary;
    t.colors.hover = map.color_primary_hover;
    t.effects = {alias.line_width_focus, alias.focus_outline_offset};
    return t;
}

void apply_anchor_override(AnchorThemeToken& t, const AnchorTokenOverride& o) {
    if (o.item) {
        t.colors.item = *o.item;
    }
    if (o.hover) {
        t.colors.hover = *o.hover;
    }
    if (o.current) {
        t.colors.current = *o.current;
    }
    if (o.separator) {
        t.colors.separator = *o.separator;
    }
    if (o.disabled) {
        t.colors.disabled = *o.disabled;
    }
    if (o.focus) {
        t.colors.focus = *o.focus;
    }
    if (o.hover_background) {
        t.colors.hover_background = *o.hover_background;
    }
    const auto length = [](float& target, const std::optional<LogicalLength>& value) {
        if (!value) {
            return;
        }
        if (value->is_auto() || !std::isfinite(value->value()) || value->value() < 0) {
            throw std::invalid_argument("Anchor token length is invalid");
        }
        target = value->value();
    };
    length(t.metrics.font_size, o.font_size);
    length(t.metrics.line_height, o.line_height);
    length(t.metrics.padding_block, o.padding_block);
    length(t.metrics.indent, o.indent);
    length(t.metrics.padding, o.padding);
    length(t.metrics.radius, o.radius);
    length(t.effects.focus_width, o.focus_width);
    length(t.effects.focus_offset, o.focus_offset);
    if (o.font_family) {
        t.metrics.font_family = *o.font_family;
    }
    float extent = 0;
    for (float value : t.metrics.values()) {
        if (!std::isfinite(value) || value < 0) {
            throw std::invalid_argument("Anchor final metric is invalid");
        }
        extent += value * 4;
    }
    for (float value : {t.effects.focus_width, t.effects.focus_offset}) {
        if (!std::isfinite(value) || value < 0) {
            throw std::invalid_argument("Anchor final effect is invalid");
        }
        extent += value * 4;
    }
    const auto renderable_font = [](float value) {
        return value >= .5F && value < static_cast<float>(std::numeric_limits<std::int32_t>::max());
    };
    if (!std::isfinite(extent) || !renderable_font(t.metrics.font_size) || t.metrics.line_height <= 0) {
        throw std::invalid_argument("Anchor final typography or extent is invalid");
    }
}
} // namespace ryn::detail
