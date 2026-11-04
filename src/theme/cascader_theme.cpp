#include "theme/cascader_theme.hpp"
#include "theme/popover_theme.hpp"
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
CascaderThemeToken derive_cascader(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                                   const ThemeAliasToken& alias) {
    CascaderThemeToken t;
    // Ant Design 6.6.5 Cascader columns and shared Select selector recipe.
    t.colors = {alias.color_background_container,
                alias.color_text,
                alias.color_text_disabled,
                alias.color_border,
                map.color_primary_hover,
                map.color_primary,
                alias.color_background_container_disabled,
                alias.color_text_disabled,
                alias.color_background_elevated,
                alias.color_text,
                Color(map.color_text_base.red(), map.color_text_base.green(), map.color_text_base.blue(), .04F),
                Color(map.color_primary.red(), map.color_primary.green(), map.color_primary.blue(), .1F),
                alias.color_text,
                alias.color_split,
                alias.color_text_disabled,
                alias.color_text_disabled,
                Color(map.color_text_base.red(), map.color_text_base.green(), map.color_text_base.blue(), .06F),
                alias.color_text,
                map.color_primary,
                alias.color_text_disabled,
                alias.color_focus_outline};
    t.metrics = {184,
                 111,
                 180,
                 map.control_height,
                 map.size_small,
                 std::max(0.F, std::round((map.control_height - map.font_size * map.line_height) / 2)),
                 map.size_xs / 2,
                 map.control_height,
                 map.control_height_small,
                 map.control_height_large,
                 map.size_small,
                 map.control_height - 8,
                 4,
                 8,
                 map.border_radius};
    t.typography = {seed.font_family, 400, 600, map.font_size, map.font_size * map.line_height};
    t.effects = {alias.line_width_focus, alias.focus_outline_offset};
    t.shadow = alias.box_shadow_secondary;
    if (seed.z_index_popup_base > std::numeric_limits<std::int32_t>::max() - 50) {
        throw std::invalid_argument("Cascader z-index overflow");
    }
    t.z_index_popup = seed.z_index_popup_base + 50;
    return t;
}

