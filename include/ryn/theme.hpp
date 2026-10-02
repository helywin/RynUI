#pragma once

#include <ryn/design_token.hpp>
#include <ryn/button_types.hpp>
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ryn {
namespace detail {

struct ThemePropsAccess;
struct InputTokenSet;
struct InputTokenAccess;

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
    std::optional<Color> color_link;
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
    std::optional<bool> focus_outline;
    std::optional<bool> motion;

    friend bool operator==(const SeedTokenOverride&, const SeedTokenOverride&) = default;
};

struct AliasTokenOverride final {
    std::optional<Color> color_text;
    std::optional<Color> color_text_secondary;
    std::optional<Color> color_text_disabled;
    std::optional<Color> color_background_container;
    std::optional<Color> color_border;
    std::optional<Color> color_split;
    std::optional<Color> color_focus_outline;
    std::optional<ShadowList> box_shadow;
    std::optional<ShadowList> box_shadow_secondary;
    std::optional<ShadowList> box_shadow_tertiary;

    friend bool operator==(const AliasTokenOverride&, const AliasTokenOverride&) = default;
};

struct ButtonColorTokenOverride final {
    std::optional<Color> base;
    std::optional<Color> hover;
    std::optional<Color> active;
    std::optional<Color> light;
    std::optional<Color> light_hover;
    std::optional<Color> light_active;
    std::optional<Color> solid_text;
    std::optional<ShadowList> shadow;
    friend bool operator==(const ButtonColorTokenOverride&, const ButtonColorTokenOverride&) = default;
};

struct ButtonTokenOverride final {
    std::optional<Color> default_color;
    std::optional<Color> default_background;
    std::optional<Color> default_border_color;
    std::optional<Color> text_color;
    std::optional<Color> text_background;
    std::optional<Color> primary_color;
    std::optional<Color> primary_background;
    std::optional<Color> danger_background;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> icon_gap;
    std::optional<LogicalLength> border_radius;
    std::optional<ShadowList> default_shadow;
    std::optional<ShadowList> primary_shadow;
    std::optional<ShadowList> danger_shadow;
    std::array<ButtonColorTokenOverride, button_color_count> colors;
    std::optional<Color> ghost_background;
    std::optional<Color> default_ghost_color;
    std::optional<Color> default_ghost_border_color;
    std::optional<Color> link_color;
    std::optional<Color> link_hover_color;
    std::optional<Color> link_active_color;
    std::optional<Color> link_hover_background;
    std::optional<Color> default_solid_background;
    std::optional<Color> default_solid_hover_background;
    std::optional<Color> default_solid_active_background;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> dash_length;
    std::optional<LogicalLength> dash_gap;

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

struct InputTokenOverride final {
    std::optional<Color> color;
    std::optional<Color> placeholder_color;
    std::optional<Color> background;
    std::optional<Color> border_color;
    std::optional<Color> hover_border_color;
    std::optional<Color> active_border_color;
    std::optional<Color> hover_background;
    std::optional<Color> active_background;
    std::optional<Color> selection_background;
    std::optional<Color> selection_color;
    std::optional<Color> caret_color;
    std::optional<ShadowList> active_shadow;
    std::optional<ShadowList> error_active_shadow;
    std::optional<ShadowList> warning_active_shadow;
    std::optional<LogicalLength> input_font_size;
    std::optional<LogicalLength> input_font_size_small;
    std::optional<LogicalLength> input_font_size_large;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> padding_inline_small;
    std::optional<LogicalLength> padding_inline_large;
    std::optional<LogicalLength> padding_block;
    std::optional<LogicalLength> padding_block_small;
    std::optional<LogicalLength> padding_block_large;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> affix_padding;
    friend bool operator==(const InputTokenOverride&, const InputTokenOverride&) = default;
};

struct InputThemeConfig final {
    InputTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const InputThemeConfig&, const InputThemeConfig&) = default;
};

struct SwitchTokenOverride final {
    std::optional<LogicalLength> track_height;
    std::optional<LogicalLength> track_height_small;
    std::optional<LogicalLength> track_min_width;
    std::optional<LogicalLength> track_min_width_small;
    std::optional<LogicalLength> track_padding;
    std::optional<Color> handle_background;
    std::optional<LogicalLength> handle_size;
    std::optional<LogicalLength> handle_size_small;
    friend bool operator==(const SwitchTokenOverride&, const SwitchTokenOverride&) = default;
};

struct SwitchThemeConfig final {
    SwitchTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const SwitchThemeConfig&, const SwitchThemeConfig&) = default;
};

