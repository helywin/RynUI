#include "theme/media_theme.hpp"
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
ImageThemeToken derive_image(const AntDesignDefaultSeed& seed, const ThemeMapToken& map, const ThemeAliasToken& alias) {
    ImageThemeToken t;
    t.colors = {alias.color_background_container, alias.color_text,
                alias.color_text_secondary,       map.color_error,
                alias.color_text_disabled,        alias.color_border,
                map.color_primary_border,         alias.color_focus_outline,
                Color::rgba8(0, 0, 0, 96),        Color::rgba8(0, 0, 0, 115),
                Color::rgba8(255, 255, 255),      Color::rgba8(0, 0, 0, 210)};
    t.metrics.row_height = map.control_height;
    t.metrics.gap = map.size_xs;
    t.metrics.padding = map.size_xs;
    t.metrics.radius = map.border_radius;
    t.metrics.border_width = seed.line_width;
    t.typography = {map.font_size, map.font_size * map.line_height, seed.font_family};
    t.effects = {alias.line_width_focus, alias.focus_outline_offset};
    return t;
}

void apply_image_override(ImageThemeToken& t, const ImageTokenOverride& o) {
    if (o.background) {
        t.colors.background = *o.background;
    }
    if (o.text) {
        t.colors.text = *o.text;
    }
    if (o.secondary) {
        t.colors.secondary = *o.secondary;
    }
    if (o.error) {
        t.colors.error = *o.error;
    }
    if (o.disabled) {
        t.colors.disabled = *o.disabled;
    }
    if (o.border) {
        t.colors.border = *o.border;
    }
    if (o.hover) {
        t.colors.hover = *o.hover;
    }
    if (o.focus) {
        t.colors.focus = *o.focus;
    }
    if (o.cover) {
        t.colors.cover = *o.cover;
    }
    if (o.mask) {
        t.colors.mask = *o.mask;
    }
    if (o.preview_text) {
        t.colors.preview_text = *o.preview_text;
    }
    if (o.toolbar) {
        t.colors.toolbar = *o.toolbar;
    }
    const auto length = [](float& target, const std::optional<LogicalLength>& value) {
        if (value) {
            if (value->is_auto() || !std::isfinite(value->value()) || value->value() < 0 || value->value() > 1000000) {
                throw std::invalid_argument("Invalid media token length");
            }
            target = value->value();
        }
    };
    length(t.metrics.placeholder_size, o.placeholder_size);
    length(t.metrics.thumbnail_size, o.thumbnail_size);
    length(t.metrics.preview_width, o.preview_width);
    length(t.metrics.preview_height, o.preview_height);
    length(t.metrics.row_height, o.row_height);
    length(t.metrics.gap, o.gap);
    length(t.metrics.padding, o.padding);
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
    if (t.typography.font_size < 1 || t.typography.line_height <= 0 || t.metrics.preview_width <= 0 ||
        t.metrics.preview_height <= 0 || t.metrics.thumbnail_size <= 0 || t.metrics.row_height <= 0 || t.order < 0 ||
        t.order > 1000000) {
        throw std::invalid_argument("Invalid media typography, metric or order");
    }
}

UploadThemeToken derive_upload(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                               const ThemeAliasToken& alias) {
    UploadThemeToken t;
    t.colors = {alias.color_background_container,
                alias.color_text,
                alias.color_text_secondary,
                map.color_error,
                alias.color_text_disabled,
                alias.color_border,
                map.color_primary_border,
                alias.color_focus_outline,
                Color::rgba8(0, 0, 0, 96),
                Color::rgba8(0, 0, 0, 115),
                alias.color_text,
                alias.color_background_elevated};
    t.metrics.row_height = map.control_height;
    t.metrics.gap = map.size_xs;
    t.metrics.padding = map.size_xs;
    t.metrics.radius = map.border_radius;
    t.metrics.border_width = seed.line_width;
    t.typography = {map.font_size, map.font_size * map.line_height, seed.font_family};
    t.effects = {alias.line_width_focus, alias.focus_outline_offset};
    return t;
}

void apply_upload_override(UploadThemeToken& t, const UploadTokenOverride& o) {
    if (o.background) {
        t.colors.background = *o.background;
    }
    if (o.text) {
        t.colors.text = *o.text;
    }
    if (o.secondary) {
        t.colors.secondary = *o.secondary;
    }
    if (o.error) {
        t.colors.error = *o.error;
    }
    if (o.disabled) {
        t.colors.disabled = *o.disabled;
    }
    if (o.border) {
        t.colors.border = *o.border;
    }
    if (o.hover) {
        t.colors.hover = *o.hover;
    }
    if (o.focus) {
        t.colors.focus = *o.focus;
    }
    if (o.cover) {
        t.colors.cover = *o.cover;
    }
    if (o.mask) {
        t.colors.mask = *o.mask;
    }
    if (o.preview_text) {
        t.colors.preview_text = *o.preview_text;
    }
    if (o.toolbar) {
        t.colors.toolbar = *o.toolbar;
    }
    const auto length = [](float& target, const std::optional<LogicalLength>& value) {
        if (value) {
            if (value->is_auto() || !std::isfinite(value->value()) || value->value() < 0 || value->value() > 1000000) {
                throw std::invalid_argument("Invalid media token length");
            }
            target = value->value();
        }
    };
    length(t.metrics.placeholder_size, o.placeholder_size);
    length(t.metrics.thumbnail_size, o.thumbnail_size);
    length(t.metrics.preview_width, o.preview_width);
    length(t.metrics.preview_height, o.preview_height);
    length(t.metrics.row_height, o.row_height);
    length(t.metrics.gap, o.gap);
    length(t.metrics.padding, o.padding);
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
    if (t.typography.font_size < 1 || t.typography.line_height <= 0 || t.metrics.preview_width <= 0 ||
        t.metrics.preview_height <= 0 || t.metrics.thumbnail_size <= 0 || t.metrics.row_height <= 0 || t.order < 0 ||
        t.order > 1000000) {
        throw std::invalid_argument("Invalid media typography, metric or order");
    }
}
} // namespace ryn::detail
