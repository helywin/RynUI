#include "theme/notice_theme.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
NoticeThemeToken derive_notice(const AntDesignDefaultSeed& seed, const ThemeMapToken& map, const ThemeAliasToken& alias,
                               bool message) {
    NoticeThemeToken t;
    t.colors = {alias.color_background_elevated,
                alias.color_text,
                alias.color_text_secondary,
                map.color_success,
                map.color_info,
                map.color_warning,
                map.color_error,
                map.color_primary};
    auto& m = t.metrics;
    m.padding_block = message ? (map.control_height_large - map.font_size * map.line_height) / 2 : map.size;
    m.padding_inline = message ? map.size_small : map.size_large;
    m.gap = map.size_xs;
    m.stack_gap = message ? map.size_xs : map.size;
    m.offset = message ? 8 : map.size_large;
    m.border_radius = map.border_radius_large;
    m.font_size = map.font_size;
    m.line_height = map.font_size * map.line_height;
    m.title_font_size = message ? map.font_size : map.font_size_large;
    m.title_line_height = message ? m.line_height : map.font_size_large * map.line_height_large;
    m.icon_size = message ? map.font_size_large : m.title_line_height;
    m.close_size = map.control_height_large * .55F;
    m.font_family = seed.font_family;
    m.title_font_weight = message ? 400 : 600;
    t.shadow = alias.box_shadow;
    const auto z = static_cast<std::int64_t>(seed.z_index_popup_base) + (message ? 1010 : 1050);
    if (z > std::numeric_limits<std::int32_t>::max()) {
        throw std::invalid_argument("Notice z-index overflow");
    }
    t.z_index = static_cast<std::int32_t>(z);
    return t;
}

void apply_notice_override(NoticeThemeToken& t, const NoticeTokenOverride& o) {
    if (o.background) {
        t.colors.background = *o.background;
    }
    if (o.text) {
        t.colors.text = *o.text;
    }
    if (o.description) {
        t.colors.description = *o.description;
    }
    if (o.success) {
        t.colors.success = *o.success;
    }
    if (o.info) {
        t.colors.info = *o.info;
    }
    if (o.warning) {
        t.colors.warning = *o.warning;
    }
    if (o.error) {
        t.colors.error = *o.error;
    }
    if (o.loading) {
        t.colors.loading = *o.loading;
    }
    const auto length = [](float& target, const std::optional<LogicalLength>& value) {
        if (value) {
            if (value->is_auto()) {
                throw std::invalid_argument("Notice token cannot be auto");
            }
            target = value->value();
        }
    };
    length(t.metrics.width, o.width);
    length(t.metrics.padding_block, o.padding_block);
    length(t.metrics.padding_inline, o.padding_inline);
    length(t.metrics.gap, o.gap);
    length(t.metrics.stack_gap, o.stack_gap);
    length(t.metrics.offset, o.offset);
    length(t.metrics.border_radius, o.border_radius);
    length(t.metrics.font_size, o.font_size);
    length(t.metrics.line_height, o.line_height);
    length(t.metrics.title_font_size, o.title_font_size);
    length(t.metrics.title_line_height, o.title_line_height);
    length(t.metrics.icon_size, o.icon_size);
    length(t.metrics.close_size, o.close_size);
    if (o.font_family) {
        t.metrics.font_family = *o.font_family;
    }
    if (o.title_font_weight) {
        t.metrics.title_font_weight = *o.title_font_weight;
    }
    if (o.shadow) {
        t.shadow = *o.shadow;
    }
    if (o.z_index) {
        t.z_index = *o.z_index;
    }
    const auto& m = t.metrics;
    float sum = 0;
    for (float v : m.values()) {
        if (!std::isfinite(v) || v < 0) {
            throw std::invalid_argument("Notice metrics must be finite and nonnegative");
        }
        sum += v * 8;
    }
    if (!std::isfinite(sum) || m.width <= 0 || m.line_height <= 0 || m.title_line_height <= 0 || m.close_size <= 0 ||
        m.font_size < .5F || m.title_font_size < .5F || m.icon_size < .5F ||
        static_cast<double>(m.font_size) >= std::numeric_limits<long>::max() ||
        static_cast<double>(m.title_font_size) >= std::numeric_limits<long>::max() ||
        static_cast<double>(m.icon_size) >= std::numeric_limits<long>::max() ||
        (m.font_family != SystemFontFamily::ui_sans && m.font_family != SystemFontFamily::ui_monospace) ||
        m.title_font_weight < 1 || m.title_font_weight > 1000) {
        throw std::invalid_argument("Notice final typography or extent is invalid");
    }
}
} // namespace ryn::detail
