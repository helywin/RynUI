#include <ryn/theme.hpp>
#include "theme/input_tokens.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ryn {
namespace {

constexpr std::string_view ant_design_version = "6.6.5";
constexpr std::string_view ant_design_commit =
    "4a39f54842eade4e565ab336ef6097cd7e723cdd";

[[nodiscard]] float fixed_length(
    const std::optional<LogicalLength>& value,
    float fallback,
    std::string_view name,
    bool strictly_positive = false) {
    if (!value.has_value()) {
        return fallback;
    }
    if (value->is_auto() || !detail::finite(value->value())
        || (strictly_positive ? value->value() <= 0.0F : value->value() < 0.0F)) {
        throw std::invalid_argument(std::string(name));
    }
    return value->value();
}

void apply_seed_override(AntDesignDefaultSeed& seed, const SeedTokenOverride& override) {
    if (override.color_primary) seed.color_primary = *override.color_primary;
    if (override.color_success) seed.color_success = *override.color_success;
    if (override.color_warning) seed.color_warning = *override.color_warning;
    if (override.color_error) seed.color_error = *override.color_error;
    if (override.color_info) seed.color_info = *override.color_info;
    if (override.color_link) seed.color_link = *override.color_link;
    seed.font_size = static_cast<std::uint32_t>(std::lround(fixed_length(
        override.font_size, static_cast<float>(seed.font_size),
        "font size must be a positive fixed logical length", true)));
    seed.line_width = fixed_length(
        override.line_width, seed.line_width,
        "line width must be a non-negative fixed logical length");
    seed.border_radius = fixed_length(
        override.border_radius, seed.border_radius,
        "border radius must be a non-negative fixed logical length");
    seed.size_unit = fixed_length(
        override.size_unit, seed.size_unit,
        "size unit must be a positive fixed logical length", true);
    seed.size_step = fixed_length(
        override.size_step, seed.size_step,
        "size step must be at least two logical units", true);
    if (seed.size_step < 2.0F) {
        throw std::invalid_argument("size step must be at least two logical units");
    }
    seed.control_height = fixed_length(
        override.control_height, seed.control_height,
        "control height must be a positive fixed logical length", true);
    if (override.z_index_base) seed.z_index_base = *override.z_index_base;
    if (override.z_index_popup_base) seed.z_index_popup_base = *override.z_index_popup_base;
    if (override.opacity_image) {
        if (!detail::finite(*override.opacity_image) || *override.opacity_image < 0.0F
            || *override.opacity_image > 1.0F) {
            throw std::invalid_argument("image opacity must be finite and in [0, 1]");
        }
        seed.opacity_image = *override.opacity_image;
    }
    if (override.motion_unit) seed.motion_unit = *override.motion_unit;
    if (override.motion_base) seed.motion_base = *override.motion_base;
    if (override.focus_outline) seed.focus_outline = *override.focus_outline;
    if (override.motion) seed.motion = *override.motion;
}

[[nodiscard]] constexpr float rounded_channel(float value) noexcept {
    return static_cast<float>(static_cast<int>(value * 255.0F + 0.5F)) / 255.0F;
}

// Ant Design palette adaptations are evaluated in sRGB and quantized to 8-bit
// channels after each mix so C++ and both GPU backends receive identical values.
[[nodiscard]] constexpr Color mix(Color from, Color to, float amount) {
    return Color(
        rounded_channel(from.red() + (to.red() - from.red()) * amount),
        rounded_channel(from.green() + (to.green() - from.green()) * amount),
        rounded_channel(from.blue() + (to.blue() - from.blue()) * amount),
        from.alpha() + (to.alpha() - from.alpha()) * amount);
}

struct HsvColor final {
    float hue{};
    float saturation{};
    float value{};
};

[[nodiscard]] HsvColor to_hsv(Color color) noexcept {
    const float maximum = std::max({color.red(), color.green(), color.blue()});
    const float minimum = std::min({color.red(), color.green(), color.blue()});
    const float delta = maximum - minimum;
    float hue = 0.0F;
    if (delta != 0.0F) {
        if (maximum == color.red()) {
            hue = 60.0F * std::fmod(
                (color.green() - color.blue()) / delta, 6.0F);
        } else if (maximum == color.green()) {
            hue = 60.0F * (
                (color.blue() - color.red()) / delta + 2.0F);
        } else {
            hue = 60.0F * (
                (color.red() - color.green()) / delta + 4.0F);
        }
    }
    if (hue < 0.0F) {
        hue += 360.0F;
    }
    return {
        hue,
        maximum == 0.0F ? 0.0F : delta / maximum,
        maximum,
    };
}

[[nodiscard]] Color from_hsv(HsvColor color) {
    const float chroma = color.value * color.saturation;
    const float sector = color.hue / 60.0F;
    const float secondary = chroma
        * (1.0F - std::fabs(std::fmod(sector, 2.0F) - 1.0F));
    float red = 0.0F;
    float green = 0.0F;
    float blue = 0.0F;
    if (sector < 1.0F) {
        red = chroma;
        green = secondary;
    } else if (sector < 2.0F) {
        red = secondary;
        green = chroma;
    } else if (sector < 3.0F) {
        green = chroma;
        blue = secondary;
    } else if (sector < 4.0F) {
        green = secondary;
        blue = chroma;
    } else if (sector < 5.0F) {
        red = secondary;
        blue = chroma;
    } else {
        red = chroma;
        blue = secondary;
    }
    const float match = color.value - chroma;
    return Color(
        rounded_channel(red + match),
        rounded_channel(green + match),
        rounded_channel(blue + match));
}

[[nodiscard]] float round_hundredth(float value) noexcept {
    return std::round(value * 100.0F) / 100.0F;
}

// @ant-design/colors 8.0.1 generate.ts: palette key 5 is the semantic hover
// color and key 7 is the semantic active color around the seed at key 6.
[[nodiscard]] Color palette_variant(
    Color seed,
    int step,
    bool light) {
    auto hsv = to_hsv(seed);
    const bool grayscale = hsv.hue == 0.0F && hsv.saturation == 0.0F;
    const float rounded_hue = std::round(hsv.hue);
    const bool reverse_hue = rounded_hue >= 60.0F && rounded_hue <= 240.0F;
    hsv.hue = light == reverse_hue
        ? rounded_hue - 2.0F * static_cast<float>(step)
        : rounded_hue + 2.0F * static_cast<float>(step);
    if (hsv.hue < 0.0F) {
        hsv.hue += 360.0F;
    } else if (hsv.hue >= 360.0F) {
        hsv.hue -= 360.0F;
    }

    if (!grayscale) {
        hsv.saturation = light
            ? hsv.saturation - 0.16F * static_cast<float>(step)
            : hsv.saturation + (step == 4
                ? 0.16F : 0.05F * static_cast<float>(step));
        hsv.saturation = std::min(hsv.saturation, 1.0F);
        if (light && step == 5 && hsv.saturation > 0.1F) {
            hsv.saturation = 0.1F;
        }
        hsv.saturation = round_hundredth(std::max(hsv.saturation, 0.06F));
    }
    hsv.value = round_hundredth(std::clamp(
        hsv.value + (light ? 0.05F : -0.15F) * static_cast<float>(step),
        0.0F,
        1.0F));
    return from_hsv(hsv);
}

struct SemanticPalette final {
    Color hover;
    Color active;
};

[[nodiscard]] SemanticPalette semantic_palette(Color seed) {
    return {
        palette_variant(seed, 1, true),
        palette_variant(seed, 1, false),
    };
}

[[nodiscard]] constexpr bool is_default_primary(Color color) noexcept {
    return color == Color::rgba8(22, 119, 255);
}

// Upstream `getAlphaColor(color, background)`: find the smallest 1% alpha whose
// integer channel solve stays inside [0, 255]. Preserve the double loop and
// Math.round semantics; inputs are 8-bit palette values.
[[nodiscard]] Color get_alpha_color(Color foreground, Color background) {
    if (foreground.alpha() < 1.0F) {
        return foreground;
    }
    const std::array front{foreground.red(), foreground.green(), foreground.blue()};
    const std::array back{background.red(), background.green(), background.blue()};
    for (double alpha = 0.01; alpha <= 1.0; alpha += 0.01) {
        std::array<int, 3> channels{};
        bool stable = true;
        for (std::size_t i = 0; i < channels.size(); ++i) {
            const auto f = std::round(static_cast<double>(front[i]) * 255.0);
            const auto b = std::round(static_cast<double>(back[i]) * 255.0);
            channels[i] = static_cast<int>(
                std::floor((f - b * (1.0 - alpha)) / alpha + 0.5));
            stable = stable && channels[i] >= 0 && channels[i] <= 255;
        }
        if (stable) {
            return Color(static_cast<float>(channels[0]) / 255.0F,
                static_cast<float>(channels[1]) / 255.0F,
                static_cast<float>(channels[2]) / 255.0F,
                static_cast<float>(std::round(alpha * 100.0) / 100.0));
        }
    }
    return foreground;
}

// Upstream `genColorMapToken` reads `generateColorPalettes(base)` by palette
// KEY: key 9 is the semantic text color, key 8 its hover and key 10 its active.
// `palette_variant` takes a step RELATIVE to the seed, which is key 6, so keys
// 8/9/10 map to the dark-side relative steps 2/3/4. The light side brightens as
// the step grows and therefore cannot reach keys 8/9/10 at all.
// The palette is a pure function of the seed and does not depend on the theme
// algorithm, so the surface mix stays fixed: identity for the light container
// and the established 0.85 mix for the dark container.
[[nodiscard]] constexpr int palette_key_step(int key) noexcept {
    return key - 6;
}

[[nodiscard]] Color semantic_text_color(Color seed, int key, bool dark) {
    const Color palette = palette_variant(seed, palette_key_step(key), false);
    if (!dark) {
        return palette;
    }
    return mix(Color::rgba8(20, 20, 20), palette, 0.85F);
}

void apply_semantic_text_colors(
    ThemeMapToken& map, const AntDesignDefaultSeed& seed, bool dark) {
    map.color_success_text = semantic_text_color(seed.color_success, 9, dark);
    map.color_warning_text = semantic_text_color(seed.color_warning, 9, dark);
    map.color_error_text = semantic_text_color(seed.color_error, 9, dark);
    const Color link = seed.color_link.value_or(seed.color_info);
    map.color_link = semantic_text_color(link, 6, dark);
    map.color_link_hover = semantic_text_color(link, 8, dark);
    map.color_link_active = semantic_text_color(link, 10, dark);
}

[[nodiscard]] ThemeMapToken derive_default_map(const AntDesignDefaultSeed& seed) {
    const Color white = Color::rgba8(255, 255, 255);
    const Color black = Color::rgba8(0, 0, 0);
    const bool default_primary = is_default_primary(seed.color_primary);
    const float base_font = static_cast<float>(seed.font_size);
    const float small_font = std::floor(std::ceil(base_font / std::exp(0.2F)) / 2.0F) * 2.0F;
    const float large_font = std::floor((base_font * std::exp(0.2F)) / 2.0F) * 2.0F;
    const auto line_height = [](float font_size) { return (font_size + 8.0F) / font_size; };
    const float radius = seed.border_radius;
    const float radius_small = radius >= 5.0F && radius < 7.0F ? 4.0F : radius;
    const float radius_large = radius >= 6.0F && radius < 16.0F ? radius + 2.0F
        : (radius >= 16.0F ? 16.0F : radius);
    const auto error_palette = semantic_palette(seed.color_error);
    return {
        .color_primary = seed.color_primary,
        .color_primary_hover = default_primary
            ? Color::rgba8(64, 150, 255) : mix(seed.color_primary, white, 0.18F),
        .color_primary_active = default_primary
            ? Color::rgba8(9, 88, 217) : mix(seed.color_primary, black, 0.15F),
        .color_primary_border = default_primary
            ? Color::rgba8(145, 202, 255) : mix(seed.color_primary, white, 0.55F),
        .color_success = seed.color_success,
        .color_warning = seed.color_warning,
        .color_error = seed.color_error,
        .color_error_hover = error_palette.hover,
        .color_error_active = error_palette.active,
        .color_info = seed.color_info,
        .color_success_text = semantic_text_color(seed.color_success, 9, false),
        .color_warning_text = semantic_text_color(seed.color_warning, 9, false),
        .color_error_text = semantic_text_color(seed.color_error, 9, false),
        .color_link = semantic_text_color(seed.color_link.value_or(seed.color_info), 6, false),
        .color_link_hover = semantic_text_color(seed.color_link.value_or(seed.color_info), 8, false),
        .color_link_active = semantic_text_color(seed.color_link.value_or(seed.color_info), 10, false),
        .color_text_base = seed.color_text_base.value_or(black),
        .color_background_base = seed.color_background_base.value_or(white),
        .font_size_small = small_font,
        .font_size = base_font,
        .font_size_large = large_font,
        .line_height_small = line_height(small_font),
        .line_height = line_height(base_font),
        .line_height_large = line_height(large_font),
        .size_xs = seed.size_unit * (seed.size_step - 2.0F),
        .size_small = seed.size_unit * (seed.size_step - 1.0F),
        .size = seed.size_unit * seed.size_step,
        .size_large = seed.size_unit * (seed.size_step + 2.0F),
        .control_height_small = seed.control_height * 0.75F,
        .control_height = seed.control_height,
        .control_height_large = seed.control_height * 1.25F,
        .border_radius_small = radius_small,
        .border_radius = radius,
        .border_radius_large = radius_large,
        .motion_unit = seed.motion_unit,
        .motion_base = seed.motion_base,
        .motion = seed.motion,
    };
}

void apply_dark(ThemeMapToken& map, const AntDesignDefaultSeed& seed) {
    const bool default_primary = is_default_primary(map.color_primary);
    const Color black = Color::rgba8(0, 0, 0);
    const Color white = Color::rgba8(255, 255, 255);
    const Color dark_surface = Color::rgba8(20, 20, 20);
    const Color error_seed = map.color_error;
    const Color error_light_hover = map.color_error_hover;
    map.color_primary = default_primary
        ? Color::rgba8(22, 104, 220) : mix(map.color_primary, black, 0.12F);
    map.color_primary_hover = default_primary
        ? Color::rgba8(60, 137, 232) : mix(map.color_primary, white, 0.18F);
    map.color_primary_active = default_primary
        ? Color::rgba8(21, 84, 173) : mix(map.color_primary, black, 0.2F);
    map.color_primary_border = default_primary
        ? Color::rgba8(21, 50, 91) : mix(map.color_primary, black, 0.55F);
    map.color_error_hover = mix(dark_surface, error_seed, 0.65F);
    map.color_error = mix(dark_surface, error_seed, 0.85F);
    map.color_error_active = mix(dark_surface, error_light_hover, 0.90F);
    apply_semantic_text_colors(map, seed, true);
    map.color_text_base = white;
    map.color_background_base = black;
}

void apply_compact(ThemeMapToken& map, const AntDesignDefaultSeed& seed) {
    const float compact_font = map.font_size_small;
    map.font_size = compact_font;
    map.font_size_small =
        std::floor(std::ceil(compact_font / std::exp(0.2F)) / 2.0F) * 2.0F;
    map.font_size_large = std::floor((compact_font * std::exp(0.2F)) / 2.0F) * 2.0F;
    map.line_height_small = (map.font_size_small + 8.0F) / map.font_size_small;
    map.line_height = (map.font_size + 8.0F) / map.font_size;
    map.line_height_large = (map.font_size_large + 8.0F) / map.font_size_large;
    const float compact_step = seed.size_step - 2.0F;
    map.size_xs = seed.size_unit * (compact_step - 1.0F);
    map.size_small = seed.size_unit * compact_step;
    map.size = seed.size_unit * compact_step;
    map.size_large = seed.size_unit * (compact_step + 2.0F);
    map.control_height -= 4.0F;
    map.control_height_small = map.control_height * 0.75F;
    map.control_height_large = map.control_height * 1.25F;
}

[[nodiscard]] bool contains_dark(std::span<const ThemeAlgorithm> algorithms) {
    return std::find(algorithms.begin(), algorithms.end(), ThemeAlgorithm::Dark)
        != algorithms.end();
}

[[nodiscard]] ThemeMapToken derive_map(
    const AntDesignDefaultSeed& seed,
    std::span<const ThemeAlgorithm> algorithms) {
    ThemeMapToken map = derive_default_map(seed);
    for (const ThemeAlgorithm algorithm : algorithms) {
        switch (algorithm) {
        case ThemeAlgorithm::Default:
            map = derive_default_map(seed);
            break;
        case ThemeAlgorithm::Dark:
            apply_dark(map, seed);
            break;
        case ThemeAlgorithm::Compact:
            apply_compact(map, seed);
            break;
        default:
            throw std::invalid_argument("theme algorithm chain contains an invalid value");
        }
    }
    return map;
}

[[nodiscard]] ThemeAliasToken derive_alias(
    const AntDesignDefaultSeed& seed,
    const ThemeMapToken& map,
    std::span<const ThemeAlgorithm> algorithms) {
    const bool dark = contains_dark(algorithms);
    const auto& shadows = ant_design_default_shadows();
    ThemeAliasToken alias{
        .color_text = dark ? Color(1.0F, 1.0F, 1.0F, 0.85F)
                           : Color(0.0F, 0.0F, 0.0F, 0.88F),
        .color_text_secondary = dark ? Color(1.0F, 1.0F, 1.0F, 0.65F)
                                     : Color(0.0F, 0.0F, 0.0F, 0.65F),
        .color_text_disabled = dark ? Color(1.0F, 1.0F, 1.0F, 0.25F)
                                    : Color(0.0F, 0.0F, 0.0F, 0.25F),
        .color_background_container = dark ? Color::rgba8(20, 20, 20)
                                           : Color::rgba8(255, 255, 255),
        .color_background_elevated = dark ? Color::rgba8(31, 31, 31)
                                          : Color::rgba8(255, 255, 255),
        .color_background_container_disabled = dark
            ? Color(1.0F, 1.0F, 1.0F, 0.08F) : Color(0.0F, 0.0F, 0.0F, 0.04F),
        .color_border = dark ? Color::rgba8(66, 66, 66) : Color::rgba8(217, 217, 217),
        .color_border_secondary = dark ? Color::rgba8(48, 48, 48)
                                       : Color::rgba8(240, 240, 240),
        .color_split = get_alpha_color(
            dark ? Color::rgba8(48, 48, 48) : Color::rgba8(240, 240, 240),
            dark ? Color::rgba8(20, 20, 20) : Color::rgba8(255, 255, 255)),
        .color_focus_outline = map.color_primary_border,
        .line_width_focus = seed.focus_outline ? seed.line_width * 3.0F : 0.0F,
        .box_shadow = shadows.box_shadow,
        .box_shadow_secondary = shadows.box_shadow_secondary,
        .box_shadow_tertiary = shadows.box_shadow_tertiary,
    };
    return alias;
}

void apply_alias_override(ThemeAliasToken& alias, const AliasTokenOverride& override) {
    if (override.color_text) alias.color_text = *override.color_text;
    if (override.color_text_secondary) alias.color_text_secondary = *override.color_text_secondary;
    if (override.color_text_disabled) alias.color_text_disabled = *override.color_text_disabled;
    if (override.color_background_container) {
        alias.color_background_container = *override.color_background_container;
    }
    if (override.color_border) alias.color_border = *override.color_border;
    if (override.color_split) alias.color_split = *override.color_split;
    if (override.color_focus_outline) alias.color_focus_outline = *override.color_focus_outline;
    if (override.box_shadow) alias.box_shadow = *override.box_shadow;
    if (override.box_shadow_secondary) alias.box_shadow_secondary = *override.box_shadow_secondary;
    if (override.box_shadow_tertiary) alias.box_shadow_tertiary = *override.box_shadow_tertiary;
}

[[nodiscard]] detail::InputTokenSet derive_input_theme(
    const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
    const ThemeAliasToken& alias, std::span<const ThemeAlgorithm> algorithms) {
    auto input = detail::derive_input_tokens(seed, map);
    const bool dark = contains_dark(algorithms);
    const Color surface = Color::rgba8(20, 20, 20);
    struct Palette { Color active, hover, border_hover, background; };
    const auto palette = [&](Color base) {
        // @ant-design/colors 8.0.1: normal keys 6/5/4/1; dark keys remap to
        // 85% seed / 90% light step 1 / 45% seed / 15% dark step 2.
        return dark ? Palette{mix(surface, base, 0.85F), mix(surface, palette_variant(base, 1, true), 0.90F),
            mix(surface, base, 0.45F), mix(surface, palette_variant(base, 2, false), 0.15F)}
            : Palette{base, palette_variant(base, 1, true), palette_variant(base, 2, true), palette_variant(base, 5, true)};
    };
    const auto primary = palette(seed.color_primary);
    const auto error = palette(seed.color_error);
    const auto warning = palette(seed.color_warning);
    const auto outline_color = [&](Color foreground) {
        return get_alpha_color(foreground, alias.color_background_container);
    };
    const auto shadow = [&](Color background) {
        return ShadowList{{ShadowKind::outer, {}, 0, 2 * seed.line_width, outline_color(background)}};
    };
    input.colors = {
        alias.color_text,
        Color(map.color_text_base.red(), map.color_text_base.green(), map.color_text_base.blue(), 0.25F),
        alias.color_background_container, alias.color_border,
        alias.color_text_disabled, alias.color_background_container_disabled, alias.color_border,
        primary.hover, primary.active, alias.color_background_container, alias.color_background_container,
        error.active, error.border_hover, warning.active, warning.border_hover,
        // Desktop selection/caret are explicit RynUI tokens, not browser-native
        // selection CSS. Status caret follows the approved Input contract.
        Color(primary.active.red(), primary.active.green(), primary.active.blue(), 0.25F),
        alias.color_text, alias.color_text, error.active, warning.active,
    };
    input.active_shadow = shadow(primary.background);
    input.error_active_shadow = shadow(error.background);
    input.warning_active_shadow = shadow(warning.background);
    return input;
}

[[nodiscard]] TypographyThemeToken derive_typography(
    const AntDesignDefaultSeed& seed,
    const ThemeMapToken& map,
    const ThemeAliasToken& alias) {
    // Heading sizes are the locked reference chain 38/30/24/20/16 at the default
    // 14 base size. Keep them as ratios of the base size so a `base_font_size`
    // override or the Compact algorithm (base 12) stays proportional instead of
    // freezing the reference pixels. Note that level 5 is `fontSizeLG`, which is
    // one step ABOVE the base size.
    constexpr std::array<float, typography_level_count> reference_sizes{
        38.0F, 30.0F, 24.0F, 20.0F, 16.0F};
    constexpr float reference_base_size = 14.0F;
    std::array<float, typography_level_count> computed_sizes{};
    for (std::size_t index = 0; index < typography_level_count; ++index) {
        computed_sizes[index] =
            std::round(map.font_size * reference_sizes[index] / reference_base_size);
    }
    const std::array<float, typography_level_count> computed_line_heights{
        1.4F, 1.35F, 1.3F, 1.25F, 1.2F};
    TypographyThemeToken token;
    for (std::size_t index = 0; index < typography_level_count; ++index) {
        token.headings[index].font_size = computed_sizes[index];
        token.headings[index].line_height =
            computed_sizes[index] * computed_line_heights[index];
    }
    token.font_family = seed.font_family;
    token.font_family_code = seed.font_family_code;
    token.font_weight = 400;
    token.font_weight_strong = 600;
    token.base_font_size = map.font_size;
    token.base_line_height = map.font_size + 8.0F;
    // Upstream `titleMarginTop: '1.2em'` / `titleMarginBottom: '0.5em'` relative
    // to the heading's own font size. Keep the ratios so a per-level heading size
    // override scales the margins the way the em units do.
    token.title_margin_top_em = 1.2F;
    token.title_margin_bottom_em = 0.5F;
    token.colors = {
        .text = alias.color_text,
        .description = alias.color_text_secondary,
        .success = map.color_success_text,
        .warning = map.color_warning_text,
        .error = map.color_error_text,
        .error_text_hover = map.color_error_hover,
        .error_text_active = map.color_error_active,
        .disabled = alias.color_text_disabled,
        .link = map.color_link,
        // The upstream highlight is a fixed reference colour (`gold[2]`), not a
        // seed derivation, so it stays a constant rather than tracking the seed.
        .mark_background = Color::rgba8(255, 229, 143),
    };
    token.code = {
        .background = Color(0.588F, 0.588F, 0.588F, 0.1F),
        .border_color = Color(0.392F, 0.392F, 0.392F, 0.2F),
        .font_scale = 0.85F,
        .padding_inline_em = 0.4F,
        .padding_block_start_em = 0.2F,
        .padding_block_end_em = 0.1F,
        .border_width = seed.line_width,
        .border_radius = 3.0F,
        .border_bottom_width = seed.line_width,
    };
    token.keyboard = {
        .background = Color(0.588F, 0.588F, 0.588F, 0.06F),
        .border_color = Color(0.392F, 0.392F, 0.392F, 0.2F),
        .font_scale = 0.9F,
        .padding_inline_em = 0.4F,
        .padding_block_start_em = 0.15F,
        .padding_block_end_em = 0.1F,
        .border_width = seed.line_width,
        .border_radius = 3.0F,
        .border_bottom_width = seed.line_width * 2.0F,
    };
    return token;
}

void apply_typography_override(
    TypographyThemeToken& token, const TypographyTokenOverride& override_) {
    const auto weight = [](const std::optional<std::uint32_t>& value,
                            std::uint32_t fallback, std::string_view name) {
        if (!value) return fallback;
        if (*value < 100 || *value > 1000) throw std::invalid_argument(std::string(name));
        return *value;
    };
    const auto ratio = [](const std::optional<float>& value, float fallback,
                           std::string_view name, bool positive = false) {
        if (!value) return fallback;
        if (!detail::finite(*value) || (positive ? *value <= 0.0F : *value < 0.0F)) {
            throw std::invalid_argument(std::string(name));
        }
        return *value;
    };
    if (override_.text) token.colors.text = *override_.text;
    if (override_.description) token.colors.description = *override_.description;
    if (override_.success) token.colors.success = *override_.success;
    if (override_.warning) token.colors.warning = *override_.warning;
    if (override_.error) token.colors.error = *override_.error;
    if (override_.error_text_hover) token.colors.error_text_hover = *override_.error_text_hover;
    if (override_.error_text_active) token.colors.error_text_active = *override_.error_text_active;
    if (override_.disabled) token.colors.disabled = *override_.disabled;
    if (override_.link) token.colors.link = *override_.link;
    if (override_.mark_background) token.colors.mark_background = *override_.mark_background;
    if (override_.font_family) token.font_family = *override_.font_family;
    if (override_.font_family_code) token.font_family_code = *override_.font_family_code;
    token.font_weight = weight(override_.font_weight, token.font_weight,
        "Typography font weight must be in [100, 1000]");
    token.font_weight_strong = weight(override_.font_weight_strong, token.font_weight_strong,
        "Typography strong font weight must be in [100, 1000]");
    token.base_font_size = fixed_length(override_.base_font_size, token.base_font_size,
        "Typography base font size must be a positive fixed logical length", true);
    token.base_line_height = fixed_length(override_.base_line_height, token.base_line_height,
        "Typography base line height must be positive", true);
    token.title_margin_top_em = ratio(override_.title_margin_top_em, token.title_margin_top_em,
        "Typography title margin top must be non-negative");
    token.title_margin_bottom_em = ratio(override_.title_margin_bottom_em,
        token.title_margin_bottom_em, "Typography title margin bottom must be non-negative");
    for (std::size_t index = 0; index < typography_level_count; ++index) {
        auto& heading = token.headings[index];
        heading.font_size = fixed_length(override_.heading_font_sizes[index], heading.font_size,
            "Typography heading font size must be a positive fixed logical length", true);
        heading.line_height = ratio(override_.heading_line_heights[index], heading.line_height,
            "Typography heading line height must be positive", true);
    }
    const auto inline_code = [&](InlineCodeThemeToken& target,
                                  const std::optional<Color>& background,
                                  const std::optional<Color>& border_color,
                                  const std::optional<float>& font_scale,
                                  const std::optional<float>& padding_inline,
                                  const std::optional<float>& padding_block_start,
                                  const std::optional<float>& padding_block_end,
                                  const std::optional<LogicalLength>& border_width,
                                  const std::optional<LogicalLength>& border_radius,
                                  const std::optional<LogicalLength>& border_bottom_width,
                                  std::string_view prefix) {
        const auto message = [prefix](std::string_view detail) {
            return std::string(prefix) + " " + std::string(detail);
        };
        if (background) target.background = *background;
        if (border_color) target.border_color = *border_color;
        target.font_scale = ratio(font_scale, target.font_scale,
            message("font scale must be positive"), true);
        target.padding_inline_em = ratio(padding_inline, target.padding_inline_em,
            message("padding inline must be non-negative"));
        target.padding_block_start_em = ratio(padding_block_start,
            target.padding_block_start_em, message("padding block start must be non-negative"));
        target.padding_block_end_em = ratio(padding_block_end, target.padding_block_end_em,
            message("padding block end must be non-negative"));
        target.border_width = fixed_length(border_width, target.border_width,
            message("border width must be non-negative"));
        target.border_radius = fixed_length(border_radius, target.border_radius,
            message("border radius must be non-negative"));
        if (border_bottom_width) {
            if (border_bottom_width->is_auto()
                || !detail::finite(border_bottom_width->value())
                || border_bottom_width->value() < 0.0F) {
                throw std::invalid_argument(message("border bottom width must be non-negative"));
            }
            target.border_bottom_width = border_bottom_width->value();
        }
        if (target.border_bottom_width < target.border_width) {
            throw std::invalid_argument(
                message("border bottom width must not be smaller than its border width"));
        }
    };
    inline_code(token.code, override_.code.background, override_.code.border_color,
        override_.code.font_scale, override_.code.padding_inline_em,
        override_.code.padding_block_start_em, override_.code.padding_block_end_em,
        override_.code.border_width, override_.code.border_radius, std::nullopt,
        "Typography code");
    inline_code(token.keyboard, override_.keyboard.background, override_.keyboard.border_color,
        override_.keyboard.font_scale, override_.keyboard.padding_inline_em,
        override_.keyboard.padding_block_start_em, override_.keyboard.padding_block_end_em,
        override_.keyboard.border_width, override_.keyboard.border_radius,
        override_.keyboard.border_bottom_width, "Typography keyboard");
}

[[nodiscard]] DividerThemeToken derive_divider(
    const AntDesignDefaultSeed& seed,
    const ThemeMapToken& map,
    const ThemeAliasToken& alias) {
    DividerThemeToken token;
    token.colors = {
        .line = alias.color_split,
        .text = alias.color_text,
        .plain_text = alias.color_text,
    };
    token.metrics = {
        .line_width = seed.line_width,
        .orientation_margin = 0.05F,
        .text_padding_inline = map.font_size_large,
        .vertical_margin_inline = map.size_xs,
        .horizontal_margin = map.size_large,
        .horizontal_with_text_margin = map.size,
    };
    token.typography = {
        .text_font_size = map.font_size_large,
        .text_font_weight = 500,
        .plain_font_size = map.font_size,
        .plain_font_weight = 400,
    };
    return token;
}

void apply_divider_override(
    DividerThemeToken& token, const DividerTokenOverride& override_) {
    if (override_.line) token.colors.line = *override_.line;
    if (override_.text) token.colors.text = *override_.text;
    if (override_.plain_text) token.colors.plain_text = *override_.plain_text;
    token.metrics.line_width = fixed_length(override_.line_width, token.metrics.line_width,
        "Divider line width must be non-negative");
    if (override_.orientation_margin) {
        if (!detail::finite(*override_.orientation_margin)
            || *override_.orientation_margin < 0.0F
            || *override_.orientation_margin > 1.0F) {
            throw std::invalid_argument("Divider orientation margin must be in [0, 1]");
        }
        token.metrics.orientation_margin = *override_.orientation_margin;
    }
    token.metrics.text_padding_inline = fixed_length(override_.text_padding_inline,
        token.metrics.text_padding_inline, "Divider text padding must be non-negative");
    token.metrics.vertical_margin_inline = fixed_length(override_.vertical_margin_inline,
        token.metrics.vertical_margin_inline, "Divider vertical margin must be non-negative");
    token.metrics.horizontal_margin = fixed_length(override_.horizontal_margin,
        token.metrics.horizontal_margin, "Divider horizontal margin must be non-negative");
    token.metrics.horizontal_with_text_margin = fixed_length(
        override_.horizontal_with_text_margin, token.metrics.horizontal_with_text_margin,
        "Divider horizontal margin with text must be non-negative");
    token.typography.text_font_size = fixed_length(override_.text_font_size,
        token.typography.text_font_size, "Divider text font size must be positive", true);
    token.typography.plain_font_size = fixed_length(override_.plain_font_size,
        token.typography.plain_font_size, "Divider plain font size must be positive", true);
    const auto weight = [](const std::optional<std::uint32_t>& value,
                            std::uint32_t fallback, std::string_view name) {
        if (!value) return fallback;
        if (*value < 100 || *value > 1000) throw std::invalid_argument(std::string(name));
        return *value;
    };
    token.typography.text_font_weight = weight(override_.text_font_weight,
        token.typography.text_font_weight, "Divider text font weight must be in [100, 1000]");
    token.typography.plain_font_weight = weight(override_.plain_font_weight,
        token.typography.plain_font_weight, "Divider plain font weight must be in [100, 1000]");
}

[[nodiscard]] ButtonThemeToken derive_button(
    const ThemeMapToken& map,
    const ThemeAliasToken& alias) {
    const auto& shadows = ant_design_default_shadows();
    return {
        .default_color = alias.color_text,
        .default_background = alias.color_background_container,
        .default_border_color = alias.color_border,
        .default_hover_color = map.color_primary_hover,
        .default_active_color = map.color_primary_active,
        .text_color = alias.color_text,
        .text_background = Color::rgba8(0, 0, 0, 0),
        .text_hover_color = map.color_primary_hover,
        .text_active_color = map.color_primary_active,
        .text_hover_background = Color(map.color_primary.red(), map.color_primary.green(),
            map.color_primary.blue(), 0.08F),
        .text_active_background = Color(map.color_primary.red(), map.color_primary.green(),
            map.color_primary.blue(), 0.15F),
        .primary_color = Color::rgba8(255, 255, 255),
        .primary_background = map.color_primary,
        .primary_hover_background = map.color_primary_hover,
        .primary_active_background = map.color_primary_active,
        .danger_color = Color::rgba8(255, 255, 255),
        .danger_background = map.color_error,
        .danger_hover_background = map.color_error_hover,
        .danger_active_background = map.color_error_active,
        .disabled_color = alias.color_text_disabled,
        .disabled_background = alias.color_background_container_disabled,
        .disabled_border_color = alias.color_border,
        .control_height_small = map.control_height_small,
        .control_height = map.control_height,
        .control_height_large = map.control_height_large,
        .padding_inline_small = 7.0F,
        .padding_inline = map.size < 16.0F ? 11.0F : 15.0F,
        .padding_inline_large = 15.0F,
        .content_font_size_small = map.font_size,
        .content_font_size = map.font_size,
        .content_font_size_large = map.font_size_large,
        .content_line_height_small = map.font_size + 8.0F,
        .content_line_height = map.font_size + 8.0F,
        .content_line_height_large = map.font_size_large + 8.0F,
        .border_radius_small = map.border_radius_small,
        .border_radius = map.border_radius,
        .border_radius_large = map.border_radius_large,
        .icon_gap = map.size_xs,
        .loading_indicator_size = map.font_size,
        .loading_opacity = 0.65F,
        .default_shadow = shadows.button_default,
        .primary_shadow = shadows.button_primary,
        .danger_shadow = shadows.button_danger,
    };
}

[[nodiscard]] SwitchThemeToken derive_switch(const ThemeMapToken& map) {
    constexpr float padding = 2.0F;
    const float height = map.font_size * map.line_height;
    const float small_height = map.control_height / 2.0F;
    const float handle = height - 2.0F * padding;
    const float small_handle = small_height - 2.0F * padding;
    return {height, small_height, handle * 2.0F + padding * 4.0F,
        small_handle * 2.0F + padding * 2.0F, padding,
        Color::rgba8(255, 255, 255), handle, small_handle};
}

SliderThemeToken derive_slider(const AntDesignDefaultSeed& seed, const ThemeMapToken& map,
    const ThemeAliasToken& alias, std::span<const ThemeAlgorithm> algorithms) {
    const bool dark = std::find(algorithms.begin(), algorithms.end(), ThemeAlgorithm::Dark) != algorithms.end();
    const auto with_alpha = [](Color value, float alpha) { return Color(value.red(), value.green(), value.blue(), alpha); };
    const auto border_hover = is_default_primary(seed.color_primary)
        ? (dark ? Color::rgba8(21, 65, 126) : Color::rgba8(105, 177, 255))
        : (dark ? mix(map.color_primary_border, map.color_primary, 0.25F) : palette_variant(seed.color_primary, 2, true));
    const auto disabled = mix(alias.color_background_container, alias.color_text_disabled, alias.color_text_disabled.alpha());
    return {{4, map.control_height_large / 4, map.control_height_small / 2,
            seed.line_width + 1, seed.line_width + 1.5F},
        {with_alpha(map.color_text_base, dark ? 0.08F : 0.04F), with_alpha(map.color_text_base, dark ? 0.12F : 0.06F),
            map.color_primary_border, border_hover, alias.color_background_container_disabled,
            map.color_primary_border, map.color_primary, with_alpha(map.color_primary, 0.2F),
            Color(disabled.red(), disabled.green(), disabled.blue(), 1), alias.color_background_elevated}};
}
void apply_slider_override(SliderThemeToken& token, const SliderTokenOverride& o) {
    auto& m = token.metrics; auto& c = token.colors;
    m.rail_size = fixed_length(o.rail_size, m.rail_size, "Slider rail size must be positive", true);
    m.handle_size = fixed_length(o.handle_size, m.handle_size, "Slider handle size must be positive", true);
    m.handle_size_hover = fixed_length(o.handle_size_hover, m.handle_size_hover, "Slider hover handle size must be positive", true);
    m.handle_line_width = fixed_length(o.handle_line_width, m.handle_line_width, "Slider handle line width must be non-negative");
    m.handle_line_width_hover = fixed_length(o.handle_line_width_hover, m.handle_line_width_hover, "Slider hover line width must be non-negative");
    if (o.rail) c.rail = *o.rail;
    if (o.rail_hover) c.rail_hover = *o.rail_hover;
    if (o.track) c.track = *o.track;
    if (o.track_hover) c.track_hover = *o.track_hover;
    if (o.track_disabled) c.track_disabled = *o.track_disabled;
    if (o.handle) c.handle = *o.handle;
    if (o.handle_active) c.handle_active = *o.handle_active;
    if (o.handle_outline) c.handle_outline = *o.handle_outline;
    if (o.handle_disabled) c.handle_disabled = *o.handle_disabled;
    if (o.handle_background) c.handle_background = *o.handle_background;
    if (!std::isfinite(m.handle_size + 2 * m.handle_line_width)
        || !std::isfinite(m.handle_size_hover + 2 * m.handle_line_width_hover))
        throw std::invalid_argument("Slider handle extent must be finite");
}

void apply_switch_override(SwitchThemeToken& token,
    const SwitchTokenOverride& override) {
    token.track_height = fixed_length(override.track_height, token.track_height,
        "Switch trackHeight must be positive", true);
    token.track_height_small = fixed_length(override.track_height_small,
        token.track_height_small, "Switch trackHeightSM must be positive", true);
    token.track_min_width = fixed_length(override.track_min_width,
        token.track_min_width, "Switch trackMinWidth must be positive", true);
    token.track_min_width_small = fixed_length(override.track_min_width_small,
        token.track_min_width_small, "Switch trackMinWidthSM must be positive", true);
    token.track_padding = fixed_length(override.track_padding, token.track_padding,
        "Switch trackPadding must be non-negative");
    token.handle_size = fixed_length(override.handle_size, token.handle_size,
        "Switch handleSize must be positive", true);
    token.handle_size_small = fixed_length(override.handle_size_small,
        token.handle_size_small, "Switch handleSizeSM must be positive", true);
    if (override.handle_background) token.handle_background = *override.handle_background;
    if (token.track_height < token.handle_size + token.track_padding * 2.0F
        || token.track_height_small < token.handle_size_small + token.track_padding * 2.0F
        || token.track_min_width < token.handle_size + token.track_padding * 2.0F
        || token.track_min_width_small < token.handle_size_small + token.track_padding * 2.0F) {
        throw std::invalid_argument("Switch Component Token geometry does not fit its track");
    }
}

void apply_button_override(ButtonThemeToken& button, const ButtonTokenOverride& override) {
    if (override.default_color) button.default_color = *override.default_color;
    if (override.default_background) button.default_background = *override.default_background;
    if (override.default_border_color) button.default_border_color = *override.default_border_color;
    if (override.text_color) button.text_color = *override.text_color;
    if (override.text_background) button.text_background = *override.text_background;
    if (override.primary_color) button.primary_color = *override.primary_color;
    if (override.primary_background) button.primary_background = *override.primary_background;
    if (override.danger_background) button.danger_background = *override.danger_background;
    button.padding_inline = fixed_length(
        override.padding_inline, button.padding_inline,
        "Button padding must be a non-negative fixed logical length");
    button.icon_gap = fixed_length(
        override.icon_gap, button.icon_gap,
        "Button icon gap must be a non-negative fixed logical length");
    button.border_radius = fixed_length(
        override.border_radius, button.border_radius,
        "Button radius must be a non-negative fixed logical length");
    if (override.default_shadow) button.default_shadow = *override.default_shadow;
    if (override.primary_shadow) button.primary_shadow = *override.primary_shadow;
    if (override.danger_shadow) button.danger_shadow = *override.danger_shadow;
}

[[nodiscard]] TextThemeToken derive_text(
    const AntDesignDefaultSeed& seed,
    const ThemeMapToken& map,
    const ThemeAliasToken& alias) {
    return {
        .color = alias.color_text,
        .font_family = seed.font_family,
        .font_weight = 400,
        .font_size = map.font_size,
        .line_height = map.font_size + 8.0F,
    };
}

void apply_text_override(TextThemeToken& text, const TextTokenOverride& override) {
    if (override.color) text.color = *override.color;
    if (override.font_family) text.font_family = *override.font_family;
    if (override.font_weight) {
        if (*override.font_weight < 100 || *override.font_weight > 1000) {
            throw std::invalid_argument("Text font weight must be in [100, 1000]");
        }
        text.font_weight = *override.font_weight;
    }
    text.font_size = fixed_length(
        override.font_size, text.font_size,
        "Text font size must be a positive fixed logical length", true);
    text.line_height = fixed_length(
        override.line_height, text.line_height,
        "Text line height must be a positive fixed logical length", true);
}

[[nodiscard]] const char* algorithm_name(ThemeAlgorithm algorithm) noexcept {
    switch (algorithm) {
    case ThemeAlgorithm::Default: return "Default";
    case ThemeAlgorithm::Dark: return "Dark";
    case ThemeAlgorithm::Compact: return "Compact";
    }
    return "Unknown";
}

void append_color(std::ostringstream& stream, Color color) {
    stream << '[' << color.red() << ',' << color.green() << ',' << color.blue() << ','
           << color.alpha() << ']';
}

[[nodiscard]] std::string serialize_snapshot(
    const AntDesignDefaultSeed& seed,
    const ThemeMapToken& map,
    const ThemeAliasToken& alias,
    const ButtonThemeToken& button,
    const TextThemeToken& text,
    const SwitchThemeToken& switch_token,
    const TypographyThemeToken& typography,
    const DividerThemeToken& divider,
    const SliderThemeToken& slider,
    const detail::InputTokenSet& input,
    std::span<const ThemeAlgorithm> algorithms,
    std::uint64_t identity) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(6);
    stream << "{\"source\":{\"version\":\"" << ant_design_version
           << "\",\"commit\":\"" << ant_design_commit
           << "\"},\"impactMetadata\":\"catalog-invalidation-domain-v1\",\"algorithms\":[";
    for (std::size_t index = 0; index < algorithms.size(); ++index) {
        if (index != 0) stream << ',';
        stream << '\"' << algorithm_name(algorithms[index]) << '\"';
    }
    stream << "],\"identity\":\"" << std::hex << std::setw(16) << std::setfill('0')
           << identity << std::dec << std::setfill(' ') << "\",\"seed\":{\"colorPrimary\":";
    append_color(stream, seed.color_primary);
    stream << ",\"focusOutline\":" << (seed.focus_outline ? "true" : "false")
           << ",\"fontSize\":" << seed.font_size << ",\"sizeUnit\":" << seed.size_unit
           << ",\"sizeStep\":" << seed.size_step << ",\"controlHeight\":"
           << seed.control_height << "},\"map\":{\"colorPrimary\":";
    append_color(stream, map.color_primary);
    stream << ",\"colorPrimaryHover\":";
    append_color(stream, map.color_primary_hover);
    stream << ",\"colorPrimaryActive\":";
    append_color(stream, map.color_primary_active);
    stream << ",\"colorError\":";
    append_color(stream, map.color_error);
    stream << ",\"colorErrorHover\":";
    append_color(stream, map.color_error_hover);
    stream << ",\"colorErrorActive\":";
    append_color(stream, map.color_error_active);
    stream << ",\"colorSuccessText\":";
    append_color(stream, map.color_success_text);
    stream << ",\"colorWarningText\":";
    append_color(stream, map.color_warning_text);
    stream << ",\"colorErrorText\":";
    append_color(stream, map.color_error_text);
    stream << ",\"colorLink\":";
    append_color(stream, map.color_link);
    stream << ",\"colorLinkHover\":";
    append_color(stream, map.color_link_hover);
    stream << ",\"colorLinkActive\":";
    append_color(stream, map.color_link_active);
    stream << ",\"fontSizeSM\":" << map.font_size_small << ",\"fontSize\":"
           << map.font_size << ",\"fontSizeLG\":" << map.font_size_large
           << ",\"sizeXS\":" << map.size_xs << ",\"sizeSM\":" << map.size_small
           << ",\"size\":" << map.size << ",\"sizeLG\":" << map.size_large
           << ",\"controlHeightSM\":" << map.control_height_small
           << ",\"controlHeight\":" << map.control_height
           << ",\"controlHeightLG\":" << map.control_height_large
           << ",\"motionUnitMs\":" << map.motion_unit.count_milliseconds()
           << ",\"motionBaseMs\":" << map.motion_base.count_milliseconds()
           << ",\"motion\":" << (map.motion ? "true" : "false")
           << "},\"alias\":{\"colorText\":";
    append_color(stream, alias.color_text);
    stream << ",\"colorBgContainer\":";
    append_color(stream, alias.color_background_container);
    stream << ",\"colorBorder\":";
    append_color(stream, alias.color_border);
    stream << ",\"colorSplit\":";
    append_color(stream, alias.color_split);
    stream << ",\"lineWidthFocus\":" << alias.line_width_focus
           << ",\"focusOutlineOffset\":" << alias.focus_outline_offset
           << ",\"boxShadowLayers\":" << alias.box_shadow.size()
           << "},\"button\":{\"primaryBg\":";
    append_color(stream, button.primary_background);
    stream << ",\"controlHeight\":" << button.control_height
           << ",\"paddingInline\":" << button.padding_inline
           << ",\"borderRadius\":" << button.border_radius
           << ",\"shadowLayers\":" << button.primary_shadow.size()
           << "},\"switch\":{\"trackHeight\":" << switch_token.track_height
           << ",\"trackHeightSM\":" << switch_token.track_height_small
           << ",\"trackMinWidth\":" << switch_token.track_min_width
           << ",\"trackMinWidthSM\":" << switch_token.track_min_width_small
           << ",\"trackPadding\":" << switch_token.track_padding
           << ",\"handleBg\":";
    append_color(stream, switch_token.handle_background);
    stream << ",\"handleSize\":" << switch_token.handle_size
           << ",\"handleSizeSM\":" << switch_token.handle_size_small
           << "},\"slider\":{\"metrics\":[";
    const auto slider_metrics = slider.metrics.values();
    for (std::size_t i = 0; i < slider_metrics.size(); ++i) { if (i) stream << ','; stream << slider_metrics[i]; }
    stream << "],\"colors\":[";
    const auto slider_colors = slider.colors.values();
    for (std::size_t i = 0; i < slider_colors.size(); ++i) { if (i) stream << ','; append_color(stream, slider_colors[i]); }
    stream << "]},\"text\":{\"color\":";
    append_color(stream, text.color);
    stream << ",\"fontFamily\":" << static_cast<int>(text.font_family)
           << ",\"fontWeight\":" << text.font_weight
           << ",\"fontSize\":" << text.font_size
           << ",\"lineHeight\":" << text.line_height << "},\"input\":{\"sizes\":[";
    for(std::size_t i = 0; i < input.sizes.size(); ++i) {
        if(i) stream << ',';
        const auto& size = input.sizes[i];
        stream << "{\"controlHeight\":" << size.control_height << ",\"fontSize\":" << size.font_size
            << ",\"lineHeight\":" << size.line_height << ",\"paddingInline\":" << size.padding_inline
            << ",\"paddingBlock\":" << size.padding_block << ",\"borderRadius\":" << size.border_radius << '}';
    }
    stream << "],\"borderWidth\":" << input.border_width << ",\"affixPadding\":" << input.affix_padding << ",\"colors\":[";
    const auto input_colors = input.colors.values();
    for(std::size_t i = 0; i < input_colors.size(); ++i) {
        if(i) stream << ',';
        append_color(stream, input_colors[i]);
    }
    stream << "],\"activeShadowLayers\":" << input.active_shadow.size()
        << ",\"errorActiveShadowLayers\":" << input.error_active_shadow.size()
        << ",\"warningActiveShadowLayers\":" << input.warning_active_shadow.size()
        << "},\"typography\":{\"headings\":[";
    for (std::size_t index = 0; index < typography_level_count; ++index) {
        if (index) stream << ',';
        stream << "{\"fontSize\":" << typography.headings[index].font_size
               << ",\"lineHeight\":" << typography.headings[index].line_height << '}';
    }
    stream << "],\"fontFamily\":" << static_cast<int>(typography.font_family)
        << ",\"fontFamilyCode\":" << static_cast<int>(typography.font_family_code)
        << ",\"fontWeight\":" << typography.font_weight
        << ",\"fontWeightStrong\":" << typography.font_weight_strong
        << ",\"baseFontSize\":" << typography.base_font_size
        << ",\"baseLineHeight\":" << typography.base_line_height
        << ",\"titleMarginTopEm\":" << typography.title_margin_top_em
        << ",\"titleMarginBottomEm\":" << typography.title_margin_bottom_em
        << ",\"colors\":{\"text\":";
    append_color(stream, typography.colors.text);
    stream << ",\"description\":";
    append_color(stream, typography.colors.description);
    stream << ",\"success\":";
    append_color(stream, typography.colors.success);
    stream << ",\"warning\":";
    append_color(stream, typography.colors.warning);
    stream << ",\"error\":";
    append_color(stream, typography.colors.error);
    stream << ",\"errorTextHover\":";
    append_color(stream, typography.colors.error_text_hover);
    stream << ",\"errorTextActive\":";
    append_color(stream, typography.colors.error_text_active);
    stream << ",\"disabled\":";
    append_color(stream, typography.colors.disabled);
    stream << ",\"link\":";
    append_color(stream, typography.colors.link);
    stream << ",\"markBackground\":";
    append_color(stream, typography.colors.mark_background);
    stream << "},\"code\":{\"background\":";
    append_color(stream, typography.code.background);
    stream << ",\"borderColor\":";
    append_color(stream, typography.code.border_color);
    stream << ",\"fontScale\":" << typography.code.font_scale
        << ",\"paddingInlineEm\":" << typography.code.padding_inline_em
        << ",\"paddingBlockStartEm\":" << typography.code.padding_block_start_em
        << ",\"paddingBlockEndEm\":" << typography.code.padding_block_end_em
        << ",\"borderWidth\":" << typography.code.border_width
        << ",\"borderRadius\":" << typography.code.border_radius
        << "},\"keyboard\":{\"background\":";
    append_color(stream, typography.keyboard.background);
    stream << ",\"borderColor\":";
    append_color(stream, typography.keyboard.border_color);
    stream << ",\"fontScale\":" << typography.keyboard.font_scale
        << ",\"paddingInlineEm\":" << typography.keyboard.padding_inline_em
        << ",\"paddingBlockStartEm\":" << typography.keyboard.padding_block_start_em
        << ",\"paddingBlockEndEm\":" << typography.keyboard.padding_block_end_em
        << ",\"borderWidth\":" << typography.keyboard.border_width
        << ",\"borderRadius\":" << typography.keyboard.border_radius
        << ",\"borderBottomWidth\":" << typography.keyboard.border_bottom_width
        << "}},\"divider\":{\"colors\":{\"line\":";
    append_color(stream, divider.colors.line);
    stream << ",\"text\":";
    append_color(stream, divider.colors.text);
    stream << ",\"plainText\":";
    append_color(stream, divider.colors.plain_text);
    stream << "},\"metrics\":{\"lineWidth\":" << divider.metrics.line_width
        << ",\"orientationMargin\":" << divider.metrics.orientation_margin
        << ",\"textPaddingInline\":" << divider.metrics.text_padding_inline
        << ",\"verticalMarginInline\":" << divider.metrics.vertical_margin_inline
        << ",\"horizontalMargin\":" << divider.metrics.horizontal_margin
        << ",\"horizontalWithTextMargin\":" << divider.metrics.horizontal_with_text_margin
        << "},\"typography\":{\"textFontSize\":" << divider.typography.text_font_size
        << ",\"textFontWeight\":" << divider.typography.text_font_weight
        << ",\"plainFontSize\":" << divider.typography.plain_font_size
        << ",\"plainFontWeight\":" << divider.typography.plain_font_weight << "}}}\n";
    return stream.str();
}

