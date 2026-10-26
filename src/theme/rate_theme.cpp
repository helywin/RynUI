#include "theme/rate_theme.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
RateThemeToken derive_rate(const AntDesignDefaultSeed& seed, const ThemeMapToken& map, const ThemeAliasToken& alias,
                           bool dark) {
    RateThemeToken t;
    // Locked 6.6.5 yellow6: dark palette index 6 blends seed yellow with #141414 at .85.
    t.colors.star = dark ? Color::rgba8(216, 189, 20) : Color::rgba8(250, 219, 20);
    t.colors.background =
        Color(map.color_text_base.red(), map.color_text_base.green(), map.color_text_base.blue(), dark ? .12F : .06F);
    t.metrics = {map.control_height * .625F,
                 map.control_height_small * .625F,
                 map.control_height_large * .625F,
                 map.size_xs,
                 1.1F,
                 alias.line_width_focus == 0 ? 0 : seed.line_width,
                 seed.font_family};
    return t;
}

void apply_rate_override(RateThemeToken& t, const RateTokenOverride& o) {
    if (o.star) {
        t.colors.star = *o.star;
    }
    if (o.background) {
        t.colors.background = *o.background;
    }
    auto& m = t.metrics;
    const auto length = [](float& target, const std::optional<LogicalLength>& value) {
        if (value) {
            if (value->is_auto()) {
                throw std::invalid_argument("Rate token cannot be auto");
            }
            target = value->value();
        }
    };
    length(m.star_size, o.star_size);
    length(m.star_size_small, o.star_size_small);
    length(m.star_size_large, o.star_size_large);
    length(m.gap, o.gap);
    length(m.focus_width, o.focus_width);
    if (o.hover_scale) {
        m.hover_scale = *o.hover_scale;
    }
    if (o.font_family) {
        m.font_family = *o.font_family;
    }
    for (float v : m.values()) {
        if (!std::isfinite(v) || v < 0) {
            throw std::invalid_argument("Rate metrics must be finite and nonnegative");
        }
    }
    const float largest = std::max({m.star_size, m.star_size_small, m.star_size_large});
    const float smallest = std::min({m.star_size, m.star_size_small, m.star_size_large});
    // Match Rate's float product and Text's lround without converting an out-of-range value to an integer.
    // FontRuntime::resolve_raster_pixel_size accepts logical pixels only through INT_MAX / 64.
    for (float size : {m.star_size, m.star_size_small, m.star_size_large}) {
        for (float scale : {1.0F, m.hover_scale}) {
            const float logical = size * scale;
            const double pixels = std::round(static_cast<double>(logical));
            if (!std::isfinite(logical) || logical < .5F ||
                pixels > static_cast<double>(std::numeric_limits<int>::max() / 64)) {
                throw std::invalid_argument("Rate final font exceeds the native logical pixel range");
            }
        }
    }
    const float scaled = largest * m.hover_scale;
    const double extent = (scaled + m.gap + m.focus_width * 2.0) * 1000;
    if (smallest < .5F || m.hover_scale < 1 || !std::isfinite(extent) || extent >= std::numeric_limits<float>::max() ||
        (m.font_family != SystemFontFamily::ui_sans && m.font_family != SystemFontFamily::ui_monospace)) {
        throw std::invalid_argument("Rate final typography or combined extents are invalid");
    }
}
} // namespace ryn::detail
