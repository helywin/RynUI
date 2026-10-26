#include "theme/statistic_theme.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
StatisticThemeToken derive_statistic(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                                     const ThemeAliasToken& alias) {
    StatisticThemeToken t;
    const auto base = map.color_text_base;
    t.colors = {Color(base.red(), base.green(), base.blue(), .45F), alias.color_text};
    auto& m = t.metrics;
    m.title_font_size = map.font_size;
    // Locked genFontSizes: heading3 is index 4, converted to an even integer.
    m.content_font_size = std::floor(std::floor(map.font_size * std::exp(.6F)) / 2) * 2;
    // Statistic inherits resetComponent's global lineHeight for both sizes.
    m.title_line_height = m.title_font_size * map.line_height;
    m.content_line_height = m.content_font_size * map.line_height;
    m.title_gap = seed.size_unit * (seed.size_step - 3);
    m.affix_gap = m.title_gap;
    m.loading_padding = map.size;
    m.font_family = seed.font_family;
    return t;
}

void apply_statistic_override(StatisticThemeToken& t, const StatisticTokenOverride& o) {
    if (o.title) {
        t.colors.title = *o.title;
    }
    if (o.content) {
        t.colors.content = *o.content;
    }
    auto& m = t.metrics;
    const auto length = [](float& target, const std::optional<LogicalLength>& value) {
        if (value) {
            if (value->is_auto()) {
                throw std::invalid_argument("Statistic token cannot be auto");
            }
            target = value->value();
        }
    };
    length(m.title_font_size, o.title_font_size);
    length(m.content_font_size, o.content_font_size);
    length(m.title_line_height, o.title_line_height);
    length(m.content_line_height, o.content_line_height);
    length(m.title_gap, o.title_gap);
    length(m.affix_gap, o.affix_gap);
    length(m.loading_padding, o.loading_padding);
    if (o.font_family) {
        m.font_family = *o.font_family;
    }
    float sum = 0;
    for (const auto value : m.values()) {
        if (!std::isfinite(value) || value < 0) {
            throw std::invalid_argument("Statistic metrics must be finite and nonnegative");
        }
        sum += value * 4;
    }
    if (!std::isfinite(sum) || m.title_font_size < .5F || m.content_font_size < .5F || m.title_line_height <= 0 ||
        m.content_line_height <= 0 || static_cast<double>(m.title_font_size) >= std::numeric_limits<long>::max() ||
        static_cast<double>(m.content_font_size) >= std::numeric_limits<long>::max() ||
        (m.font_family != SystemFontFamily::ui_sans && m.font_family != SystemFontFamily::ui_monospace)) {
        throw std::invalid_argument("Statistic final typography or combined extents are invalid");
    }
}
} // namespace ryn::detail