void hash_byte(std::uint64_t& hash, std::uint8_t byte) noexcept {
    hash ^= byte;
    hash *= 1099511628211ULL;
}

template <typename Integer>
void hash_integer(std::uint64_t& hash, Integer value) noexcept {
    if constexpr (std::is_same_v<std::remove_cv_t<Integer>, bool>) {
        hash_byte(hash, value ? 1U : 0U);
    } else if constexpr (std::is_enum_v<Integer>) {
        using Unsigned = std::make_unsigned_t<std::underlying_type_t<Integer>>;
        Unsigned bits = static_cast<Unsigned>(value);
        for (std::size_t index = 0; index < sizeof(Unsigned); ++index) {
            hash_byte(hash, static_cast<std::uint8_t>(bits & 0xffU));
            bits >>= 8U;
        }
    } else {
        using Unsigned = std::make_unsigned_t<Integer>;
        Unsigned bits = static_cast<Unsigned>(value);
        for (std::size_t index = 0; index < sizeof(Unsigned); ++index) {
            hash_byte(hash, static_cast<std::uint8_t>(bits & 0xffU));
            bits >>= 8U;
        }
    }
}

void hash_float(std::uint64_t& hash, float value) noexcept {
    static_assert(std::numeric_limits<float>::is_iec559);
    hash_integer(hash, std::bit_cast<std::uint32_t>(value));
}

