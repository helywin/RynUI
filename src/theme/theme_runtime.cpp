#include "theme/theme_runtime.hpp"
#include "theme/input_tokens.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace ryn::theme_runtime {

Color ThemeScope::color_link_hover() const {
    ensure_owner_thread();
    record(TokenIdentity::map_color_link_hover);
    return snapshot_->map().color_link_hover;
}

Color ThemeScope::color_link_active() const {
    ensure_owner_thread();
    record(TokenIdentity::map_color_link_active);
    return snapshot_->map().color_link_active;
}

namespace {

constexpr std::size_t identity_count = static_cast<std::size_t>(TokenIdentity::count);

struct ReadCapture final {
    const ThemeScope* scope{};
    std::array<TokenIdentity, identity_count> identities{};
    std::size_t count{};
    ReadCapture* previous{};
};

thread_local ReadCapture* active_capture = nullptr;

class ReadCaptureGuard final {
public:
    explicit ReadCaptureGuard(ReadCapture& capture) noexcept : capture_(&capture) {
        capture.previous = active_capture;
        active_capture = &capture;
    }

    ~ReadCaptureGuard() {
        active_capture = capture_->previous;
    }

private:
    ReadCapture* capture_;
};

template <typename Value>
void append_if_changed(const Value& before, const Value& after, TokenIdentity identity,
                       std::array<TokenIdentity, identity_count>& changed, std::size_t& count) {
    if (before != after) {
        changed[count++] = identity;
    }
}

std::size_t collect_changed(const ThemeSnapshot& before, const ThemeSnapshot& after,
                            std::array<TokenIdentity, identity_count>& changed) {
    std::size_t count = 0;
    const auto& before_alias = before.alias();
    const auto& after_alias = after.alias();
    append_if_changed(before_alias.opacity_loading, after_alias.opacity_loading, TokenIdentity::alias_opacity_loading,
                      changed, count);
    append_if_changed(before_alias.color_text, after_alias.color_text, TokenIdentity::alias_color_text, changed, count);
    append_if_changed(before_alias.color_text_secondary, after_alias.color_text_secondary,
                      TokenIdentity::alias_color_text_secondary, changed, count);
    append_if_changed(before_alias.color_text_disabled, after_alias.color_text_disabled,
                      TokenIdentity::alias_color_text_disabled, changed, count);
    append_if_changed(before_alias.color_background_container, after_alias.color_background_container,
                      TokenIdentity::alias_color_background_container, changed, count);
    append_if_changed(before_alias.color_background_elevated, after_alias.color_background_elevated,
                      TokenIdentity::alias_color_background_elevated, changed, count);
    append_if_changed(before_alias.color_background_container_disabled, after_alias.color_background_container_disabled,
                      TokenIdentity::alias_color_background_container_disabled, changed, count);
    append_if_changed(before_alias.color_border, after_alias.color_border, TokenIdentity::alias_color_border, changed,
                      count);
    append_if_changed(before_alias.color_border_secondary, after_alias.color_border_secondary,
                      TokenIdentity::alias_color_border_secondary, changed, count);
    append_if_changed(before_alias.color_split, after_alias.color_split, TokenIdentity::alias_color_split, changed,
                      count);
    append_if_changed(before_alias.color_focus_outline, after_alias.color_focus_outline,
                      TokenIdentity::alias_color_focus_outline, changed, count);
    append_if_changed(before_alias.line_width_focus, after_alias.line_width_focus,
                      TokenIdentity::alias_line_width_focus, changed, count);
    append_if_changed(before_alias.focus_outline_offset, after_alias.focus_outline_offset,
                      TokenIdentity::alias_focus_outline_offset, changed, count);
    append_if_changed(before_alias.box_shadow, after_alias.box_shadow, TokenIdentity::alias_box_shadow, changed, count);
    append_if_changed(before_alias.box_shadow_secondary, after_alias.box_shadow_secondary,
                      TokenIdentity::alias_box_shadow_secondary, changed, count);
    append_if_changed(before_alias.box_shadow_tertiary, after_alias.box_shadow_tertiary,
                      TokenIdentity::alias_box_shadow_tertiary, changed, count);

    const auto& before_map = before.map();
    const auto& after_map = after.map();
    append_if_changed(before_map.color_primary, after_map.color_primary, TokenIdentity::map_color_primary, changed,
                      count);
    append_if_changed(before_map.color_primary_hover, after_map.color_primary_hover,
                      TokenIdentity::map_color_primary_hover, changed, count);
    append_if_changed(before_map.color_primary_active, after_map.color_primary_active,
                      TokenIdentity::map_color_primary_active, changed, count);
    append_if_changed(before_map.color_primary_border, after_map.color_primary_border,
                      TokenIdentity::map_color_primary_border, changed, count);
    append_if_changed(before_map.color_success, after_map.color_success, TokenIdentity::map_color_success, changed,
                      count);
    append_if_changed(before_map.color_warning, after_map.color_warning, TokenIdentity::map_color_warning, changed,
                      count);
    append_if_changed(before_map.color_error, after_map.color_error, TokenIdentity::map_color_error, changed, count);
    append_if_changed(before_map.color_error_hover, after_map.color_error_hover, TokenIdentity::map_color_error_hover,
                      changed, count);
    append_if_changed(before_map.color_error_active, after_map.color_error_active,
                      TokenIdentity::map_color_error_active, changed, count);
    append_if_changed(before_map.color_info, after_map.color_info, TokenIdentity::map_color_info, changed, count);
    append_if_changed(before_map.color_success_text, after_map.color_success_text,
                      TokenIdentity::map_color_success_text, changed, count);
    append_if_changed(before_map.color_warning_text, after_map.color_warning_text,
                      TokenIdentity::map_color_warning_text, changed, count);
    append_if_changed(before_map.color_error_text, after_map.color_error_text, TokenIdentity::map_color_error_text,
                      changed, count);
    append_if_changed(before_map.color_link, after_map.color_link, TokenIdentity::map_color_link, changed, count);
    append_if_changed(before_map.color_link_hover, after_map.color_link_hover, TokenIdentity::map_color_link_hover,
                      changed, count);
    append_if_changed(before_map.color_link_active, after_map.color_link_active, TokenIdentity::map_color_link_active,
                      changed, count);
    append_if_changed(before_map.color_text_base, after_map.color_text_base, TokenIdentity::map_color_text_base,
                      changed, count);
    append_if_changed(before_map.color_background_base, after_map.color_background_base,
                      TokenIdentity::map_color_background_base, changed, count);
    append_if_changed(before_map.font_size_small, after_map.font_size_small, TokenIdentity::map_font_size_small,
                      changed, count);
    append_if_changed(before_map.font_size, after_map.font_size, TokenIdentity::map_font_size, changed, count);
    append_if_changed(before_map.font_size_large, after_map.font_size_large, TokenIdentity::map_font_size_large,
                      changed, count);
    append_if_changed(before_map.line_height_small, after_map.line_height_small, TokenIdentity::map_line_height_small,
                      changed, count);
    append_if_changed(before_map.line_height, after_map.line_height, TokenIdentity::map_line_height, changed, count);
    append_if_changed(before_map.line_height_large, after_map.line_height_large, TokenIdentity::map_line_height_large,
                      changed, count);
    append_if_changed(before_map.size_xs, after_map.size_xs, TokenIdentity::map_size_xs, changed, count);
    append_if_changed(before_map.size_small, after_map.size_small, TokenIdentity::map_size_small, changed, count);
    append_if_changed(before_map.size, after_map.size, TokenIdentity::map_size, changed, count);
    append_if_changed(before_map.size_large, after_map.size_large, TokenIdentity::map_size_large, changed, count);
    append_if_changed(before_map.control_height_small, after_map.control_height_small,
                      TokenIdentity::map_control_height_small, changed, count);
    append_if_changed(before_map.control_height, after_map.control_height, TokenIdentity::map_control_height, changed,
                      count);
    append_if_changed(before_map.control_height_large, after_map.control_height_large,
                      TokenIdentity::map_control_height_large, changed, count);
    append_if_changed(before_map.border_radius_small, after_map.border_radius_small,
                      TokenIdentity::map_border_radius_small, changed, count);
    append_if_changed(before_map.border_radius, after_map.border_radius, TokenIdentity::map_border_radius, changed,
                      count);
    append_if_changed(before_map.border_radius_large, after_map.border_radius_large,
                      TokenIdentity::map_border_radius_large, changed, count);
    append_if_changed(before_map.motion_unit, after_map.motion_unit, TokenIdentity::map_motion_unit, changed, count);
    append_if_changed(before_map.motion_base, after_map.motion_base, TokenIdentity::map_motion_base, changed, count);
    append_if_changed(before_map.motion, after_map.motion, TokenIdentity::map_motion_enabled, changed, count);

    const auto& before_button = before.button();
    const auto& after_button = after.button();
    bool variant_colors_changed = before_button.variants.values() != after_button.variants.values();
    bool variant_shadows_changed{};
    for (std::size_t index = 0; index < button_color_count; ++index) {
        variant_colors_changed = variant_colors_changed || before_button.variants.colors[index].values() !=
                                                               after_button.variants.colors[index].values();
        variant_shadows_changed = variant_shadows_changed || before_button.variants.colors[index].shadow !=
                                                                 after_button.variants.colors[index].shadow;
    }
    const bool button_colors_changed =
        before_button.default_color != after_button.default_color ||
        before_button.default_background != after_button.default_background ||
        before_button.default_border_color != after_button.default_border_color ||
        before_button.default_hover_color != after_button.default_hover_color ||
        before_button.default_active_color != after_button.default_active_color ||
        before_button.text_color != after_button.text_color ||
        before_button.text_background != after_button.text_background ||
        before_button.text_hover_color != after_button.text_hover_color ||
        before_button.text_active_color != after_button.text_active_color ||
        before_button.text_hover_background != after_button.text_hover_background ||
        before_button.text_active_background != after_button.text_active_background ||
        before_button.primary_color != after_button.primary_color ||
        before_button.primary_background != after_button.primary_background ||
        before_button.primary_hover_background != after_button.primary_hover_background ||
        before_button.primary_active_background != after_button.primary_active_background ||
        before_button.danger_color != after_button.danger_color ||
        before_button.danger_background != after_button.danger_background ||
        before_button.danger_hover_background != after_button.danger_hover_background ||
        before_button.danger_active_background != after_button.danger_active_background ||
        before_button.disabled_color != after_button.disabled_color ||
        before_button.disabled_background != after_button.disabled_background ||
        before_button.disabled_border_color != after_button.disabled_border_color ||
        before_button.wave_opacity != after_button.wave_opacity || variant_colors_changed;
    append_if_changed(false, button_colors_changed, TokenIdentity::button_colors, changed, count);
    append_if_changed(
        std::array{before_button.control_height_small, before_button.control_height,
                   before_button.control_height_large},
        std::array{after_button.control_height_small, after_button.control_height, after_button.control_height_large},
        TokenIdentity::button_control_heights, changed, count);
    append_if_changed(
        std::array{before_button.padding_inline_small, before_button.padding_inline,
                   before_button.padding_inline_large},
        std::array{after_button.padding_inline_small, after_button.padding_inline, after_button.padding_inline_large},
        TokenIdentity::button_padding_inline, changed, count);
    append_if_changed(std::array{before_button.content_font_size_small, before_button.content_font_size,
                                 before_button.content_font_size_large, before_button.content_line_height_small,
                                 before_button.content_line_height, before_button.content_line_height_large,
                                 before_button.loading_indicator_size, before_button.loading_opacity},
                      std::array{after_button.content_font_size_small, after_button.content_font_size,
                                 after_button.content_font_size_large, after_button.content_line_height_small,
                                 after_button.content_line_height, after_button.content_line_height_large,
                                 after_button.loading_indicator_size, after_button.loading_opacity},
                      TokenIdentity::button_typography, changed, count);
    append_if_changed(
        std::array{before_button.border_radius_small, before_button.border_radius, before_button.border_radius_large},
        std::array{after_button.border_radius_small, after_button.border_radius, after_button.border_radius_large},
        TokenIdentity::button_border_radius, changed, count);
    append_if_changed(std::array{before_button.border_width, before_button.dash_length, before_button.dash_gap,
                                 before_button.wave_spread, before_button.wave_width},
                      std::array{after_button.border_width, after_button.dash_length, after_button.dash_gap,
                                 after_button.wave_spread, after_button.wave_width},
                      TokenIdentity::button_border_width, changed, count);
    append_if_changed(before_button.icon_gap, after_button.icon_gap, TokenIdentity::button_icon_gap, changed, count);
    const bool button_shadows_changed = before_button.default_shadow != after_button.default_shadow ||
                                        before_button.primary_shadow != after_button.primary_shadow ||
                                        before_button.danger_shadow != after_button.danger_shadow ||
                                        variant_shadows_changed;
    append_if_changed(false, button_shadows_changed, TokenIdentity::button_shadows, changed, count);

    const auto& before_text = before.text();
    const auto& after_text = after.text();
    append_if_changed(before_text.color, after_text.color, TokenIdentity::text_color, changed, count);
    append_if_changed(before_text.font_family, after_text.font_family, TokenIdentity::text_font_family, changed, count);
    append_if_changed(before_text.font_weight, after_text.font_weight, TokenIdentity::text_font_weight, changed, count);
    append_if_changed(before_text.font_size, after_text.font_size, TokenIdentity::text_font_size, changed, count);
    append_if_changed(before_text.line_height, after_text.line_height, TokenIdentity::text_line_height, changed, count);
    append_if_changed(before.seed().line_width, after.seed().line_width, TokenIdentity::seed_line_width, changed,
                      count);
    const auto& old_input = detail::InputTokenAccess::get(before);
    const auto& new_input = detail::InputTokenAccess::get(after);
    bool input_layout =
        old_input.border_width != new_input.border_width || old_input.affix_padding != new_input.affix_padding;
    bool input_typography{};
    bool input_radius{};
    for (std::size_t i = 0; i < old_input.sizes.size(); ++i) {
        const auto& old_size = old_input.sizes[i];
        const auto& new_size = new_input.sizes[i];
        input_layout = input_layout || old_size.control_height != new_size.control_height ||
                       old_size.padding_inline != new_size.padding_inline ||
                       old_size.padding_block != new_size.padding_block;
        input_typography = input_typography || old_size.font_size != new_size.font_size ||
                           old_size.line_height != new_size.line_height;
        input_radius = input_radius || old_size.border_radius != new_size.border_radius;
    }
    append_if_changed(false, input_layout, TokenIdentity::input_layout_metrics, changed, count);
    append_if_changed(false, input_typography, TokenIdentity::input_typography, changed, count);
    input_radius = input_radius || old_input.focus_width != new_input.focus_width;
    append_if_changed(false, input_radius, TokenIdentity::input_border_radius, changed, count);
    append_if_changed(old_input.colors, new_input.colors, TokenIdentity::input_colors, changed, count);
    const bool input_shadows = old_input.active_shadow != new_input.active_shadow ||
                               old_input.error_active_shadow != new_input.error_active_shadow ||
                               old_input.warning_active_shadow != new_input.warning_active_shadow;
    append_if_changed(false, input_shadows, TokenIdentity::input_shadows, changed, count);
    const auto& old_switch = before.switch_token();
    const auto& new_switch = after.switch_token();
    append_if_changed(
        std::array{old_switch.track_height, old_switch.track_height_small, old_switch.track_min_width,
                   old_switch.track_min_width_small, old_switch.track_padding, old_switch.handle_size,
                   old_switch.handle_size_small, old_switch.inner_min_margin, old_switch.inner_max_margin,
                   old_switch.inner_min_margin_small, old_switch.inner_max_margin_small, old_switch.content_font_size},
        std::array{new_switch.track_height, new_switch.track_height_small, new_switch.track_min_width,
                   new_switch.track_min_width_small, new_switch.track_padding, new_switch.handle_size,
                   new_switch.handle_size_small, new_switch.inner_min_margin, new_switch.inner_max_margin,
                   new_switch.inner_min_margin_small, new_switch.inner_max_margin_small, new_switch.content_font_size},
        TokenIdentity::switch_geometry, changed, count);
    const bool switch_colors_changed = old_switch.colors() != new_switch.colors() ||
                                       old_switch.handle_background != new_switch.handle_background ||
                                       old_switch.loading_opacity != new_switch.loading_opacity;
    append_if_changed(false, switch_colors_changed, TokenIdentity::switch_colors, changed, count);
    const bool switch_effects_changed =
        old_switch.handle_shadow != new_switch.handle_shadow || old_switch.wave_spread != new_switch.wave_spread ||
        old_switch.wave_width != new_switch.wave_width || old_switch.wave_opacity != new_switch.wave_opacity ||
        old_switch.focus_width != new_switch.focus_width || old_switch.focus_offset != new_switch.focus_offset;
    append_if_changed(false, switch_effects_changed, TokenIdentity::switch_effects, changed, count);
    append_if_changed(before.checkbox().metrics(), after.checkbox().metrics(), TokenIdentity::checkbox_metrics, changed,
                      count);
    append_if_changed(before.checkbox().colors(), after.checkbox().colors(), TokenIdentity::checkbox_colors, changed,
                      count);
    append_if_changed(before.checkbox().effects(), after.checkbox().effects(), TokenIdentity::checkbox_effects, changed,
                      count);
    append_if_changed(before.radio().metrics(), after.radio().metrics(), TokenIdentity::radio_metrics, changed, count);
    append_if_changed(before.radio().colors(), after.radio().colors(), TokenIdentity::radio_colors, changed, count);
    append_if_changed(before.radio().effects(), after.radio().effects(), TokenIdentity::radio_effects, changed, count);
    append_if_changed(before.slider().colors, after.slider().colors, TokenIdentity::slider_colors, changed, count);
    append_if_changed(before.slider().metrics, after.slider().metrics, TokenIdentity::slider_metrics, changed, count);
    append_if_changed(before.table().colors, after.table().colors, TokenIdentity::table_colors, changed, count);
    append_if_changed(before.table().metrics, after.table().metrics, TokenIdentity::table_metrics, changed, count);
    append_if_changed(before.empty().colors, after.empty().colors, TokenIdentity::empty_colors, changed, count);
    append_if_changed(before.empty().metrics, after.empty().metrics, TokenIdentity::empty_metrics, changed, count);
    append_if_changed(before.result().colors, after.result().colors, TokenIdentity::result_colors, changed, count);
    append_if_changed(before.result().metrics, after.result().metrics, TokenIdentity::result_metrics, changed, count);
    const auto& old_tooltip = before.tooltip();
    append_if_changed(before.tag().colors, after.tag().colors, TokenIdentity::tag_colors, changed, count);
    append_if_changed(before.input_number().colors(), after.input_number().colors(), TokenIdentity::input_number_colors,
                      changed, count);
    append_if_changed(before.input_number().metrics(), after.input_number().metrics(),
                      TokenIdentity::input_number_metrics, changed, count);
    append_if_changed(before.tag().metrics, after.tag().metrics, TokenIdentity::tag_metrics, changed, count);
    append_if_changed(before.badge().colors, after.badge().colors, TokenIdentity::badge_colors, changed, count);
    append_if_changed(before.badge().metrics, after.badge().metrics, TokenIdentity::badge_metrics, changed, count);
    append_if_changed(before.alert().colors, after.alert().colors, TokenIdentity::alert_colors, changed, count);
    append_if_changed(before.alert().metrics, after.alert().metrics, TokenIdentity::alert_metrics, changed, count);
    append_if_changed(before.spin().colors, after.spin().colors, TokenIdentity::spin_colors, changed, count);
    append_if_changed(before.spin().metrics, after.spin().metrics, TokenIdentity::spin_metrics, changed, count);
    append_if_changed(before.progress().colors, after.progress().colors, TokenIdentity::progress_colors, changed,
                      count);
    append_if_changed(before.progress().metrics, after.progress().metrics, TokenIdentity::progress_metrics, changed,
                      count);
    append_if_changed(before.pagination().colors, after.pagination().colors, TokenIdentity::pagination_colors, changed,
                      count);
    append_if_changed(before.pagination().metrics, after.pagination().metrics, TokenIdentity::pagination_metrics,
                      changed, count);
    append_if_changed(before.card().colors, after.card().colors, TokenIdentity::card_colors, changed, count);
    append_if_changed(before.card().metrics, after.card().metrics, TokenIdentity::card_metrics, changed, count);
    append_if_changed(before.card().typography, after.card().typography, TokenIdentity::card_typography, changed,
                      count);
    append_if_changed(before.card().effects, after.card().effects, TokenIdentity::card_effects, changed, count);
    append_if_changed(before.tabs().colors, after.tabs().colors, TokenIdentity::tabs_colors, changed, count);
    append_if_changed(before.tabs().metrics, after.tabs().metrics, TokenIdentity::tabs_metrics, changed, count);
    append_if_changed(before.tabs().typography, after.tabs().typography, TokenIdentity::tabs_typography, changed,
                      count);
    append_if_changed(before.tabs().effects, after.tabs().effects, TokenIdentity::tabs_effects, changed, count);
    append_if_changed(before.collapse().colors, after.collapse().colors, TokenIdentity::collapse_colors, changed,
                      count);
    append_if_changed(before.collapse().metrics, after.collapse().metrics, TokenIdentity::collapse_metrics, changed,
                      count);
    append_if_changed(before.collapse().typography, after.collapse().typography, TokenIdentity::collapse_typography,
                      changed, count);
    append_if_changed(before.collapse().effects, after.collapse().effects, TokenIdentity::collapse_effects, changed,
                      count);
    append_if_changed(before.segmented().colors, after.segmented().colors, TokenIdentity::segmented_colors, changed,
                      count);
    append_if_changed(before.segmented().metrics, after.segmented().metrics, TokenIdentity::segmented_metrics, changed,
                      count);
    append_if_changed(before.segmented().effects, after.segmented().effects, TokenIdentity::segmented_effects, changed,
                      count);
    append_if_changed(before.modal().colors, after.modal().colors, TokenIdentity::modal_colors, changed, count);
    const auto& old_popconfirm = before.popconfirm();
    const auto& new_popconfirm = after.popconfirm();
    append_if_changed(old_popconfirm.colors(), new_popconfirm.colors(), TokenIdentity::popconfirm_colors, changed,
                      count);
    append_if_changed(old_popconfirm.metrics(), new_popconfirm.metrics(), TokenIdentity::popconfirm_metrics, changed,
                      count);
    append_if_changed(old_popconfirm.font_family, new_popconfirm.font_family, TokenIdentity::popconfirm_typography,
                      changed, count);
    append_if_changed(old_popconfirm.font_size, new_popconfirm.font_size, TokenIdentity::popconfirm_typography, changed,
                      count);
    append_if_changed(old_popconfirm.line_height, new_popconfirm.line_height, TokenIdentity::popconfirm_typography,
                      changed, count);
    append_if_changed(old_popconfirm.font_weight, new_popconfirm.font_weight, TokenIdentity::popconfirm_typography,
                      changed, count);
    append_if_changed(old_popconfirm.title_font_weight, new_popconfirm.title_font_weight,
                      TokenIdentity::popconfirm_typography, changed, count);
    append_if_changed(old_popconfirm.shadow, new_popconfirm.shadow, TokenIdentity::popconfirm_effects, changed, count);
    append_if_changed(old_popconfirm.z_index_popup, new_popconfirm.z_index_popup, TokenIdentity::popconfirm_order,
                      changed, count);
    append_if_changed(before.modal().metrics, after.modal().metrics, TokenIdentity::modal_metrics, changed, count);
    append_if_changed(before.modal().typography, after.modal().typography, TokenIdentity::modal_typography, changed,
                      count);
    append_if_changed(before.modal().effects, after.modal().effects, TokenIdentity::modal_effects, changed, count);
    append_if_changed(before.modal().z_index_popup, after.modal().z_index_popup, TokenIdentity::modal_order, changed,
                      count);
    append_if_changed(std::pair{before.drawer().colors, before.drawer().border},
                      std::pair{after.drawer().colors, after.drawer().border}, TokenIdentity::drawer_colors, changed,
                      count);
    append_if_changed(before.drawer().metrics, after.drawer().metrics, TokenIdentity::drawer_metrics, changed, count);
    append_if_changed(before.drawer().typography, after.drawer().typography, TokenIdentity::drawer_typography, changed,
                      count);
    append_if_changed(before.drawer().effects, after.drawer().effects, TokenIdentity::drawer_effects, changed, count);
    append_if_changed(before.drawer().z_index_popup, after.drawer().z_index_popup, TokenIdentity::drawer_order, changed,
                      count);
    append_if_changed(before.menu().colors, after.menu().colors, TokenIdentity::menu_colors, changed, count);
    append_if_changed(before.menu().metrics, after.menu().metrics, TokenIdentity::menu_metrics, changed, count);
    append_if_changed(before.menu().typography, after.menu().typography, TokenIdentity::menu_typography, changed,
                      count);
    append_if_changed(before.dropdown().background, after.dropdown().background, TokenIdentity::dropdown_colors,
                      changed, count);
    append_if_changed(before.dropdown().metrics, after.dropdown().metrics, TokenIdentity::dropdown_metrics, changed,
                      count);
    append_if_changed(before.dropdown().shadow, after.dropdown().shadow, TokenIdentity::dropdown_effects, changed,
                      count);
    append_if_changed(before.dropdown().z_index_popup, after.dropdown().z_index_popup, TokenIdentity::dropdown_order,
                      changed, count);
    append_if_changed(before.select().colors, after.select().colors, TokenIdentity::select_colors, changed, count);
    append_if_changed(before.select().metrics, after.select().metrics, TokenIdentity::select_metrics, changed, count);
    append_if_changed(before.select().typography, after.select().typography, TokenIdentity::select_typography, changed,
                      count);
    append_if_changed(before.select().shadow, after.select().shadow, TokenIdentity::select_effects, changed, count);
    append_if_changed(before.select().z_index_popup, after.select().z_index_popup, TokenIdentity::select_order, changed,
                      count);
    const auto& new_tooltip = after.tooltip();
    append_if_changed(old_tooltip.background, new_tooltip.background, TokenIdentity::tooltip_colors, changed, count);
    append_if_changed(old_tooltip.text, new_tooltip.text, TokenIdentity::tooltip_colors, changed, count);
    append_if_changed(old_tooltip.metrics(), new_tooltip.metrics(), TokenIdentity::tooltip_metrics, changed, count);
    append_if_changed(old_tooltip.font_size, new_tooltip.font_size, TokenIdentity::tooltip_typography, changed, count);
    append_if_changed(old_tooltip.line_height, new_tooltip.line_height, TokenIdentity::tooltip_typography, changed,
                      count);
    append_if_changed(before.typography().font_family, after.typography().font_family,
                      TokenIdentity::tooltip_typography, changed, count);
    append_if_changed(before.typography().font_weight, after.typography().font_weight,
                      TokenIdentity::tooltip_typography, changed, count);
    append_if_changed(old_tooltip.shadow, new_tooltip.shadow, TokenIdentity::tooltip_shadow, changed, count);
    append_if_changed(old_tooltip.z_index_popup, new_tooltip.z_index_popup, TokenIdentity::tooltip_order, changed,
                      count);
    const auto& old_typography = before.typography();
    const auto& new_typography = after.typography();
    append_if_changed(old_typography.colors, new_typography.colors, TokenIdentity::typography_colors, changed, count);
    append_if_changed(old_typography.headings, new_typography.headings, TokenIdentity::typography_headings, changed,
                      count);
    const auto typography_fonts = [](const TypographyThemeToken& token) {
        return std::array<std::uint32_t, 4>{static_cast<std::uint32_t>(token.font_family),
                                            static_cast<std::uint32_t>(token.font_family_code), token.font_weight,
                                            token.font_weight_strong};
    };
    append_if_changed(typography_fonts(old_typography), typography_fonts(new_typography),
                      TokenIdentity::typography_fonts, changed, count);
    append_if_changed(std::array{old_typography.base_font_size, old_typography.base_line_height},
                      std::array{new_typography.base_font_size, new_typography.base_line_height},
                      TokenIdentity::typography_base_typography, changed, count);
    append_if_changed(std::array{old_typography.title_margin_top_em, old_typography.title_margin_bottom_em},
                      std::array{new_typography.title_margin_top_em, new_typography.title_margin_bottom_em},
                      TokenIdentity::typography_metrics, changed, count);
    const auto inline_metrics = [](const InlineCodeThemeToken& t) {
        return std::array{t.font_scale,   t.padding_inline_em, t.padding_block_start_em, t.padding_block_end_em,
                          t.border_width, t.border_radius,     t.border_bottom_width};
    };
    const auto inline_colors = [](const TypographyThemeToken& t) {
        return std::array{t.code.background, t.code.border_color, t.keyboard.background, t.keyboard.border_color};
    };
    append_if_changed(inline_metrics(old_typography.code), inline_metrics(new_typography.code),
                      TokenIdentity::typography_inline_code, changed, count);
    append_if_changed(inline_metrics(old_typography.keyboard), inline_metrics(new_typography.keyboard),
                      TokenIdentity::typography_inline_keyboard, changed, count);
    append_if_changed(inline_colors(old_typography), inline_colors(new_typography),
                      TokenIdentity::typography_inline_colors, changed, count);
    const auto& old_divider = before.divider();
    const auto& new_divider = after.divider();
    append_if_changed(old_divider.colors, new_divider.colors, TokenIdentity::divider_colors, changed, count);
    append_if_changed(old_divider.metrics, new_divider.metrics, TokenIdentity::divider_metrics, changed, count);
    append_if_changed(old_divider.typography, new_divider.typography, TokenIdentity::divider_typography, changed,
                      count);
    return count;
}

bool contains(std::span<const TokenIdentity> identities, TokenIdentity identity) {
    return std::find(identities.begin(), identities.end(), identity) != identities.end();
}

// Keep the large resolved snapshot temporary out of the recursive validation frame.
std::shared_ptr<const ThemeSnapshot> resolve_validation_snapshot(const ThemeConfig& config,
                                                                 const ThemeSnapshot& parent) {
    return std::make_shared<ThemeSnapshot>(resolve_theme(config, &parent));
}

} // namespace

