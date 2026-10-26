#pragma once

#include <ryn/menu_theme.hpp>

#include <ryn/design_token.hpp>
#include <ryn/tag_badge_tokens.hpp>
#include <ryn/feedback_tokens.hpp>
#include <ryn/empty_result_tokens.hpp>
#include <ryn/segmented_tokens.hpp>
#include <ryn/date_picker_tokens.hpp>
#include <ryn/form_tokens.hpp>
#include <ryn/breadcrumb_tokens.hpp>
#include <ryn/steps_tokens.hpp>
#include <ryn/time_picker_tokens.hpp>
#include <ryn/skeleton_tokens.hpp>
#include <ryn/descriptions_tokens.hpp>
#include <ryn/notice_tokens.hpp>
#include <ryn/timeline_tokens.hpp>
#include <ryn/avatar_tokens.hpp>
#include <ryn/rate_tokens.hpp>
#include <ryn/statistic_tokens.hpp>
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
    std::optional<bool> wireframe;

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
    std::optional<float> opacity_loading;

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
    std::optional<LogicalLength> wave_spread;
    std::optional<LogicalLength> wave_width;
    std::optional<float> wave_opacity;

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
    std::optional<Color> filled_background;
    std::optional<Color> filled_hover_background;
    std::optional<Color> error_background;
    std::optional<Color> error_hover_background;
    std::optional<Color> warning_background;
    std::optional<Color> warning_hover_background;
    std::optional<Color> error_color;
    std::optional<Color> warning_color;
    std::optional<LogicalLength> focus_width;
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
    std::optional<LogicalLength> inner_min_margin;
    std::optional<LogicalLength> inner_max_margin;
    std::optional<LogicalLength> inner_min_margin_small;
    std::optional<LogicalLength> inner_max_margin_small;
    std::optional<ShadowList> handle_shadow;
    std::optional<LogicalLength> wave_spread;
    std::optional<LogicalLength> wave_width;
    std::optional<float> wave_opacity;
    friend bool operator==(const SwitchTokenOverride&, const SwitchTokenOverride&) = default;
};

struct SwitchThemeConfig final {
    SwitchTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const SwitchThemeConfig&, const SwitchThemeConfig&) = default;
};

// Native adaptation of Checkbox's component-local map/alias style values.
// The locked upstream ComponentToken interface is empty.
struct CheckboxTokenOverride final {
    std::optional<LogicalLength> size;
    std::optional<LogicalLength> indeterminate_size;
    std::optional<LogicalLength> line_width;
    std::optional<LogicalLength> check_width;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> label_gap;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    std::optional<LogicalLength> wave_spread;
    std::optional<LogicalLength> wave_width;
    std::optional<float> wave_opacity;
    std::optional<Color> primary;
    std::optional<Color> primary_hover;
    std::optional<Color> primary_active;
    std::optional<Color> background;
    std::optional<Color> border;
    std::optional<Color> disabled_background;
    std::optional<Color> disabled_foreground;
    std::optional<Color> checkmark;
    std::optional<Color> label;
    std::optional<Color> focus;
    friend bool operator==(const CheckboxTokenOverride&, const CheckboxTokenOverride&) = default;
};

struct CheckboxThemeConfig final {
    CheckboxTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const CheckboxThemeConfig&, const CheckboxThemeConfig&) = default;
};

// Locked Radio Component Token plus native geometry/focus/wave adaptation.
struct RadioTokenOverride final {
    std::optional<LogicalLength> size;
    std::optional<LogicalLength> dot_size;
    std::optional<LogicalLength> line_width;
    std::optional<LogicalLength> label_gap;
    std::optional<LogicalLength> wrapper_margin_inline_end;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> button_height;
    std::optional<LogicalLength> button_height_small;
    std::optional<LogicalLength> button_height_large;
    std::optional<LogicalLength> button_padding_inline;
    std::optional<LogicalLength> button_padding_inline_small;
    std::optional<LogicalLength> button_font_size_large;
    std::optional<LogicalLength> button_radius;
    std::optional<LogicalLength> button_radius_small;
    std::optional<LogicalLength> button_radius_large;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    std::optional<LogicalLength> wave_spread;
    std::optional<LogicalLength> wave_width;
    std::optional<float> wave_opacity;
    std::optional<Color> primary;
    std::optional<Color> primary_hover;
    std::optional<Color> primary_active;
    std::optional<Color> background;
    std::optional<Color> border;
    std::optional<Color> disabled_background;
    std::optional<Color> disabled_foreground;
    std::optional<Color> dot;
    std::optional<Color> dot_disabled;
    std::optional<Color> checked_background;
    std::optional<Color> label;
    std::optional<Color> focus;
    std::optional<Color> button_background;
    std::optional<Color> button_checked_background;
    std::optional<Color> button_color;
    std::optional<Color> button_checked_background_disabled;
    std::optional<Color> button_checked_color_disabled;
    std::optional<Color> button_solid_checked_color;
    std::optional<Color> button_solid_checked_background;
    std::optional<Color> button_solid_checked_hover_background;
    std::optional<Color> button_solid_checked_active_background;
    friend bool operator==(const RadioTokenOverride&, const RadioTokenOverride&) = default;
};

struct RadioThemeConfig final {
    RadioTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const RadioThemeConfig&, const RadioThemeConfig&) = default;
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
    std::optional<LogicalLength> small_horizontal_margin;
    std::optional<LogicalLength> middle_horizontal_margin;
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

struct SelectColorToken final {
    Color popup_background;
    Color option_color;
    Color option_disabled_color;
    Color option_selected_color;
    Color option_selected_background;
    Color option_active_background;
    Color selector_background;
    Color clear_background;
    Color hover_border_color;
    Color active_border_color;
    Color active_outline_color;

    [[nodiscard]] auto values() const noexcept {
        return std::array{popup_background,           option_color,
                          option_disabled_color,      option_selected_color,
                          option_selected_background, option_active_background,
                          selector_background,        clear_background,
                          hover_border_color,         active_border_color,
                          active_outline_color};
    }

    friend bool operator==(const SelectColorToken&, const SelectColorToken&) = default;
};

struct SelectMetricToken final {
    float option_height{};
    float option_padding_inline{};
    float option_padding_block{};
    float popup_padding{};
    float border_radius{};
    float gap{};

    [[nodiscard]] auto values() const noexcept {
        return std::array{option_height, option_padding_inline, option_padding_block,
                          popup_padding, border_radius,         gap};
    }