void hash_color(std::uint64_t& hash, Color color) noexcept {
    hash_float(hash, color.red());
    hash_float(hash, color.green());
    hash_float(hash, color.blue());
    hash_float(hash, color.alpha());
}

void hash_optional_color(
    std::uint64_t& hash, const std::optional<Color>& color) noexcept {
    hash_integer(hash, color.has_value());
    if (color) {
        hash_color(hash, *color);
    }
}

void hash_inline_code(std::uint64_t& hash, const InlineCodeThemeToken& token) noexcept {
    hash_color(hash, token.background);
    hash_color(hash, token.border_color);
    for (const float value : {token.font_scale, token.padding_inline_em,
            token.padding_block_start_em, token.padding_block_end_em,
            token.border_width, token.border_radius, token.border_bottom_width}) {
        hash_float(hash, value);
    }
}

void hash_typography(std::uint64_t& hash, const TypographyThemeToken& token) noexcept {
    for (const auto& heading : token.headings) {
        hash_float(hash, heading.font_size);
        hash_float(hash, heading.line_height);
    }
    hash_integer(hash, token.font_family);
    hash_integer(hash, token.font_family_code);
    hash_integer(hash, token.font_weight);
    hash_integer(hash, token.font_weight_strong);
    hash_float(hash, token.base_font_size);
    hash_float(hash, token.base_line_height);
    hash_float(hash, token.title_margin_top_em);
    hash_float(hash, token.title_margin_bottom_em);
    const auto& colors = token.colors;
    for (const Color color : {colors.text, colors.description, colors.success,
            colors.warning, colors.error, colors.error_text_hover,
            colors.error_text_active, colors.disabled, colors.link,
            colors.mark_background}) {
        hash_color(hash, color);
    }
    hash_inline_code(hash, token.code);
    hash_inline_code(hash, token.keyboard);
}