enum class TypographyLevel : std::uint8_t {
    H1,
    H2,
    H3,
    H4,
    H5,
};

inline constexpr std::size_t typography_level_count = 5;

[[nodiscard]] constexpr std::size_t typography_level_index(TypographyLevel level) noexcept {
    return static_cast<std::size_t>(level);
}

// Inline code / keyboard keycap appearance. Sizes are ratios of the surrounding
// font size, matching the upstream `em` units, so padding resolves against the
// font size actually used instead of a frozen pixel value.
struct InlineCodeTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> border_color;
    std::optional<float> font_scale;
    std::optional<float> padding_inline_em;
    std::optional<float> padding_block_start_em;
    std::optional<float> padding_block_end_em;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> border_radius;
    friend bool operator==(const InlineCodeTokenOverride&, const InlineCodeTokenOverride&) = default;
};

struct KeyboardTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> border_color;
    std::optional<float> font_scale;
    std::optional<float> padding_inline_em;
    std::optional<float> padding_block_start_em;
    std::optional<float> padding_block_end_em;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> border_bottom_width;
    friend bool operator==(const KeyboardTokenOverride&, const KeyboardTokenOverride&) = default;
};

// Typography Component Token. Colors are grouped separately from typography and
// metrics because the Theme invalidation domain is fixed per identity group: a
// color-only change must stay in paint/material, while font and geometry changes
// must re-run text shaping and measurement.
struct TypographyTokenOverride final {
    std::optional<Color> text;
    std::optional<Color> description;
    std::optional<Color> success;
    std::optional<Color> warning;
    std::optional<Color> error;
    std::optional<Color> error_text_hover;
    std::optional<Color> error_text_active;
    std::optional<Color> disabled;
    std::optional<Color> link;
    std::optional<Color> mark_background;
    std::optional<SystemFontFamily> font_family;
    std::optional<SystemFontFamily> font_family_code;
    std::optional<std::uint32_t> font_weight;
    std::optional<std::uint32_t> font_weight_strong;
    std::array<std::optional<LogicalLength>, typography_level_count> heading_font_sizes;
    std::array<std::optional<float>, typography_level_count> heading_line_heights;
    std::optional<LogicalLength> base_font_size;
    std::optional<LogicalLength> base_line_height;
    std::optional<float> title_margin_top_em;
    std::optional<float> title_margin_bottom_em;
    InlineCodeTokenOverride code;
    KeyboardTokenOverride keyboard;
    friend bool operator==(const TypographyTokenOverride&, const TypographyTokenOverride&) = default;
};

struct TypographyThemeConfig final {
    TypographyTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const TypographyThemeConfig&, const TypographyThemeConfig&) = default;
};

// Divider Component Token. `orientation_margin` keeps the upstream unitless
// 0..1 ratio and `text_padding_inline` is its `1em` gutter resolved against
// `typography.text_font_size`.
struct DividerTokenOverride final {
    std::optional<Color> line;
    std::optional<Color> text;
    std::optional<Color> plain_text;
    std::optional<LogicalLength> line_width;
    std::optional<float> orientation_margin;
    std::optional<LogicalLength> text_padding_inline;
    std::optional<LogicalLength> vertical_margin_inline;
    std::optional<LogicalLength> horizontal_margin;
    std::optional<LogicalLength> horizontal_with_text_margin;
    std::optional<LogicalLength> text_font_size;
    std::optional<std::uint32_t> text_font_weight;
    std::optional<LogicalLength> plain_font_size;
    std::optional<std::uint32_t> plain_font_weight;
    friend bool operator==(const DividerTokenOverride&, const DividerTokenOverride&) = default;
};

struct DividerThemeConfig final {
    DividerTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const DividerThemeConfig&, const DividerThemeConfig&) = default;
};

