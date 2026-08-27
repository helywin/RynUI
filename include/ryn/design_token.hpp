#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>

namespace ryn {

namespace detail {

[[nodiscard]] constexpr bool finite(float value) noexcept {
    constexpr float finite_limit = 3.4028234663852886e+38F;
    return value == value && value <= finite_limit && value >= -finite_limit;
}

constexpr void require_finite(float value, const char* message) {
    if (!finite(value)) {
        throw std::invalid_argument(message);
    }
}

} // namespace detail

class Color final {
public:
    constexpr Color() noexcept = default;

    constexpr Color(float red, float green, float blue, float alpha = 1.0F)
        : red_(red), green_(green), blue_(blue), alpha_(alpha) {
        if (!valid_channel(red_) || !valid_channel(green_) || !valid_channel(blue_)
            || !valid_channel(alpha_)) {
            throw std::invalid_argument("color channels must be finite and in [0, 1]");
        }
    }

    [[nodiscard]] static constexpr Color rgba8(
        std::uint8_t red,
        std::uint8_t green,
        std::uint8_t blue,
        std::uint8_t alpha = 255) {
        constexpr float maximum = 255.0F;
        return Color(red / maximum, green / maximum, blue / maximum, alpha / maximum);
    }

    [[nodiscard]] constexpr float red() const noexcept { return red_; }
    [[nodiscard]] constexpr float green() const noexcept { return green_; }
    [[nodiscard]] constexpr float blue() const noexcept { return blue_; }
    [[nodiscard]] constexpr float alpha() const noexcept { return alpha_; }

    friend constexpr bool operator==(Color, Color) = default;

private:
    [[nodiscard]] static constexpr bool valid_channel(float value) noexcept {
        return detail::finite(value) && value >= 0.0F && value <= 1.0F;
    }

    float red_{};
    float green_{};
    float blue_{};
    float alpha_{};
};

struct LogicalOffset final {
    float x{};
    float y{};

    constexpr LogicalOffset(float x_value = 0.0F, float y_value = 0.0F)
        : x(x_value), y(y_value) {
        detail::require_finite(x, "logical x offset must be finite");
        detail::require_finite(y, "logical y offset must be finite");
    }

    friend constexpr bool operator==(LogicalOffset, LogicalOffset) = default;
};

class Duration final {
public:
    constexpr Duration() noexcept = default;

    [[nodiscard]] static constexpr Duration milliseconds(float value) {
        return Duration(value);
    }

    [[nodiscard]] static constexpr Duration seconds(float value) {
        return Duration(value * 1000.0F);
    }

    [[nodiscard]] constexpr float count_milliseconds() const noexcept {
        return milliseconds_;
    }

    friend constexpr bool operator==(Duration, Duration) = default;

private:
    explicit constexpr Duration(float value) : milliseconds_(value) {
        if (!detail::finite(value) || value < 0.0F) {
            throw std::invalid_argument("duration must be finite and non-negative");
        }
    }

    float milliseconds_{};
};

struct CubicBezier final {
    float x1{};
    float y1{};
    float x2{1.0F};
    float y2{1.0F};

    constexpr CubicBezier(
        float first_x,
        float first_y,
        float second_x,
        float second_y)
        : x1(first_x), y1(first_y), x2(second_x), y2(second_y) {
        if (!detail::finite(x1) || !detail::finite(y1) || !detail::finite(x2)
            || !detail::finite(y2) || x1 < 0.0F || x1 > 1.0F || x2 < 0.0F
            || x2 > 1.0F) {
            throw std::invalid_argument(
                "cubic-bezier x values must be in [0, 1] and all values finite");
        }
    }

    friend constexpr bool operator==(CubicBezier, CubicBezier) = default;
};

enum class BorderStyle : std::uint8_t {
    none,
    solid,
    dashed,
};

struct BorderToken final {
    float width{};
    BorderStyle style{BorderStyle::none};
    Color color{};

    constexpr BorderToken(
        float logical_width = 0.0F,
        BorderStyle border_style = BorderStyle::none,
        Color border_color = {})
        : width(logical_width), style(border_style), color(border_color) {
        if (!detail::finite(width) || width < 0.0F) {
            throw std::invalid_argument("border width must be finite and non-negative");
        }
    }

    friend constexpr bool operator==(BorderToken, BorderToken) = default;
};

enum class ShadowKind : std::uint8_t {
    outer,
    inset,
};

struct ShadowLayer final {
    ShadowKind kind{ShadowKind::outer};
    LogicalOffset offset{};
    float blur{};
    float spread{};
    Color color{};

    constexpr ShadowLayer() noexcept = default;