    friend bool operator==(const SelectMetricToken&, const SelectMetricToken&) = default;
};

struct SelectTypographyToken final {
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    float font_size{14};
    float line_height{22};
    std::uint32_t font_weight{400};
    std::uint32_t selected_font_weight{600};
    friend bool operator==(const SelectTypographyToken&, const SelectTypographyToken&) = default;
};

struct SelectThemeToken final {
    SelectColorToken colors;
    SelectMetricToken metrics;
    SelectTypographyToken typography;
    ShadowList shadow;
    std::int32_t z_index_popup{1050};
    friend bool operator==(const SelectThemeToken&, const SelectThemeToken&) = default;
};

struct SelectTokenOverride final {
    std::optional<Color> popup_background;
    std::optional<Color> option_color;
    std::optional<Color> option_disabled_color;
    std::optional<Color> option_selected_color;
    std::optional<Color> option_selected_background;
    std::optional<Color> option_active_background;
    std::optional<Color> selector_background;
    std::optional<Color> clear_background;
    std::optional<Color> hover_border_color;
    std::optional<Color> active_border_color;
    std::optional<Color> active_outline_color;
    std::optional<LogicalLength> option_height;
    std::optional<LogicalLength> option_padding_inline;
    std::optional<LogicalLength> option_padding_block;
    std::optional<LogicalLength> popup_padding;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> gap;
    std::optional<SystemFontFamily> font_family;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<std::uint32_t> font_weight;
    std::optional<std::uint32_t> selected_font_weight;
    std::optional<ShadowList> shadow;
    std::optional<std::int32_t> z_index_popup;
    friend bool operator==(const SelectTokenOverride&, const SelectTokenOverride&) = default;
};

struct SelectThemeConfig final {
    SelectTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const SelectThemeConfig&, const SelectThemeConfig&) = default;
};

struct CardColorToken final {
    Color background;
    Color header_background;
    Color actions_background;
    Color text;
    Color header_text;
    Color extra;
    Color border;
    Color separator;
    Color placeholder;

    [[nodiscard]] auto values() const {
        return std::array{background, header_background, actions_background, text, header_text, extra,
                          border,     separator,         placeholder};
    }

    friend bool operator==(const CardColorToken&, const CardColorToken&) = default;
};

struct CardMetricToken final {
    float header_height{56};
    float header_height_small{38};
    float body_padding{24};
    float body_padding_small{12};
    float header_padding{24};
    float header_padding_small{12};
    float actions_padding{12};
    float border_width{1};
    float border_radius{8};
    float loading_line_height{16};
    float loading_gap{12};
    float header_gap{16};

    [[nodiscard]] auto values() const {
        return std::array{header_height,  header_height_small,  body_padding,    body_padding_small,
                          header_padding, header_padding_small, actions_padding, border_width,
                          border_radius,  loading_line_height,  loading_gap,     header_gap};
    }

    friend bool operator==(const CardMetricToken&, const CardMetricToken&) = default;
};

struct CardTypographyToken final {
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    float font_size{14};
    float line_height{22};
    float header_font_size{16};
    float header_line_height{24};
    float header_font_size_small{14};
    float header_line_height_small{22};
    std::uint32_t font_weight{400};
    std::uint32_t header_font_weight{600};
    friend bool operator==(const CardTypographyToken&, const CardTypographyToken&) = default;
};

struct CardEffectToken final {
    ShadowList shadow;
    ShadowList hover_shadow;
    friend bool operator==(const CardEffectToken&, const CardEffectToken&) = default;
};

struct CardThemeToken final {
    CardColorToken colors;
    CardMetricToken metrics;
    CardTypographyToken typography;
    CardEffectToken effects;
    friend bool operator==(const CardThemeToken&, const CardThemeToken&) = default;
};

struct CardTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> header_background;
    std::optional<Color> actions_background;
    std::optional<Color> text;
    std::optional<Color> header_text;
    std::optional<Color> extra;
    std::optional<Color> border;
    std::optional<Color> separator;
    std::optional<Color> placeholder;
    std::optional<LogicalLength> header_height;
    std::optional<LogicalLength> header_height_small;
    std::optional<LogicalLength> body_padding;
    std::optional<LogicalLength> body_padding_small;
    std::optional<LogicalLength> header_padding;
    std::optional<LogicalLength> header_padding_small;
    std::optional<LogicalLength> actions_padding;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> loading_line_height;
    std::optional<LogicalLength> loading_gap;
    std::optional<LogicalLength> header_gap;
    std::optional<SystemFontFamily> font_family;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> header_font_size;
    std::optional<LogicalLength> header_line_height;
    std::optional<LogicalLength> header_font_size_small;
    std::optional<LogicalLength> header_line_height_small;
    std::optional<std::uint32_t> font_weight;
    std::optional<std::uint32_t> header_font_weight;
    std::optional<ShadowList> shadow;
    std::optional<ShadowList> hover_shadow;
    friend bool operator==(const CardTokenOverride&, const CardTokenOverride&) = default;
};

struct CardThemeConfig final {
    CardTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const CardThemeConfig&, const CardThemeConfig&) = default;
};

struct CollapseColorToken final {
    Color header_background;
    Color body_background;
    Color header_text;
    Color body_text;
    Color disabled_text;
    Color border;
    Color focus_outline;

    [[nodiscard]] auto values() const {
        return std::array{header_background, body_background, header_text,  body_text,
                          disabled_text,     border,          focus_outline};
    }

    friend bool operator==(const CollapseColorToken&, const CollapseColorToken&) = default;
};

struct CollapseMetricToken final {
    float header_block_small{8};
    float header_block{12};
    float header_block_large{16};
    float header_start_small{8};
    float header_start{16};
    float header_start_large{16};
    float header_end_small{12};
    float header_end{16};
    float header_end_large{24};
    float body_padding_small{12};
    float body_padding{16};
    float body_padding_large{24};
    float arrow_size{12};
    float header_gap{12};
    float border_width{1};
    float border_radius{8};

    [[nodiscard]] auto values() const {
        return std::array{header_block_small, header_block,       header_block_large, header_start_small,
                          header_start,       header_start_large, header_end_small,   header_end,
                          header_end_large,   body_padding_small, body_padding,       body_padding_large,
                          arrow_size,         header_gap,         border_width,       border_radius};
    }

    friend bool operator==(const CollapseMetricToken&, const CollapseMetricToken&) = default;
};

