#include <ryn/theme.hpp>

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

constexpr std::string_view ant_design_version = "6.5.0";
constexpr std::string_view ant_design_commit =
    "740ad964dc2397f33e40944367b0536a7314cc32";

[[nodiscard]] float fixed_length(
    const std::optional<LogicalLength>& value,
    float fallback,
    const char* name,
    bool strictly_positive = false) {
    if (!value.has_value()) {
        return fallback;
    }
    if (value->is_auto() || !detail::finite(value->value())
        || (strictly_positive ? value->value() <= 0.0F : value->value() < 0.0F)) {
        throw std::invalid_argument(name);
    }
    return value->value();
}

void apply_seed_override(AntDesignDefaultSeed& seed, const SeedTokenOverride& override) {
    if (override.color_primary) seed.color_primary = *override.color_primary;
    if (override.color_success) seed.color_success = *override.color_success;
    if (override.color_warning) seed.color_warning = *override.color_warning;
    if (override.color_error) seed.color_error = *override.color_error;
    if (override.color_info) seed.color_info = *override.color_info;
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

[[nodiscard]] constexpr bool is_default_primary(Color color) noexcept {
    return color == Color::rgba8(22, 119, 255);
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
        .color_info = seed.color_info,
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

void apply_dark(ThemeMapToken& map) {
    const bool default_primary = is_default_primary(map.color_primary);
    const Color black = Color::rgba8(0, 0, 0);
    const Color white = Color::rgba8(255, 255, 255);
    map.color_primary = default_primary
        ? Color::rgba8(22, 104, 220) : mix(map.color_primary, black, 0.12F);
    map.color_primary_hover = default_primary
        ? Color::rgba8(60, 137, 232) : mix(map.color_primary, white, 0.18F);
    map.color_primary_active = default_primary
        ? Color::rgba8(21, 84, 173) : mix(map.color_primary, black, 0.2F);
    map.color_primary_border = default_primary
        ? Color::rgba8(21, 50, 91) : mix(map.color_primary, black, 0.55F);
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
            apply_dark(map);
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
        .color_focus_outline = map.color_primary_border,
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
    if (override.color_focus_outline) alias.color_focus_outline = *override.color_focus_outline;
    if (override.box_shadow) alias.box_shadow = *override.box_shadow;
    if (override.box_shadow_secondary) alias.box_shadow_secondary = *override.box_shadow_secondary;
    if (override.box_shadow_tertiary) alias.box_shadow_tertiary = *override.box_shadow_tertiary;
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
        .primary_color = Color::rgba8(255, 255, 255),
        .primary_background = map.color_primary,
        .primary_hover_background = map.color_primary_hover,
        .primary_active_background = map.color_primary_active,
        .danger_background = map.color_error,
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
        .default_shadow = shadows.button_default,
        .primary_shadow = shadows.button_primary,
        .danger_shadow = shadows.button_danger,
    };
}

void apply_button_override(ButtonThemeToken& button, const ButtonTokenOverride& override) {
    if (override.default_color) button.default_color = *override.default_color;
    if (override.default_background) button.default_background = *override.default_background;
    if (override.default_border_color) button.default_border_color = *override.default_border_color;
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
    stream << ",\"fontSize\":" << seed.font_size << ",\"sizeUnit\":" << seed.size_unit
           << ",\"sizeStep\":" << seed.size_step << ",\"controlHeight\":"
           << seed.control_height << "},\"map\":{\"colorPrimary\":";
    append_color(stream, map.color_primary);
    stream << ",\"colorPrimaryHover\":";
    append_color(stream, map.color_primary_hover);
    stream << ",\"colorPrimaryActive\":";
    append_color(stream, map.color_primary_active);
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
    stream << ",\"lineWidthFocus\":" << alias.line_width_focus
           << ",\"focusOutlineOffset\":" << alias.focus_outline_offset
           << ",\"boxShadowLayers\":" << alias.box_shadow.size()
           << "},\"button\":{\"primaryBg\":";
    append_color(stream, button.primary_background);
    stream << ",\"controlHeight\":" << button.control_height
           << ",\"paddingInline\":" << button.padding_inline
           << ",\"borderRadius\":" << button.border_radius
           << ",\"shadowLayers\":" << button.primary_shadow.size()
           << "},\"text\":{\"color\":";
    append_color(stream, text.color);
    stream << ",\"fontFamily\":" << static_cast<int>(text.font_family)
           << ",\"fontWeight\":" << text.font_weight
           << ",\"fontSize\":" << text.font_size
           << ",\"lineHeight\":" << text.line_height << "}}\n";
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
    hash_integer(hash, seed.motion);

    const std::array map_colors{
        map.color_primary, map.color_primary_hover, map.color_primary_active,
        map.color_primary_border, map.color_success, map.color_warning, map.color_error,
        map.color_info, map.color_text_base, map.color_background_base,
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
        alias.color_border_secondary, alias.color_focus_outline,
    };
    for (const Color color : alias_colors) hash_color(hash, color);
    hash_float(hash, alias.line_width_focus);
    hash_float(hash, alias.focus_outline_offset);
    hash_shadow(hash, alias.box_shadow);
    hash_shadow(hash, alias.box_shadow_secondary);
    hash_shadow(hash, alias.box_shadow_tertiary);

    const std::array button_colors{
        button.default_color, button.default_background, button.default_border_color,
        button.default_hover_color, button.default_active_color, button.primary_color,
        button.primary_background, button.primary_hover_background,
        button.primary_active_background, button.danger_background,
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
        button.border_width, button.icon_gap,
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
    std::vector<ThemeAlgorithm> algorithms)
    : seed_(std::move(seed)),
      map_(std::move(map)),
      alias_(std::move(alias)),
      button_(std::move(button)),
      text_(std::move(text)),
      algorithms_(std::move(algorithms)) {
    identity_ = snapshot_identity(seed_, map_, alias_, button_, text_, algorithms_);
    diagnostic_json_ = serialize_snapshot(
        seed_, map_, alias_, button_, text_, algorithms_, identity_);
}

const AntDesignDefaultSeed& ThemeSnapshot::seed() const noexcept { return seed_; }
const ThemeMapToken& ThemeSnapshot::map() const noexcept { return map_; }
const ThemeAliasToken& ThemeSnapshot::alias() const noexcept { return alias_; }
const ButtonThemeToken& ThemeSnapshot::button() const noexcept { return button_; }
const TextThemeToken& ThemeSnapshot::text() const noexcept { return text_; }
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
        && left.text_ == right.text_ && left.algorithms_ == right.algorithms_;
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
    ThemeAliasToken alias = derive_alias(map, algorithms);
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
        const ThemeAliasToken component_alias = derive_alias(component_map, algorithms);
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
        const ThemeAliasToken component_alias = derive_alias(component_map, algorithms);
        text = derive_text(component_seed, component_map, component_alias);
    } else {
        text = derive_text(seed, map, alias);
    }
    apply_text_override(text, config.text.tokens);
    return ThemeSnapshot(
        std::move(seed), std::move(map), std::move(alias), std::move(button),
        std::move(text),
        std::move(algorithms));
}

} // namespace ryn