struct SliderTokenOverride final {
    std::optional<LogicalLength> dot_size;
    std::optional<LogicalLength> dot_border_width;
    std::optional<LogicalLength> mark_gap;
    std::optional<LogicalLength> mark_font_size;
    std::optional<LogicalLength> mark_line_height;
    std::optional<Color> dot_border;
    std::optional<Color> dot_active_border;
    std::optional<Color> dot_background;
    std::optional<Color> mark_text;
    std::optional<Color> mark_active_text;
    std::optional<Color> mark_disabled_text;
    std::optional<LogicalLength> rail_size;
    std::optional<LogicalLength> handle_size;
    std::optional<LogicalLength> handle_size_hover;
    std::optional<LogicalLength> handle_line_width;
    std::optional<LogicalLength> handle_line_width_hover;
    std::optional<Color> rail;
    std::optional<Color> rail_hover;
    std::optional<Color> track;
    std::optional<Color> track_hover;
    std::optional<Color> track_disabled;
    std::optional<Color> handle;
    std::optional<Color> handle_active;
    std::optional<Color> handle_outline;
    std::optional<Color> handle_disabled;
    std::optional<Color> handle_background;
    friend bool operator==(const SliderTokenOverride&, const SliderTokenOverride&) = default;
};

struct SliderThemeConfig final {
    SliderTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const SliderThemeConfig&, const SliderThemeConfig&) = default;
};

struct TooltipTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> text;
    std::optional<LogicalLength> max_width;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> padding_block;
    std::optional<LogicalLength> min_height;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> arrow_size;
    std::optional<LogicalLength> gap;
    std::optional<ShadowList> shadow;
    std::optional<std::int32_t> z_index_popup;
    friend bool operator==(const TooltipTokenOverride&, const TooltipTokenOverride&) = default;
};

struct TooltipThemeConfig final {
    TooltipTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const TooltipThemeConfig&, const TooltipThemeConfig&) = default;
};

struct TooltipThemeToken final {
    Color background;
    Color text;
    float max_width{250};
    float padding_inline{8};
    float padding_block{6};
    float min_height{32};
    float border_radius{6};
    float arrow_size{8};
    float gap{4};
    float font_size{14};
    float line_height{22};
    ShadowList shadow;
    std::int32_t z_index_popup{1070};

    [[nodiscard]] std::array<float, 7> metrics() const {
        return {max_width, padding_inline, padding_block, min_height, border_radius, arrow_size, gap};
    }

    friend bool operator==(const TooltipThemeToken&, const TooltipThemeToken&) = default;
};

struct ThemeConfig final {
    SeedTokenOverride seed;
    AliasTokenOverride alias;
    ButtonThemeConfig button;
    TextThemeConfig text;
    InputThemeConfig input;
    SwitchThemeConfig switch_;
    TypographyThemeConfig typography;
    DividerThemeConfig divider;
    SliderThemeConfig slider;
    TooltipThemeConfig tooltip;
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
    Color color_success_text;
    Color color_warning_text;
    Color color_error_text;
    Color color_link;
    Color color_link_hover;
    Color color_link_active;
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
    Color color_split;
    Color color_focus_outline;
    float line_width_focus{3.0F};
    float focus_outline_offset{1.0F};
    ShadowList box_shadow;
    ShadowList box_shadow_secondary;
    ShadowList box_shadow_tertiary;

    friend constexpr bool operator==(const ThemeAliasToken&, const ThemeAliasToken&) = default;
};

struct ButtonColorThemeToken final {
    Color base;
    Color hover;
    Color active;
    Color light;
    Color light_hover;
    Color light_active;
    Color solid_text;
    ShadowList shadow;

    [[nodiscard]] constexpr std::array<Color, 7> values() const noexcept {
        return {base, hover, active, light, light_hover, light_active, solid_text};
    }

    friend constexpr bool operator==(const ButtonColorThemeToken&, const ButtonColorThemeToken&) = default;
};

struct ButtonVariantThemeToken final {
    std::array<ButtonColorThemeToken, button_color_count> colors;
    Color ghost_background;
    Color default_ghost_color;
    Color default_ghost_border_color;
    Color link_color;
    Color link_hover_color;
    Color link_active_color;
    Color link_hover_background;
    Color default_solid_background;
    Color default_solid_hover_background;
    Color default_solid_active_background;

    [[nodiscard]] constexpr std::array<Color, 10> values() const noexcept {
        return {ghost_background,
                default_ghost_color,
                default_ghost_border_color,
                link_color,
                link_hover_color,
                link_active_color,
                link_hover_background,
                default_solid_background,
                default_solid_hover_background,
                default_solid_active_background};
    }

