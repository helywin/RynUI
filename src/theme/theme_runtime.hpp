#pragma once

#include "theme/theme_runtime_types.hpp"

#include <ryn/theme.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <thread>
#include <vector>

namespace ryn::theme_runtime {

class ThemeScope;

class Subscription final {
public:
    Subscription() = default;
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    Subscription(Subscription&& other) noexcept;
    Subscription& operator=(Subscription&& other) noexcept;
    ~Subscription();

    void reset() noexcept;
    [[nodiscard]] bool active() const noexcept;

private:
    friend class ThemeScope;
    struct State;

    explicit Subscription(std::shared_ptr<State> state) noexcept;
    std::shared_ptr<State> state_;
};

class ThemeScope final : public std::enable_shared_from_this<ThemeScope> {
public:
    using InvalidationCallback = std::function<void(DirtyPhase)>;

    [[nodiscard]] static std::shared_ptr<ThemeScope> create_default();
    [[nodiscard]] static std::shared_ptr<ThemeScope> create(std::shared_ptr<ThemeScope> parent,
                                                            const ThemeConfig& config);

    ThemeScope(const ThemeScope&) = delete;
    ThemeScope& operator=(const ThemeScope&) = delete;

    bool update(const ThemeConfig& config);

    [[nodiscard]] const ThemeSnapshot& snapshot() const;
    [[nodiscard]] std::uint64_t generation() const;
    [[nodiscard]] Diagnostics diagnostics() const;
    [[nodiscard]] std::span<const TokenIdentity> changed_identities() const;

    Subscription capture(InvalidationCallback callback, const std::function<void()>& typed_token_reads);

