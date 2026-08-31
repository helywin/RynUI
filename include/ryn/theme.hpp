#pragma once

#include <ryn/design_token.hpp>
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ryn {
namespace detail {

struct ThemePropsAccess;

} // namespace detail

enum class ThemeAlgorithm : std::uint8_t {
    Default,
    Dark,
    Compact,
};

struct SeedTokenOverride final {
    std::optional<Color> color_primary;
    std::optional<Color> color_success;
    std::optional<Color> color_warning;
    std::optional<Color> color_error;
    std::optional<Color> color_info;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_width;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> size_unit;
    std::optional<LogicalLength> size_step;
    std::optional<LogicalLength> control_height;
    std::optional<std::int32_t> z_index_base;
    std::optional<std::int32_t> z_index_popup_base;
    std::optional<float> opacity_image;
    std::optional<Duration> motion_unit;
    std::optional<Duration> motion_base;
    std::optional<bool> motion;

    friend bool operator==(const SeedTokenOverride&, const SeedTokenOverride&) = default;
};

struct AliasTokenOverride final {
    std::optional<Color> color_text;
    std::optional<Color> color_text_secondary;
    std::optional<Color> color_text_disabled;
    std::optional<Color> color_background_container;
    std::optional<Color> color_border;
    std::optional<Color> color_focus_outline;
    std::optional<ShadowList> box_shadow;
    std::optional<ShadowList> box_shadow_secondary;
    std::optional<ShadowList> box_shadow_tertiary;

    friend bool operator==(const AliasTokenOverride&, const AliasTokenOverride&) = default;
};

struct ButtonTokenOverride final {
    std::optional<Color> default_color;
    std::optional<Color> default_background;
    std::optional<Color> default_border_color;
    std::optional<Color> primary_color;
    std::optional<Color> primary_background;
    std::optional<Color> danger_background;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> icon_gap;
    std::optional<LogicalLength> border_radius;
    std::optional<ShadowList> default_shadow;
    std::optional<ShadowList> primary_shadow;
    std::optional<ShadowList> danger_shadow;

    friend bool operator==(const ButtonTokenOverride&, const ButtonTokenOverride&) = default;
};

struct ButtonThemeConfig final {
    ButtonTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};

    friend bool operator==(const ButtonThemeConfig&, const ButtonThemeConfig&) = default;
};

struct TextTokenOverride final {
    std::optional<Color> color;
    std::optional<SystemFontFamily> font_family;
    std::optional<std::uint32_t> font_weight;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;

    friend bool operator==(const TextTokenOverride&, const TextTokenOverride&) = default;
};

struct TextThemeConfig final {
    TextTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};

    friend bool operator==(const TextThemeConfig&, const TextThemeConfig&) = default;
};

struct ThemeConfig final {
    SeedTokenOverride seed;
    AliasTokenOverride alias;
    ButtonThemeConfig button;
    TextThemeConfig text;
    std::vector<ThemeAlgorithm> algorithms;
    bool inherit{true};

    friend bool operator==(const ThemeConfig&, const ThemeConfig&) = default;
};

struct ThemeMapToken final {
    Color color_primary;
    Color color_primary_hover;
    Color color_primary_active;
    Color color_primary_border;
    Color color_success;
    Color color_warning;
    Color color_error;
    Color color_error_hover;
    Color color_error_active;
    Color color_info;
    Color color_text_base;
    Color color_background_base;
    float font_size_small{};
    float font_size{};
    float font_size_large{};
    float line_height_small{};
    float line_height{};
    float line_height_large{};
    float size_xs{};
    float size_small{};
    float size{};
    float size_large{};
    float control_height_small{};
    float control_height{};
    float control_height_large{};
    float border_radius_small{};
    float border_radius{};
    float border_radius_large{};
    Duration motion_unit;
    Duration motion_base;
    bool motion{true};

    friend constexpr bool operator==(const ThemeMapToken&, const ThemeMapToken&) = default;
};

struct ThemeAliasToken final {
    Color color_text;
    Color color_text_secondary;
    Color color_text_disabled;
    Color color_background_container;
    Color color_background_elevated;
    Color color_background_container_disabled;
    Color color_border;
    Color color_border_secondary;
    Color color_focus_outline;
    float line_width_focus{3.0F};
    float focus_outline_offset{1.0F};
    ShadowList box_shadow;
    ShadowList box_shadow_secondary;
    ShadowList box_shadow_tertiary;

