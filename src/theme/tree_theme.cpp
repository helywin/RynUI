#include "theme/tree_theme.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
TreeThemeToken derive_tree(const AntDesignDefaultSeed& seed, const ThemeMapToken& map, const ThemeAliasToken& alias,
                           const SelectThemeToken& palette) {
    TreeThemeToken t;
    t.colors = {alias.color_background_container,
                alias.color_text,
                alias.color_text_disabled,
                palette.colors.option_active_background,
                alias.color_text,
                palette.colors.option_selected_background,
                alias.color_text,
                alias.color_border,
                alias.color_background_container,
                map.color_primary,
                Color::rgba8(255, 255, 255),
                alias.color_focus_outline};
    t.metrics = {map.control_height_small,
                 map.control_height_small,
                 map.control_height_small,
                 map.size_xs / 2,
                 10,
                 map.size_xs / 2,
                 map.size_xs / 2,
                 map.border_radius_small,
                 map.font_size + seed.line_width * 2,
                 seed.line_width,
                 alias.line_width_focus,
                 alias.focus_outline_offset,
                 map.font_size,
                 map.font_size * map.line_height,
                 seed.font_family};
    return t;
}

void apply_tree_override(TreeThemeToken& t, const TreeTokenOverride& o) {
    if (o.background) {
        t.colors.background = *o.background;
    }
    if (o.text) {
        t.colors.text = *o.text;
    }
    if (o.disabled) {
        t.colors.disabled = *o.disabled;
    }
    if (o.node_hover_background) {
        t.colors.node_hover_background = *o.node_hover_background;
    }
    if (o.node_hover_text) {
        t.colors.node_hover_text = *o.node_hover_text;
    }
    if (o.node_selected_background) {
        t.colors.node_selected_background = *o.node_selected_background;
    }
    if (o.node_selected_text) {
        t.colors.node_selected_text = *o.node_selected_text;
    }
    if (o.checkbox_border) {
        t.colors.checkbox_border = *o.checkbox_border;
    }
    if (o.checkbox_background) {
        t.colors.checkbox_background = *o.checkbox_background;
    }
    if (o.checkbox_checked) {
        t.colors.checkbox_checked = *o.checkbox_checked;
    }
    if (o.checkbox_mark) {
        t.colors.checkbox_mark = *o.checkbox_mark;
    }
    if (o.focus) {
        t.colors.focus = *o.focus;
    }
    auto& m = t.metrics;
    const auto length = [](float& target, const std::optional<LogicalLength>& value) {
        if (value) {
            if (value->is_auto()) {
                throw std::invalid_argument("Tree token cannot be auto");
            }
            target = value->value();
        }
    };
    length(m.title_height, o.title_height);
    length(m.switcher_size, o.switcher_size);
    length(m.indent_size, o.indent_size);
    length(m.row_gap, o.row_gap);
    length(m.icon_size, o.icon_size);
    length(m.icon_gap, o.icon_gap);
    length(m.padding_inline, o.padding_inline);
    length(m.border_radius, o.border_radius);
    length(m.checkbox_size, o.checkbox_size);
    length(m.border_width, o.border_width);
    length(m.focus_width, o.focus_width);
    length(m.focus_offset, o.focus_offset);
    length(m.font_size, o.font_size);
    length(m.line_height, o.line_height);
    if (o.font_family) {
        m.font_family = *o.font_family;
    }
    double sum = 0;
    for (float value : m.values()) {
        if (!std::isfinite(value) || value < 0) {
            throw std::invalid_argument("Tree metrics must be finite and nonnegative");
        }
        sum += static_cast<double>(value) * 65536;
    }
    if (sum > std::numeric_limits<float>::max() || m.title_height <= 0 || m.line_height <= 0 ||
        (m.font_family != SystemFontFamily::ui_sans && m.font_family != SystemFontFamily::ui_monospace)) {
        throw std::invalid_argument("Tree combined extents or typography are invalid");
    }
    for (float font : {m.font_size, m.icon_size}) {
        const double pixels = std::round(static_cast<double>(font));
        if (font < .5F || pixels > std::numeric_limits<int>::max() / 64) {
            throw std::invalid_argument("Tree final font is not renderable");
        }
    }
}
} // namespace ryn::detail