struct CollapseTypographyToken final {
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    std::uint32_t font_weight{400};
    float font_size{14};
    float line_height{22};
    float header_font_size_large{16};
    float header_line_height_large{24};

    [[nodiscard]] auto values() const {
        return std::array{font_size, line_height, header_font_size_large, header_line_height_large};
    }

    friend bool operator==(const CollapseTypographyToken&, const CollapseTypographyToken&) = default;
};

struct CollapseEffectToken final {
    float focus_width{3};
    float focus_offset{1};

    [[nodiscard]] auto values() const {
        return std::array{focus_width, focus_offset};
    }

    friend bool operator==(const CollapseEffectToken&, const CollapseEffectToken&) = default;
};

struct CollapseThemeToken final {
    CollapseColorToken colors;
    CollapseMetricToken metrics;
    CollapseTypographyToken typography;
    CollapseEffectToken effects;
    friend bool operator==(const CollapseThemeToken&, const CollapseThemeToken&) = default;
};

struct CollapseTokenOverride final {
    std::optional<Color> header_background;
    std::optional<Color> body_background;
    std::optional<Color> header_text;
    std::optional<Color> body_text;
    std::optional<Color> disabled_text;
    std::optional<Color> border;
    std::optional<Color> focus_outline;
    std::optional<LogicalLength> header_block_small;
    std::optional<LogicalLength> header_block;
    std::optional<LogicalLength> header_block_large;
    std::optional<LogicalLength> header_start_small;
    std::optional<LogicalLength> header_start;
    std::optional<LogicalLength> header_start_large;
    std::optional<LogicalLength> header_end_small;
    std::optional<LogicalLength> header_end;
    std::optional<LogicalLength> header_end_large;
    std::optional<LogicalLength> body_padding_small;
    std::optional<LogicalLength> body_padding;
    std::optional<LogicalLength> body_padding_large;
    std::optional<LogicalLength> arrow_size;
    std::optional<LogicalLength> header_gap;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> border_radius;
    std::optional<SystemFontFamily> font_family;
    std::optional<std::uint32_t> font_weight;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> header_font_size_large;
    std::optional<LogicalLength> header_line_height_large;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    friend bool operator==(const CollapseTokenOverride&, const CollapseTokenOverride&) = default;
};

struct CollapseThemeConfig final {
    CollapseTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const CollapseThemeConfig&, const CollapseThemeConfig&) = default;
};

struct TabsColorToken final {
    Color item;
    Color item_selected;
    Color item_hover;
    Color item_active;
    Color item_disabled;
    Color rail;
    Color indicator;
    Color card_background;
    Color card_selected_background;
    Color card_border;
    Color focus_outline;

    [[nodiscard]] auto values() const {
        return std::array{item,        item_selected,   item_hover,
                          item_active, item_disabled,   rail,
                          indicator,   card_background, card_selected_background,
                          card_border, focus_outline};
    }

    friend bool operator==(const TabsColorToken&, const TabsColorToken&) = default;
};

struct TabsMetricToken final {
    float line_padding_small{8};
    float line_padding{12};
    float line_padding_large{16};
    float card_height_small{32};
    float card_height{40};
    float card_height_large{48};
    float card_padding_small{8};
    float card_padding{16};
    float card_padding_large{16};
    float horizontal_gutter{32};
    float card_gutter{2};
    float body_gap{16};
    float indicator_width{2};
    float border_width{1};
    float border_radius{8};
    float scroll_button_width{24};
    float focus_inset{4};

    [[nodiscard]] auto values() const {
        return std::array{
            line_padding_small, line_padding, line_padding_large, card_height_small,   card_height, card_height_large,
            card_padding_small, card_padding, card_padding_large, horizontal_gutter,   card_gutter, body_gap,
            indicator_width,    border_width, border_radius,      scroll_button_width, focus_inset};
    }

    friend bool operator==(const TabsMetricToken&, const TabsMetricToken&) = default;
};

struct TabsTypographyToken final {
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    float font_size_small{14};
    float font_size{14};
    float font_size_large{16};
    float line_height_small{22};
    float line_height{22};
    float line_height_large{24};
    std::uint32_t font_weight{400};

    [[nodiscard]] auto values() const {
        return std::array{font_size_small,   font_size,   font_size_large,
                          line_height_small, line_height, line_height_large};
    }

    friend bool operator==(const TabsTypographyToken&, const TabsTypographyToken&) = default;
};

struct TabsEffectToken final {
    float focus_width{3};
    float focus_offset{1};
    float focus_radius{4};

    [[nodiscard]] auto values() const {
        return std::array{focus_width, focus_offset, focus_radius};
    }

    friend bool operator==(const TabsEffectToken&, const TabsEffectToken&) = default;
};

struct TabsThemeToken final {
    TabsColorToken colors;
    TabsMetricToken metrics;
    TabsTypographyToken typography;
    TabsEffectToken effects;
    friend bool operator==(const TabsThemeToken&, const TabsThemeToken&) = default;
};

struct TabsTokenOverride final {
    std::optional<Color> item;
    std::optional<Color> item_selected;
    std::optional<Color> item_hover;
    std::optional<Color> item_active;
    std::optional<Color> item_disabled;
    std::optional<Color> rail;
    std::optional<Color> indicator;
    std::optional<Color> card_background;
    std::optional<Color> card_selected_background;
    std::optional<Color> card_border;
    std::optional<Color> focus_outline;
    std::optional<LogicalLength> line_padding_small;
    std::optional<LogicalLength> line_padding;
    std::optional<LogicalLength> line_padding_large;
    std::optional<LogicalLength> card_height_small;
    std::optional<LogicalLength> card_height;
    std::optional<LogicalLength> card_height_large;
    std::optional<LogicalLength> card_padding_small;
    std::optional<LogicalLength> card_padding;
    std::optional<LogicalLength> card_padding_large;
    std::optional<LogicalLength> horizontal_gutter;
    std::optional<LogicalLength> card_gutter;
    std::optional<LogicalLength> body_gap;
    std::optional<LogicalLength> indicator_width;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> scroll_button_width;
    std::optional<LogicalLength> focus_inset;
    std::optional<LogicalLength> font_size_small;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> font_size_large;
    std::optional<LogicalLength> line_height_small;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> line_height_large;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    std::optional<LogicalLength> focus_radius;
    std::optional<SystemFontFamily> font_family;
    std::optional<std::uint32_t> font_weight;
    friend bool operator==(const TabsTokenOverride&, const TabsTokenOverride&) = default;
};

