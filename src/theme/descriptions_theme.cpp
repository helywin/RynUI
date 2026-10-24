#include "theme/descriptions_theme.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
DescriptionsThemeToken derive_descriptions(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
                                           const ThemeAliasToken& alias, std::span<const ThemeAlgorithm> algorithms) {
    DescriptionsThemeToken token;
    const auto base = map.color_text_base;
    // Locked 6.6.5 descriptions/style/index.ts prepareComponentToken and size recipes.
    const bool dark = std::ranges::find(algorithms, ThemeAlgorithm::Dark) != algorithms.end();
    token.colors = {Color(base.red(), base.green(), base.blue(), dark ? .04F : .02F),
                    Color(base.red(), base.green(), base.blue(), .45F),
                    alias.color_text_secondary,
                    alias.color_text,
                    alias.color_text,
                    alias.color_text,
                    alias.color_split};
    auto& m = token.metrics;
    m.title_margin_bottom = map.font_size_small * map.line_height_small;
    m.item_padding_bottom = map.size;
    m.item_padding_bottom_middle = map.size_small;
    m.item_padding_bottom_small = map.size_xs;
    m.item_padding_end = map.size;
    m.colon_margin_left = seed.size_unit / 2;
    m.colon_margin_right = map.size_xs;
    m.padding_block = map.size;
    m.padding_block_middle = map.size_small;
    m.padding_block_small = map.size_xs;
    m.padding_inline = map.size_large;
    m.padding_inline_small = map.size;
    m.border_width = seed.line_width;
    m.border_radius = map.border_radius_large;
    m.font_size = map.font_size;
    m.line_height = map.font_size * map.line_height;
    m.title_font_size = map.font_size_large;
    m.title_line_height = map.font_size_large * map.line_height_large;
    m.font_family = seed.font_family;
    return token;
}

void apply_descriptions_override(DescriptionsThemeToken& token, const DescriptionsTokenOverride& o) {
    auto& c = token.colors;
    if (o.label_background) {
        c.label_background = *o.label_background;
    }
    if (o.label) {
        c.label = *o.label;
    }
    if (o.bordered_label) {
        c.bordered_label = *o.bordered_label;
    }
    if (o.content) {
        c.content = *o.content;
    }
    if (o.title) {
        c.title = *o.title;
    }
    if (o.extra) {
        c.extra = *o.extra;
    }
    if (o.border) {
        c.border = *o.border;
    }
    auto& m = token.metrics;
    const auto length = [](float& target, const std::optional<LogicalLength>& value) {
        if (value) {
            if (value->is_auto()) {
                throw std::invalid_argument("Descriptions token cannot be auto");
            }
            target = value->value();
        }
    };
    length(m.title_margin_bottom, o.title_margin_bottom);
    length(m.item_padding_bottom, o.item_padding_bottom);
    length(m.item_padding_bottom_middle, o.item_padding_bottom_middle);
    length(m.item_padding_bottom_small, o.item_padding_bottom_small);
    length(m.item_padding_end, o.item_padding_end);
    length(m.colon_margin_left, o.colon_margin_left);
    length(m.colon_margin_right, o.colon_margin_right);
    length(m.padding_block, o.padding_block);
    length(m.padding_block_middle, o.padding_block_middle);
    length(m.padding_block_small, o.padding_block_small);
    length(m.padding_inline, o.padding_inline);
    length(m.padding_inline_small, o.padding_inline_small);
    length(m.border_width, o.border_width);
    length(m.border_radius, o.border_radius);
    length(m.font_size, o.font_size);
    length(m.line_height, o.line_height);
    length(m.title_font_size, o.title_font_size);
    length(m.title_line_height, o.title_line_height);
    if (o.font_family) {
        m.font_family = *o.font_family;
    }
    if (o.title_font_weight) {
        m.title_font_weight = *o.title_font_weight;
    }
    float sum = 0;
    for (float value : m.values()) {
        if (!std::isfinite(value) || value < 0) {
            throw std::invalid_argument("Descriptions final metrics must be finite and non-negative");
        }
        sum += value * 4;
    }
    if (!std::isfinite(sum) || m.line_height <= 0 || m.title_line_height <= 0 || m.font_size < .5F ||
        m.title_font_size < .5F || static_cast<double>(m.font_size) >= std::numeric_limits<long>::max() ||
        static_cast<double>(m.title_font_size) >= std::numeric_limits<long>::max() ||
        static_cast<double>(m.font_size) >= std::numeric_limits<std::uint32_t>::max() ||
        static_cast<double>(m.title_font_size) >= std::numeric_limits<std::uint32_t>::max() ||
        (m.font_family != SystemFontFamily::ui_sans && m.font_family != SystemFontFamily::ui_monospace) ||
        m.title_font_weight < 1 || m.title_font_weight > 1000) {
        throw std::invalid_argument("Descriptions final typography or combined extents are invalid");
    }
}
} // namespace ryn::detail