struct Subscription::State final {
    bool active{true};
    std::vector<TokenIdentity> identities;
    ThemeScope::InvalidationCallback callback;
};

struct ThemeScope::Subscriber final {
    std::weak_ptr<Subscription::State> state;
};

Subscription::Subscription(std::shared_ptr<State> state) noexcept : state_(std::move(state)) {}

Subscription::Subscription(Subscription&& other) noexcept : state_(std::move(other.state_)) {}

Subscription& Subscription::operator=(Subscription&& other) noexcept {
    if (this != &other) {
        reset();
        state_ = std::move(other.state_);
    }
    return *this;
}

Subscription::~Subscription() {
    reset();
}

void Subscription::reset() noexcept {
    if (state_) {
        state_->active = false;
        state_.reset();
    }
}

bool Subscription::active() const noexcept {
    return state_ && state_->active;
}

std::shared_ptr<ThemeScope> ThemeScope::create_default() {
    return std::shared_ptr<ThemeScope>(new ThemeScope(nullptr, ThemeConfig{}));
}

std::shared_ptr<ThemeScope> ThemeScope::create(std::shared_ptr<ThemeScope> parent, const ThemeConfig& config) {
    if (!parent) {
        throw std::invalid_argument("Nested ThemeScope requires a parent scope");
    }
    parent->ensure_owner_thread();
    auto scope = std::shared_ptr<ThemeScope>(new ThemeScope(parent, config));
    parent->children_.push_back(scope);
    return scope;
}