    [[nodiscard]] const ThemeAliasToken& alias() const;
    [[nodiscard]] const ThemeMapToken& map() const;
    [[nodiscard]] const ButtonThemeToken& button() const;
    [[nodiscard]] const TextThemeToken& text() const;
    [[nodiscard]] const detail::InputTokenSet& input_layout_metrics() const;
    [[nodiscard]] const detail::InputTokenSet& input_typography() const;
    [[nodiscard]] const detail::InputTokenSet& input_border_radius() const;
    [[nodiscard]] const detail::InputTokenSet& input_colors() const;
    [[nodiscard]] const detail::InputTokenSet& input_shadows() const;
    [[nodiscard]] const SwitchThemeToken& switch_geometry() const;
    [[nodiscard]] const SwitchThemeToken& switch_colors() const;
    [[nodiscard]] const SwitchThemeToken& switch_effects() const;
    [[nodiscard]] const CheckboxThemeToken& checkbox_metrics() const;
    [[nodiscard]] const CheckboxThemeToken& checkbox_colors() const;
    [[nodiscard]] const CheckboxThemeToken& checkbox_effects() const;
    [[nodiscard]] const RadioThemeToken& radio_metrics() const;
    [[nodiscard]] const RadioThemeToken& radio_colors() const;
    [[nodiscard]] const RadioThemeToken& radio_effects() const;
    [[nodiscard]] const SliderThemeToken& slider_colors() const;
    [[nodiscard]] const SliderThemeToken& slider_metrics() const;
    [[nodiscard]] const TooltipThemeToken& tooltip_colors() const;
    [[nodiscard]] const MenuThemeToken& menu_colors() const;
    [[nodiscard]] const MenuThemeToken& menu_metrics() const;
    [[nodiscard]] const MenuThemeToken& menu_typography() const;
    [[nodiscard]] const DropdownThemeToken& dropdown_colors() const;
    [[nodiscard]] const DropdownThemeToken& dropdown_metrics() const;
    [[nodiscard]] const DropdownThemeToken& dropdown_effects() const;
    [[nodiscard]] const DropdownThemeToken& dropdown_order() const;
    [[nodiscard]] const SelectThemeToken& select_colors() const;
    [[nodiscard]] const TimePickerThemeToken& time_picker_colors() const;
    [[nodiscard]] const TimePickerThemeToken& time_picker_metrics() const;
    [[nodiscard]] const TimePickerThemeToken& time_picker_effects() const;
    [[nodiscard]] const TimePickerThemeToken& time_picker_order() const;
    [[nodiscard]] const SelectThemeToken& select_metrics() const;
    [[nodiscard]] const SelectThemeToken& select_typography() const;
    [[nodiscard]] const SelectThemeToken& select_effects() const;
    [[nodiscard]] const SelectThemeToken& select_order() const;
    [[nodiscard]] const CardThemeToken& card_colors() const;
    [[nodiscard]] const CardThemeToken& card_metrics() const;
    [[nodiscard]] const CardThemeToken& card_typography() const;
    [[nodiscard]] const CardThemeToken& card_effects() const;
    [[nodiscard]] const TabsThemeToken& tabs_colors() const;
    [[nodiscard]] const TabsThemeToken& tabs_metrics() const;
    [[nodiscard]] const TabsThemeToken& tabs_typography() const;
    [[nodiscard]] const TabsThemeToken& tabs_effects() const;
    [[nodiscard]] const CollapseThemeToken& collapse_colors() const;
    [[nodiscard]] const CollapseThemeToken& collapse_metrics() const;
    [[nodiscard]] const CollapseThemeToken& collapse_typography() const;
    [[nodiscard]] const CollapseThemeToken& collapse_effects() const;
    [[nodiscard]] const SegmentedThemeToken& segmented_colors() const;
    [[nodiscard]] const SegmentedThemeToken& segmented_metrics() const;
    [[nodiscard]] const SegmentedThemeToken& segmented_effects() const;
    [[nodiscard]] const DatePickerThemeToken& date_picker_colors() const;
    [[nodiscard]] const DatePickerThemeToken& date_picker_metrics() const;
    [[nodiscard]] const DatePickerThemeToken& date_picker_effects() const;
    [[nodiscard]] const DatePickerThemeToken& date_picker_order() const;
    [[nodiscard]] const FormThemeToken& form_colors() const;
    [[nodiscard]] const FormThemeToken& form_metrics() const;
    [[nodiscard]] const DescriptionsThemeToken& descriptions_colors() const;
    [[nodiscard]] const DescriptionsThemeToken& descriptions_metrics() const;
    [[nodiscard]] const TimelineThemeToken& timeline_colors() const;
    [[nodiscard]] const TimelineThemeToken& timeline_metrics() const;
    [[nodiscard]] const BreadcrumbThemeToken& breadcrumb_colors() const;
    [[nodiscard]] const BreadcrumbThemeToken& breadcrumb_metrics() const;
    [[nodiscard]] const BreadcrumbThemeToken& breadcrumb_effects() const;
    [[nodiscard]] const StepsThemeToken& steps_colors() const;
    [[nodiscard]] const StepsThemeToken& steps_metrics() const;
    [[nodiscard]] const StepsThemeToken& steps_effects() const;
    [[nodiscard]] const SkeletonThemeToken& skeleton_colors() const;
    [[nodiscard]] const SkeletonThemeToken& skeleton_metrics() const;
    [[nodiscard]] const NoticeThemeToken& message_colors() const;
    [[nodiscard]] const NoticeThemeToken& message_metrics() const;
    [[nodiscard]] const NoticeThemeToken& message_effects() const;
    [[nodiscard]] const NoticeThemeToken& message_order() const;
    [[nodiscard]] const NoticeThemeToken& notification_colors() const;
    [[nodiscard]] const NoticeThemeToken& notification_metrics() const;
    [[nodiscard]] const NoticeThemeToken& notification_effects() const;
    [[nodiscard]] const NoticeThemeToken& notification_order() const;
    [[nodiscard]] const ModalThemeToken& modal_colors() const;
    [[nodiscard]] const PaginationThemeToken& pagination_colors() const;
    [[nodiscard]] const PaginationThemeToken& pagination_metrics() const;
    [[nodiscard]] const TableThemeToken& table_colors() const;
    [[nodiscard]] const TableThemeToken& table_metrics() const;
    [[nodiscard]] const EmptyThemeToken& empty_colors() const;
    [[nodiscard]] const EmptyThemeToken& empty_metrics() const;
    [[nodiscard]] const ResultThemeToken& result_colors() const;
    [[nodiscard]] const ResultThemeToken& result_metrics() const;
    [[nodiscard]] const ModalThemeToken& modal_metrics() const;
    [[nodiscard]] const ModalThemeToken& modal_typography() const;
    [[nodiscard]] const ModalThemeToken& modal_effects() const;
    [[nodiscard]] const ModalThemeToken& modal_order() const;
    [[nodiscard]] const TagThemeToken& tag_colors() const;
    [[nodiscard]] const InputNumberThemeToken& input_number_colors() const;
    [[nodiscard]] const InputNumberThemeToken& input_number_metrics() const;
    [[nodiscard]] const TagThemeToken& tag_metrics() const;
    [[nodiscard]] const BadgeThemeToken& badge_colors() const;
    [[nodiscard]] const BadgeThemeToken& badge_metrics() const;
    [[nodiscard]] const DrawerThemeToken& drawer_colors() const;
    [[nodiscard]] const DrawerThemeToken& drawer_metrics() const;
    [[nodiscard]] const DrawerThemeToken& drawer_typography() const;
    [[nodiscard]] const DrawerThemeToken& drawer_effects() const;
    [[nodiscard]] const DrawerThemeToken& drawer_order() const;
    [[nodiscard]] const PopconfirmThemeToken& popconfirm_colors() const;
    [[nodiscard]] const PopconfirmThemeToken& popconfirm_metrics() const;
    [[nodiscard]] const PopconfirmThemeToken& popconfirm_typography() const;
    [[nodiscard]] const PopconfirmThemeToken& popconfirm_effects() const;
    [[nodiscard]] const PopconfirmThemeToken& popconfirm_order() const;
    [[nodiscard]] const PopoverThemeToken& popover_colors() const;
    [[nodiscard]] const PopoverThemeToken& popover_metrics() const;
    [[nodiscard]] const PopoverThemeToken& popover_typography() const;
    [[nodiscard]] const PopoverThemeToken& popover_effects() const;
    [[nodiscard]] const PopoverThemeToken& popover_order() const;
    [[nodiscard]] const AlertThemeToken& alert_colors() const;
    [[nodiscard]] const AlertThemeToken& alert_metrics() const;
    [[nodiscard]] const SpinThemeToken& spin_colors() const;
    [[nodiscard]] const SpinThemeToken& spin_metrics() const;
    [[nodiscard]] const ProgressThemeToken& progress_colors() const;
    [[nodiscard]] const ProgressThemeToken& progress_metrics() const;
    [[nodiscard]] const TooltipThemeToken& tooltip_metrics() const;
    [[nodiscard]] const TooltipThemeToken& tooltip_typography() const;
    [[nodiscard]] const TooltipThemeToken& tooltip_shadow() const;
    [[nodiscard]] const TooltipThemeToken& tooltip_order() const;
    [[nodiscard]] const TypographyThemeToken& typography_colors() const;
    [[nodiscard]] const TypographyThemeToken& typography_headings() const;
    [[nodiscard]] const TypographyThemeToken& typography_fonts() const;
    [[nodiscard]] const TypographyThemeToken& typography_base_typography() const;
    [[nodiscard]] const TypographyThemeToken& typography_metrics() const;
    [[nodiscard]] const InlineCodeThemeToken& typography_inline_code() const;
    [[nodiscard]] const InlineCodeThemeToken& typography_inline_keyboard() const;
    [[nodiscard]] const TypographyThemeToken& typography_inline_colors() const;
    [[nodiscard]] const DividerThemeToken& divider_colors() const;
    [[nodiscard]] const DividerThemeToken& divider_metrics() const;
    [[nodiscard]] const DividerThemeToken& divider_typography() const;

