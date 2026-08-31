#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ryn::theme_runtime {

enum class TokenIdentity : std::uint8_t {
    alias_color_text,
    alias_color_text_secondary,
    alias_color_text_disabled,
    alias_color_background_container,
    alias_color_background_elevated,
    alias_color_background_container_disabled,
    alias_color_border,
    alias_color_border_secondary,
    alias_color_focus_outline,
    alias_line_width_focus,
    alias_focus_outline_offset,
    alias_box_shadow,
    alias_box_shadow_secondary,
    alias_box_shadow_tertiary,
    map_color_primary,
    map_color_primary_hover,
    map_color_primary_active,
    map_color_primary_border,
    map_color_success,
    map_color_warning,
    map_color_error,
    map_color_error_hover,
    map_color_error_active,
    map_color_info,
    map_color_text_base,
    map_color_background_base,
    map_font_size_small,
    map_font_size,
    map_font_size_large,
    map_line_height_small,
    map_line_height,
    map_line_height_large,
    map_size_xs,
    map_size_small,
    map_size,
    map_size_large,
    map_control_height_small,
    map_control_height,
    map_control_height_large,
    map_border_radius_small,
    map_border_radius,
    map_border_radius_large,
    map_motion_unit,
    map_motion_base,
    map_motion_enabled,
    button_colors,
    button_control_heights,
    button_padding_inline,
    button_typography,
    button_border_radius,
    button_border_width,
    button_icon_gap,
    button_shadows,
    text_color,
    text_font_family,
    text_font_weight,
    text_font_size,
    text_line_height,
    count,
};

enum class DirtyPhase : std::uint32_t {
    none = 0,
    paint_material = 1U << 0U,
    geometry = 1U << 1U,
    text = 1U << 2U,
    measure_layout = 1U << 3U,
    hit_test = 1U << 4U,
    animation = 1U << 5U,
};

[[nodiscard]] constexpr DirtyPhase operator|(
    DirtyPhase left,
    DirtyPhase right) noexcept {
    return static_cast<DirtyPhase>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

[[nodiscard]] constexpr DirtyPhase operator&(
    DirtyPhase left,
    DirtyPhase right) noexcept {
    return static_cast<DirtyPhase>(
        static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

constexpr DirtyPhase& operator|=(DirtyPhase& left, DirtyPhase right) noexcept {
    left = left | right;
    return left;
}

[[nodiscard]] constexpr bool has_any(
    DirtyPhase value,
    DirtyPhase mask) noexcept {
    return static_cast<std::uint32_t>(value & mask) != 0U;
}

[[nodiscard]] std::string_view token_identity_name(TokenIdentity identity) noexcept;
[[nodiscard]] DirtyPhase dirty_phase_for(TokenIdentity identity) noexcept;

struct Diagnostics final {
    std::uint64_t generation{1};
    std::uint64_t snapshot_allocations{1};
    std::uint64_t snapshot_reuses{0};
    std::uint64_t subscription_allocations{0};
    std::uint64_t notifications{0};
    std::size_t changed_identity_count{0};
    std::size_t subscriber_count{0};
    DirtyPhase dirty_phase{DirtyPhase::none};
};

} // namespace ryn::theme_runtime