struct SegmentedThemeConfig final {
    SegmentedTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const SegmentedThemeConfig&, const SegmentedThemeConfig&) = default;
};

struct FormThemeConfig final {
    FormTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const FormThemeConfig&, const FormThemeConfig&) = default;
};

struct DescriptionsThemeConfig final {
    DescriptionsTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const DescriptionsThemeConfig&, const DescriptionsThemeConfig&) = default;
};

struct NoticeThemeConfig final {
    NoticeTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const NoticeThemeConfig&, const NoticeThemeConfig&) = default;
};

struct TabsThemeConfig final {
    TabsTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const TabsThemeConfig&, const TabsThemeConfig&) = default;
};

struct ModalColorToken final {
    Color background;
    Color header_background;
    Color footer_background;
    Color title;
    Color text;
    Color close;
    Color close_hover;
    Color close_hover_background;
    Color mask;
    Color focus_outline;

    [[nodiscard]] auto values() const {
        return std::array{background, header_background, footer_background,      title, text,
                          close,      close_hover,       close_hover_background, mask,  focus_outline};
    }

    friend bool operator==(const ModalColorToken&, const ModalColorToken&) = default;
};

struct ModalMetricToken final {
    float padding_inline{24};
    float padding_block{20};
    float header_gap{8};
    float footer_gap{12};
    float button_gap{8};
    float border_radius{8};
    float close_size{32};
    float close_icon_size{16};
    float close_inset{12};
    float viewport_margin{16};
    float top_offset{100};

    [[nodiscard]] auto values() const {
        return std::array{padding_inline, padding_block,   header_gap,  footer_gap,      button_gap, border_radius,
                          close_size,     close_icon_size, close_inset, viewport_margin, top_offset};
    }

    friend bool operator==(const ModalMetricToken&, const ModalMetricToken&) = default;
};

struct ModalTypographyToken final {
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    float font_size{14};
    float line_height{22};
    float title_font_size{16};
    float title_line_height{24};
    std::uint32_t font_weight{400};
    std::uint32_t title_font_weight{600};

    [[nodiscard]] auto values() const {
        return std::array{font_size, line_height, title_font_size, title_line_height};
    }

    friend bool operator==(const ModalTypographyToken&, const ModalTypographyToken&) = default;
};

struct ModalEffectToken final {
    float focus_width{3};
    float focus_offset{1};
    float focus_radius{8};
    ShadowList shadow;

    [[nodiscard]] auto values() const {
        return std::array{focus_width, focus_offset, focus_radius};
    }

    friend bool operator==(const ModalEffectToken&, const ModalEffectToken&) = default;
};

struct ModalThemeToken final {
    ModalColorToken colors;
    ModalMetricToken metrics;
    ModalTypographyToken typography;
    ModalEffectToken effects;
    std::int32_t z_index_popup{1000};
    friend bool operator==(const ModalThemeToken&, const ModalThemeToken&) = default;
};

struct ModalTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> header_background;
    std::optional<Color> footer_background;
    std::optional<Color> title;
    std::optional<Color> text;
    std::optional<Color> close;
    std::optional<Color> close_hover;
    std::optional<Color> close_hover_background;
    std::optional<Color> mask;
    std::optional<Color> focus_outline;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> padding_block;
    std::optional<LogicalLength> header_gap;
    std::optional<LogicalLength> footer_gap;
    std::optional<LogicalLength> button_gap;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> close_size;
    std::optional<LogicalLength> close_icon_size;
    std::optional<LogicalLength> close_inset;
    std::optional<LogicalLength> viewport_margin;
    std::optional<LogicalLength> top_offset;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> title_font_size;
    std::optional<LogicalLength> title_line_height;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    std::optional<LogicalLength> focus_radius;
    std::optional<SystemFontFamily> font_family;
    std::optional<std::uint32_t> font_weight;
    std::optional<std::uint32_t> title_font_weight;
    std::optional<ShadowList> shadow;
    std::optional<std::int32_t> z_index_popup;
    friend bool operator==(const ModalTokenOverride&, const ModalTokenOverride&) = default;
};

struct ModalThemeConfig final {
    ModalTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const ModalThemeConfig&, const ModalThemeConfig&) = default;
};

struct TagThemeConfig final {
    TagTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const TagThemeConfig&, const TagThemeConfig&) = default;
};

struct BadgeThemeConfig final {
    BadgeTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const BadgeThemeConfig&, const BadgeThemeConfig&) = default;
};

// Drawer shares the strongly typed overlay color, typography and effect values;
// its layout and component override remain independent from Modal.
using DrawerColorToken = ModalColorToken;
using DrawerTypographyToken = ModalTypographyToken;
using DrawerEffectToken = ModalEffectToken;

struct DrawerMetricToken final {
    float padding_inline{24};
    float padding_block{24};
    float header_padding_inline{24};
    float header_padding_block{16};
    float footer_padding_inline{16};
    float footer_padding_block{8};
    float close_size{24};
    float close_icon_size{16};
    float close_gap{8};
    float border_width{1};

    [[nodiscard]] auto values() const {
        return std::array{padding_inline,       padding_block,         header_padding_inline,
                          header_padding_block, footer_padding_inline, footer_padding_block,
                          close_size,           close_icon_size,       close_gap,
                          border_width};
    }

    friend bool operator==(const DrawerMetricToken&, const DrawerMetricToken&) = default;
};

struct DrawerThemeToken final {
    DrawerColorToken colors;
    Color border;
    DrawerMetricToken metrics;
    DrawerTypographyToken typography;
    DrawerEffectToken effects;
    std::int32_t z_index_popup{1000};
    friend bool operator==(const DrawerThemeToken&, const DrawerThemeToken&) = default;
};

struct DrawerTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> header_background;
    std::optional<Color> footer_background;
    std::optional<Color> title;
    std::optional<Color> text;
    std::optional<Color> close;
    std::optional<Color> close_hover;
    std::optional<Color> close_hover_background;
    std::optional<Color> mask;
    std::optional<Color> focus_outline;
    std::optional<Color> border;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> padding_block;
    std::optional<LogicalLength> header_padding_inline;
    std::optional<LogicalLength> header_padding_block;
    std::optional<LogicalLength> footer_padding_inline;
    std::optional<LogicalLength> footer_padding_block;
    std::optional<LogicalLength> close_size;
    std::optional<LogicalLength> close_icon_size;
    std::optional<LogicalLength> close_gap;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> title_font_size;
    std::optional<LogicalLength> title_line_height;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    std::optional<SystemFontFamily> font_family;
    std::optional<std::uint32_t> font_weight;
    std::optional<std::uint32_t> title_font_weight;
    std::optional<ShadowList> shadow;
    std::optional<std::int32_t> z_index_popup;
    friend bool operator==(const DrawerTokenOverride&, const DrawerTokenOverride&) = default;
};