    [[nodiscard]] Color text_color() const;
    [[nodiscard]] Color primary_color() const;
    [[nodiscard]] SystemFontFamily text_font_family() const;
    [[nodiscard]] std::uint32_t text_font_weight() const;
    [[nodiscard]] float text_font_size() const;
    [[nodiscard]] float text_line_height() const;
    [[nodiscard]] float line_width() const;
    [[nodiscard]] Color text_secondary_color() const;
    [[nodiscard]] Color text_disabled_color() const;
    [[nodiscard]] SystemFontFamily code_font_family() const;

    [[nodiscard]] const ButtonThemeToken& button_colors() const;
    [[nodiscard]] const ButtonThemeToken& button_control_heights() const;
    [[nodiscard]] const ButtonThemeToken& button_padding_inline() const;
    [[nodiscard]] const ButtonThemeToken& button_typography() const;
    [[nodiscard]] const ButtonThemeToken& button_border_radius() const;
    [[nodiscard]] float button_border_width() const;
    [[nodiscard]] float button_icon_gap() const;
    [[nodiscard]] const ButtonThemeToken& button_shadows() const;
    [[nodiscard]] Color focus_outline_color() const;
    [[nodiscard]] Color color_link_hover() const;
    [[nodiscard]] Color color_link_active() const;
    [[nodiscard]] float focus_outline_width() const;
    [[nodiscard]] float focus_outline_offset() const;

    [[nodiscard]] float layout_gap_small() const;
    [[nodiscard]] float layout_gap_middle() const;
    [[nodiscard]] float layout_gap_large() const;
    [[nodiscard]] Duration motion_unit() const;
    [[nodiscard]] Duration motion_base() const;
    [[nodiscard]] bool motion_enabled() const;

private:
    struct Subscriber;

    ThemeScope(std::shared_ptr<ThemeScope> parent, const ThemeConfig& config);
    void ensure_owner_thread() const;
    void record(TokenIdentity identity) const;
    void validate_children(const ThemeSnapshot& parent_snapshot) const;
    void recompute_from_parent();
    void commit_snapshot(const ThemeConfig& config, ThemeSnapshot&& next);
    void notify_subscribers();
    void notify_children();
    [[nodiscard]] std::size_t live_subscriber_count() const noexcept;

    std::shared_ptr<ThemeScope> parent_;
    ThemeConfig config_;
    std::shared_ptr<const ThemeSnapshot> snapshot_;
    std::vector<std::weak_ptr<ThemeScope>> children_;
    std::vector<Subscriber> subscribers_;
    std::array<TokenIdentity, static_cast<std::size_t>(TokenIdentity::count)> changed_identities_{};
    std::size_t changed_identity_count_{0};
    Diagnostics diagnostics_;
    std::thread::id owner_thread_;
};

} // namespace ryn::theme_runtime
