#include "theme/popover_theme.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
PopoverThemeToken derive_popover(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                                 const ThemeAliasToken& alias) {
    if (seed.z_index_popup_base > std::numeric_limits<std::int32_t>::max() - 30) {
        throw std::invalid_argument("Popover z-index exceeds the supported integer range");
    }
    PopoverThemeToken t;
    t.background = alias.color_background_elevated;
    t.title = alias.color_text;
    t.text = alias.color_text;
    t.border_radius = map.border_radius_large;
    t.title_gap = map.size_xs;
    t.font_size = map.font_size;
    t.line_height = map.line_height * map.font_size;
    t.font_family = seed.font_family;
    t.shadow = alias.box_shadow_secondary;
    t.z_index_popup = seed.z_index_popup_base + 30;
    return t;
}

void apply_popover_override(PopoverThemeToken& t, const PopoverTokenOverride& o) {
    if (o.background) {
        t.background = *o.background;
    }
    if (o.title) {
        t.title = *o.title;
    }
    if (o.text) {
        t.text = *o.text;
    }
    const auto length = [](float& target, const std::optional<LogicalLength>& value) {
        if (value) {
            if (value->is_auto()) {
                throw std::invalid_argument("Popover token length cannot be auto");
            }
            target = value->value();
        }
    };
    length(t.max_width, o.max_width);
    length(t.title_min_width, o.title_min_width);
    length(t.padding, o.padding);
    length(t.title_gap, o.title_gap);
    length(t.border_radius, o.border_radius);
    length(t.arrow_size, o.arrow_size);
    length(t.gap, o.gap);
    length(t.font_size, o.font_size);
    length(t.line_height, o.line_height);
    if (o.font_family) {
        t.font_family = *o.font_family;
    }
    if (o.font_weight) {
        t.font_weight = *o.font_weight;
    }
    if (o.title_font_weight) {
        t.title_font_weight = *o.title_font_weight;
    }
    if (o.shadow) {
        t.shadow = *o.shadow;
    }
    if (o.z_index_popup) {
        t.z_index_popup = *o.z_index_popup;
    }
    float extent{};
    for (auto value : t.metrics()) {
        if (!std::isfinite(value) || value < 0) {
            throw std::invalid_argument("Popover metric must be finite and nonnegative");
        }
        extent += value * 2048;
    }
    if (t.max_width <= 0 || !std::isfinite(t.font_size) || t.font_size < .5F ||
        static_cast<double>(t.font_size) >= std::numeric_limits<long>::max() ||
        static_cast<double>(t.font_size) >= std::numeric_limits<std::uint32_t>::max() ||
        !std::isfinite(t.line_height) || t.line_height <= 0 || t.font_weight < 100 || t.font_weight > 1000 ||
        t.title_font_weight < 100 || t.title_font_weight > 1000 || t.z_index_popup < 0 ||
        !std::isfinite(extent + t.font_size * 2048 + t.line_height * 2048)) {
        throw std::invalid_argument("Popover final token is invalid");
    }
    if (t.font_family != SystemFontFamily::ui_sans && t.font_family != SystemFontFamily::ui_monospace) {
        throw std::invalid_argument("Popover font family is invalid");
    }
    for (std::size_t i = 0; i < t.shadow.size(); ++i) {
        const auto& s = t.shadow[i];
        if (!std::isfinite(s.offset.x) || !std::isfinite(s.offset.y) || !std::isfinite(s.blur) || s.blur < 0 ||
            !std::isfinite(s.spread) ||
            !std::isfinite(std::abs(s.offset.x) + std::abs(s.offset.y) + s.blur * 4 + std::abs(s.spread))) {
            throw std::invalid_argument("Popover shadow is invalid");
        }
    }
}
} // namespace ryn::detail
