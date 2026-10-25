#include "theme/timeline_theme.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
TimelineThemeToken derive_timeline(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                                   const ThemeAliasToken& alias) {
    TimelineThemeToken t;
    // Locked Ant Design 6.6.5 Timeline/Steps dot recipe; no Steps navigation semantics.
    t.colors = {alias.color_text,  alias.color_split,        alias.color_background_container,
                map.color_primary, map.color_primary,        map.color_error,
                map.color_success, alias.color_text_disabled};
    auto& m = t.metrics;
    m.tail_width = seed.line_width * 2;
    m.dot_border_width = seed.line_width * 2;
    m.dot_size = map.control_height_large / 4;
    m.icon_size = map.font_size;
    m.item_padding_bottom = map.size * 1.25F;
    m.content_gap = map.size;
    m.font_size = map.font_size;
    m.line_height = map.font_size * map.line_height;
    m.font_family = seed.font_family;
    return t;
}

void apply_timeline_override(TimelineThemeToken& t, const TimelineTokenOverride& o) {
    auto& c = t.colors;
    if (o.text) {
        c.text = *o.text;
    }
    if (o.tail) {
        c.tail = *o.tail;
    }
    if (o.dot_background) {
        c.dot_background = *o.dot_background;
    }
    if (o.node) {
        c.node = *o.node;
    }
    if (o.blue) {
        c.blue = *o.blue;
    }
    if (o.red) {
        c.red = *o.red;
    }
    if (o.green) {
        c.green = *o.green;
    }
    if (o.gray) {
        c.gray = *o.gray;
    }
    auto& m = t.metrics;
    const auto length = [](float& target, const std::optional<LogicalLength>& value) {
        if (value) {
            if (value->is_auto()) {
                throw std::invalid_argument("Timeline token cannot be auto");
            }
            target = value->value();
        }
    };
    length(m.tail_width, o.tail_width);
    length(m.dot_border_width, o.dot_border_width);
    length(m.dot_size, o.dot_size);
    length(m.icon_size, o.icon_size);
    length(m.item_padding_bottom, o.item_padding_bottom);
    length(m.content_gap, o.content_gap);
    length(m.font_size, o.font_size);
    length(m.line_height, o.line_height);
    if (o.font_family) {
        m.font_family = *o.font_family;
    }
    float sum = 0;
    for (float value : m.values()) {
        if (!std::isfinite(value) || value < 0) {
            throw std::invalid_argument("Timeline metrics must be finite and nonnegative");
        }
        sum += value * 4;
    }
    if (!std::isfinite(sum) || m.font_size < .5F || m.line_height <= 0 || m.icon_size < .5F ||
        static_cast<double>(m.font_size) >= std::numeric_limits<long>::max() ||
        static_cast<double>(m.font_size) >= std::numeric_limits<std::uint32_t>::max() ||
        static_cast<double>(m.icon_size) >= std::numeric_limits<long>::max() ||
        (m.font_family != SystemFontFamily::ui_sans && m.font_family != SystemFontFamily::ui_monospace)) {
        throw std::invalid_argument("Timeline final typography or combined extents are invalid");
    }
}
} // namespace ryn::detail