void hash_divider(std::uint64_t& hash, const DividerThemeToken& token) noexcept {
    hash_color(hash, token.colors.line);
    hash_color(hash, token.colors.text);
    hash_color(hash, token.colors.plain_text);
    for (const float value : {token.metrics.line_width, token.metrics.orientation_margin,
            token.metrics.text_padding_inline, token.metrics.vertical_margin_inline,
            token.metrics.horizontal_margin, token.metrics.horizontal_with_text_margin,
            token.typography.text_font_size, token.typography.plain_font_size}) {
        hash_float(hash, value);
    }
    hash_integer(hash, token.typography.text_font_weight);
    hash_integer(hash, token.typography.plain_font_weight);
}

void hash_shadow(std::uint64_t& hash, const ShadowList& shadows) noexcept {
    hash_integer(hash, shadows.size());
    for (const ShadowLayer& layer : shadows.layers()) {
        hash_integer(hash, layer.kind);
        hash_float(hash, layer.offset.x);
        hash_float(hash, layer.offset.y);
        hash_float(hash, layer.blur);
        hash_float(hash, layer.spread);
        hash_color(hash, layer.color);
    }
}

[[nodiscard]] std::uint64_t snapshot_identity(
    const AntDesignDefaultSeed& seed,
    const ThemeMapToken& map,
    const ThemeAliasToken& alias,
    const ButtonThemeToken& button,
    const TextThemeToken& text,
    const SwitchThemeToken& switch_token,
    const TypographyThemeToken& typography,
    const DividerThemeToken& divider,
    const SliderThemeToken& slider,
    const detail::InputTokenSet& input,
    std::span<const ThemeAlgorithm> algorithms) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : ant_design_commit) {
        hash_byte(hash, static_cast<std::uint8_t>(character));
    }
    hash_color(hash, seed.color_primary);
    hash_color(hash, seed.color_success);
    hash_color(hash, seed.color_warning);
    hash_color(hash, seed.color_error);
    hash_color(hash, seed.color_info);
    hash_optional_color(hash, seed.color_link);
    hash_integer(hash, seed.font_family);
    hash_integer(hash, seed.font_family_code);
    hash_integer(hash, seed.font_size);
    hash_float(hash, seed.line_width);
    hash_float(hash, seed.border_radius);
    hash_float(hash, seed.size_unit);
    hash_float(hash, seed.size_step);
    hash_float(hash, seed.size_popup_arrow);
    hash_float(hash, seed.control_height);
    hash_integer(hash, seed.z_index_base);
    hash_integer(hash, seed.z_index_popup_base);
    hash_float(hash, seed.opacity_image);
    hash_float(hash, seed.motion_unit.count_milliseconds());
    hash_float(hash, seed.motion_base.count_milliseconds());
    hash_integer(hash, seed.wireframe);
    hash_integer(hash, seed.focus_outline);
    hash_integer(hash, seed.motion);

    const std::array map_colors{
        map.color_primary, map.color_primary_hover, map.color_primary_active,
        map.color_primary_border, map.color_success, map.color_warning, map.color_error,
        map.color_error_hover, map.color_error_active, map.color_info,
        map.color_success_text, map.color_warning_text, map.color_error_text,
        map.color_link, map.color_link_hover, map.color_link_active,
        map.color_text_base, map.color_background_base,
    };
    for (const Color color : map_colors) hash_color(hash, color);
    const std::array map_values{
        map.font_size_small, map.font_size, map.font_size_large,
        map.line_height_small, map.line_height, map.line_height_large,
        map.size_xs, map.size_small, map.size, map.size_large,
        map.control_height_small, map.control_height, map.control_height_large,
        map.border_radius_small, map.border_radius, map.border_radius_large,
    };
    for (const float value : map_values) hash_float(hash, value);
    hash_float(hash, map.motion_unit.count_milliseconds());
    hash_float(hash, map.motion_base.count_milliseconds());
    hash_integer(hash, map.motion);

    const std::array alias_colors{
        alias.color_text, alias.color_text_secondary, alias.color_text_disabled,
        alias.color_background_container, alias.color_background_elevated,
        alias.color_background_container_disabled, alias.color_border,
        alias.color_border_secondary, alias.color_split, alias.color_focus_outline,
    };
    for (const Color color : alias_colors) hash_color(hash, color);
    hash_float(hash, alias.line_width_focus);
    hash_float(hash, alias.focus_outline_offset);
    hash_shadow(hash, alias.box_shadow);
    hash_shadow(hash, alias.box_shadow_secondary);
    hash_shadow(hash, alias.box_shadow_tertiary);

    const std::array button_colors{
        button.default_color, button.default_background, button.default_border_color,
        button.default_hover_color, button.default_active_color,
        button.text_color, button.text_background, button.text_hover_color, button.text_active_color,
        button.text_hover_background, button.text_active_background, button.primary_color,
        button.primary_background, button.primary_hover_background,
        button.primary_active_background, button.danger_color, button.danger_background,
        button.danger_hover_background, button.danger_active_background,
        button.disabled_color, button.disabled_background, button.disabled_border_color,
    };
    for (const Color color : button_colors) hash_color(hash, color);
    const std::array button_values{
        button.control_height_small, button.control_height, button.control_height_large,
        button.padding_inline_small, button.padding_inline, button.padding_inline_large,
        button.content_font_size_small, button.content_font_size,
        button.content_font_size_large, button.content_line_height_small,
        button.content_line_height, button.content_line_height_large,
        button.border_radius_small, button.border_radius, button.border_radius_large,
        button.border_width, button.icon_gap, button.loading_indicator_size,
        button.loading_opacity,
    };
    for (const float value : button_values) hash_float(hash, value);
    hash_shadow(hash, button.default_shadow);
    hash_shadow(hash, button.primary_shadow);
    hash_shadow(hash, button.danger_shadow);
    hash_color(hash, text.color);
    hash_integer(hash, text.font_family);
    hash_integer(hash, text.font_weight);
    hash_float(hash, text.font_size);
    hash_float(hash, text.line_height);
    hash_color(hash, switch_token.handle_background);
    for (const float value : {switch_token.track_height,
            switch_token.track_height_small, switch_token.track_min_width,
            switch_token.track_min_width_small, switch_token.track_padding,
            switch_token.handle_size, switch_token.handle_size_small}) {
        hash_float(hash, value);
    }
    hash_typography(hash, typography);
    hash_divider(hash, divider);
    for (auto value : slider.metrics.values()) hash_float(hash, value);
    for (auto value : slider.colors.values()) hash_color(hash, value);
    for(const auto& size : input.sizes) {
        hash_float(hash, size.control_height); hash_float(hash, size.font_size);
        hash_float(hash, size.line_height); hash_float(hash, size.padding_inline);
        hash_float(hash, size.padding_block); hash_float(hash, size.border_radius);
    }
    hash_float(hash, input.border_width); hash_float(hash, input.affix_padding);
    for(const bool explicit_padding : input.padding_block_explicit) hash_integer(hash, explicit_padding);
    hash_integer(hash, input.small_font_explicit);
    for(const auto color : input.colors.values()) hash_color(hash, color);
    hash_shadow(hash, input.active_shadow); hash_shadow(hash, input.error_active_shadow);
    hash_shadow(hash, input.warning_active_shadow);
    for (const ThemeAlgorithm algorithm : algorithms) hash_integer(hash, algorithm);
    return hash;
}

} // namespace