ThemeScope::ThemeScope(std::shared_ptr<ThemeScope> parent, const ThemeConfig& config)
    : parent_(std::move(parent)), config_(config),
      snapshot_(std::make_shared<ThemeSnapshot>(
          resolve_theme(config_, config_.inherit && parent_ ? &parent_->snapshot() : nullptr))),
      owner_thread_(std::this_thread::get_id()) {}

bool ThemeScope::update(const ThemeConfig& config) {
    ensure_owner_thread();
    if (config == config_) {
        ++diagnostics_.snapshot_reuses;
        changed_identity_count_ = 0;
        diagnostics_.changed_identity_count = 0;
        diagnostics_.dirty_phase = DirtyPhase::none;
        return false;
    }
    ThemeSnapshot next = resolve_theme(config, config.inherit && parent_ ? &parent_->snapshot() : nullptr);
    if (next != *snapshot_) {
        validate_children(next);
    }
    commit_snapshot(config, std::move(next));
    return true;
}

const ThemeSnapshot& ThemeScope::snapshot() const {
    ensure_owner_thread();
    return *snapshot_;
}

std::uint64_t ThemeScope::generation() const {
    ensure_owner_thread();
    return diagnostics_.generation;
}

Diagnostics ThemeScope::diagnostics() const {
    ensure_owner_thread();
    auto current = diagnostics_;
    current.subscriber_count = live_subscriber_count();
    return current;
}

