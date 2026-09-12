#include "theme/input_tokens.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {

const InputSizeTokens& InputTokenSet::size(ControlSize value) const {
    switch(value) {
    case ControlSize::Small: return sizes[0];
    case ControlSize::Middle: return sizes[1];
    case ControlSize::Large: return sizes[2];
    }
    throw std::invalid_argument("Invalid Input token size");
}

const InputTokenSet& InputTokenAccess::get(const ThemeSnapshot& theme) noexcept { return *theme.input_; }
const InputTokenSet& derive_input_tokens(const ThemeSnapshot& theme) noexcept { return InputTokenAccess::get(theme); }

InputTokenSet derive_input_tokens(const AntDesignDefaultSeed& seed, const ThemeMapToken& map) {
    // Ant Design 6.5.0, components/input/style/token.ts, commit
    // 740ad964dc2397f33e40944367b0536a7314cc32. SM uses the base font,
    // LG uses lineHeightLG; horizontal control padding is 8 / 12, not sizeXS.
    InputTokenSet result;
    result.border_width = seed.line_width;
    result.affix_padding = std::max(0.0F, seed.size_unit * (seed.size_step - 3.0F));
    // Compact sizeXXS and default sizeXXS share this formula, unlike sizeXS.
    const auto metrics = [&](float height, float font, float ratio, float padding,
                             float radius, bool large) {
        const float line = font * ratio;
        const float half = (height - line) * 5.0F;
        // JavaScript Math.round rounds ties towards positive infinity.
        const float block = (large ? std::ceil(half) : std::floor(half + 0.5F)) / 10.0F
            - seed.line_width;
        return InputSizeTokens{height, font, line, std::max(0.0F, padding - seed.line_width),
            std::max(0.0F, block), radius};
    };
    result.sizes = {{
        metrics(map.control_height_small, map.font_size, map.line_height, 8.0F, map.border_radius_small, false),
        metrics(map.control_height, map.font_size, map.line_height, map.size_small, map.border_radius, false),
        metrics(map.control_height_large, map.font_size_large, map.line_height_large, 12.0F, map.border_radius_large, true),
    }};
    return result;
}

void apply_input_override(InputTokenSet& tokens, const InputTokenOverride& overrides) {
    const auto length = [](const std::optional<LogicalLength>& value, float fallback, bool positive = false) {
        if(!value) return fallback;
        if(value->is_auto() || !std::isfinite(value->value()) || value->value() < 0
            || (positive && value->value() == 0)) throw std::invalid_argument("Invalid Input token length");
        return value->value();
    };
    const std::array fonts{overrides.input_font_size_small, overrides.input_font_size, overrides.input_font_size_large};
    const std::array horizontal{overrides.padding_inline_small, overrides.padding_inline, overrides.padding_inline_large};
    const std::array vertical{overrides.padding_block_small, overrides.padding_block, overrides.padding_block_large};
    auto next = tokens;
    if(overrides.input_font_size_small) next.small_font_explicit = true;
    const float base_font = length(overrides.input_font_size, next.sizes[1].font_size, true);
    for(std::size_t i = 0; i < next.sizes.size(); ++i) {
        auto& size = next.sizes[i];
        const float font = length(fonts[i], i == 0 && !next.small_font_explicit ? base_font : size.font_size, true);
        if(font != size.font_size) {
            size.line_height = font * (size.line_height / size.font_size);
            if(!std::isfinite(size.line_height)) throw std::invalid_argument("Input line height overflow");
            size.font_size = font;
            if(!next.padding_block_explicit[i]) {
                const float half = (size.control_height - size.line_height) * 5.0F;
                size.padding_block = std::max(0.0F, (i == 2 ? std::ceil(half) : std::floor(half + 0.5F)) / 10.0F - next.border_width);
            }
        }
        size.padding_inline = length(horizontal[i], size.padding_inline);
        size.padding_block = length(vertical[i], size.padding_block);
        if(vertical[i]) next.padding_block_explicit[i] = true;
    }
    next.sizes[1].border_radius = length(overrides.border_radius, next.sizes[1].border_radius);
    next.affix_padding = length(overrides.affix_padding, next.affix_padding);
    const auto color = [](Color& target, const std::optional<Color>& value) { if(value) target = *value; };
    color(next.colors.foreground, overrides.color);
    color(next.colors.placeholder, overrides.placeholder_color);
    color(next.colors.background, overrides.background);
    color(next.colors.border, overrides.border_color);
    color(next.colors.hover_border, overrides.hover_border_color);
    color(next.colors.active_border, overrides.active_border_color);
    color(next.colors.hover_background, overrides.hover_background);
    color(next.colors.active_background, overrides.active_background);
    color(next.colors.selection_background, overrides.selection_background);
    color(next.colors.selection_foreground, overrides.selection_color);
    color(next.colors.caret, overrides.caret_color);
    if(overrides.active_shadow) next.active_shadow = *overrides.active_shadow;
    if(overrides.error_active_shadow) next.error_active_shadow = *overrides.error_active_shadow;
    if(overrides.warning_active_shadow) next.warning_active_shadow = *overrides.warning_active_shadow;
    tokens = next;
}

} // namespace ryn::detail
