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

struct InputColorTokens final {
    Color foreground, placeholder, background, border;
    Color disabled_foreground, disabled_background, disabled_border;
    Color hover_border, active_border, hover_background, active_background;
    Color error_border, error_hover_border, warning_border, warning_hover_border;
    Color selection_background, selection_foreground, caret, error_caret, warning_caret;
    [[nodiscard]] constexpr auto values() const noexcept {
        return std::array{foreground, placeholder, background, border, disabled_foreground,
            disabled_background, disabled_border, hover_border, active_border, hover_background,
            active_background, error_border, error_hover_border, warning_border, warning_hover_border,
            selection_background, selection_foreground, caret, error_caret, warning_caret};
    }
    friend constexpr bool operator==(const InputColorTokens&, const InputColorTokens&) = default;
};

// Internal resolved tokens; public customization belongs to ThemeConfig.
struct InputTokenSet final {
    std::array<InputSizeTokens, 3> sizes;
    float border_width{}, affix_padding{};
    // Keep explicit inherited padding when a nested scope changes typography.
    std::array<bool, 3> padding_block_explicit{};
    bool small_font_explicit{};
    InputColorTokens colors;
    ShadowList active_shadow, error_active_shadow, warning_active_shadow;
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
