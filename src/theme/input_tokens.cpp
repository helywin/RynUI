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

InputTokenSet derive_input_tokens(const ThemeSnapshot& theme) {
    // Ant Design 6.5.0, components/input/style/token.ts, commit
    // 740ad964dc2397f33e40944367b0536a7314cc32. SM uses the base font,
    // LG uses lineHeightLG; horizontal control padding is 8 / 12, not sizeXS.
    const auto& map = theme.map();
    const auto& seed = theme.seed();
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

} // namespace ryn::detail