struct DrawerThemeConfig final {
    DrawerTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const DrawerThemeConfig&, const DrawerThemeConfig&) = default;
};

struct PopconfirmThemeToken final {
    Color background;
    Color title;
    Color text;
    Color warning;
    float max_width{360};
    float padding{12};
    float message_gap{8};
    float description_gap{4};
    float button_gap{8};
    float border_radius{8};
    float arrow_size{8};
    float gap{4};
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    std::uint32_t font_weight{400};
    std::uint32_t title_font_weight{600};
    ShadowList shadow;
    std::int32_t z_index_popup{1060};

    [[nodiscard]] std::array<Color, 4> colors() const {
        return {background, title, text, warning};
    }

    [[nodiscard]] std::array<float, 8> metrics() const {
        return {max_width, padding, message_gap, description_gap, button_gap, border_radius, arrow_size, gap};
    }

    friend bool operator==(const PopconfirmThemeToken&, const PopconfirmThemeToken&) = default;
};

struct PopconfirmTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> title;
    std::optional<Color> text;
    std::optional<Color> warning;
    std::optional<LogicalLength> max_width;
    std::optional<LogicalLength> padding;
    std::optional<LogicalLength> message_gap;
    std::optional<LogicalLength> description_gap;
    std::optional<LogicalLength> button_gap;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> arrow_size;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<SystemFontFamily> font_family;
    std::optional<std::uint32_t> font_weight;
    std::optional<std::uint32_t> title_font_weight;
    std::optional<ShadowList> shadow;
    std::optional<std::int32_t> z_index_popup;
    friend bool operator==(const PopconfirmTokenOverride&, const PopconfirmTokenOverride&) = default;
};

struct PopconfirmThemeConfig final {
    PopconfirmTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const PopconfirmThemeConfig&, const PopconfirmThemeConfig&) = default;
};

struct PopoverThemeToken final {
    Color background;
    Color title;
    Color text;
    float max_width{480};
    float title_min_width{177};
    float padding{12};
    float title_gap{8};
    float border_radius{8};
    float arrow_size{8};
    float gap{4};
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    std::uint32_t font_weight{400};
    std::uint32_t title_font_weight{600};
    ShadowList shadow;
    std::int32_t z_index_popup{1030};

    [[nodiscard]] std::array<Color, 3> colors() const {
        return {background, title, text};
    }

    [[nodiscard]] std::array<float, 7> metrics() const {
        return {max_width, title_min_width, padding, title_gap, border_radius, arrow_size, gap};
    }

    friend bool operator==(const PopoverThemeToken&, const PopoverThemeToken&) = default;
};

struct PopoverTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> title;
    std::optional<Color> text;
    std::optional<LogicalLength> max_width;
    std::optional<LogicalLength> title_min_width;
    std::optional<LogicalLength> padding;
    std::optional<LogicalLength> title_gap;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> arrow_size;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<SystemFontFamily> font_family;
    std::optional<std::uint32_t> font_weight;
    std::optional<std::uint32_t> title_font_weight;
    std::optional<ShadowList> shadow;
    std::optional<std::int32_t> z_index_popup;
    friend bool operator==(const PopoverTokenOverride&, const PopoverTokenOverride&) = default;
};

struct PopoverThemeConfig final {
    PopoverTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const PopoverThemeConfig&, const PopoverThemeConfig&) = default;
};

struct AlertThemeConfig final {
    AlertTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const AlertThemeConfig&, const AlertThemeConfig&) = default;
};

struct SpinThemeConfig final {
    SpinTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const SpinThemeConfig&, const SpinThemeConfig&) = default;
};

struct ProgressThemeConfig final {
    ProgressTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const ProgressThemeConfig&, const ProgressThemeConfig&) = default;
};

struct InputNumberThemeToken final {
    float control_width{90};
    float handle_width{22};
    float handle_font_size{7};
    Color handle_background;
    Color handle_active_background;
    Color handle_hover_color;
    Color handle_border_color;
    Color filled_handle_background;
    Color handle_color;
    Color disabled_color;

    [[nodiscard]] auto colors() const noexcept {
        return std::array{handle_background,   handle_active_background, handle_hover_color,
                          handle_border_color, filled_handle_background, handle_color,
                          disabled_color};
    }

    [[nodiscard]] auto metrics() const noexcept {
        return std::array{control_width, handle_width, handle_font_size};
    }

    friend bool operator==(const InputNumberThemeToken&, const InputNumberThemeToken&) = default;
};

struct InputNumberTokenOverride final {
    std::optional<LogicalLength> control_width;
    std::optional<LogicalLength> handle_width;
    std::optional<LogicalLength> handle_font_size;
    std::optional<Color> handle_background;
    std::optional<Color> handle_active_background;
    std::optional<Color> handle_hover_color;
    std::optional<Color> handle_border_color;
    std::optional<Color> filled_handle_background;
    std::optional<Color> handle_color;
    std::optional<Color> disabled_color;
    friend bool operator==(const InputNumberTokenOverride&, const InputNumberTokenOverride&) = default;
};

struct InputNumberThemeConfig final {
    InputNumberTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const InputNumberThemeConfig&, const InputNumberThemeConfig&) = default;
};

struct PaginationColorToken final {
    Color item_background;
    Color active_background;
    Color item;
    Color active;
    Color disabled;
    Color border;

    auto values() const {
        return std::array{item_background, active_background, item, active, disabled, border};
    }

    friend bool operator==(const PaginationColorToken&, const PaginationColorToken&) = default;
};

struct PaginationMetricToken final {
    float item_size{32};
    float item_size_small{24};
    float item_size_large{40};
    float gap{8};
    float border_radius{6};

    auto values() const {
        return std::array{item_size, item_size_small, item_size_large, gap, border_radius};
    }

    friend bool operator==(const PaginationMetricToken&, const PaginationMetricToken&) = default;
};

struct PaginationThemeToken final {
    PaginationColorToken colors;
    PaginationMetricToken metrics;
    friend bool operator==(const PaginationThemeToken&, const PaginationThemeToken&) = default;
};

