#include "theme/carousel_theme.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
CarouselThemeToken derive_carousel(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                                   const ThemeAliasToken& alias) {
    CarouselThemeToken t;
    const auto background = alias.color_background_container;
    t.colors = {Color(0, 0, 0, 0),
                Color::rgba8(255, 255, 255),
                Color(background.red(), background.green(), background.blue(), background.alpha() * .2F),
                background,
                background,
                alias.color_focus_outline};
    t.metrics.font_family = seed.font_family;
    t.metrics.font_size = map.font_size;
    t.metrics.line_height = map.font_size * map.line_height;
    // Pinned marginXXS is sizeXXS: Compact uses sizeXS, Default uses one size unit less.
    t.metrics.dot_gap = std::max(0.0F, map.size == map.size_small ? map.size_xs : map.size_xs - seed.size_unit);
    t.metrics.arrow_offset = map.size_xs;
    // Pinned arrowSize=16 and dotOffset=12 remain fixed across font/size algorithms.
    return t;
}

void apply_carousel_override(CarouselThemeToken& t, const CarouselTokenOverride& o) {
    if (o.background) {
        t.colors.background = *o.background;
    }
    if (o.arrow) {
        t.colors.arrow = *o.arrow;
    }
    if (o.dot) {
        t.colors.dot = *o.dot;
    }
    if (o.dot_active) {
        t.colors.dot_active = *o.dot_active;
    }
    if (o.progress) {
        t.colors.progress = *o.progress;
    }
    if (o.focus) {
        t.colors.focus = *o.focus;
    }
    if (o.width) {
        const auto v = *o.width;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel width is invalid");
        }
        t.metrics.width = v.value();
    }
    if (o.height) {
        const auto v = *o.height;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel height is invalid");
        }
        t.metrics.height = v.value();
    }
    if (o.dot_width) {
        const auto v = *o.dot_width;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel dot_width is invalid");
        }
        t.metrics.dot_width = v.value();
    }
    if (o.dot_height) {
        const auto v = *o.dot_height;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel dot_height is invalid");
        }
        t.metrics.dot_height = v.value();
    }
    if (o.dot_gap) {
        const auto v = *o.dot_gap;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel dot_gap is invalid");
        }
        t.metrics.dot_gap = v.value();
    }
    if (o.dot_offset) {
        const auto v = *o.dot_offset;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel dot_offset is invalid");
        }
        t.metrics.dot_offset = v.value();
    }
    if (o.dot_active_width) {
        const auto v = *o.dot_active_width;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel dot_active_width is invalid");
        }
        t.metrics.dot_active_width = v.value();
    }
    if (o.arrow_size) {
        const auto v = *o.arrow_size;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel arrow_size is invalid");
        }
        t.metrics.arrow_size = v.value();
    }
    if (o.arrow_hit_size) {
        const auto v = *o.arrow_hit_size;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel arrow_hit_size is invalid");
        }
        t.metrics.arrow_hit_size = v.value();
    }
    if (o.dot_hit_size) {
        const auto v = *o.dot_hit_size;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel dot_hit_size is invalid");
        }
        t.metrics.dot_hit_size = v.value();
    }
    if (o.arrow_offset) {
        const auto v = *o.arrow_offset;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel arrow_offset is invalid");
        }
        t.metrics.arrow_offset = v.value();
    }
    if (o.focus_width) {
        const auto v = *o.focus_width;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel focus_width is invalid");
        }
        t.metrics.focus_width = v.value();
    }
    if (o.font_size) {
        const auto v = *o.font_size;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel font_size is invalid");
        }
        t.metrics.font_size = v.value();
    }
    if (o.line_height) {
        const auto v = *o.line_height;
        if (v.is_auto() || !std::isfinite(v.value()) || v.value() < 0) {
            throw std::invalid_argument("Carousel line_height is invalid");
        }
        t.metrics.line_height = v.value();
    }
    if (o.font_family) {
        t.metrics.font_family = *o.font_family;
    }
    if (o.shadow) {
        t.shadow = *o.shadow;
    }
    if (o.disabled_opacity) {
        if (!std::isfinite(*o.disabled_opacity) || *o.disabled_opacity < 0 || *o.disabled_opacity > 1) {
            throw std::invalid_argument("Carousel opacity");
        }
        t.disabled_opacity = *o.disabled_opacity;
    }
    double extent{};
    for (auto v : t.metrics.values()) {
        extent += static_cast<double>(v) * 10000;
    }
    if (extent > std::numeric_limits<float>::max() || t.metrics.font_size < 1 || t.metrics.line_height < 1 ||
        t.metrics.font_size > std::numeric_limits<int>::max() / 64 ||
        (t.metrics.font_family != SystemFontFamily::ui_sans &&
         t.metrics.font_family != SystemFontFamily::ui_monospace)) {
        throw std::invalid_argument("Carousel final metrics are not renderable");
    }
}
} // namespace ryn::detail