std::span<const TokenIdentity> ThemeScope::changed_identities() const {
    ensure_owner_thread();
    return {changed_identities_.data(), changed_identity_count_};
}

Subscription ThemeScope::capture(InvalidationCallback callback, const std::function<void()>& typed_token_reads) {
    ensure_owner_thread();
    if (!callback || !typed_token_reads) {
        throw std::invalid_argument("Theme token capture requires reads and an invalidation callback");
    }
    ReadCapture capture{.scope = this};
    {
        ReadCaptureGuard guard(capture);
        typed_token_reads();
    }
    if (capture.count == 0) {
        throw std::logic_error("Theme token capture did not read a typed token accessor");
    }
    auto state = std::make_shared<Subscription::State>();
    state->identities.assign(capture.identities.begin(),
                             capture.identities.begin() + static_cast<std::ptrdiff_t>(capture.count));
    state->callback = std::move(callback);
    subscribers_.push_back({state});
    ++diagnostics_.subscription_allocations;
    diagnostics_.subscriber_count = live_subscriber_count();
    return Subscription(std::move(state));
}

const ThemeAliasToken& ThemeScope::alias() const {
    ensure_owner_thread();
    constexpr std::array identities{
        TokenIdentity::alias_opacity_loading,
        TokenIdentity::alias_color_text,
        TokenIdentity::alias_color_text_secondary,
        TokenIdentity::alias_color_text_disabled,
        TokenIdentity::alias_color_background_container,
        TokenIdentity::alias_color_background_elevated,
        TokenIdentity::alias_color_background_container_disabled,
        TokenIdentity::alias_color_border,
        TokenIdentity::alias_color_border_secondary,
        TokenIdentity::alias_color_split,
        TokenIdentity::alias_color_focus_outline,
        TokenIdentity::alias_line_width_focus,
        TokenIdentity::alias_focus_outline_offset,
        TokenIdentity::alias_box_shadow,
        TokenIdentity::alias_box_shadow_secondary,
        TokenIdentity::alias_box_shadow_tertiary,
    };
    for (const auto identity : identities) {
        record(identity);
    }
    return snapshot_->alias();
}