ThemeSnapshot::ThemeSnapshot(
    AntDesignDefaultSeed seed,
    ThemeMapToken map,
    ThemeAliasToken alias,
    ButtonThemeToken button,
    TextThemeToken text,
    SwitchThemeToken switch_token,
    TypographyThemeToken typography,
    DividerThemeToken divider,
    SliderThemeToken slider,
    std::shared_ptr<const detail::InputTokenSet> input,
    std::vector<ThemeAlgorithm> algorithms)
    : seed_(std::move(seed)),
      map_(std::move(map)),
      alias_(std::move(alias)),
      button_(std::move(button)),
      text_(std::move(text)),
      switch_token_(std::move(switch_token)),
      typography_(std::move(typography)),
      divider_(std::move(divider)),
      slider_(std::move(slider)),
      input_(std::move(input)),
      algorithms_(std::move(algorithms)) {
    identity_ = snapshot_identity(seed_, map_, alias_, button_, text_,
        switch_token_, typography_, divider_, slider_, *input_, algorithms_);
    diagnostic_json_ = serialize_snapshot(seed_, map_, alias_, button_, text_,
        switch_token_, typography_, divider_, slider_, *input_, algorithms_, identity_);
}

const AntDesignDefaultSeed& ThemeSnapshot::seed() const noexcept { return seed_; }
const ThemeMapToken& ThemeSnapshot::map() const noexcept { return map_; }
const ThemeAliasToken& ThemeSnapshot::alias() const noexcept { return alias_; }
const ButtonThemeToken& ThemeSnapshot::button() const noexcept { return button_; }
const TextThemeToken& ThemeSnapshot::text() const noexcept { return text_; }
const SwitchThemeToken& ThemeSnapshot::switch_token() const noexcept { return switch_token_; }
const TypographyThemeToken& ThemeSnapshot::typography() const noexcept { return typography_; }
const DividerThemeToken& ThemeSnapshot::divider() const noexcept { return divider_; }
const SliderThemeToken& ThemeSnapshot::slider() const noexcept { return slider_; }
std::span<const ThemeAlgorithm> ThemeSnapshot::algorithms() const noexcept {
    return algorithms_;
}
std::string_view ThemeSnapshot::source_version() const noexcept { return ant_design_version; }
std::string_view ThemeSnapshot::source_commit() const noexcept { return ant_design_commit; }
std::uint64_t ThemeSnapshot::identity() const noexcept { return identity_; }
const std::string& ThemeSnapshot::diagnostic_json() const noexcept { return diagnostic_json_; }

