#include "theme/avatar_theme.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
AvatarThemeToken derive_avatar(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                               const ThemeAliasToken& alias) {
    AvatarThemeToken t;
    t.colors = {Color::rgba8(255, 255, 255),
                Color(map.color_text_base.red(), map.color_text_base.green(), map.color_text_base.blue(), .25F),
                alias.color_background_container};
    auto& m = t.metrics;
    m.container_size = map.control_height;
    m.container_size_large = map.control_height_large;
    m.container_size_small = map.control_height_small;
    m.text_font_size = map.font_size;
    m.text_font_size_large = map.font_size;
    m.text_font_size_small = map.font_size;
    const float xl = std::floor(std::floor(map.font_size * std::exp(.4F)) / 2) * 2;
    m.icon_font_size = std::round((map.font_size_large + xl) / 2);
    m.icon_font_size_large = std::floor(std::floor(map.font_size * std::exp(.6F)) / 2) * 2;
    m.icon_font_size_small = map.font_size;
    m.border_radius = map.border_radius;
    m.border_radius_large = map.border_radius_large;
    m.border_radius_small = map.border_radius_small;
    m.border_width = seed.line_width;
    m.group_space = map.size_small == map.size ? map.size_xs : seed.size_unit * (seed.size_step - 3);
    m.group_overlapping = -map.size_xs;
    m.font_family = seed.font_family;
    return t;
}

void apply_avatar_override(AvatarThemeToken& t, const AvatarTokenOverride& o) {
    if (o.text) {
        t.colors.text = *o.text;
    }
    if (o.background) {
        t.colors.background = *o.background;
    }
    if (o.group_border) {
        t.colors.group_border = *o.group_border;
    }
    auto& m = t.metrics;
    const auto length = [](float& target, const std::optional<LogicalLength>& value) {
        if (value) {
            if (value->is_auto()) {
                throw std::invalid_argument("Avatar token cannot be auto");
            }
            target = value->value();
        }
    };
    length(m.container_size, o.container_size);
    length(m.container_size_large, o.container_size_large);
    length(m.container_size_small, o.container_size_small);
    length(m.text_font_size, o.text_font_size);
    length(m.text_font_size_large, o.text_font_size_large);
    length(m.text_font_size_small, o.text_font_size_small);
    length(m.icon_font_size, o.icon_font_size);
    length(m.icon_font_size_large, o.icon_font_size_large);
    length(m.icon_font_size_small, o.icon_font_size_small);
    length(m.border_radius, o.border_radius);
    length(m.border_radius_large, o.border_radius_large);
    length(m.border_radius_small, o.border_radius_small);
    length(m.border_width, o.border_width);
    length(m.group_space, o.group_space);
    length(m.group_overlapping, o.group_overlapping);
    if (o.font_family) {
        m.font_family = *o.font_family;
    }
    double combined = 0;
    const auto values = m.values();
    for (std::size_t i = 0; i < values.size(); ++i) {
        const float value = values[i];
        if (!std::isfinite(value) || (value < 0 && i + 1 != values.size())) {
            throw std::invalid_argument("Avatar metric must be finite and nonnegative except overlap");
        }
        combined += std::abs(static_cast<double>(value)) * 2048;
    }
    if (combined > std::numeric_limits<float>::max() || m.container_size <= 0 || m.container_size_large <= 0 ||
        m.container_size_small <= 0 || m.group_space < 0 || m.group_overlapping > 0 || m.border_width < 0 ||
        m.border_radius < 0 || m.border_radius_large < 0 || m.border_radius_small < 0 ||
        (m.font_family != SystemFontFamily::ui_sans && m.font_family != SystemFontFamily::ui_monospace)) {
        throw std::invalid_argument("Avatar final extents are invalid");
    }
    for (float font : {m.text_font_size, m.text_font_size_large, m.text_font_size_small, m.icon_font_size,
                       m.icon_font_size_large, m.icon_font_size_small}) {
        if (font < .5F || static_cast<double>(font) >= std::numeric_limits<long>::max() ||
            static_cast<double>(font) >= std::numeric_limits<std::uint32_t>::max()) {
            throw std::invalid_argument("Avatar final font is not renderable");
        }
    }
}
} // namespace ryn::detail