Color ThemeScope::primary_color() const {
    ensure_owner_thread();
    record(TokenIdentity::map_color_primary);
    return snapshot_->map().color_primary;
}

const ThemeMapToken& ThemeScope::map() const {
    ensure_owner_thread();
    for (auto identity = TokenIdentity::map_color_primary; identity <= TokenIdentity::map_motion_enabled;
         identity = static_cast<TokenIdentity>(static_cast<std::uint8_t>(identity) + 1U)) {
        record(identity);
    }
    return snapshot_->map();
}

const ButtonThemeToken& ThemeScope::button() const {
    ensure_owner_thread();
    for (auto identity = TokenIdentity::button_colors; identity <= TokenIdentity::button_shadows;
         identity = static_cast<TokenIdentity>(static_cast<std::uint8_t>(identity) + 1U)) {
        record(identity);
    }
    return snapshot_->button();
}

const TextThemeToken& ThemeScope::text() const {
    ensure_owner_thread();
    for (auto identity = TokenIdentity::text_color; identity <= TokenIdentity::text_line_height;
         identity = static_cast<TokenIdentity>(static_cast<std::uint8_t>(identity) + 1U)) {
        record(identity);
    }
    return snapshot_->text();
}

Color ThemeScope::text_color() const {
    ensure_owner_thread();
    record(TokenIdentity::text_color);
    return snapshot_->text().color;
}

SystemFontFamily ThemeScope::text_font_family() const {
    ensure_owner_thread();
    record(TokenIdentity::text_font_family);
    return snapshot_->text().font_family;
}

std::uint32_t ThemeScope::text_font_weight() const {
    ensure_owner_thread();
    record(TokenIdentity::text_font_weight);
    return snapshot_->text().font_weight;
}

float ThemeScope::text_font_size() const {
    ensure_owner_thread();
    record(TokenIdentity::text_font_size);
    return snapshot_->text().font_size;
}

float ThemeScope::line_width() const {
    ensure_owner_thread();
    record(TokenIdentity::seed_line_width);
    return snapshot_->seed().line_width;
}

float ThemeScope::text_line_height() const {
    ensure_owner_thread();
    record(TokenIdentity::text_line_height);
    return snapshot_->text().line_height;
}

Color ThemeScope::text_secondary_color() const {
    ensure_owner_thread();
    record(TokenIdentity::alias_color_text_secondary);
    return snapshot_->alias().color_text_secondary;
}

Color ThemeScope::text_disabled_color() const {
    ensure_owner_thread();
    record(TokenIdentity::alias_color_text_disabled);
    return snapshot_->alias().color_text_disabled;
}

const ButtonThemeToken& ThemeScope::button_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::button_colors);
    return snapshot_->button();
}

const ButtonThemeToken& ThemeScope::button_control_heights() const {
    ensure_owner_thread();
    record(TokenIdentity::button_control_heights);
    return snapshot_->button();
}

const ButtonThemeToken& ThemeScope::button_padding_inline() const {
    ensure_owner_thread();
    record(TokenIdentity::button_padding_inline);
    return snapshot_->button();
}

const detail::InputTokenSet& ThemeScope::input_layout_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::input_layout_metrics);
    return detail::InputTokenAccess::get(*snapshot_);
}

const detail::InputTokenSet& ThemeScope::input_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::input_typography);
    return detail::InputTokenAccess::get(*snapshot_);
}

const detail::InputTokenSet& ThemeScope::input_border_radius() const {
    ensure_owner_thread();
    record(TokenIdentity::input_border_radius);
    return detail::InputTokenAccess::get(*snapshot_);
}

const detail::InputTokenSet& ThemeScope::input_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::input_colors);
    return detail::InputTokenAccess::get(*snapshot_);
}

const detail::InputTokenSet& ThemeScope::input_shadows() const {
    ensure_owner_thread();
    record(TokenIdentity::input_shadows);
    return detail::InputTokenAccess::get(*snapshot_);
}

const CheckboxThemeToken& ThemeScope::checkbox_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::checkbox_metrics);
    return snapshot_->checkbox();
}

const CheckboxThemeToken& ThemeScope::checkbox_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::checkbox_colors);
    return snapshot_->checkbox();
}

const CheckboxThemeToken& ThemeScope::checkbox_effects() const {
    ensure_owner_thread();
    record(TokenIdentity::checkbox_effects);
    return snapshot_->checkbox();
}

const RadioThemeToken& ThemeScope::radio_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::radio_metrics);
    return snapshot_->radio();
}

const RadioThemeToken& ThemeScope::radio_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::radio_colors);
    return snapshot_->radio();
}

const RadioThemeToken& ThemeScope::radio_effects() const {
    ensure_owner_thread();
    record(TokenIdentity::radio_effects);
    return snapshot_->radio();
}

const SwitchThemeToken& ThemeScope::switch_geometry() const {
    ensure_owner_thread();
    record(TokenIdentity::switch_geometry);
    return snapshot_->switch_token();
}

const SwitchThemeToken& ThemeScope::switch_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::switch_colors);
    return snapshot_->switch_token();
}

const SwitchThemeToken& ThemeScope::switch_effects() const {
    ensure_owner_thread();
    record(TokenIdentity::switch_effects);
    return snapshot_->switch_token();
}

const TypographyThemeToken& ThemeScope::typography_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::typography_colors);
    return snapshot_->typography();
}

const TypographyThemeToken& ThemeScope::typography_headings() const {
    ensure_owner_thread();
    record(TokenIdentity::typography_headings);
    return snapshot_->typography();
}

const TypographyThemeToken& ThemeScope::typography_fonts() const {
    ensure_owner_thread();
    record(TokenIdentity::typography_fonts);
    return snapshot_->typography();
}

const TypographyThemeToken& ThemeScope::typography_base_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::typography_base_typography);
    return snapshot_->typography();
}

const TypographyThemeToken& ThemeScope::typography_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::typography_metrics);
    return snapshot_->typography();
}

const InlineCodeThemeToken& ThemeScope::typography_inline_code() const {
    ensure_owner_thread();
    record(TokenIdentity::typography_inline_code);
    return snapshot_->typography().code;
}

const InlineCodeThemeToken& ThemeScope::typography_inline_keyboard() const {
    ensure_owner_thread();
    record(TokenIdentity::typography_inline_keyboard);
    return snapshot_->typography().keyboard;
}

const DividerThemeToken& ThemeScope::divider_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::divider_colors);
    return snapshot_->divider();
}

const DividerThemeToken& ThemeScope::divider_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::divider_metrics);
    return snapshot_->divider();
}

const DividerThemeToken& ThemeScope::divider_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::divider_typography);
    return snapshot_->divider();
}

SystemFontFamily ThemeScope::code_font_family() const {
    ensure_owner_thread();
    record(TokenIdentity::typography_fonts);
    return snapshot_->typography().font_family_code;
}

const ButtonThemeToken& ThemeScope::button_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::button_typography);
    return snapshot_->button();
}

const ButtonThemeToken& ThemeScope::button_border_radius() const {
    ensure_owner_thread();
    record(TokenIdentity::button_border_radius);
    return snapshot_->button();
}

float ThemeScope::button_border_width() const {
    ensure_owner_thread();
    record(TokenIdentity::button_border_width);
    return snapshot_->button().border_width;
}

float ThemeScope::button_icon_gap() const {
    ensure_owner_thread();
    record(TokenIdentity::button_icon_gap);
    return snapshot_->button().icon_gap;
}

const ButtonThemeToken& ThemeScope::button_shadows() const {
    ensure_owner_thread();
    record(TokenIdentity::button_shadows);
    return snapshot_->button();
}

Color ThemeScope::focus_outline_color() const {
    ensure_owner_thread();
    record(TokenIdentity::alias_color_focus_outline);
    return snapshot_->alias().color_focus_outline;
}

float ThemeScope::focus_outline_width() const {
    ensure_owner_thread();
    record(TokenIdentity::alias_line_width_focus);
    return snapshot_->alias().line_width_focus;
}

float ThemeScope::focus_outline_offset() const {
    ensure_owner_thread();
    record(TokenIdentity::alias_focus_outline_offset);
    return snapshot_->alias().focus_outline_offset;
}

float ThemeScope::layout_gap_small() const {
    ensure_owner_thread();
    record(TokenIdentity::map_size_xs);
    return snapshot_->map().size_xs;
}

float ThemeScope::layout_gap_middle() const {
    ensure_owner_thread();
    record(TokenIdentity::map_size);
    return snapshot_->map().size;
}

float ThemeScope::layout_gap_large() const {
    ensure_owner_thread();
    record(TokenIdentity::map_size_large);
    return snapshot_->map().size_large;
}