struct PaginationTokenOverride final {
    std::optional<Color> item_background;
    std::optional<Color> active_background;
    std::optional<Color> item;
    std::optional<Color> active;
    std::optional<Color> disabled;
    std::optional<Color> border;
    std::optional<LogicalLength> item_size;
    std::optional<LogicalLength> item_size_small;
    std::optional<LogicalLength> item_size_large;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> border_radius;
    friend bool operator==(const PaginationTokenOverride&, const PaginationTokenOverride&) = default;
};

struct PaginationThemeConfig final {
    PaginationTokenOverride tokens;
    friend bool operator==(const PaginationThemeConfig&, const PaginationThemeConfig&) = default;
};

struct TableColorToken final {
    Color background;
    Color header_background;
    Color text;
    Color header_text;
    Color border;
    Color hover;
    Color selected;
    Color selected_hover;

    auto values() const {
        return std::array{background, header_background, text, header_text, border, hover, selected, selected_hover};
    }

    friend bool operator==(const TableColorToken&, const TableColorToken&) = default;
};

struct TableMetricToken final {
    float padding_block{16};
    float padding_block_middle{12};
    float padding_block_small{8};
    float padding_inline{16};
    float padding_inline_small{8};
    float font_size{14};
    float line_height{22};
    float min_column_width{80};
    float selection_width{32};
    float empty_height{96};
    float border_width{1};

    auto values() const {
        return std::array{
            padding_block, padding_block_middle, padding_block_small, padding_inline, padding_inline_small, font_size,
            line_height,   min_column_width,     selection_width,     empty_height,   border_width};
    }

    friend bool operator==(const TableMetricToken&, const TableMetricToken&) = default;
};

struct TableThemeToken final {
    TableColorToken colors;
    TableMetricToken metrics;
    friend bool operator==(const TableThemeToken&, const TableThemeToken&) = default;
};

struct TableTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> header_background;
    std::optional<Color> text;
    std::optional<Color> header_text;
    std::optional<Color> border;
    std::optional<Color> hover;
    std::optional<Color> selected;
    std::optional<Color> selected_hover;
    std::optional<LogicalLength> padding_block;
    std::optional<LogicalLength> padding_block_middle;
    std::optional<LogicalLength> padding_block_small;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> padding_inline_small;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> min_column_width;
    std::optional<LogicalLength> selection_width;
    std::optional<LogicalLength> empty_height;
    std::optional<LogicalLength> border_width;
    friend bool operator==(const TableTokenOverride&, const TableTokenOverride&) = default;
};

struct TableThemeConfig final {
    TableTokenOverride tokens;
    friend bool operator==(const TableThemeConfig&, const TableThemeConfig&) = default;
};

struct EmptyThemeConfig final {
    EmptyTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const EmptyThemeConfig&, const EmptyThemeConfig&) = default;
};

struct ResultThemeConfig final {
    ResultTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const ResultThemeConfig&, const ResultThemeConfig&) = default;
};

struct DatePickerThemeConfig final {
    DatePickerTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const DatePickerThemeConfig&, const DatePickerThemeConfig&) = default;
};

struct BreadcrumbThemeConfig final {
    BreadcrumbTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const BreadcrumbThemeConfig&, const BreadcrumbThemeConfig&) = default;
};

struct StepsThemeConfig final {
    StepsTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const StepsThemeConfig&, const StepsThemeConfig&) = default;
};

struct TimePickerThemeConfig final {
    TimePickerTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const TimePickerThemeConfig&, const TimePickerThemeConfig&) = default;
};

struct SkeletonThemeConfig final {
    SkeletonTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const SkeletonThemeConfig&, const SkeletonThemeConfig&) = default;
};

struct TimelineThemeConfig final {
    TimelineTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const TimelineThemeConfig&, const TimelineThemeConfig&) = default;
};

struct AvatarThemeConfig final {
    AvatarTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const AvatarThemeConfig&, const AvatarThemeConfig&) = default;
};

struct RateThemeConfig final {
    RateTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const RateThemeConfig&, const RateThemeConfig&) = default;
};

struct StatisticThemeConfig final {
    StatisticTokenOverride tokens;
    SeedTokenOverride seed;
    bool algorithm{};
    friend bool operator==(const StatisticThemeConfig&, const StatisticThemeConfig&) = default;
};

struct ThemeConfig final {
    StepsThemeConfig steps;
    BreadcrumbThemeConfig breadcrumb;
    DatePickerThemeConfig date_picker;
    SeedTokenOverride seed;
    AliasTokenOverride alias;
    ButtonThemeConfig button;
    TextThemeConfig text;
    InputThemeConfig input;
    InputNumberThemeConfig input_number;
    SwitchThemeConfig switch_;
    CheckboxThemeConfig checkbox;
    RadioThemeConfig radio;
    TypographyThemeConfig typography;
    DividerThemeConfig divider;
    SliderThemeConfig slider;
    TooltipThemeConfig tooltip;
    SelectThemeConfig select;
    CardThemeConfig card;
    TabsThemeConfig tabs;
    SegmentedThemeConfig segmented;
    FormThemeConfig form;
    TimePickerThemeConfig time_picker;
    DescriptionsThemeConfig descriptions;
    NoticeThemeConfig message;
    NoticeThemeConfig notification;
    ModalThemeConfig modal;
    TagThemeConfig tag;
    BadgeThemeConfig badge;
    DrawerThemeConfig drawer;
    PopconfirmThemeConfig popconfirm;
    PopoverThemeConfig popover;
    AlertThemeConfig alert;
    SpinThemeConfig spin;
    ProgressThemeConfig progress;
    MenuThemeConfig menu;
    DropdownThemeConfig dropdown;
    PaginationThemeConfig pagination;
    TableThemeConfig table;
    EmptyThemeConfig empty;
    ResultThemeConfig result;
    CollapseThemeConfig collapse;
    SkeletonThemeConfig skeleton;
    TimelineThemeConfig timeline;
    AvatarThemeConfig avatar;
    RateThemeConfig rate;
    StatisticThemeConfig statistic;
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
    float opacity_loading{0.65F};

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
    float wave_spread{6.0F};
    float wave_width{6.0F};
    float wave_opacity{0.2F};

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
    float inner_min_margin{};
    float inner_max_margin{};
    float inner_min_margin_small{};
    float inner_max_margin_small{};
    ShadowList handle_shadow;
    float content_font_size{12};
    float loading_opacity{0.65F};
    float wave_spread{6};
    float wave_width{2};
    float wave_opacity{0.2F};
    Color checked_background;
    Color checked_hover_background;
    Color unchecked_background;
    Color unchecked_hover_background;
    Color focus_color;
    float focus_width{3};
    float focus_offset{1};