    friend constexpr bool operator==(
        const ThemeAliasToken&,
        const ThemeAliasToken&) = default;
};

struct ButtonThemeToken final {
    Color default_color;
    Color default_background;
    Color default_border_color;
    Color default_hover_color;
    Color default_active_color;
    Color primary_color;
    Color primary_background;
    Color primary_hover_background;
    Color primary_active_background;
    Color danger_color;
    Color danger_background;
    Color danger_hover_background;
    Color danger_active_background;
    Color disabled_color;
    Color disabled_background;
    Color disabled_border_color;
    float control_height_small{};
    float control_height{};
    float control_height_large{};
    float padding_inline_small{};
    float padding_inline{};
    float padding_inline_large{};
    float content_font_size_small{};
    float content_font_size{};
    float content_font_size_large{};
    float content_line_height_small{};
    float content_line_height{};
    float content_line_height_large{};
    float border_radius_small{};
    float border_radius{};
    float border_radius_large{};
    float border_width{1.0F};
    float icon_gap{8.0F};
    float loading_indicator_size{14.0F};
    float loading_opacity{0.65F};
    ShadowList default_shadow;
    ShadowList primary_shadow;
    ShadowList danger_shadow;

    friend constexpr bool operator==(
        const ButtonThemeToken&,
        const ButtonThemeToken&) = default;
};

struct TextThemeToken final {
    Color color;
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    std::uint32_t font_weight{400};
    float font_size{14.0F};
    float line_height{22.0F};

    friend constexpr bool operator==(const TextThemeToken&, const TextThemeToken&) = default;
};

class ThemeSnapshot final {
public:
    ThemeSnapshot(const ThemeSnapshot&) = default;
    ThemeSnapshot(ThemeSnapshot&&) noexcept = default;
    ThemeSnapshot& operator=(const ThemeSnapshot&) = default;
    ThemeSnapshot& operator=(ThemeSnapshot&&) noexcept = default;
    ~ThemeSnapshot() = default;

    [[nodiscard]] const AntDesignDefaultSeed& seed() const noexcept;
    [[nodiscard]] const ThemeMapToken& map() const noexcept;
    [[nodiscard]] const ThemeAliasToken& alias() const noexcept;
    [[nodiscard]] const ButtonThemeToken& button() const noexcept;
    [[nodiscard]] const TextThemeToken& text() const noexcept;
    [[nodiscard]] std::span<const ThemeAlgorithm> algorithms() const noexcept;
    [[nodiscard]] std::string_view source_version() const noexcept;
    [[nodiscard]] std::string_view source_commit() const noexcept;
    [[nodiscard]] std::uint64_t identity() const noexcept;
    [[nodiscard]] const std::string& diagnostic_json() const noexcept;

    friend bool operator==(const ThemeSnapshot&, const ThemeSnapshot&);

private:
    friend ThemeSnapshot resolve_theme(
        const ThemeConfig&,
        const ThemeSnapshot*);

    ThemeSnapshot(
        AntDesignDefaultSeed seed,
        ThemeMapToken map,
        ThemeAliasToken alias,
        ButtonThemeToken button,
        TextThemeToken text,
        std::vector<ThemeAlgorithm> algorithms);

    AntDesignDefaultSeed seed_;
    ThemeMapToken map_;
    ThemeAliasToken alias_;
    ButtonThemeToken button_;
    TextThemeToken text_;
    std::vector<ThemeAlgorithm> algorithms_;
    std::uint64_t identity_{};
    std::string diagnostic_json_;
};

[[nodiscard]] ThemeSnapshot resolve_theme(
    const ThemeConfig& config = {},
    const ThemeSnapshot* parent = nullptr);

class ThemeProps final {
public:
    ThemeProps& config(Prop<ThemeConfig> value) {
        config_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::ThemePropsAccess;

    Prop<ThemeConfig> config_{ThemeConfig{}};
};

struct ThemeContentSlot final {};
using ThemeContent = SlotContent<ThemeContentSlot>;

void Theme(ThemeProps props, ThemeContent content);

} // namespace ryn