Duration ThemeScope::motion_unit() const {
    ensure_owner_thread();
    record(TokenIdentity::map_motion_unit);
    return snapshot_->map().motion_unit;
}

Duration ThemeScope::motion_base() const {
    ensure_owner_thread();
    record(TokenIdentity::map_motion_base);
    return snapshot_->map().motion_base;
}

bool ThemeScope::motion_enabled() const {
    ensure_owner_thread();
    record(TokenIdentity::map_motion_enabled);
    return snapshot_->map().motion;
}

void ThemeScope::ensure_owner_thread() const {
    if (std::this_thread::get_id() != owner_thread_) {
        throw std::logic_error("ThemeScope can only be used on its owner thread");
    }
}

void ThemeScope::record(TokenIdentity identity) const {
    if (active_capture == nullptr || active_capture->scope != this) {
        return;
    }
    const auto present =
        std::find(active_capture->identities.begin(),
                  active_capture->identities.begin() + static_cast<std::ptrdiff_t>(active_capture->count), identity);
    if (present == active_capture->identities.begin() + static_cast<std::ptrdiff_t>(active_capture->count)) {
        active_capture->identities[active_capture->count++] = identity;
    }
}

void ThemeScope::validate_children(const ThemeSnapshot& parent_snapshot) const {
    for (const auto& weak_child : children_) {
        const auto child = weak_child.lock();
        if (!child || !child->config_.inherit) {
            continue;
        }
        const auto next = resolve_validation_snapshot(child->config_, parent_snapshot);
        if (*next != *child->snapshot_) {
            child->validate_children(*next);
        }
    }
}

void ThemeScope::recompute_from_parent() {
    ensure_owner_thread();
    if (!config_.inherit) {
        return;
    }
    ThemeSnapshot next = resolve_theme(config_, &parent_->snapshot());
    commit_snapshot(config_, std::move(next));
}

void ThemeScope::commit_snapshot(const ThemeConfig& config, ThemeSnapshot&& next) {
    const auto changed = collect_changed(*snapshot_, next, changed_identities_);
    if (&config != &config_) {
        config_ = config;
    }
    if (next == *snapshot_) {
        ++diagnostics_.snapshot_reuses;
        changed_identity_count_ = 0;
        diagnostics_.changed_identity_count = 0;
        diagnostics_.dirty_phase = DirtyPhase::none;
        return;
    }
    snapshot_ = std::make_shared<ThemeSnapshot>(std::move(next));
    ++diagnostics_.snapshot_allocations;
    ++diagnostics_.generation;
    changed_identity_count_ = changed;
    diagnostics_.changed_identity_count = changed;
    diagnostics_.dirty_phase = DirtyPhase::none;
    for (std::size_t index = 0; index < changed; ++index) {
        diagnostics_.dirty_phase |= dirty_phase_for(changed_identities_[index]);
    }
    notify_subscribers();
    notify_children();
}

void ThemeScope::notify_subscribers() {
    const auto changed = changed_identities();
    std::erase_if(subscribers_, [](const Subscriber& subscriber) {
        const auto state = subscriber.state.lock();
        return !state || !state->active;
    });
    for (const auto& subscriber : subscribers_) {
        const auto state = subscriber.state.lock();
        if (!state || !state->active) {
            continue;
        }
        DirtyPhase phase = DirtyPhase::none;
        for (const auto identity : state->identities) {
            if (contains(changed, identity)) {
                phase |= dirty_phase_for(identity);
            }
        }
        if (phase != DirtyPhase::none) {
            ++diagnostics_.notifications;
            state->callback(phase);
        }
    }
    diagnostics_.subscriber_count = live_subscriber_count();
}

void ThemeScope::notify_children() {
    std::erase_if(children_, [](const auto& child) { return child.expired(); });
    for (const auto& weak_child : children_) {
        if (const auto child = weak_child.lock()) {
            child->recompute_from_parent();
        }
    }
}

std::size_t ThemeScope::live_subscriber_count() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(subscribers_.begin(), subscribers_.end(), [](const Subscriber& subscriber) {
            const auto state = subscriber.state.lock();
            return state && state->active;
        }));
}

const TypographyThemeToken& ThemeScope::typography_inline_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::typography_inline_colors);
    return snapshot_->typography();
}

const SliderThemeToken& ThemeScope::slider_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::slider_colors);
    return snapshot_->slider();
}

const SliderThemeToken& ThemeScope::slider_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::slider_metrics);
    return snapshot_->slider();
}

const TooltipThemeToken& ThemeScope::tooltip_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::tooltip_colors);
    return snapshot_->tooltip();
}

const TooltipThemeToken& ThemeScope::tooltip_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::tooltip_metrics);
    return snapshot_->tooltip();
}

const TooltipThemeToken& ThemeScope::tooltip_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::tooltip_typography);
    return snapshot_->tooltip();
}

const TooltipThemeToken& ThemeScope::tooltip_shadow() const {
    ensure_owner_thread();
    record(TokenIdentity::tooltip_shadow);
    return snapshot_->tooltip();
}

const TooltipThemeToken& ThemeScope::tooltip_order() const {
    ensure_owner_thread();
    record(TokenIdentity::tooltip_order);
    return snapshot_->tooltip();
}

const CollapseThemeToken& ThemeScope::collapse_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::collapse_colors);
    return snapshot_->collapse();
}

const CollapseThemeToken& ThemeScope::collapse_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::collapse_metrics);
    return snapshot_->collapse();
}

const CollapseThemeToken& ThemeScope::collapse_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::collapse_typography);
    return snapshot_->collapse();
}

const CollapseThemeToken& ThemeScope::collapse_effects() const {
    ensure_owner_thread();
    record(TokenIdentity::collapse_effects);
    return snapshot_->collapse();
}

const TabsThemeToken& ThemeScope::tabs_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::tabs_colors);
    return snapshot_->tabs();
}

const TagThemeToken& ThemeScope::tag_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::tag_colors);
    return snapshot_->tag();
}

const InputNumberThemeToken& ThemeScope::input_number_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::input_number_colors);
    return snapshot_->input_number();
}

const InputNumberThemeToken& ThemeScope::input_number_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::input_number_metrics);
    return snapshot_->input_number();
}

const TagThemeToken& ThemeScope::tag_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::tag_metrics);
    return snapshot_->tag();
}

const BadgeThemeToken& ThemeScope::badge_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::badge_colors);
    return snapshot_->badge();
}

const BadgeThemeToken& ThemeScope::badge_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::badge_metrics);
    return snapshot_->badge();
}

const PopconfirmThemeToken& ThemeScope::popconfirm_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::popconfirm_colors);
    return snapshot_->popconfirm();
}

const PopconfirmThemeToken& ThemeScope::popconfirm_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::popconfirm_metrics);
    return snapshot_->popconfirm();
}

const PopconfirmThemeToken& ThemeScope::popconfirm_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::popconfirm_typography);
    return snapshot_->popconfirm();
}

const PopconfirmThemeToken& ThemeScope::popconfirm_effects() const {
    ensure_owner_thread();
    record(TokenIdentity::popconfirm_effects);
    return snapshot_->popconfirm();
}

const PopconfirmThemeToken& ThemeScope::popconfirm_order() const {
    ensure_owner_thread();
    record(TokenIdentity::popconfirm_order);
    return snapshot_->popconfirm();
}

const AlertThemeToken& ThemeScope::alert_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::alert_colors);
    return snapshot_->alert();
}

const AlertThemeToken& ThemeScope::alert_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::alert_metrics);
    return snapshot_->alert();
}

const SpinThemeToken& ThemeScope::spin_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::spin_colors);
    return snapshot_->spin();
}

const SpinThemeToken& ThemeScope::spin_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::spin_metrics);
    return snapshot_->spin();
}

const ProgressThemeToken& ThemeScope::progress_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::progress_colors);
    return snapshot_->progress();
}

const ProgressThemeToken& ThemeScope::progress_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::progress_metrics);
    return snapshot_->progress();
}

const ModalThemeToken& ThemeScope::modal_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::modal_colors);
    return snapshot_->modal();
}

const ModalThemeToken& ThemeScope::modal_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::modal_metrics);
    return snapshot_->modal();
}

const ModalThemeToken& ThemeScope::modal_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::modal_typography);
    return snapshot_->modal();
}

const ModalThemeToken& ThemeScope::modal_effects() const {
    ensure_owner_thread();
    record(TokenIdentity::modal_effects);
    return snapshot_->modal();
}

const ModalThemeToken& ThemeScope::modal_order() const {
    ensure_owner_thread();
    record(TokenIdentity::modal_order);
    return snapshot_->modal();
}

const DrawerThemeToken& ThemeScope::drawer_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::drawer_colors);
    return snapshot_->drawer();
}

const DrawerThemeToken& ThemeScope::drawer_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::drawer_metrics);
    return snapshot_->drawer();
}

const DrawerThemeToken& ThemeScope::drawer_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::drawer_typography);
    return snapshot_->drawer();
}