    friend constexpr bool operator==(const ButtonVariantThemeToken&, const ButtonVariantThemeToken&) = default;
};

struct ButtonThemeToken final {
    Color default_color;
    Color default_background;
    Color default_border_color;
    Color default_hover_color;
    Color default_active_color;
    Color text_color;
    Color text_background;
    Color text_hover_color;
    Color text_active_color;
    Color text_hover_background;
    Color text_active_background;
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
    ButtonVariantThemeToken variants;
    float dash_length{3.0F};
    float dash_gap{3.0F};

    friend constexpr bool operator==(const ButtonThemeToken&, const ButtonThemeToken&) = default;
};

struct TextThemeToken final {
    Color color;
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    std::uint32_t font_weight{400};
    float font_size{14.0F};
    float line_height{22.0F};

    friend constexpr bool operator==(const TextThemeToken&, const TextThemeToken&) = default;
};

struct SwitchThemeToken final {
    float track_height{};
    float track_height_small{};
    float track_min_width{};
    float track_min_width_small{};
    float track_padding{};
    Color handle_background;
    float handle_size{};
    float handle_size_small{};
    friend constexpr bool operator==(const SwitchThemeToken&, const SwitchThemeToken&) = default;
};

// Inline `code` / `kbd` appearance. Sizes stay as ratios of the resolved font
// size so padding tracks the surrounding typography. These have no upstream
// Component Token; they are a RynUI typed adaptation of the locked reference
// styles and are grouped with metrics because a background change and a padding
// change share one visual unit.
struct InlineCodeThemeToken final {
    Color background;
    Color border_color;
    float font_scale{0.85F};
    float padding_inline_em{0.4F};
    float padding_block_start_em{0.2F};
    float padding_block_end_em{0.1F};
    float border_width{1.0F};
    float border_radius{3.0F};
    float border_bottom_width{1.0F};

    friend constexpr bool operator==(const InlineCodeThemeToken&, const InlineCodeThemeToken&) = default;
};

struct TypographyHeadingToken final {
    float font_size{};
    float line_height{};

    friend constexpr bool operator==(const TypographyHeadingToken&, const TypographyHeadingToken&) = default;
};

struct TypographyColorToken final {
    Color text;
    Color description;
    Color success;
    Color warning;
    Color error;
    Color error_text_hover;
    Color error_text_active;
    Color disabled;
    Color link;
    Color mark_background;

    friend constexpr bool operator==(const TypographyColorToken&, const TypographyColorToken&) = default;
};

// Groups follow the Theme invalidation domains: `colors` only repaints,
// `headings`/`fonts` re-shape and re-measure, `metrics` only re-lays out.
struct TypographyThemeToken final {
    std::array<TypographyHeadingToken, typography_level_count> headings{};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    SystemFontFamily font_family_code{SystemFontFamily::ui_monospace};
    std::uint32_t font_weight{400};
    std::uint32_t font_weight_strong{600};
    float base_font_size{14.0F};
    float base_line_height{22.0F};
    // Title margins are em ratios of the heading's own font size, matching the
    // upstream `titleMarginTop: '1.2em'` / `titleMarginBottom: '0.5em'`.
    float title_margin_top_em{1.2F};
    float title_margin_bottom_em{0.5F};
    TypographyColorToken colors;
    InlineCodeThemeToken code;
    InlineCodeThemeToken keyboard;

    [[nodiscard]] const TypographyHeadingToken& heading(TypographyLevel level) const noexcept {
        return headings[typography_level_index(level)];
    }

    friend constexpr bool operator==(const TypographyThemeToken&, const TypographyThemeToken&) = default;
};

struct DividerColorToken final {
    Color line;
    Color text;
    Color plain_text;

    friend constexpr bool operator==(const DividerColorToken&, const DividerColorToken&) = default;
};

struct DividerMetricToken final {
    float line_width{1.0F};
    float orientation_margin{0.05F};
    float text_padding_inline{};
    float vertical_margin_inline{};
    float horizontal_margin{};
    float horizontal_with_text_margin{};

    friend constexpr bool operator==(const DividerMetricToken&, const DividerMetricToken&) = default;
};

struct DividerTypographyToken final {
    float text_font_size{16.0F};
    std::uint32_t text_font_weight{500};
    float plain_font_size{14.0F};
    std::uint32_t plain_font_weight{400};