    constexpr ShadowLayer(
        ShadowKind shadow_kind,
        LogicalOffset logical_offset,
        float logical_blur,
        float logical_spread,
        Color shadow_color)
        : kind(shadow_kind),
          offset(logical_offset),
          blur(logical_blur),
          spread(logical_spread),
          color(shadow_color) {
        if (!detail::finite(blur) || blur < 0.0F || !detail::finite(spread)) {
            throw std::invalid_argument(
                "shadow blur must be non-negative and shadow values must be finite");
        }
    }

    friend constexpr bool operator==(ShadowLayer, ShadowLayer) = default;
};

class ShadowList final {
public:
    static constexpr std::size_t capacity = 8;

    constexpr ShadowList() noexcept = default;

    constexpr ShadowList(std::initializer_list<ShadowLayer> layers) {
        if (layers.size() > capacity) {
            throw std::invalid_argument("shadow list exceeds its fixed capacity");
        }
        for (const ShadowLayer& layer : layers) {
            layers_[size_++] = layer;
        }
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }

    [[nodiscard]] constexpr const ShadowLayer& operator[](std::size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("shadow layer index is out of range");
        }
        return layers_[index];
    }

    [[nodiscard]] constexpr std::span<const ShadowLayer> layers() const noexcept {
        return {layers_.data(), size_};
    }

    friend constexpr bool operator==(const ShadowList& left, const ShadowList& right) {
        if (left.size_ != right.size_) {
            return false;
        }
        for (std::size_t index = 0; index < left.size_; ++index) {
            if (left.layers_[index] != right.layers_[index]) {
                return false;
            }
        }
        return true;
    }

private:
    std::array<ShadowLayer, capacity> layers_{};
    std::size_t size_{};
};

enum class SystemFontFamily : std::uint8_t {
    ui_sans,
    ui_monospace,
};

struct AntDesignDefaultSeed final {
    Color color_primary;
    Color color_success;
    Color color_warning;
    Color color_error;
    Color color_info;
    std::optional<Color> color_link;
    std::optional<Color> color_text_base;
    std::optional<Color> color_background_base;
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    SystemFontFamily font_family_code{SystemFontFamily::ui_monospace};
    std::uint32_t font_size{14};
    float line_width{1.0F};
    float border_radius{6.0F};
    float size_unit{4.0F};
    float size_step{4.0F};
    float size_popup_arrow{16.0F};
    float control_height{32.0F};
    std::int32_t z_index_base{};
    std::int32_t z_index_popup_base{1000};
    float opacity_image{1.0F};
    Duration motion_unit{Duration::seconds(0.1F)};
    Duration motion_base{};
    bool wireframe{};
    bool motion{true};

    friend constexpr bool operator==(
        const AntDesignDefaultSeed&,
        const AntDesignDefaultSeed&) = default;
};

struct AntDesignShadowSnapshot final {
    ShadowList box_shadow;
    ShadowList box_shadow_secondary;
    ShadowList box_shadow_tertiary;
    ShadowList button_default;
    ShadowList button_primary;
    ShadowList button_danger;
    ShadowList popover_arrow;
    ShadowList popover_drop;
    ShadowList card;
    ShadowList drawer_right;
    ShadowList drawer_left;
    ShadowList drawer_up;
    ShadowList drawer_down;
    ShadowList tabs_overflow_left;
    ShadowList tabs_overflow_right;
    ShadowList tabs_overflow_top;
    ShadowList tabs_overflow_bottom;
};

[[nodiscard]] const AntDesignDefaultSeed& ant_design_default_seed() noexcept;
[[nodiscard]] const AntDesignShadowSnapshot& ant_design_default_shadows() noexcept;

enum class TokenValueKind : std::uint8_t {
    boolean,
    color,
    cubic_bezier,
    duration,
    integer,
    logical_length,
    shadow_list,
    string,
    typed_expression,
    web_css_value,
};

enum class TokenSupportStatus : std::uint8_t {
    runtime,
    metadata,
    web_only,
    deprecated,
    component_not_yet_implemented,
};

enum class TokenInvalidationDomain : std::uint8_t {
    animation,
    effect_geometry_material,
    geometry_paint,
    measure_layout_hittest,
    metadata,
    paint_material,
    paint_order,
    text_measure_layout,
};

struct TokenMetadata final {
    std::uint64_t stable_id{};
    std::string_view identity;
    TokenValueKind value_kind{TokenValueKind::string};
    std::string_view component_owner;
    TokenSupportStatus support{TokenSupportStatus::metadata};
    TokenInvalidationDomain invalidation{TokenInvalidationDomain::metadata};
};

[[nodiscard]] std::span<const TokenMetadata> ant_design_token_metadata() noexcept;
[[nodiscard]] const TokenMetadata* find_ant_design_token(std::string_view identity) noexcept;

} // namespace ryn