const DrawerThemeToken& ThemeScope::drawer_effects() const {
    ensure_owner_thread();
    record(TokenIdentity::drawer_effects);
    return snapshot_->drawer();
}

const DrawerThemeToken& ThemeScope::drawer_order() const {
    ensure_owner_thread();
    record(TokenIdentity::drawer_order);
    return snapshot_->drawer();
}

const TabsThemeToken& ThemeScope::tabs_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::tabs_metrics);
    return snapshot_->tabs();
}

const TabsThemeToken& ThemeScope::tabs_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::tabs_typography);
    return snapshot_->tabs();
}

const TabsThemeToken& ThemeScope::tabs_effects() const {
    ensure_owner_thread();
    record(TokenIdentity::tabs_effects);
    return snapshot_->tabs();
}

const PaginationThemeToken& ThemeScope::pagination_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::pagination_colors);
    return snapshot_->pagination();
}

const PaginationThemeToken& ThemeScope::pagination_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::pagination_metrics);
    return snapshot_->pagination();
}

const TableThemeToken& ThemeScope::table_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::table_colors);
    return snapshot_->table();
}

const TableThemeToken& ThemeScope::table_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::table_metrics);
    return snapshot_->table();
}

const EmptyThemeToken& ThemeScope::empty_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::empty_colors);
    return snapshot_->empty();
}

const EmptyThemeToken& ThemeScope::empty_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::empty_metrics);
    return snapshot_->empty();
}

const ResultThemeToken& ThemeScope::result_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::result_colors);
    return snapshot_->result();
}

const ResultThemeToken& ThemeScope::result_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::result_metrics);
    return snapshot_->result();
}

const SegmentedThemeToken& ThemeScope::segmented_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::segmented_colors);
    return snapshot_->segmented();
}

const SegmentedThemeToken& ThemeScope::segmented_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::segmented_metrics);
    return snapshot_->segmented();
}

const SegmentedThemeToken& ThemeScope::segmented_effects() const {
    ensure_owner_thread();
    record(TokenIdentity::segmented_effects);
    return snapshot_->segmented();
}

const CardThemeToken& ThemeScope::card_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::card_colors);
    return snapshot_->card();
}

const CardThemeToken& ThemeScope::card_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::card_metrics);
    return snapshot_->card();
}

const CardThemeToken& ThemeScope::card_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::card_typography);
    return snapshot_->card();
}

const CardThemeToken& ThemeScope::card_effects() const {
    ensure_owner_thread();
    record(TokenIdentity::card_effects);
    return snapshot_->card();
}

const MenuThemeToken& ThemeScope::menu_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::menu_colors);
    return snapshot_->menu();
}

const MenuThemeToken& ThemeScope::menu_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::menu_metrics);
    return snapshot_->menu();
}

const MenuThemeToken& ThemeScope::menu_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::menu_typography);
    return snapshot_->menu();
}

const DropdownThemeToken& ThemeScope::dropdown_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::dropdown_colors);
    return snapshot_->dropdown();
}

const DropdownThemeToken& ThemeScope::dropdown_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::dropdown_metrics);
    return snapshot_->dropdown();
}

const DropdownThemeToken& ThemeScope::dropdown_effects() const {
    ensure_owner_thread();
    record(TokenIdentity::dropdown_effects);
    return snapshot_->dropdown();
}

const DropdownThemeToken& ThemeScope::dropdown_order() const {
    ensure_owner_thread();
    record(TokenIdentity::dropdown_order);
    return snapshot_->dropdown();
}

const SelectThemeToken& ThemeScope::select_colors() const {
    ensure_owner_thread();
    record(TokenIdentity::select_colors);
    return snapshot_->select();
}

const SelectThemeToken& ThemeScope::select_metrics() const {
    ensure_owner_thread();
    record(TokenIdentity::select_metrics);
    return snapshot_->select();
}

const SelectThemeToken& ThemeScope::select_typography() const {
    ensure_owner_thread();
    record(TokenIdentity::select_typography);
    return snapshot_->select();
}

const SelectThemeToken& ThemeScope::select_effects() const {
    ensure_owner_thread();
    record(TokenIdentity::select_effects);
    return snapshot_->select();
}

const SelectThemeToken& ThemeScope::select_order() const {
    ensure_owner_thread();
    record(TokenIdentity::select_order);
    return snapshot_->select();
}

std::string_view token_identity_name(TokenIdentity identity) noexcept {
    // Order MUST match the TokenIdentity declaration: several readers walk the
    // enum by contiguous range, and the count assert guards the pairing.
    constexpr std::array names{
        "alias.colorText",
        "alias.colorTextSecondary",
        "alias.colorTextDisabled",
        "alias.colorBgContainer",
        "alias.colorBgElevated",
        "alias.colorBgContainerDisabled",
        "alias.colorBorder",
        "alias.colorBorderSecondary",
        "alias.colorSplit",
        "alias.colorFocusOutline",
        "alias.lineWidthFocus",
        "alias.focusOutlineOffset",
        "alias.boxShadow",
        "alias.boxShadowSecondary",
        "alias.boxShadowTertiary",
        "map.colorPrimary",
        "map.colorPrimaryHover",
        "map.colorPrimaryActive",
        "map.colorPrimaryBorder",
        "map.colorSuccess",
        "map.colorWarning",
        "map.colorError",
        "map.colorErrorHover",
        "map.colorErrorActive",
        "map.colorInfo",
        "map.colorSuccessText",
        "map.colorWarningText",
        "map.colorErrorText",
        "map.colorLink",
        "map.colorLinkHover",
        "map.colorLinkActive",
        "map.colorTextBase",
        "map.colorBgBase",
        "map.fontSizeSM",
        "map.fontSize",
        "map.fontSizeLG",
        "map.lineHeightSM",
        "map.lineHeight",
        "map.lineHeightLG",
        "map.sizeXS",
        "map.sizeSM",
        "map.size",
        "map.sizeLG",
        "map.controlHeightSM",
        "map.controlHeight",
        "map.controlHeightLG",
        "map.borderRadiusSM",
        "map.borderRadius",
        "map.borderRadiusLG",
        "map.motionUnit",
        "map.motionBase",
        "map.motion",
        "Button.colors",
        "Button.controlHeights",
        "Button.paddingInline",
        "Button.typography",
        "Button.borderRadius",
        "Button.borderWidth",
        "Button.iconGap",
        "Button.shadows",
        "Text.color",
        "Text.fontFamily",
        "Text.fontWeight",
        "Text.fontSize",
        "Text.lineHeight",
        "seed.lineWidth",
        "Input.layoutMetrics",
        "Input.typography",
        "Input.borderRadius",
        "Input.colors",
        "Input.shadows",
        "Switch.geometry",
        "Switch.colors",
        "Typography.colors",
        "Typography.headings",
        "Typography.fonts",
        "Typography.baseTypography",
        "Typography.metrics",
        "Typography.inlineCode",
        "Typography.inlineKeyboard",
        "Typography.inlineColors",
        "Divider.colors",
        "Divider.metrics",
        "Divider.typography",
        "Slider.colors",
        "Slider.metrics",
        "Tooltip.colors",
        "Tooltip.metrics",
        "Tooltip.typography",
        "Tooltip.shadow",
        "Tooltip.order",
        "alias.opacityLoading",
        "Switch.effects",
        "Checkbox.metrics",
        "Checkbox.colors",
        "Checkbox.effects",
        "Radio.metrics",
        "Radio.colors",
        "Radio.effects",
        "Select.colors",
        "Select.metrics",
        "Select.typography",
        "Select.effects",
        "Select.order",
        "Card.colors",
        "Card.metrics",
        "Card.typography",
        "Card.effects",
        "Tabs.colors",
        "Tabs.metrics",
        "Tabs.typography",
        "Tabs.effects",
        "Segmented.colors",
        "Segmented.metrics",
        "Segmented.effects",
        "Modal.colors",
        "Modal.metrics",
        "Modal.typography",
        "Modal.effects",
        "Modal.order",
        "Tag.colors",
        "Tag.metrics",
        "Badge.colors",
        "Badge.metrics",
        "Drawer.colors",
        "Drawer.metrics",
        "Drawer.typography",
        "Drawer.effects",
        "Drawer.order",
        "Popconfirm.colors",
        "Popconfirm.metrics",
        "Popconfirm.typography",
        "Popconfirm.effects",
        "Popconfirm.order",
        "Alert.colors",
        "Alert.metrics",
        "Spin.colors",
        "Spin.metrics",
        "Progress.colors",
        "Progress.metrics",
        "Menu.colors",
        "Menu.metrics",
        "Menu.typography",
        "Dropdown.colors",
        "Dropdown.metrics",
        "Dropdown.effects",
        "Dropdown.order",

        "InputNumber.colors",
        "InputNumber.metrics",
        "Pagination.colors",
        "Pagination.metrics",
        "Table.colors",
        "Table.metrics",
        "Empty.colors",
        "Empty.metrics",
        "Result.colors",
        "Result.metrics",
        "Collapse.colors",
        "Collapse.metrics",
        "Collapse.typography",
        "Collapse.effects",
    };
    static_assert(names.size() == static_cast<std::size_t>(TokenIdentity::count));
    const auto index = static_cast<std::size_t>(identity);
    return index < names.size() ? names[index] : std::string_view{"invalid"};
}