void apply_cascader_override(CascaderThemeToken& t, const CascaderTokenOverride& o) {
    if (o.selector_background) {
        t.colors.selector_background = *o.selector_background;
    }
    if (o.selector_color) {
        t.colors.selector_color = *o.selector_color;
    }
    if (o.placeholder) {
        t.colors.placeholder = *o.placeholder;
    }
    if (o.border) {
        t.colors.border = *o.border;
    }
    if (o.hover_border) {
        t.colors.hover_border = *o.hover_border;
    }
    if (o.active_border) {
        t.colors.active_border = *o.active_border;
    }
    if (o.disabled_background) {
        t.colors.disabled_background = *o.disabled_background;
    }
    if (o.disabled_color) {
        t.colors.disabled_color = *o.disabled_color;
    }
    if (o.popup_background) {
        t.colors.popup_background = *o.popup_background;
    }
    if (o.option_color) {
        t.colors.option_color = *o.option_color;
    }
    if (o.option_hover_background) {
        t.colors.option_hover_background = *o.option_hover_background;
    }
    if (o.option_selected_background) {
        t.colors.option_selected_background = *o.option_selected_background;
    }
    if (o.option_selected_color) {
        t.colors.option_selected_color = *o.option_selected_color;
    }
    if (o.separator) {
        t.colors.separator = *o.separator;
    }
    if (o.arrow) {
        t.colors.arrow = *o.arrow;
    }
    if (o.clear) {
        t.colors.clear = *o.clear;
    }
    if (o.tag_background) {
        t.colors.tag_background = *o.tag_background;
    }
    if (o.tag_color) {
        t.colors.tag_color = *o.tag_color;
    }
    if (o.search_highlight) {
        t.colors.search_highlight = *o.search_highlight;
    }
    if (o.empty) {
        t.colors.empty = *o.empty;
    }
    if (o.focus) {
        t.colors.focus = *o.focus;
    }
    if (o.control_width) {
        if (o.control_width->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.control_width = o.control_width->value();
    }
    if (o.column_width) {
        if (o.column_width->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.column_width = o.column_width->value();
    }
    if (o.dropdown_height) {
        if (o.dropdown_height->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.dropdown_height = o.dropdown_height->value();
    }
    if (o.option_height) {
        if (o.option_height->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.option_height = o.option_height->value();
    }
    if (o.option_padding_inline) {
        if (o.option_padding_inline->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.option_padding_inline = o.option_padding_inline->value();
    }
    if (o.option_padding_block) {
        if (o.option_padding_block->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.option_padding_block = o.option_padding_block->value();
    }
    if (o.menu_padding) {
        if (o.menu_padding->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.menu_padding = o.menu_padding->value();
    }
    if (o.selector_height) {
        if (o.selector_height->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.selector_height = o.selector_height->value();
    }
    if (o.selector_height_small) {
        if (o.selector_height_small->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.selector_height_small = o.selector_height_small->value();
    }
    if (o.selector_height_large) {
        if (o.selector_height_large->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.selector_height_large = o.selector_height_large->value();
    }
    if (o.selector_padding) {
        if (o.selector_padding->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.selector_padding = o.selector_padding->value();
    }
    if (o.tag_height) {
        if (o.tag_height->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.tag_height = o.tag_height->value();
    }
    if (o.tag_gap) {
        if (o.tag_gap->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.tag_gap = o.tag_gap->value();
    }
    if (o.tag_padding) {
        if (o.tag_padding->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.tag_padding = o.tag_padding->value();
    }
    if (o.radius) {
        if (o.radius->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.metrics.radius = o.radius->value();
    }
    if (o.font_size) {
        if (o.font_size->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.typography.font_size = o.font_size->value();
    }
    if (o.line_height) {
        if (o.line_height->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.typography.line_height = o.line_height->value();
    }
    if (o.focus_width) {
        if (o.focus_width->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.effects.focus_width = o.focus_width->value();
    }
    if (o.focus_offset) {
        if (o.focus_offset->is_auto()) {
            throw std::invalid_argument("Cascader token cannot be auto");
        }
        t.effects.focus_offset = o.focus_offset->value();
    }
    if (o.font_family) {
        t.typography.font_family = *o.font_family;
    }
    if (o.font_weight) {
        t.typography.font_weight = *o.font_weight;
    }
    if (o.selected_font_weight) {
        t.typography.selected_font_weight = *o.selected_font_weight;
    }
    if (o.shadow) {
        t.shadow = *o.shadow;
    }
    if (o.z_index_popup) {
        t.z_index_popup = *o.z_index_popup;
    }
    double extent{};
    const auto check = [&](auto values) {
        for (float v : values) {
            if (!std::isfinite(v) || v < 0) {
                throw std::invalid_argument("Cascader metric is invalid");
            }
            extent += static_cast<double>(v) * 65536;
        }
    };
    check(t.metrics.values());
    check(t.typography.values());
    check(t.effects.values());
    if (extent > std::numeric_limits<float>::max() || t.metrics.control_width <= 0 || t.metrics.column_width <= 0 ||
        t.metrics.dropdown_height <= 0 || t.metrics.option_height <= 0 || t.metrics.selector_height <= 0 ||
        t.metrics.selector_height_small <= 0 || t.metrics.selector_height_large <= 0 || t.metrics.tag_height <= 0 ||
        t.typography.font_size < .5F ||
        t.typography.font_size >= static_cast<float>(std::numeric_limits<int>::max() / 64) ||
        t.typography.line_height <= 0 || t.typography.font_weight < 100 || t.typography.font_weight > 1000 ||
        t.typography.selected_font_weight < 100 || t.typography.selected_font_weight > 1000 ||
        (t.typography.font_family != SystemFontFamily::ui_sans &&
         t.typography.font_family != SystemFontFamily::ui_monospace) ||
        t.z_index_popup < 0) {
        throw std::invalid_argument("Cascader final token is invalid");
    }
    PopoverThemeToken popup;
    popup.shadow = t.shadow;
    apply_popover_override(popup, {});
}
} // namespace ryn::detail
