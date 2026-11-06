#include "theme/qrcode_theme.hpp"
#include <cmath>

namespace ryn::detail {
QRCodeThemeToken derive_qrcode(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                               const ThemeAliasToken& alias) {
    QRCodeThemeToken t;
    t.colors = {alias.color_text, Color{0, 0, 0, 0}, alias.color_border_secondary,
                Color{alias.color_background_container.red(), alias.color_background_container.green(),
                      alias.color_background_container.blue(), .96F},
                alias.color_text};
    t.metrics = {map.size_small, map.border_radius_large,         seed.line_width,
                 map.font_size,  map.font_size * map.line_height, seed.font_family};
    return t;
}

void apply_qrcode_override(QRCodeThemeToken& t, const QRCodeTokenOverride& o) {
    if (o.foreground) {
        t.colors.foreground = *o.foreground;
    }
    if (o.background) {
        t.colors.background = *o.background;
    }
    if (o.border) {
        t.colors.border = *o.border;
    }
    if (o.cover) {
        t.colors.cover = *o.cover;
    }
    if (o.text) {
        t.colors.text = *o.text;
    }
    if (o.padding) {
        const auto x = *o.padding;
        if (x.is_auto() || !std::isfinite(x.value()) || x.value() < 0 || x.value() > 16384) {
            throw std::invalid_argument("QRCode padding is invalid");
        }
        t.metrics.padding = x.value();
    }
    if (o.radius) {
        const auto x = *o.radius;
        if (x.is_auto() || !std::isfinite(x.value()) || x.value() < 0 || x.value() > 16384) {
            throw std::invalid_argument("QRCode radius is invalid");
        }
        t.metrics.radius = x.value();
    }
    if (o.border_width) {
        const auto x = *o.border_width;
        if (x.is_auto() || !std::isfinite(x.value()) || x.value() < 0 || x.value() > 16384) {
            throw std::invalid_argument("QRCode border_width is invalid");
        }
        t.metrics.border_width = x.value();
    }
    if (o.font_size) {
        const auto x = *o.font_size;
        if (x.is_auto() || !std::isfinite(x.value()) || x.value() < 0 || x.value() > 16384) {
            throw std::invalid_argument("QRCode font_size is invalid");
        }
        t.metrics.font_size = x.value();
    }
    if (o.line_height) {
        const auto x = *o.line_height;
        if (x.is_auto() || !std::isfinite(x.value()) || x.value() < 0 || x.value() > 16384) {
            throw std::invalid_argument("QRCode line_height is invalid");
        }
        t.metrics.line_height = x.value();
    }
    if (o.font_family) {
        t.metrics.font_family = *o.font_family;
    }
    if (t.metrics.font_size < 1 || t.metrics.line_height < 1 ||
        (t.metrics.font_family != SystemFontFamily::ui_sans &&
         t.metrics.font_family != SystemFontFamily::ui_monospace)) {
        throw std::invalid_argument("QRCode font is not renderable");
    }
}
} // namespace ryn::detail