    [[nodiscard]] constexpr std::array<Color, 5> colors() const {
        return {checked_background, checked_hover_background, unchecked_background, unchecked_hover_background,
                focus_color};
    }

    friend constexpr bool operator==(const SwitchThemeToken&, const SwitchThemeToken&) = default;
};

struct CheckboxThemeToken final {
    float size{};
    float indeterminate_size{};
    float line_width{};
    float check_width{};
    float border_radius{};
    float label_gap{};
    float font_size{};
    float line_height{};
    float focus_width{};
    float focus_offset{};
    float wave_spread{6};
    float wave_width{2};
    float wave_opacity{0.2F};
    Color primary;
    Color primary_hover;
    Color primary_active;
    Color background;
    Color border;
    Color disabled_background;
    Color disabled_foreground;
    Color checkmark;
    Color label;
    Color focus;

    [[nodiscard]] constexpr std::array<float, 8> metrics() const {
        return {size, indeterminate_size, line_width, check_width, border_radius, label_gap, font_size, line_height};
    }

    [[nodiscard]] constexpr std::array<float, 5> effects() const {
        return {focus_width, focus_offset, wave_spread, wave_width, wave_opacity};
    }

    [[nodiscard]] constexpr std::array<Color, 10> colors() const {
        return {primary,   primary_hover, primary_active, background, border, disabled_background, disabled_foreground,
                checkmark, label,         focus};
    }

    friend constexpr bool operator==(const CheckboxThemeToken&, const CheckboxThemeToken&) = default;
};

struct RadioThemeToken final {
    float size{};
    float dot_size{};
    float line_width{};
    float label_gap{};
    float wrapper_margin_inline_end{};
    float font_size{};
    float line_height{};
    float button_height{};
    float button_height_small{};
    float button_height_large{};
    float button_padding_inline{};
    float button_padding_inline_small{};
    float button_font_size_large{};
    float button_radius{};
    float button_radius_small{};
    float button_radius_large{};
    float focus_width{};
    float focus_offset{};
    float wave_spread{};
    float wave_width{};
    float wave_opacity{};
    Color primary;
    Color primary_hover;
    Color primary_active;
    Color background;
    Color border;
    Color disabled_background;
    Color disabled_foreground;
    Color dot;
    Color dot_disabled;
    Color checked_background;
    Color label;
    Color focus;
    Color button_background;
    Color button_checked_background;
    Color button_color;
    Color button_checked_background_disabled;
    Color button_checked_color_disabled;
    Color button_solid_checked_color;
    Color button_solid_checked_background;
    Color button_solid_checked_hover_background;
    Color button_solid_checked_active_background;

    [[nodiscard]] constexpr std::array<float, 16> metrics() const {
        return {size,
                dot_size,
                line_width,
                label_gap,
                wrapper_margin_inline_end,
                font_size,
                line_height,
                button_height,
                button_height_small,
                button_height_large,
                button_padding_inline,
                button_padding_inline_small,
                button_font_size_large,
                button_radius,
                button_radius_small,
                button_radius_large};
    }

    [[nodiscard]] constexpr std::array<float, 5> effects() const {
        return {focus_width, focus_offset, wave_spread, wave_width, wave_opacity};
    }

    [[nodiscard]] constexpr std::array<Color, 21> colors() const {
        return {primary,
                primary_hover,
                primary_active,
                background,
                border,
                disabled_background,
                disabled_foreground,
                dot,
                dot_disabled,
                checked_background,
                label,
                focus,
                button_background,
                button_checked_background,
                button_color,
                button_checked_background_disabled,
                button_checked_color_disabled,
                button_solid_checked_color,
                button_solid_checked_background,
                button_solid_checked_hover_background,
                button_solid_checked_active_background};
    }

    friend constexpr bool operator==(const RadioThemeToken&, const RadioThemeToken&) = default;
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
    float small_horizontal_margin{};
    float middle_horizontal_margin{};

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
    [[nodiscard]] const CheckboxThemeToken& checkbox() const noexcept;
    [[nodiscard]] const RadioThemeToken& radio() const noexcept;
    [[nodiscard]] const TypographyThemeToken& typography() const noexcept;
    [[nodiscard]] const DividerThemeToken& divider() const noexcept;
    [[nodiscard]] const SliderThemeToken& slider() const noexcept;
    [[nodiscard]] const TooltipThemeToken& tooltip() const noexcept;
    [[nodiscard]] const SelectThemeToken& select() const noexcept;

    [[nodiscard]] const TimePickerThemeToken& time_picker() const noexcept {
        return *time_picker_;
    }

    [[nodiscard]] const CardThemeToken& card() const noexcept;
    [[nodiscard]] const TabsThemeToken& tabs() const noexcept;

    [[nodiscard]] const CollapseThemeToken& collapse() const noexcept {
        return collapse_;
    }

    [[nodiscard]] const SegmentedThemeToken& segmented() const noexcept {
        return segmented_;
    }

    [[nodiscard]] const BreadcrumbThemeToken& breadcrumb() const noexcept {
        return breadcrumb_;
    }

    [[nodiscard]] const StepsThemeToken& steps() const noexcept {
        return steps_;
    }

    [[nodiscard]] const DatePickerThemeToken& date_picker() const noexcept {
        return date_picker_;
    }

    [[nodiscard]] const FormThemeToken& form() const noexcept {
        return form_;
    }

    [[nodiscard]] const DescriptionsThemeToken& descriptions() const noexcept {
        return descriptions_;
    }

    [[nodiscard]] const MessageThemeToken& message() const noexcept {
        return *message_;
    }

    [[nodiscard]] const NotificationThemeToken& notification() const noexcept {
        return *notification_;
    }

    [[nodiscard]] const ModalThemeToken& modal() const noexcept;

    [[nodiscard]] const TagThemeToken& tag() const noexcept {
        return tag_;
    }

    [[nodiscard]] const InputNumberThemeToken& input_number() const noexcept {
        return input_number_;
    }

    [[nodiscard]] const BadgeThemeToken& badge() const noexcept {
        return badge_;
    }

