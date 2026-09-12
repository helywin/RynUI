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
// State colors/shadows are added in the next Token substage.
struct InputTokenSet final {
    std::array<InputSizeTokens, 3> sizes;
    float border_width{}, affix_padding{};
    // Keep explicit inherited padding when a nested scope changes typography.
    std::array<bool, 3> padding_block_explicit{};
    bool small_font_explicit{};
    [[nodiscard]] const InputSizeTokens& size(ControlSize value) const;
    friend constexpr bool operator==(const InputTokenSet&, const InputTokenSet&) = default;
};

struct InputTokenAccess final {
    [[nodiscard]] static const InputTokenSet& get(const ThemeSnapshot& theme) noexcept;
};
[[nodiscard]] const InputTokenSet& derive_input_tokens(const ThemeSnapshot& theme) noexcept;
[[nodiscard]] InputTokenSet derive_input_tokens(const AntDesignDefaultSeed& seed, const ThemeMapToken& map);
void apply_input_override(InputTokenSet& tokens, const InputTokenOverride& overrides);

} // namespace ryn::detail
