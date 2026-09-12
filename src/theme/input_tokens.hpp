#pragma once

#include <ryn/control_size.hpp>
#include <ryn/theme.hpp>

#include <array>

namespace ryn::detail {

struct InputSizeTokens final {
    float control_height{}, font_size{}, line_height{};
    float padding_inline{}, padding_block{}, border_radius{};
    friend constexpr bool operator==(InputSizeTokens, InputSizeTokens) = default;
};

// Internal resolved geometry; public customization belongs to ThemeConfig.
// State colors/shadows and typed overrides are added in the next Token substage.
struct InputTokenSet final {
    std::array<InputSizeTokens, 3> sizes;
    float border_width{}, affix_padding{};
    [[nodiscard]] const InputSizeTokens& size(ControlSize value) const;
    friend constexpr bool operator==(const InputTokenSet&, const InputTokenSet&) = default;
};

[[nodiscard]] InputTokenSet derive_input_tokens(const ThemeSnapshot& theme);

} // namespace ryn::detail