    friend constexpr bool operator==(const DividerTypographyToken&, const DividerTypographyToken&) = default;
};

struct DividerThemeToken final {
    DividerColorToken colors;
    DividerMetricToken metrics;
    DividerTypographyToken typography;

    friend constexpr bool operator==(const DividerThemeToken&, const DividerThemeToken&) = default;
};

struct SliderMetricToken final {
    float rail_size{4};
    float handle_size{10};
    float handle_size_hover{12};
    float handle_line_width{2};
    float handle_line_width_hover{2.5F};
    float dot_size{8};
    float dot_border_width{2};
    float mark_gap{8};
    float mark_font_size{14};
    float mark_line_height{22};

    [[nodiscard]] auto values() const noexcept {
        return std::array{rail_size, handle_size,      handle_size_hover, handle_line_width, handle_line_width_hover,
                          dot_size,  dot_border_width, mark_gap,          mark_font_size,    mark_line_height};
    }

    friend constexpr bool operator==(const SliderMetricToken&, const SliderMetricToken&) = default;
};

struct SliderColorToken final {
    Color rail;
    Color rail_hover;
    Color track;
    Color track_hover;
    Color track_disabled;
    Color handle;
    Color handle_active;
    Color handle_outline;
    Color handle_disabled;
    Color handle_background;
    Color dot_border;
    Color dot_active_border;
    Color dot_background;
    Color mark_text;
    Color mark_active_text;
    Color mark_disabled_text;

    [[nodiscard]] auto values() const noexcept {
        return std::array{rail,
                          rail_hover,
                          track,
                          track_hover,
                          track_disabled,
                          handle,
                          handle_active,
                          handle_outline,
                          handle_disabled,
                          handle_background,
                          dot_border,
                          dot_active_border,
                          dot_background,
                          mark_text,
                          mark_active_text,
                          mark_disabled_text};
    }

    friend constexpr bool operator==(const SliderColorToken&, const SliderColorToken&) = default;
};

struct SliderThemeToken final {
    SliderMetricToken metrics;
    SliderColorToken colors;
    friend constexpr bool operator==(const SliderThemeToken&, const SliderThemeToken&) = default;
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
    [[nodiscard]] const SwitchThemeToken& switch_token() const noexcept;
    [[nodiscard]] const TypographyThemeToken& typography() const noexcept;
    [[nodiscard]] const DividerThemeToken& divider() const noexcept;
    [[nodiscard]] const SliderThemeToken& slider() const noexcept;
    [[nodiscard]] const TooltipThemeToken& tooltip() const noexcept;
    [[nodiscard]] std::span<const ThemeAlgorithm> algorithms() const noexcept;
    [[nodiscard]] std::string_view source_version() const noexcept;
    [[nodiscard]] std::string_view source_commit() const noexcept;
    [[nodiscard]] std::uint64_t identity() const noexcept;
    [[nodiscard]] const std::string& diagnostic_json() const noexcept;

    friend bool operator==(const ThemeSnapshot&, const ThemeSnapshot&);

private:
    friend struct detail::InputTokenAccess;
    friend ThemeSnapshot resolve_theme(const ThemeConfig&, const ThemeSnapshot*);

    ThemeSnapshot(AntDesignDefaultSeed seed, ThemeMapToken map, ThemeAliasToken alias, ButtonThemeToken button,
                  TextThemeToken text, SwitchThemeToken switch_token, TypographyThemeToken typography,
                  DividerThemeToken divider, SliderThemeToken slider, TooltipThemeToken tooltip,
                  std::shared_ptr<const detail::InputTokenSet> input, std::vector<ThemeAlgorithm> algorithms);

    AntDesignDefaultSeed seed_;
    ThemeMapToken map_;
    ThemeAliasToken alias_;
    ButtonThemeToken button_;
    TextThemeToken text_;
    SwitchThemeToken switch_token_;
    TypographyThemeToken typography_;
    DividerThemeToken divider_;
    SliderThemeToken slider_;
    TooltipThemeToken tooltip_;
    std::shared_ptr<const detail::InputTokenSet> input_;
    std::vector<ThemeAlgorithm> algorithms_;
    std::uint64_t identity_{};
    std::string diagnostic_json_;
};

[[nodiscard]] ThemeSnapshot resolve_theme(const ThemeConfig& config = {}, const ThemeSnapshot* parent = nullptr);

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