bool operator==(const ThemeSnapshot& left, const ThemeSnapshot& right) {
    return left.seed_ == right.seed_ && left.map_ == right.map_
        && left.alias_ == right.alias_ && left.button_ == right.button_
        && left.text_ == right.text_ && left.switch_token_ == right.switch_token_
        && left.typography_ == right.typography_ && left.divider_ == right.divider_ && left.slider_ == right.slider_
        && *left.input_ == *right.input_
        && left.algorithms_ == right.algorithms_;
}

ThemeSnapshot resolve_theme(const ThemeConfig& config, const ThemeSnapshot* parent) {
    AntDesignDefaultSeed seed = parent != nullptr && config.inherit
        ? parent->seed() : ant_design_default_seed();
    apply_seed_override(seed, config.seed);

    std::vector<ThemeAlgorithm> algorithms = config.algorithms;
    if (algorithms.empty()) {
        if (parent != nullptr && config.inherit) {
            algorithms.assign(parent->algorithms().begin(), parent->algorithms().end());
        } else {
            algorithms.push_back(ThemeAlgorithm::Default);
        }
    }
    if (algorithms.size() > 8) {
        throw std::invalid_argument("theme algorithm chain exceeds eight entries");
    }

    ThemeMapToken map = derive_map(seed, algorithms);
    ThemeAliasToken alias = derive_alias(seed, map, algorithms);
    if (parent != nullptr && config.inherit && config.seed == SeedTokenOverride{}
        && config.algorithms.empty()) {
        alias = parent->alias();
    }
    apply_alias_override(alias, config.alias);

    ButtonThemeToken button;
    const bool inherit_parent_button = parent != nullptr && config.inherit
        && config.seed == SeedTokenOverride{} && config.alias == AliasTokenOverride{}
        && config.algorithms.empty() && !config.button.algorithm
        && config.button.seed == SeedTokenOverride{};
    if (inherit_parent_button) {
        button = parent->button();
    } else if (config.button.algorithm) {
        AntDesignDefaultSeed component_seed = seed;
        apply_seed_override(component_seed, config.button.seed);
        const ThemeMapToken component_map = derive_map(component_seed, algorithms);
        const ThemeAliasToken component_alias = derive_alias(component_seed, component_map, algorithms);
        button = derive_button(component_map, component_alias);
    } else {
        button = derive_button(map, alias);
    }
    apply_button_override(button, config.button.tokens);

    TextThemeToken text;
    const bool inherit_parent_text = parent != nullptr && config.inherit
        && config.seed == SeedTokenOverride{} && config.alias == AliasTokenOverride{}
        && config.algorithms.empty() && !config.text.algorithm
        && config.text.seed == SeedTokenOverride{};
    if (inherit_parent_text) {
        text = parent->text();
    } else if (config.text.algorithm) {
        AntDesignDefaultSeed component_seed = seed;
        apply_seed_override(component_seed, config.text.seed);
        const ThemeMapToken component_map = derive_map(component_seed, algorithms);
        const ThemeAliasToken component_alias = derive_alias(component_seed, component_map, algorithms);
        text = derive_text(component_seed, component_map, component_alias);
    } else {
        text = derive_text(seed, map, alias);
    }
    apply_text_override(text, config.text.tokens);
    detail::InputTokenSet input;
    const bool inherit_parent_input = parent != nullptr && config.inherit
        && config.seed == SeedTokenOverride{} && config.alias == AliasTokenOverride{}
        && config.algorithms.empty() && !config.input.algorithm
        && config.input.seed == SeedTokenOverride{};
    if(inherit_parent_input) {
        input = detail::InputTokenAccess::get(*parent);
    } else if(config.input.algorithm) {
        auto component_seed = seed;
        apply_seed_override(component_seed, config.input.seed);
        const auto component_map = derive_map(component_seed, algorithms);
        input = derive_input_theme(component_seed, component_map, derive_alias(component_seed, component_map, algorithms), algorithms);
    } else {
        input = derive_input_theme(seed, map, alias, algorithms);
    }
    detail::apply_input_override(input, config.input.tokens);
    SwitchThemeToken switch_token;
    const bool inherit_parent_switch = parent != nullptr && config.inherit
        && config.seed == SeedTokenOverride{} && config.alias == AliasTokenOverride{}
        && config.algorithms.empty() && !config.switch_.algorithm
        && config.switch_.seed == SeedTokenOverride{};
    if (inherit_parent_switch) {
        switch_token = parent->switch_token();
    } else if (config.switch_.algorithm) {
        auto component_seed = seed;
        apply_seed_override(component_seed, config.switch_.seed);
        switch_token = derive_switch(derive_map(component_seed, algorithms));
    } else {
        switch_token = derive_switch(map);
    }
    apply_switch_override(switch_token, config.switch_.tokens);
    const bool inherit_parent_typography = parent != nullptr && config.inherit
        && config.seed == SeedTokenOverride{} && config.alias == AliasTokenOverride{}
        && config.algorithms.empty() && !config.typography.algorithm
        && config.typography.seed == SeedTokenOverride{};
    TypographyThemeToken typography;
    if (inherit_parent_typography) {
        typography = parent->typography();
    } else if (config.typography.algorithm) {
        auto component_seed = seed;
        apply_seed_override(component_seed, config.typography.seed);
        const auto component_map = derive_map(component_seed, algorithms);
        typography = derive_typography(component_seed, component_map,
            derive_alias(component_seed, component_map, algorithms));
    } else {
        typography = derive_typography(seed, map, alias);
    }
    apply_typography_override(typography, config.typography.tokens);
    const bool inherit_parent_divider = parent != nullptr && config.inherit
        && config.seed == SeedTokenOverride{} && config.alias == AliasTokenOverride{}
        && config.algorithms.empty() && !config.divider.algorithm
        && config.divider.seed == SeedTokenOverride{};
    DividerThemeToken divider;
    if (inherit_parent_divider) {
        divider = parent->divider();
    } else if (config.divider.algorithm) {
        auto component_seed = seed;
        apply_seed_override(component_seed, config.divider.seed);
        const auto component_map = derive_map(component_seed, algorithms);
        divider = derive_divider(component_seed, component_map,
            derive_alias(component_seed, component_map, algorithms));
    } else {
        divider = derive_divider(seed, map, alias);
    }
    apply_divider_override(divider, config.divider.tokens);
    SliderThemeToken slider;
    const bool inherit_slider = parent && config.inherit && config.seed == SeedTokenOverride{}
        && config.alias == AliasTokenOverride{} && config.algorithms.empty()
        && !config.slider.algorithm && config.slider.seed == SeedTokenOverride{};
    if (inherit_slider) slider = parent->slider();
    else if (config.slider.algorithm) {
        auto component_seed = seed; apply_seed_override(component_seed, config.slider.seed);
        const auto component_map = derive_map(component_seed, algorithms);
        slider = derive_slider(component_seed, component_map, derive_alias(component_seed, component_map, algorithms), algorithms);
    } else slider = derive_slider(seed, map, alias, algorithms);
    apply_slider_override(slider, config.slider.tokens);
    return ThemeSnapshot(
        std::move(seed), std::move(map), std::move(alias), std::move(button),
        std::move(text), std::move(switch_token), std::move(typography),
        std::move(divider),
        std::move(slider),
        std::make_shared<const detail::InputTokenSet>(std::move(input)),
        std::move(algorithms));
}

} // namespace ryn