DirtyPhase dirty_phase_for(TokenIdentity identity) noexcept {
    switch (identity) {
    case TokenIdentity::empty_metrics:
    case TokenIdentity::result_metrics:
        return DirtyPhase::measure_layout | DirtyPhase::geometry | DirtyPhase::text | DirtyPhase::hit_test;
    case TokenIdentity::alias_color_text:
    case TokenIdentity::alias_color_text_secondary:
    case TokenIdentity::alias_color_text_disabled:
    case TokenIdentity::alias_color_background_container:
    case TokenIdentity::alias_color_background_elevated:
    case TokenIdentity::alias_color_background_container_disabled:
    case TokenIdentity::alias_color_border:
    case TokenIdentity::alias_color_border_secondary:
    case TokenIdentity::alias_color_split:
    case TokenIdentity::map_color_primary:
    case TokenIdentity::map_color_primary_hover:
    case TokenIdentity::map_color_primary_active:
    case TokenIdentity::map_color_primary_border:
    case TokenIdentity::map_color_success:
    case TokenIdentity::map_color_warning:
    case TokenIdentity::map_color_error:
    case TokenIdentity::map_color_error_hover:
    case TokenIdentity::map_color_error_active:
    case TokenIdentity::map_color_info:
    case TokenIdentity::map_color_success_text:
    case TokenIdentity::map_color_warning_text:
    case TokenIdentity::map_color_error_text:
    case TokenIdentity::map_color_link:
    case TokenIdentity::map_color_link_hover:
    case TokenIdentity::map_color_link_active:
    case TokenIdentity::map_color_text_base:
    case TokenIdentity::map_color_background_base:
    case TokenIdentity::button_colors:
    case TokenIdentity::input_colors:
    case TokenIdentity::switch_colors:
    case TokenIdentity::checkbox_colors:
    case TokenIdentity::radio_colors:
    case TokenIdentity::alias_opacity_loading:
    case TokenIdentity::text_color:
    case TokenIdentity::typography_colors:
    case TokenIdentity::typography_inline_colors:
    case TokenIdentity::divider_colors:
    case TokenIdentity::slider_colors:
    case TokenIdentity::tooltip_colors:
    case TokenIdentity::tooltip_order:
    case TokenIdentity::menu_colors:
    case TokenIdentity::dropdown_colors:
    case TokenIdentity::select_colors:
    case TokenIdentity::card_colors:
    case TokenIdentity::tabs_colors:
    case TokenIdentity::collapse_colors:
    case TokenIdentity::segmented_colors:
    case TokenIdentity::modal_colors:
    case TokenIdentity::tag_colors:
    case TokenIdentity::input_number_colors:
    case TokenIdentity::badge_colors:
    case TokenIdentity::drawer_colors:
    case TokenIdentity::popconfirm_colors:
    case TokenIdentity::alert_colors:
    case TokenIdentity::spin_colors:
    case TokenIdentity::progress_colors:
    case TokenIdentity::dropdown_order:
    case TokenIdentity::pagination_colors:
    case TokenIdentity::table_colors:
    case TokenIdentity::empty_colors:
    case TokenIdentity::result_colors:
    case TokenIdentity::select_order:
        return DirtyPhase::paint_material;
    case TokenIdentity::alias_color_focus_outline:
        return DirtyPhase::paint_material;
    case TokenIdentity::alias_line_width_focus:
    case TokenIdentity::alias_focus_outline_offset:
    case TokenIdentity::alias_box_shadow:
    case TokenIdentity::alias_box_shadow_secondary:
    case TokenIdentity::alias_box_shadow_tertiary:
    case TokenIdentity::button_border_radius:
    case TokenIdentity::input_border_radius:
    case TokenIdentity::button_border_width:
    case TokenIdentity::button_shadows:
    case TokenIdentity::switch_effects:
    case TokenIdentity::checkbox_effects:
    case TokenIdentity::radio_effects:
    case TokenIdentity::input_shadows:
    case TokenIdentity::tooltip_shadow:
    case TokenIdentity::dropdown_effects:
    case TokenIdentity::select_effects:
    case TokenIdentity::card_effects:
    case TokenIdentity::tabs_effects:
    case TokenIdentity::collapse_effects:
    case TokenIdentity::segmented_effects:
    case TokenIdentity::modal_effects:
    case TokenIdentity::drawer_effects:
    case TokenIdentity::popconfirm_effects:
        return DirtyPhase::geometry | DirtyPhase::paint_material;
    case TokenIdentity::map_font_size_small:
    case TokenIdentity::map_font_size:
    case TokenIdentity::map_font_size_large:
    case TokenIdentity::map_line_height_small:
    case TokenIdentity::map_line_height:
    case TokenIdentity::map_line_height_large:
    case TokenIdentity::button_typography:
    case TokenIdentity::input_typography:
    case TokenIdentity::text_font_family:
    case TokenIdentity::text_font_weight:
    case TokenIdentity::text_font_size:
    case TokenIdentity::text_line_height:
    case TokenIdentity::typography_headings:
    case TokenIdentity::typography_fonts:
    case TokenIdentity::typography_base_typography:
    case TokenIdentity::divider_typography:
    case TokenIdentity::tooltip_typography:
    case TokenIdentity::menu_typography:
    case TokenIdentity::select_typography:
    case TokenIdentity::card_typography:
    case TokenIdentity::tabs_typography:
    case TokenIdentity::collapse_typography:
    case TokenIdentity::modal_typography:
    case TokenIdentity::drawer_typography:
    case TokenIdentity::popconfirm_typography:
        return DirtyPhase::text | DirtyPhase::measure_layout;
    case TokenIdentity::tag_metrics:
    case TokenIdentity::segmented_metrics:
    case TokenIdentity::alert_metrics:
    case TokenIdentity::spin_metrics:
    case TokenIdentity::progress_metrics:
    case TokenIdentity::input_number_metrics:
        return DirtyPhase::text | DirtyPhase::measure_layout | DirtyPhase::geometry | DirtyPhase::hit_test;
    case TokenIdentity::badge_metrics:
        return DirtyPhase::text | DirtyPhase::measure_layout | DirtyPhase::geometry;
    case TokenIdentity::map_size_xs:
    case TokenIdentity::seed_line_width:
    case TokenIdentity::map_size_small:
    case TokenIdentity::map_size:
    case TokenIdentity::map_size_large:
    case TokenIdentity::map_control_height_small:
    case TokenIdentity::map_control_height:
    case TokenIdentity::map_control_height_large:
    case TokenIdentity::button_control_heights:
    case TokenIdentity::input_layout_metrics:
    case TokenIdentity::menu_metrics:
    case TokenIdentity::dropdown_metrics:
    case TokenIdentity::select_metrics:
    case TokenIdentity::card_metrics:
    case TokenIdentity::tabs_metrics:
    case TokenIdentity::collapse_metrics:
    case TokenIdentity::modal_metrics:
    case TokenIdentity::drawer_metrics:
    case TokenIdentity::popconfirm_metrics:
    case TokenIdentity::pagination_metrics:
    case TokenIdentity::table_metrics:
    case TokenIdentity::switch_geometry:
    case TokenIdentity::checkbox_metrics:
    case TokenIdentity::radio_metrics:
    case TokenIdentity::button_padding_inline:
    case TokenIdentity::button_icon_gap:
    case TokenIdentity::typography_metrics:
    case TokenIdentity::typography_inline_code:
    case TokenIdentity::typography_inline_keyboard:
    case TokenIdentity::divider_metrics:
    case TokenIdentity::slider_metrics:
    case TokenIdentity::tooltip_metrics:
        return DirtyPhase::measure_layout | DirtyPhase::geometry | DirtyPhase::hit_test;
    case TokenIdentity::map_border_radius_small:
    case TokenIdentity::map_border_radius:
    case TokenIdentity::map_border_radius_large:
        return DirtyPhase::geometry | DirtyPhase::paint_material;
    case TokenIdentity::map_motion_unit:
    case TokenIdentity::map_motion_base:
    case TokenIdentity::map_motion_enabled:
        return DirtyPhase::animation;
    case TokenIdentity::modal_order:
    case TokenIdentity::drawer_order:
    case TokenIdentity::popconfirm_order:
        return DirtyPhase::paint_material | DirtyPhase::hit_test;
    case TokenIdentity::count:
        return DirtyPhase::none;
    }
    return DirtyPhase::none;
}

} // namespace ryn::theme_runtime