    [[nodiscard]] const DrawerThemeToken& drawer() const noexcept;
    [[nodiscard]] const PopconfirmThemeToken& popconfirm() const noexcept;
    [[nodiscard]] const PopoverThemeToken& popover() const noexcept;

    [[nodiscard]] const AlertThemeToken& alert() const noexcept {
        return alert_;
    }

    [[nodiscard]] const SpinThemeToken& spin() const noexcept {
        return spin_;
    }

    [[nodiscard]] const ProgressThemeToken& progress() const noexcept {
        return progress_;
    }

    [[nodiscard]] const MenuThemeToken& menu() const noexcept;
    [[nodiscard]] const DropdownThemeToken& dropdown() const noexcept;
    [[nodiscard]] const PaginationThemeToken& pagination() const noexcept;
    [[nodiscard]] const TableThemeToken& table() const noexcept;

    [[nodiscard]] const EmptyThemeToken& empty() const noexcept {
        return empty_;
    }

    [[nodiscard]] const ResultThemeToken& result() const noexcept {
        return result_;
    }

    [[nodiscard]] std::span<const ThemeAlgorithm> algorithms() const noexcept;
    [[nodiscard]] std::string_view source_version() const noexcept;
    [[nodiscard]] std::string_view source_commit() const noexcept;
    [[nodiscard]] std::uint64_t identity() const noexcept;
    [[nodiscard]] const std::string& diagnostic_json() const noexcept;

    friend bool operator==(const ThemeSnapshot&, const ThemeSnapshot&);

    [[nodiscard]] const SkeletonThemeToken& skeleton() const noexcept {
        return *skeleton_;
    }

    [[nodiscard]] const TimelineThemeToken& timeline() const noexcept {
        return *timeline_;
    }

    [[nodiscard]] const AvatarThemeToken& avatar() const noexcept {
        return *avatar_;
    }

    [[nodiscard]] const RateThemeToken& rate() const noexcept {
        return *rate_;
    }

    [[nodiscard]] const StatisticThemeToken& statistic() const noexcept {
        return *statistic_;
    }

private:
    friend struct detail::InputTokenAccess;
    friend ThemeSnapshot resolve_theme(const ThemeConfig&, const ThemeSnapshot*);

    ThemeSnapshot(AntDesignDefaultSeed seed, ThemeMapToken map, ThemeAliasToken alias, ButtonThemeToken button,
                  TextThemeToken text, SwitchThemeToken switch_token, CheckboxThemeToken checkbox,
                  RadioThemeToken radio, TypographyThemeToken typography, DividerThemeToken divider,
                  SliderThemeToken slider, TooltipThemeToken tooltip, SelectThemeToken select, CardThemeToken card,
                  TabsThemeToken tabs, ModalThemeToken modal, DrawerThemeToken drawer, PopconfirmThemeToken popconfirm,
                  MenuThemeToken menu, DropdownThemeToken dropdown, std::shared_ptr<const detail::InputTokenSet> input,
                  std::vector<ThemeAlgorithm> algorithms, TagThemeToken tag, BadgeThemeToken badge,
                  AlertThemeToken alert, SpinThemeToken spin, ProgressThemeToken progress,
                  InputNumberThemeToken input_number, PaginationThemeToken pagination, TableThemeToken table,
                  CollapseThemeToken collapse, SegmentedThemeToken segmented, EmptyThemeToken empty,
                  ResultThemeToken result, DatePickerThemeToken date_picker, FormThemeToken form,
                  BreadcrumbThemeToken breadcrumb, StepsThemeToken steps,
                  std::shared_ptr<const TimePickerThemeToken> time_picker,
                  std::shared_ptr<const SkeletonThemeToken> skeleton, DescriptionsThemeToken descriptions,
                  std::shared_ptr<const PopoverThemeToken> popover, std::shared_ptr<const NoticeThemeToken> message,
                  std::shared_ptr<const NoticeThemeToken> notification,
                  std::shared_ptr<const TimelineThemeToken> timeline, std::shared_ptr<const AvatarThemeToken> avatar,
                  std::shared_ptr<const RateThemeToken> rate, std::shared_ptr<const StatisticThemeToken> statistic);

    AntDesignDefaultSeed seed_;
    ThemeMapToken map_;
    ThemeAliasToken alias_;
    ButtonThemeToken button_;
    TextThemeToken text_;
    SwitchThemeToken switch_token_;
    CheckboxThemeToken checkbox_;
    RadioThemeToken radio_;
    TypographyThemeToken typography_;
    DividerThemeToken divider_;
    SliderThemeToken slider_;
    TooltipThemeToken tooltip_;
    SelectThemeToken select_;
    CardThemeToken card_;
    TabsThemeToken tabs_;
    SegmentedThemeToken segmented_;
    DatePickerThemeToken date_picker_;
    FormThemeToken form_;
    BreadcrumbThemeToken breadcrumb_;
    StepsThemeToken steps_;
    std::shared_ptr<const TimePickerThemeToken> time_picker_;
    std::shared_ptr<const SkeletonThemeToken> skeleton_;
    std::shared_ptr<const TimelineThemeToken> timeline_;
    std::shared_ptr<const AvatarThemeToken> avatar_;
    std::shared_ptr<const RateThemeToken> rate_;
    std::shared_ptr<const StatisticThemeToken> statistic_;
    DescriptionsThemeToken descriptions_;
    std::shared_ptr<const PopoverThemeToken> popover_;
    std::shared_ptr<const NoticeThemeToken> message_;
    std::shared_ptr<const NoticeThemeToken> notification_;
    ModalThemeToken modal_;
    TagThemeToken tag_;
    BadgeThemeToken badge_;
    DrawerThemeToken drawer_;
    PopconfirmThemeToken popconfirm_;
    AlertThemeToken alert_;
    SpinThemeToken spin_;
    ProgressThemeToken progress_;
    MenuThemeToken menu_;
    DropdownThemeToken dropdown_;
    InputNumberThemeToken input_number_;
    PaginationThemeToken pagination_;
    TableThemeToken table_;
    CollapseThemeToken collapse_;
    EmptyThemeToken empty_;
    ResultThemeToken result_;
    std::shared_ptr<const detail::InputTokenSet> input_;
    std::vector<ThemeAlgorithm> algorithms_;
    std::uint64_t identity_{};
    std::string diagnostic_json_;
};

namespace detail {
[[nodiscard]] const ThemeConfig& default_theme_config();
}

[[nodiscard]] ThemeSnapshot resolve_theme(const ThemeConfig& config = detail::default_theme_config(),
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
