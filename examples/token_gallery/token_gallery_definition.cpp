#include "runtime/prop_connection.hpp"
#include "token_gallery_definition.hpp"

#include "ant_design_reference_catalog.hpp"
#include "gallery_document_model.hpp"
#include "gallery_layout.hpp"
#include "gallery_sample_registry.hpp"
#include "gallery_test_inventory.hpp"
#include "gallery_draft_codec.hpp"
#include "gallery_i18n.hpp"
#include "reference_surface.hpp"
#include "icon_samples.hpp"

#include <ryn/rynui.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace rynui::example {
namespace {

#include "generated_media_fixture.inc"

struct GalleryRecord final {
    ryn::String key;
    ryn::String name;
    std::optional<double> quantity;
    bool locked{};
    friend bool operator==(const GalleryRecord&, const GalleryRecord&) = default;

    friend auto gallery_draft_fields(GalleryRecord& value) {
        return std::tie(value.key, value.name, value.quantity, value.locked);
    }

    friend auto gallery_draft_fields(const GalleryRecord& value) {
        return std::tie(value.key, value.name, value.quantity, value.locked);
    }
};

std::vector<GalleryRecord> gallery_records() {
    std::vector<GalleryRecord> result;
    for (int index = 1; index <= 12; ++index) {
        result.push_back({ryn::String::from_utf8("record-" + std::to_string(index)).value(),
                          ryn::String::from_utf8("任务 " + std::to_string(index)).value(),
                          static_cast<double>(index * 3), index == 12});
    }
    return result;
}

ryn::String gallery_validation_issue(const ryn::Signal<std::optional<GalleryMessage>>& issue,
                                     std::optional<GalleryMessage> next) {
    issue.set(next);
    return next ? ryn::translate(ryn::LocaleCatalog::builtin(), ryn::locales::en_US, next->key, next->args)
                : ryn::String{};
}

template <ryn::FormFieldType T>
void gallery_validated_item(ryn::FormItemProps props, ryn::FormField<T> field,
                            ryn::Signal<std::optional<GalleryMessage>> issue, ryn::Binding<ryn::String>,
                            GalleryFeedback, ryn::FormItemContent content,
                            ryn::FormIssueSource origin = ryn::FormIssueSource::Validator) {
    // The Form owns validation, dirtiness, focus and result snapshots. This app
    // projects only its declared custom issue; an unrelated literal stays literal.
    const auto tr = ryn::useI18n();
    ryn::FormItem(props.errorText(ryn::bind([=] {
        const auto error = field.error();
        const auto source = issue.get();
        if (source && field.errorSource() == origin) {
            return tr.translate(source->key, source->args);
        }
        return error;
    })),
                  content);
}

struct GalleryState final {
    ryn::I18n i18n;
    GalleryPageDraft draft;
    GalleryInputFamily page_family{};
    std::string page_component;
    bool register_targets{};
    bool readable_labels{};
    std::map<std::string, ryn::runtime::ComponentId> targets;
    std::optional<ryn::ThemeConfig> theme_request;

    ryn::String initial(std::string_view key, ryn::String fallback) const {
        return ryn::String::from_utf8(draft.get<std::string>(std::string{key}, std::string{fallback.bytes()})).value();
    }

    void remember(std::string_view key, const ryn::String& value) {
        draft.values[std::string{key}] = std::string{value.bytes()};
    }

    ryn::Signal<ryn::ThemeConfig> theme{ryn::ThemeConfig{}};
    ryn::Color background_color{ryn::Color::rgba8(255, 255, 255)};
    ryn::Signal<ryn::LogicalLength> gallery_width{ryn::dp(1120.0F)};
    ryn::Signal<ryn::LogicalLength> navigation_width{ryn::dp(216.0F)};
    ryn::Signal<ryn::LogicalLength> document_width{ryn::dp(840.0F)};
    ryn::Signal<ryn::LogicalLength> cell_width{ryn::dp(260.0F)};
    ryn::Signal<bool> narrow_layout{false};
    ryn::Signal<ryn::SplitterSizes> splitter_percentages{ryn::SplitterSizes{35, 65}};
    ryn::Signal<bool> splitter_resizable{true};
    ryn::Signal<bool> splitter_destroy{false};
    ryn::Signal<bool> splitter_motion{true};
    ryn::Signal<ryn::SplitterOrientation> splitter_orientation{ryn::SplitterOrientation::Horizontal};
    GalleryFeedback splitter_feedback{GalleryMessage{"gallery.feedback.000", {}}};
    ryn::Signal<bool> shell_collapsed{false};
    ryn::Signal<bool> shell_light{false};
    GalleryFeedback shell_feedback{GalleryMessage{"gallery.feedback.001", {}}};
    ryn::Signal<ryn::LogicalLength> navigation_track_height{ryn::dp(1.0F)};
    ryn::Signal<ryn::LogicalLength> navigation_thumb_height{ryn::dp(1.0F)};
    ryn::Signal<ryn::LogicalLength> document_track_height{ryn::dp(1.0F)};
    ryn::Signal<ryn::LogicalLength> document_thumb_height{ryn::dp(1.0F)};
    ryn::Signal<bool> navigation_bar_visible{true};
    ryn::Signal<bool> document_bar_visible{true};
    ryn::Signal<bool> disabled{true};
    ryn::Signal<bool> loading{true};
    ryn::Signal<bool> button_loading{false};
    ryn::ButtonRef button_ref;
    ryn::Signal<bool> clear_disabled{false};
    ryn::Signal<bool> switch_checked{false};
    ryn::SwitchRef switch_ref;
    GalleryFeedback selection_focus{GalleryMessage{"gallery.feedback.002", {}}};
    ryn::Signal<bool> checkbox_checked{false};
    ryn::CheckboxRef checkbox_ref;
    ryn::Signal<ryn::CheckboxValues> checkbox_values{ryn::CheckboxValues{ryn::String{u8"desktop"}}};
    ryn::Signal<std::vector<ryn::CheckboxOption>> checkbox_options{std::vector<ryn::CheckboxOption>{
        {ryn::String{u8"a"}, ryn::String{u8"A"}}, {ryn::String{u8"b"}, ryn::String{u8"B"}}}};
    ryn::Signal<bool> checkbox_options_expanded{false};
    ryn::Signal<std::optional<ryn::String>> radio_selected{std::optional<ryn::String>{ryn::String{u8"a"}}};
    ryn::Signal<std::vector<ryn::RadioOption>> radio_options{std::vector<ryn::RadioOption>{
        {ryn::String{u8"a"}, ryn::String{u8"A"}}, {ryn::String{u8"b"}, ryn::String{u8"B"}}}};
    ryn::Signal<bool> radio_options_expanded{false};
    ryn::RadioRef radio_ref;
    ryn::Signal<ryn::String> input_value{ryn::String{u8""}};
    GalleryFeedback input_feedback{GalleryMessage{"gallery.feedback.003", {}}};
    ryn::Signal<ryn::String> search_value{ryn::String{}};
    GalleryFeedback search_feedback{GalleryMessage{"gallery.feedback.004", {}}};
    ryn::InputRef input_ref;
    ryn::Signal<bool> input_clear_disabled{false};
    ryn::Signal<bool> password_toggle{true};
    ryn::Signal<bool> password_visible{false};
    ryn::Signal<ryn::String> otp_value{ryn::String{}};
    ryn::Signal<std::size_t> otp_length{6};
    ryn::Signal<ryn::TextDirection> text_direction{ryn::TextDirection::Auto};
    ryn::OTPRef otp_ref;
    ryn::Signal<ryn::String> typography_value{ryn::String{u8"点击编辑 · 受控正文"}};
    ryn::Signal<ryn::String> typography_title{ryn::String{u8"标题编辑继承字号"}};
    ryn::Signal<double> slider_value{30};
    ryn::Signal<ryn::SelectSelection> select_value{ryn::SelectSelection{u8"desktop"}};
    ryn::Signal<std::vector<ryn::SelectOption>> select_options{
        std::vector<ryn::SelectOption>{{u8"desktop", ryn::String{}}, {u8"service", ryn::String{}}}};
    ryn::Signal<bool> select_open{false};
    ryn::SelectRef select_ref;
    bool select_options_expanded{};
    ryn::Signal<bool> float_open{false};
    ryn::Signal<ryn::FloatButtonScrollState> float_scroll{ryn::FloatButtonScrollState{600, 1000, 160}};
    ryn::Signal<bool> card_loading{false};
    ryn::Signal<ryn::String> tabs_active{ryn::String{u8"draft"}};
    GalleryFeedback tabs_label{GalleryMessage{"gallery.complete.copy.122", {}}};
    ryn::Signal<bool> tabs_disabled{false};
    ryn::Signal<int> modal_active{-1};
    ryn::Signal<bool> modal_nested{false};
    ryn::Signal<bool> modal_loading{true};
    GalleryFeedback modal_feedback{GalleryMessage{"gallery.feedback.005", {}}};
    GalleryFeedback modal_lifecycle{GalleryMessage{"gallery.feedback.006", {}}};
    ryn::Signal<bool> drawer_open{false};
    ryn::Signal<ryn::DrawerPlacement> drawer_placement{ryn::DrawerPlacement::Right};
    ryn::Signal<float> drawer_width{420};
    ryn::Signal<float> drawer_height{340};
    ryn::Signal<bool> drawer_loading{false};
    ryn::Signal<bool> drawer_rtl{false};
    ryn::Signal<bool> drawer_destroy{false};
    ryn::Signal<bool> drawer_child_open{false};
    ryn::Signal<bool> tag_visible{true};
    ryn::Signal<bool> tag_checked{false};
    ryn::Signal<int> badge_count{3};
    ryn::Signal<bool> popconfirm_open{false};
    ryn::Signal<bool> popconfirm_loading{false};
    ryn::Signal<bool> alert_visible{true};
    ryn::Signal<bool> feedback_loading{false};
    ryn::Signal<float> feedback_percent{35};
    ryn::Signal<ryn::ProgressStatus> feedback_status{ryn::ProgressStatus::Normal};
    GalleryFeedback common_feedback{GalleryMessage{"gallery.feedback.007", {}}};
    ryn::Signal<std::vector<GalleryRecord>> records{gallery_records()};
    ryn::Signal<std::size_t> table_page{1};
    ryn::Signal<ryn::TableSort> table_sort{ryn::TableSort{}};
    ryn::Signal<std::vector<ryn::String>> table_selected{std::vector<ryn::String>{}};
    ryn::Signal<bool> table_loading{false};
    ryn::Signal<ryn::MenuKeys> menu_selected{ryn::MenuKeys{ryn::String{u8"overview"}}};
    ryn::Signal<ryn::MenuKeys> menu_open{ryn::MenuKeys{ryn::String{u8"tools"}}};
    GalleryFeedback menu_label{GalleryMessage{"gallery.complete.copy.123", {}}};
    ryn::Signal<bool> menu_disabled{false};
    ryn::Signal<bool> dropdown_open{false};
    ryn::Signal<ryn::InputNumberValue> number_value{ryn::InputNumberValue{1.2}};
    ryn::Signal<bool> number_disabled{true};
    ryn::InputRef number_ref;
    GalleryFeedback data_feedback{GalleryMessage{"gallery.feedback.008", {}}};
    ryn::Signal<ryn::CollapseKeys> collapse_keys{ryn::CollapseKeys{ryn::String{u8"general"}}};
    ryn::Signal<bool> collapse_accordion{false};
    ryn::Signal<bool> collapse_disabled{true};
    ryn::Signal<bool> collapse_select_open{false};
    ryn::Signal<ryn::String> segment_value{ryn::String{u8"day"}};
    GalleryFeedback segment_label{GalleryMessage{"gallery.complete.copy.124", {}}};
    ryn::Signal<bool> segment_disabled{false};
    GalleryFeedback panel_feedback{GalleryMessage{"gallery.feedback.009", {}}};
    GalleryFeedback empty_description{GalleryMessage{"gallery.complete.copy.125", {}}};
    ryn::Signal<ryn::EmptyImage> empty_image{ryn::EmptyImage::Default};
    ryn::Signal<ryn::ResultStatus> result_status{ryn::ResultStatus{ryn::Status::Success}};
    GalleryFeedback result_title{GalleryMessage{"gallery.complete.copy.126", {}}};
    GalleryFeedback result_feedback{GalleryMessage{"gallery.feedback.010", {}}};
    std::size_t result_status_step{};
    ryn::FormRef form_ref;
    ryn::FormField<ryn::String> form_name;
    ryn::FormField<ryn::SelectSelection> form_role;
    ryn::FormField<ryn::InputNumberValue> form_count{1.0};
    ryn::FormField<bool> form_consent;
    ryn::InputRef form_name_ref;
    ryn::SelectRef form_role_ref;
    ryn::InputRef form_count_ref;
    ryn::CheckboxRef form_consent_ref;
    ryn::Signal<bool> form_disabled{false};
    ryn::Signal<bool> form_read_only{false};
    GalleryFeedback form_help{GalleryMessage{"gallery.complete.copy.127", {}}};
    GalleryFeedback form_feedback{GalleryMessage{"gallery.feedback.011", {}}};
    ryn::Signal<ryn::DatePickerValue> date_value{ryn::CivilDate{2026, 10, 12}};
    ryn::Signal<bool> date_open{false};
    ryn::DatePickerRef date_ref;
    ryn::FormRef date_form_ref;
    ryn::FormField<ryn::String> date_field;
    ryn::DatePickerRef date_field_ref;
    ryn::Signal<bool> date_form_disabled{false};
    ryn::Signal<bool> date_form_read_only{false};
    GalleryFeedback date_help{GalleryMessage{"gallery.complete.copy.128", {}}};
    GalleryFeedback date_feedback{GalleryMessage{"gallery.feedback.012", {}}};
    ryn::Signal<ryn::AnchorScrollState> anchor_scroll{ryn::AnchorScrollState{0, 1200, 180}};
    ryn::Signal<bool> anchor_affix{false};
    ryn::Signal<bool> anchor_horizontal{false};
    ryn::Signal<bool> anchor_extra{true};
    GalleryFeedback anchor_feedback{GalleryMessage{"gallery.feedback.013", {}}};
    ryn::Signal<bool> breadcrumb_reversed{false};
    ryn::Signal<bool> breadcrumb_rtl{false};
    ryn::Signal<bool> breadcrumb_extra{true};
    GalleryFeedback breadcrumb_title{GalleryMessage{"gallery.complete.copy.130", {}}};
    ryn::Signal<bool> breadcrumb_visible{true};
    ryn::Signal<bool> breadcrumb_draft_visible{true};
    ryn::Signal<bool> breadcrumb_disabled{false};
    ryn::Signal<ryn::String> breadcrumb_draft{ryn::String{u8"Breadcrumb draft"}};
    GalleryFeedback breadcrumb_feedback{GalleryMessage{"gallery.feedback.014", {}}};
    ryn::InputRef breadcrumb_ref;
    ryn::Signal<std::size_t> steps_current{0};
    ryn::Signal<bool> steps_vertical{false};
    ryn::Signal<ryn::StepsSize> steps_size{ryn::StepsSize::Default};
    ryn::Signal<ryn::StepsStatus> steps_status{ryn::StepsStatus::Process};
    ryn::Signal<bool> steps_visible{true};
    ryn::Signal<bool> steps_disabled{false};
    ryn::Signal<ryn::String> steps_draft{ryn::String{u8"Steps draft"}};
    GalleryFeedback steps_feedback{GalleryMessage{"gallery.feedback.015", {}}};
    ryn::InputRef steps_ref;
    ryn::Signal<ryn::TimePickerValue> time_value{ryn::TimeOfDay{8}};
    ryn::Signal<bool> time_open{false};
    ryn::Signal<bool> time_confirm{true};
    ryn::Signal<bool> time_accept{true};
    ryn::Signal<bool> time_input_read_only{false};
    GalleryFeedback time_request{GalleryMessage{"gallery.feedback.016", {}}};
    ryn::TimePickerRef time_ref;
    ryn::FormRef time_form_ref;
    ryn::FormField<ryn::String> time_field{ryn::String{u8"08:00:00"}};
    ryn::TimePickerRef time_field_ref;
    ryn::Signal<bool> time_form_disabled{false};
    ryn::Signal<bool> time_form_read_only{false};
    GalleryFeedback time_help{GalleryMessage{"gallery.complete.copy.129", {}}};
    GalleryFeedback time_feedback{GalleryMessage{"gallery.feedback.017", {}}};
    ryn::Signal<bool> details_columns{true};
    ryn::Signal<int> details_span{2};
    ryn::Signal<bool> details_visible{true};
    ryn::Signal<bool> details_vertical{false};
    ryn::Signal<bool> details_bordered{true};
    ryn::Signal<ryn::DescriptionsSize> details_size{ryn::DescriptionsSize::Default};
    ryn::Signal<ryn::String> details_name{ryn::String{u8"晨间项目"}};
    ryn::Signal<ryn::SelectSelection> details_role{ryn::SelectSelection{u8"editor"}};
    ryn::Signal<bool> details_enabled{true};
    GalleryFeedback details_feedback{GalleryMessage{"gallery.feedback.018", {}}};
    ryn::InputRef details_ref;
    ryn::Signal<bool> skeleton_loading{false};
    ryn::Signal<bool> skeleton_title{true};
    ryn::Signal<int> skeleton_paragraph{0};
    ryn::Signal<bool> skeleton_avatar{true};
    ryn::Signal<bool> skeleton_round{false};
    ryn::Signal<bool> skeleton_block{false};
    ryn::Signal<int> skeleton_shape{0};
    ryn::Signal<ryn::String> skeleton_draft{ryn::String{u8"今天整理项目资料"}};
    ryn::Signal<ryn::SelectSelection> skeleton_audience{ryn::SelectSelection{u8"team"}};
    GalleryFeedback skeleton_feedback{GalleryMessage{"gallery.feedback.019", {}}};
    ryn::InputRef skeleton_ref;
    ryn::Signal<bool> popover_open{false};
    ryn::Signal<bool> popover_disabled{false};
    ryn::Signal<int> popover_placement{0};
    ryn::Signal<bool> popover_arrow{true};
    ryn::Signal<ryn::String> popover_draft{ryn::String{u8"下次会议前补齐项目备注"}};
    GalleryFeedback popover_feedback{GalleryMessage{"gallery.feedback.020", {}}};
    ryn::InputRef popover_ref;
    ryn::MessageRef messages;
    ryn::NotificationRef notifications;
    ryn::Signal<bool> notification_open{false};
    ryn::Signal<ryn::NotificationPlacement> notification_placement{ryn::NotificationPlacement::TopRight};
    ryn::Signal<ryn::NoticeStatus> notification_status{ryn::NoticeStatus::Info};
    ryn::Signal<ryn::String> notification_draft{ryn::String{u8"请核对项目资料"}};
    GalleryFeedback notice_feedback{GalleryMessage{"gallery.feedback.021", {}}};
    ryn::InputRef notification_ref;
    std::optional<ryn::NoticeHandle> notice_handle;
    std::size_t notice_sequence{};
    ryn::Signal<int> timeline_mode{0};
    ryn::Signal<bool> timeline_reverse{false};
    ryn::Signal<bool> timeline_loading{true};
    ryn::Signal<bool> timeline_visible{true};
    ryn::Signal<ryn::String> timeline_draft{ryn::String{u8"记录发布后的跟进事项"}};
    GalleryFeedback timeline_feedback{GalleryMessage{"gallery.feedback.022", {}}};
    ryn::InputRef timeline_ref;
    ryn::Signal<ryn::String> member_name{ryn::String{u8"江"}};
    ryn::Signal<bool> member_icon{false};
    ryn::Signal<bool> members_visible{true};
    ryn::Signal<ryn::AvatarShape> member_shape{ryn::AvatarShape::Circle};
    ryn::Signal<ryn::AvatarSizeValue> member_size{ryn::AvatarSizeValue{ryn::AvatarSize::Large}};
    ryn::Signal<std::uint32_t> member_limit{2};
    ryn::InputRef member_ref;
    GalleryFeedback member_feedback{GalleryMessage{"gallery.feedback.023", {}}};
    ryn::Signal<ryn::StatisticValue> metric_value{ryn::StatisticValue{u8"-1234.123456789"}};
    std::size_t metric_step{};
    ryn::Signal<std::optional<int>> metric_precision{std::optional{2}};
    ryn::Signal<bool> metric_separators{false};
    ryn::Signal<bool> metric_formatter{false};
    ryn::Signal<bool> metric_loading{false};
    ryn::Signal<ryn::String> metric_note{ryn::String{u8"核对本期指标"}};
    GalleryFeedback metric_feedback{GalleryMessage{"gallery.feedback.024", {}}};
    ryn::InputRef metric_ref;
    ryn::RateRef rating_control_ref;
    ryn::Signal<ryn::RateDirection> rating_direction{ryn::RateDirection::LeftToRight};
    ryn::Signal<bool> rating_hearts{false};
    GalleryFeedback rating_events{GalleryMessage{"gallery.feedback.025", {}}};
    ryn::Signal<float> rating{2.5F};
    ryn::Signal<bool> rating_accept{true};
    ryn::Signal<bool> rating_disabled{false};
    ryn::Signal<bool> rating_read_only{false};
    ryn::Signal<bool> rating_visible{true};
    GalleryFeedback rating_request{GalleryMessage{"gallery.feedback.011", {}}};
    ryn::Signal<ryn::String> rating_note{ryn::String{u8"补充服务评价"}};
    GalleryFeedback rating_feedback{GalleryMessage{"gallery.feedback.026", {}}};
    ryn::InputRef rating_ref;
    bool select_multi_accept{true};
    ryn::Signal<ryn::SelectValues> select_multi{ryn::SelectValues{ryn::SelectValue{u8"desktop"}}};
    GalleryFeedback select_multi_feedback{GalleryMessage{"gallery.feedback.027", {}}};
    std::vector<std::function<void()>> completion_jobs;
    std::vector<std::function<void()>> owner_posts;
    GalleryFeedback async_feedback{GalleryMessage{"gallery.feedback.028", {}}};
    ryn::FormRef mentions_form;
    ryn::FormField<ryn::String> mentions_field{ryn::String{u8"在这里输入 @ 或 #\n"}};
    ryn::MentionsRef mentions_ref;
    ryn::Signal<ryn::String> mentions_prefix{ryn::String{u8"@"}};
    ryn::Signal<ryn::String> mentions_note{ryn::String{u8"可编辑富建议草稿"}};
    GalleryFeedback mentions_feedback{GalleryMessage{"gallery.feedback.029", {}}};
    GalleryFeedback mentions_async_feedback{GalleryMessage{"gallery.feedback.030", {}}};
    ryn::Signal<bool> mentions_accept{true};
    ryn::Signal<bool> mentions_disabled{false};
    ryn::Signal<bool> mentions_read_only{false};
    ryn::Signal<bool> mentions_visible{true};
    ryn::Signal<bool> mentions_rtl{false};
    ryn::Signal<bool> mentions_top{false};
    ryn::Signal<ryn::String> contact_value{ryn::String{}};
    GalleryFeedback contact_request{GalleryMessage{"gallery.feedback.031", {}}};
    ryn::Signal<bool> contact_accept{true};
    ryn::Signal<bool> contact_extra{false};
    ryn::Signal<bool> contact_disabled{false};
    ryn::Signal<bool> contact_read_only{false};
    ryn::Signal<ryn::String> contact_note{ryn::String{u8"联系人备注"}};
    GalleryFeedback contact_saved{GalleryMessage{"gallery.complete.copy.131", {}}};
    ryn::AutoCompleteRef contact_ref;
    ryn::InputRef contact_note_ref;
    ryn::FormRef contact_form;
    ryn::FormField<ryn::String> contact_field{ryn::String{}};
    ryn::AutoCompleteRef contact_form_ref;
    GalleryFeedback contact_form_feedback{GalleryMessage{"gallery.feedback.032", {}}};
    ryn::Signal<ryn::TreeKeys> project_expanded{ryn::TreeKeys{u8"project", u8"src", u8"docs"}};
    ryn::Signal<ryn::TreeKeys> project_selected{ryn::TreeKeys{}};
    ryn::Signal<ryn::TreeKeys> project_checked{ryn::TreeKeys{}};
    ryn::Signal<bool> project_disabled{false};
    ryn::Signal<bool> project_visible{true};
    ryn::Signal<bool> project_locked{true};
    ryn::Signal<ryn::String> project_note{ryn::String{u8"项目处理备注"}};
    GalleryFeedback project_saved{GalleryMessage{"gallery.complete.copy.132", {}}};
    ryn::InputRef project_note_ref;
    ryn::Signal<ryn::String> destination_value{ryn::String{u8"guide"}};
    ryn::Signal<bool> destination_open{false};
    ryn::Signal<ryn::TreeKeys> destination_expanded{ryn::TreeKeys{u8"project", u8"docs"}};
    ryn::Signal<bool> destination_accept_value{true};
    ryn::Signal<bool> destination_accept_close{true};
    ryn::Signal<bool> destination_renamed{false};
    ryn::Signal<bool> destination_archive_disabled{true};
    ryn::Signal<bool> destination_disabled{false};
    ryn::Signal<bool> destination_read_only{false};
    ryn::Signal<bool> destination_visible{true};
    ryn::Signal<int> destination_closes{0};
    GalleryFeedback destination_request{GalleryMessage{"gallery.feedback.033", {}}};
    ryn::Signal<ryn::String> destination_note{ryn::String{u8"安排项目交付"}};
    GalleryFeedback destination_saved{GalleryMessage{"gallery.complete.copy.133", {}}};
    ryn::TreeSelectRef destination_ref;
    ryn::InputRef destination_note_ref;
    ryn::FormRef destination_form;
    ryn::FormField<ryn::String> destination_field{ryn::String{}};
    ryn::TreeSelectRef destination_form_ref;
    GalleryFeedback destination_form_feedback{GalleryMessage{"gallery.feedback.032", {}}};
    ryn::Signal<ryn::SliderRange> slider_range{ryn::SliderRange{20, 80}};
    ryn::Signal<ryn::IconSource> icon_source{ryn::IconSource{ryn::IconName::HeartTwoTone}};
    ryn::Signal<float> icon_angle{0};
    ryn::Signal<bool> icon_spin{false};
    std::size_t icon_source_step{};
    ryn::Signal<GallerySupportFilter> support_filter{GallerySupportFilter::all};
    ryn::Signal<GalleryNavigationTarget> active_navigation{
        GalleryNavigationTarget::to_section(GalleryDocumentSectionKind::header_source)};
    std::optional<GalleryNavigationTarget> navigation_request;
    TokenGalleryTelemetry telemetry;
    std::vector<GallerySamplePage> frame_pages;

    void bind_messages(ryn::I18n tr) {
        i18n = tr;
        destination_saved.context(tr);
        project_saved.context(tr);
        contact_saved.context(tr);
        breadcrumb_title.context(tr);
        time_help.context(tr);
        date_help.context(tr);
        form_help.context(tr);
        result_title.context(tr);
        empty_description.context(tr);
        segment_label.context(tr);
        menu_label.context(tr);
        tabs_label.context(tr);
        splitter_feedback.context(tr);
        shell_feedback.context(tr);
        selection_focus.context(tr);
        input_feedback.context(tr);
        search_feedback.context(tr);
        modal_feedback.context(tr);
        modal_lifecycle.context(tr);
        common_feedback.context(tr);
        data_feedback.context(tr);
        panel_feedback.context(tr);
        result_feedback.context(tr);
        form_feedback.context(tr);
        date_feedback.context(tr);
        anchor_feedback.context(tr);
        breadcrumb_feedback.context(tr);
        steps_feedback.context(tr);
        time_request.context(tr);
        time_feedback.context(tr);
        details_feedback.context(tr);
        skeleton_feedback.context(tr);
        popover_feedback.context(tr);
        notice_feedback.context(tr);
        timeline_feedback.context(tr);
        member_feedback.context(tr);
        metric_feedback.context(tr);
        rating_events.context(tr);
        rating_request.context(tr);
        rating_feedback.context(tr);
        select_multi_feedback.context(tr);
        async_feedback.context(tr);
        mentions_feedback.context(tr);
        mentions_async_feedback.context(tr);
        contact_request.context(tr);
        contact_form_feedback.context(tr);
        destination_request.context(tr);
        destination_form_feedback.context(tr);
    }

    ryn::Signal<std::optional<GalleryMessage>> count_issue{std::optional<GalleryMessage>{}};
    ryn::Signal<std::optional<GalleryMessage>> date_issue{std::optional<GalleryMessage>{}};
    ryn::Signal<std::optional<GalleryMessage>> time_issue{std::optional<GalleryMessage>{}};

    GalleryState() {
        destination_field.required();
        form_name.required();
        form_role.required();
        form_count.required().validator([issue = count_issue](ryn::InputNumberValue value) {
            return gallery_validation_issue(
                issue, value && *value < 1 ? std::optional{GalleryMessage{"gallery.complete.copy.145",
                                                                          {{u8"minimum", std::uint64_t{1}}}}}
                                           : std::nullopt);
        });
        form_consent.required();
        date_field.required().validator([issue = date_issue](ryn::String value) {
            if (value.empty()) {
                return gallery_validation_issue(issue, std::nullopt);
            }
            const auto date = ryn::parse_iso_date(value.bytes());
            if (!date) {
                return gallery_validation_issue(issue, GalleryMessage{"gallery.complete.copy.146", {}});
            }
            return gallery_validation_issue(issue, *date < ryn::CivilDate{2026, 10, 5} ||
                                                           *date > ryn::CivilDate{2026, 11, 25} || date->day() == 13
                                                       ? std::optional{GalleryMessage{"gallery.complete.copy.147", {}}}
                                                       : std::nullopt);
        });
        time_field.required().validator([issue = time_issue](ryn::String value) {
            if (value.empty()) {
                return gallery_validation_issue(issue, std::nullopt);
            }
            const auto time = ryn::parse_time_of_day(value.bytes());
            if (!time) {
                return gallery_validation_issue(issue, GalleryMessage{"gallery.complete.copy.148", {}});
            }
            return gallery_validation_issue(issue, *time < ryn::TimeOfDay{8} || *time > ryn::TimeOfDay{18} ||
                                                           time->hour() == 12
                                                       ? std::optional{GalleryMessage{"gallery.complete.copy.149", {}}}
                                                       : std::nullopt);
        });
    }

    void set_theme(const ryn::ThemeConfig& config, bool brand = false) {
        auto next = config;
        if (register_targets) {
            const auto current = theme.get();
            const bool compact =
                std::ranges::find(current.algorithms, ryn::ThemeAlgorithm::Compact) != current.algorithms.end();
            const bool dark =
                std::ranges::find(current.algorithms, ryn::ThemeAlgorithm::Dark) != current.algorithms.end();
            const bool density_only =
                next.algorithms.size() == 1 && next.algorithms.front() == ryn::ThemeAlgorithm::Compact;
            if (density_only) {
                next.algorithms = {dark ? ryn::ThemeAlgorithm::Dark : ryn::ThemeAlgorithm::Default,
                                   ryn::ThemeAlgorithm::Compact};
            } else if (compact &&
                       std::ranges::find(next.algorithms, ryn::ThemeAlgorithm::Compact) == next.algorithms.end()) {
                next.algorithms.push_back(ryn::ThemeAlgorithm::Compact);
            }
            if (!next.seed.color_primary) {
                next.seed.color_primary = current.seed.color_primary;
            }
            if (!next.seed.motion) {
                next.seed.motion = current.seed.motion;
            }
            theme_request = next;
        }
        background_color = ryn::resolve_theme(next).alias().color_background_container;
        theme.set(next);
        ++telemetry.theme_updates;
        if (brand) {
            ++telemetry.brand_updates;
        }
    }
};

void gallery_feedback_draft(GalleryPageDraft& draft, const std::string& key, const GalleryFeedback& feedback) {
    const auto source = feedback.source();
    const auto* message = std::get_if<GalleryMessage>(&source);
    gallery_draft_write(draft, key + "/message", message != nullptr);
    if (!message) {
        gallery_draft_write(draft, key + "/literal", std::get<ryn::String>(source));
        return;
    }
    gallery_draft_write(draft, key + "/id", message->key.id());
    gallery_draft_write(draft, key + "/count", message->args.size());
    for (std::size_t i = 0; i < message->args.size(); ++i) {
        const auto prefix = key + "/arg/" + std::to_string(i);
        gallery_draft_write(draft, prefix + "/name", message->args[i].name);
        gallery_draft_write(draft, prefix + "/type", message->args[i].value.index());
        std::visit(
            [&](const auto& value) {
                if constexpr (std::same_as<std::decay_t<decltype(value)>, ryn::Decimal>) {
                    gallery_draft_write(draft, prefix + "/value", value.text());
                } else {
                    gallery_draft_write(draft, prefix + "/value", value);
                }
            },
            message->args[i].value);
    }
}

void gallery_restore_feedback(const GalleryPageDraft& draft, const std::string& key, const GalleryFeedback& feedback) {
    if (!draft.values.contains(key + "/message")) {
        return;
    }
    if (!gallery_draft_read(draft, key + "/message", false)) {
        feedback.set(gallery_draft_read(draft, key + "/literal", ryn::String{}));
        return;
    }
    const auto identity = gallery_draft_read(draft, key + "/id", ryn::String{});
    ryn::MessageArgs arguments;
    const auto count = gallery_draft_read(draft, key + "/count", std::size_t{});
    for (std::size_t i = 0; i < count; ++i) {
        const auto prefix = key + "/arg/" + std::to_string(i);
        ryn::MessageArg::Value value;
        switch (gallery_draft_read(draft, prefix + "/type", std::size_t{})) {
        case 0:
            value = gallery_draft_read(draft, prefix + "/value", ryn::String{});
            break;
        case 1:
            value = gallery_draft_read(draft, prefix + "/value", std::int64_t{});
            break;
        case 2:
            value = gallery_draft_read(draft, prefix + "/value", std::uint64_t{});
            break;
        case 3:
            value = ryn::Decimal{gallery_draft_read(draft, prefix + "/value", ryn::String{}).bytes()};
            break;
        default:
            throw std::invalid_argument("Invalid Gallery message argument type");
        }
        arguments.push_back({gallery_draft_read(draft, prefix + "/name", ryn::String{}), std::move(value)});
    }
    feedback.set(GalleryMessage{identity.bytes(), std::move(arguments)});
}

template <typename Visitor> void visit_gallery_draft_fields(GalleryState& state, Visitor&& visit) {
    if (state.page_component == "ant.component.button") {
        visit("state/button_loading", state.button_loading);
    }
    if (state.page_component == "ant.component.input" && state.page_family == GalleryInputFamily::input) {
        visit("state/clear_disabled", state.clear_disabled);
        visit("state/input_value", state.input_value);
        visit("state/input_clear_disabled", state.input_clear_disabled);
    }
    if (state.page_component == "ant.component.switch") {
        visit("state/switch_checked", state.switch_checked);
    }
    if (state.page_component == "ant.component.checkbox") {
        visit("state/checkbox_checked", state.checkbox_checked);
        visit("state/checkbox_values", state.checkbox_values);
        visit("state/checkbox_options", state.checkbox_options);
        visit("state/checkbox_options_expanded", state.checkbox_options_expanded);
    }
    if (state.page_component == "ant.component.radio") {
        visit("state/radio_selected", state.radio_selected);
        visit("state/radio_options", state.radio_options);
        visit("state/radio_options_expanded", state.radio_options_expanded);
    }
    if (state.page_component == "ant.component.input" && state.page_family == GalleryInputFamily::search) {
        visit("state/search_value", state.search_value);
    }
    if ((state.page_component == "ant.component.input" || state.page_component == "ant.component.typography")) {
        visit("state/text_direction", state.text_direction);
    }
    if (state.page_component == "ant.component.typography") {
        visit("state/typography_value", state.typography_value);
        visit("state/typography_title", state.typography_title);
    }
    if (state.page_component == "ant.component.slider") {
        visit("state/slider_value", state.slider_value);
        visit("state/slider_range", state.slider_range);
    }
    if (state.page_component == "ant.component.select") {
        visit("state/select_value", state.select_value);
        visit("state/select_options", state.select_options);
        visit("state/select_options_expanded", state.select_options_expanded);
    }
    if (state.page_component == "ant.component.card") {
        visit("state/card_loading", state.card_loading);
    }
    if (state.page_component == "ant.component.tabs") {
        visit("state/tabs_active", state.tabs_active);
        visit("state/tabs_label", state.tabs_label);
        visit("state/tabs_disabled", state.tabs_disabled);
    }
    if (state.page_component == "ant.component.modal") {
        visit("state/modal_loading", state.modal_loading);
    }
    if (state.page_component == "ant.component.drawer") {
        visit("state/drawer_placement", state.drawer_placement);
    }
    if (state.page_component == "ant.component.tag") {
        visit("state/tag_visible", state.tag_visible);
        visit("state/tag_checked", state.tag_checked);
    }
    if (state.page_component == "ant.component.badge") {
        visit("state/badge_count", state.badge_count);
    }
    if (state.page_component == "ant.component.alert") {
        visit("state/alert_visible", state.alert_visible);
    }
    if (state.page_component == "ant.component.spin") {
        visit("state/feedback_loading", state.feedback_loading);
    }
    if (state.page_component == "ant.component.progress") {
        visit("state/feedback_percent", state.feedback_percent);
        visit("state/feedback_status", state.feedback_status);
    }
    if (state.page_component == "ant.component.table") {
        visit("state/records", state.records);
        visit("state/table_sort", state.table_sort);
        visit("state/table_selected", state.table_selected);
        visit("state/table_loading", state.table_loading);
    }
    if ((state.page_component == "ant.component.table" || state.page_component == "ant.component.pagination")) {
        visit("state/table_page", state.table_page);
    }
    if ((state.page_component == "ant.component.menu" || state.page_component == "ant.component.table")) {
        visit("state/menu_selected", state.menu_selected);
        visit("state/menu_open", state.menu_open);
        visit("state/menu_label", state.menu_label);
        visit("state/menu_disabled", state.menu_disabled);
    }
    if ((state.page_component == "ant.component.input-number" || state.page_component == "ant.component.table")) {
        visit("state/number_value", state.number_value);
        visit("state/number_disabled", state.number_disabled);
    }
    if (state.page_component == "ant.component.collapse") {
        visit("state/collapse_keys", state.collapse_keys);
        visit("state/collapse_accordion", state.collapse_accordion);
        visit("state/collapse_disabled", state.collapse_disabled);
    }
    if (state.page_component == "ant.component.segmented") {
        visit("state/segment_value", state.segment_value);
        visit("state/segment_label", state.segment_label);
        visit("state/segment_disabled", state.segment_disabled);
    }
    if (state.page_component == "ant.component.empty") {
        visit("state/empty_description", state.empty_description);
        visit("state/empty_image", state.empty_image);
    }
    if (state.page_component == "ant.component.result") {
        visit("state/result_status", state.result_status);
        visit("state/result_title", state.result_title);
        visit("state/result_status_step", state.result_status_step);
    }
    if (state.page_component == "ant.component.form") {
        visit("state/form_name", state.form_name);
        visit("state/form_role", state.form_role);
        visit("state/form_count", state.form_count);
        visit("state/form_consent", state.form_consent);
        visit("state/form_disabled", state.form_disabled);
        visit("state/form_read_only", state.form_read_only);
        visit("state/form_help", state.form_help);
    }
    if (state.page_component == "ant.component.date-picker") {
        visit("state/date_value", state.date_value);
        visit("state/date_field", state.date_field);
        visit("state/date_form_disabled", state.date_form_disabled);
        visit("state/date_form_read_only", state.date_form_read_only);
        visit("state/date_help", state.date_help);
    }
    if (state.page_component == "ant.component.anchor") {
        visit("state/anchor_affix", state.anchor_affix);
        visit("state/anchor_horizontal", state.anchor_horizontal);
        visit("state/anchor_extra", state.anchor_extra);
    }
    if (state.page_component == "ant.component.breadcrumb") {
        visit("state/breadcrumb_reversed", state.breadcrumb_reversed);
        visit("state/breadcrumb_rtl", state.breadcrumb_rtl);
        visit("state/breadcrumb_extra", state.breadcrumb_extra);
        visit("state/breadcrumb_title", state.breadcrumb_title);
        visit("state/breadcrumb_visible", state.breadcrumb_visible);
        visit("state/breadcrumb_draft_visible", state.breadcrumb_draft_visible);
        visit("state/breadcrumb_disabled", state.breadcrumb_disabled);
        visit("state/breadcrumb_draft", state.breadcrumb_draft);
    }
    if (state.page_component == "ant.component.steps") {
        visit("state/steps_current", state.steps_current);
        visit("state/steps_vertical", state.steps_vertical);
        visit("state/steps_size", state.steps_size);
        visit("state/steps_status", state.steps_status);
        visit("state/steps_visible", state.steps_visible);
        visit("state/steps_disabled", state.steps_disabled);
        visit("state/steps_draft", state.steps_draft);
    }
    if (state.page_component == "ant.component.time-picker") {
        visit("state/time_value", state.time_value);
        visit("state/time_confirm", state.time_confirm);
        visit("state/time_accept", state.time_accept);
        visit("state/time_input_read_only", state.time_input_read_only);
        visit("state/time_field", state.time_field);
        visit("state/time_form_disabled", state.time_form_disabled);
        visit("state/time_form_read_only", state.time_form_read_only);
        visit("state/time_help", state.time_help);
    }
    if (state.page_component == "ant.component.descriptions") {
        visit("state/details_columns", state.details_columns);
        visit("state/details_span", state.details_span);
        visit("state/details_visible", state.details_visible);
        visit("state/details_vertical", state.details_vertical);
        visit("state/details_bordered", state.details_bordered);
        visit("state/details_size", state.details_size);
        visit("state/details_name", state.details_name);
        visit("state/details_role", state.details_role);
        visit("state/details_enabled", state.details_enabled);
    }
    if (state.page_component == "ant.component.skeleton") {
        visit("state/skeleton_loading", state.skeleton_loading);
        visit("state/skeleton_title", state.skeleton_title);
        visit("state/skeleton_paragraph", state.skeleton_paragraph);
        visit("state/skeleton_avatar", state.skeleton_avatar);
        visit("state/skeleton_round", state.skeleton_round);
        visit("state/skeleton_block", state.skeleton_block);
        visit("state/skeleton_shape", state.skeleton_shape);
        visit("state/skeleton_draft", state.skeleton_draft);
        visit("state/skeleton_audience", state.skeleton_audience);
    }
    if (state.page_component == "ant.component.popover") {
        visit("state/popover_disabled", state.popover_disabled);
        visit("state/popover_placement", state.popover_placement);
        visit("state/popover_arrow", state.popover_arrow);
        visit("state/popover_draft", state.popover_draft);
    }
    if (state.page_component == "ant.component.message") {
        visit("state/notification_placement", state.notification_placement);
        visit("state/notification_status", state.notification_status);
        visit("state/notification_draft", state.notification_draft);
    }
    if (state.page_component == "ant.component.timeline") {
        visit("state/timeline_mode", state.timeline_mode);
        visit("state/timeline_reverse", state.timeline_reverse);
        visit("state/timeline_loading", state.timeline_loading);
        visit("state/timeline_visible", state.timeline_visible);
        visit("state/timeline_draft", state.timeline_draft);
    }
    if (state.page_component == "ant.component.avatar") {
        visit("state/member_name", state.member_name);
        visit("state/member_icon", state.member_icon);
        visit("state/members_visible", state.members_visible);
        visit("state/member_shape", state.member_shape);
        visit("state/member_size", state.member_size);
        visit("state/member_limit", state.member_limit);
    }
    if (state.page_component == "ant.component.statistic") {
        visit("state/metric_value", state.metric_value);
        visit("state/metric_precision", state.metric_precision);
        visit("state/metric_separators", state.metric_separators);
        visit("state/metric_formatter", state.metric_formatter);
        visit("state/metric_loading", state.metric_loading);
        visit("state/metric_note", state.metric_note);
        visit("state/metric_step", state.metric_step);
    }
    if (state.page_component == "ant.component.rate") {
        visit("state/rating", state.rating);
        visit("state/rating_direction", state.rating_direction);
        visit("state/rating_hearts", state.rating_hearts);
        visit("state/rating_accept", state.rating_accept);
        visit("state/rating_disabled", state.rating_disabled);
        visit("state/rating_read_only", state.rating_read_only);
        visit("state/rating_visible", state.rating_visible);
        visit("state/rating_note", state.rating_note);
    }
    if (state.page_component == "ant.component.mentions") {
        visit("state/mentions_field", state.mentions_field);
        visit("state/mentions_note", state.mentions_note);
        visit("state/mentions_accept", state.mentions_accept);
        visit("state/mentions_disabled", state.mentions_disabled);
        visit("state/mentions_read_only", state.mentions_read_only);
        visit("state/mentions_visible", state.mentions_visible);
        visit("state/mentions_rtl", state.mentions_rtl);
        visit("state/mentions_top", state.mentions_top);
    }
    if (state.page_component == "ant.component.auto-complete") {
        visit("state/contact_value", state.contact_value);
        visit("state/contact_accept", state.contact_accept);
        visit("state/contact_extra", state.contact_extra);
        visit("state/contact_disabled", state.contact_disabled);
        visit("state/contact_read_only", state.contact_read_only);
        visit("state/contact_note", state.contact_note);
        visit("state/contact_field", state.contact_field);
    }
    if (state.page_component == "ant.component.tree") {
        visit("state/project_expanded", state.project_expanded);
        visit("state/project_selected", state.project_selected);
        visit("state/project_checked", state.project_checked);
        visit("state/project_disabled", state.project_disabled);
        visit("state/project_visible", state.project_visible);
        visit("state/project_locked", state.project_locked);
        visit("state/project_note", state.project_note);
    }
    if (state.page_component == "ant.component.tree-select") {
        visit("state/destination_value", state.destination_value);
        visit("state/destination_expanded", state.destination_expanded);
        visit("state/destination_accept_value", state.destination_accept_value);
        visit("state/destination_accept_close", state.destination_accept_close);
        visit("state/destination_renamed", state.destination_renamed);
        visit("state/destination_archive_disabled", state.destination_archive_disabled);
        visit("state/destination_disabled", state.destination_disabled);
        visit("state/destination_read_only", state.destination_read_only);
        visit("state/destination_visible", state.destination_visible);
        visit("state/destination_note", state.destination_note);
        visit("state/destination_field", state.destination_field);
    }
    if (state.page_component == "ant.component.icon") {
        visit("state/icon_angle", state.icon_angle);
        visit("state/icon_spin", state.icon_spin);
        visit("state/icon_source_step", state.icon_source_step);
    }
}

void remember_target(const std::shared_ptr<GalleryState>& state, std::string_view identity, bool force = false) {
    if (!state->register_targets && !force) {
        return;
    }
    auto& build = ryn::runtime::require_component_build_context();
    auto& host = build.host();
    std::optional<ryn::runtime::ComponentId> parent;
    for (const auto root : host.root_components()) {
        if (build.has_ancestor(root)) {
            parent = root;
            break;
        }
    }
    if (!parent) {
        throw std::logic_error("Gallery test target requires a sample owner");
    }
    while (true) {
        std::optional<ryn::runtime::ComponentId> child;
        for (const auto candidate : host.children(*parent)) {
            if (build.has_ancestor(candidate)) {
                child = candidate;
                break;
            }
        }
        if (!child) {
            break;
        }
        parent = child;
    }
    const auto& children = host.children(*parent);
    if (children.empty()) {
        throw std::logic_error("Gallery test target has no mounted component");
    }
    const auto component = children.back();
    auto key = std::string{identity};
    if (state->page_component == "ant.component.table" &&
        (identity.starts_with("gallery.menu.") || identity.starts_with("gallery.dropdown.") ||
         identity.starts_with("gallery.number."))) {
        key = "gallery.table." + std::string{identity.substr(std::string_view{"gallery."}.size())};
    }
    const auto [target, inserted] = state->targets.emplace(key, component);
    if (!inserted) {
        if (target->second == component) {
            return;
        }
        throw std::logic_error("Gallery test target identity is duplicated: " + key);
    }
    build.on_resource_cleanup(component, [weak = std::weak_ptr{state}, key] {
        if (const auto state = weak.lock()) {
            state->targets.erase(key);
        }
    });
}

thread_local std::weak_ptr<GalleryState> active_gallery_draft;

class GalleryDraftBuild final {
public:
    explicit GalleryDraftBuild(const std::shared_ptr<GalleryState>& state)
        : previous_(std::exchange(active_gallery_draft, state)) {}

    ~GalleryDraftBuild() {
        active_gallery_draft = std::move(previous_);
    }

private:
    std::weak_ptr<GalleryState> previous_;
};

template <typename T> T gallery_initial_value(std::string_view identity, const T& fallback) {
    const auto state = active_gallery_draft.lock();
    if (!state || !state->register_targets) {
        return fallback;
    }
    return gallery_draft_read(state->draft, "editor/" + std::string{identity}, fallback);
}

ryn::String gallery_initial_text(std::string_view identity, ryn::String fallback) {
    return gallery_initial_value(identity, std::move(fallback));
}

template <typename T>
std::function<void(T)> gallery_value_changed(std::string_view identity, std::function<void(T)> original = {}) {
    const auto state = active_gallery_draft.lock();
    if (!state || !state->register_targets) {
        return original;
    }
    return [weak = std::weak_ptr{state}, key = "editor/" + std::string{identity},
            original = std::move(original)](T value) {
        if (const auto state = weak.lock()) {
            gallery_draft_write(state->draft, key, value);
        }
        if (original) {
            original(std::move(value));
        }
    };
}

void gallery_target(std::string_view identity) {
    if (const auto state = active_gallery_draft.lock()) {
        remember_target(state, identity);
    }
}

ryn::SelectProps gallery_select_default(std::string_view identity, ryn::SelectProps props, ryn::SelectValue fallback) {
    const auto value = gallery_initial_value(identity, ryn::SelectSelection{std::move(fallback)});
    if (value) {
        props.defaultValue(*value);
    }
    return props;
}

template <class T, class Source>
ryn::Binding<std::vector<T>>
gallery_option_labels(Source source, std::vector<std::pair<decltype(T{}.value), std::string_view>> labels) {
    const ryn::Memo<std::vector<T>> data{[source = std::move(source)] {
        if constexpr (requires { source.get(); }) {
            return source.get();
        } else {
            return source;
        }
    }};
    return ryn::bind([data, labels = std::move(labels), tr = ryn::useI18n()] {
        auto result = data.get();
        for (auto& option : result) {
            for (const auto& [value, identity] : labels) {
                const auto key = [&] {
                    if constexpr (std::same_as<T, ryn::AutoCompleteOption> || std::same_as<T, ryn::MentionsOption>) {
                        return option.key;
                    } else {
                        return option.value;
                    }
                }();
                if (key == value) {
                    option.label = tr.translate(gallery_message(identity));
                    break;
                }
            }
        }
        return result;
    });
}

template <class T>
ryn::Binding<std::vector<T>>
gallery_group_labels(std::vector<T> data, std::vector<std::pair<ryn::String, std::string_view>> group_labels,
                     std::vector<std::pair<ryn::SelectValue, std::string_view>> option_labels) {
    return ryn::bind([data = std::move(data), group_labels = std::move(group_labels),
                      option_labels = std::move(option_labels), tr = ryn::useI18n()] {
        auto result = data;
        for (auto& group : result) {
            for (const auto& [key, identity] : group_labels) {
                if (group.key == key) {
                    group.label = tr.translate(gallery_message(identity));
                }
            }
            for (auto& option : group.options) {
                for (const auto& [key, identity] : option_labels) {
                    if (option.value == key) {
                        option.label = tr.translate(gallery_message(identity));
                    }
                }
            }
        }
        return result;
    });
}

ryn::Binding<std::vector<ryn::TransferItem>>
gallery_transfer_labels(std::vector<ryn::TransferItem> data,
                        std::vector<std::pair<ryn::TransferKey, GalleryMessage>> labels) {
    return ryn::bind([data = std::move(data), labels = std::move(labels), tr = ryn::useI18n()] {
        auto result = data;
        for (auto& item : result) {
            for (const auto& [key, message] : labels) {
                if (item.key == key) {
                    item.title = tr.translate(message.key, message.args);
                }
            }
        }
        return result;
    });
}

ryn::Binding<std::vector<ryn::CascaderOption>>
gallery_cascader_labels(std::vector<ryn::CascaderOption> data,
                        std::vector<std::pair<ryn::String, GalleryMessage>> labels) {
    return ryn::bind([data = std::move(data), labels = std::move(labels), tr = ryn::useI18n()] {
        auto result = data;
        const auto project = [&](const auto& self, auto& items) -> void {
            for (auto& item : items) {
                for (const auto& [key, message] : labels) {
                    if (item.key == key) {
                        item.label = tr.translate(message.key, message.args);
                    }
                }
                self(self, item.children);
            }
        };
        project(project, result);
        return result;
    });
}

template <class T, class Source>
ryn::Binding<std::vector<T>> gallery_record_labels(Source source,
                                                   std::vector<std::pair<ryn::String, std::string_view>> labels,
                                                   ryn::String T::* field) {
    const ryn::Memo<std::vector<T>> data{[source = std::move(source)] {
        if constexpr (requires { source.get(); }) {
            return source.get();
        } else {
            return source;
        }
    }};
    return ryn::bind([data, labels = std::move(labels), field, tr = ryn::useI18n()] {
        auto result = data.get();
        for (auto& item : result) {
            for (const auto& [key, identity] : labels) {
                if (item.key == key) {
                    item.*field = tr.translate(gallery_message(identity));
                }
            }
        }
        return result;
    });
}

const std::size_t navigation_control_count = 20 + ant_design_reference_entries().size();

ryn::String utf8(std::string_view value) {
    auto parsed = ryn::String::from_utf8(value);
    if (!parsed) {
        throw std::logic_error("Token Gallery content is not valid UTF-8");
    }
    return std::move(parsed).value();
}

ryn::String label(std::string_view test_id, std::string_view caption) {
    if (test_id.starts_with("ant.") && ryn::find_ant_design_token(test_id) == nullptr) {
        throw std::logic_error("Token Gallery label identity is not in the locked catalog");
    }
    return utf8(caption);
}

ryn::Binding<ryn::String> reference_source_note(const AntDesignReferenceEntry& entry) {
    std::size_t records = entry.evidence_identifiers.empty() ? 0 : 1;
    std::size_t position = 0;
    while ((position = entry.evidence_identifiers.find(" · ", position)) != std::string_view::npos) {
        ++records;
        position += std::string_view{" · "}.size();
    }
    return gallery_text(
        "gallery.complete.copy.301",
        ryn::MessageArgs{{u8"path", utf8(entry.source_path)}, {u8"count", static_cast<std::uint64_t>(records)}});
}

ryn::ThemeConfig algorithm_config(ryn::ThemeAlgorithm algorithm) {
    ryn::ThemeConfig config;
    if (algorithm != ryn::ThemeAlgorithm::Default) {
        config.algorithms.push_back(algorithm);
    }
    return config;
}

ryn::ThemeConfig navigation_theme_config() {
    ryn::ThemeConfig config;
    config.button.tokens.padding_inline = ryn::dp(8.0F);
    config.button.tokens.border_radius = ryn::dp(6.0F);
    return config;
}

void document_text(ryn::Prop<ryn::String> content, float size, float line_height, std::uint32_t weight = 400,
                   ryn::TextTone tone = ryn::TextTone::Primary) {
    ryn::ThemeConfig config;
    config.text.tokens.font_size = ryn::dp(size);
    config.text.tokens.line_height = ryn::dp(line_height);
    config.text.tokens.font_weight = weight;
    ryn::Theme(ryn::ThemeProps{}.config(config),
               ryn::ThemeContent{[content, tone] { ryn::Text(ryn::TextProps{}.content(content).tone(tone)); }});
}

void section_surface(const std::shared_ptr<GalleryState>& state, const GalleryDocumentSection& section) {
    const bool page_title = section.kind == GalleryDocumentSectionKind::header_source;
    const auto title = gallery_field("frame." + std::string{section.identity}, "title");
    const auto summary = gallery_field("frame." + std::string{section.identity}, "summary");
    ReferenceSurface(ReferenceSurfaceProps{}
                         .identity(section.identity)
                         .role(ReferenceSurfaceRole::document_heading)
                         .layout(ryn::LayoutStyle{}
                                     .width(state->document_width)
                                     .margin_top(ryn::dp(page_title ? 0.0F : 24.0F))
                                     .margin_bottom(ryn::dp(8.0F))),
                     [state, title, summary, page_title] {
                         ++state->telemetry.reference_content_runs;
                         document_text(title, page_title ? 28.0F : 22.0F, page_title ? 40.0F : 32.0F, 600);
                         document_text(summary, 14.0F, 24.0F, 400, ryn::TextTone::Secondary);
                     });
    ++state->telemetry.document_sections;
    ++state->telemetry.reference_surfaces;
}

void source_section(const std::shared_ptr<GalleryState>& state) {
    section_surface(state, gallery_document_sections()[0]);
    ryn::Flex(ryn::FlexProps{}
                  .align(ryn::FlexAlign::Start)
                  .wrap(true)
                  .gap(ryn::dp(GalleryLayoutMetrics::card_gap))
                  .layout(ryn::LayoutStyle{}.width(state->document_width)),
              [state] {
                  for (const auto& source : ant_design_reference_sources()) {
                      const auto title = utf8(source.title);
                      const auto url = utf8(source.official_url);
                      ReferenceSurface(
                          ReferenceSurfaceProps{}
                              .role(ReferenceSurfaceRole::document_note)
                              .layout(ryn::LayoutStyle{}.width(state->cell_width).margin_bottom(ryn::dp(8.0F))),
                          [state, title, url] {
                              ++state->telemetry.reference_content_runs;
                              document_text(title, 13.0F, 22.0F, 500);
                              document_text(url, 12.0F, 20.0F, 400, ryn::TextTone::Secondary);
                          });
                      ++state->telemetry.reference_surfaces;
                  }
              });
}

void navigation_button(const std::shared_ptr<GalleryState>& state, ryn::Prop<ryn::String> caption,
                       GalleryNavigationTarget target, bool child = false) {
    const auto text = caption;
    const auto nav_width = state->navigation_width;
    const auto active = state->active_navigation;
    const auto config = ryn::bind([active, target, parent_theme = state->theme] {
        auto theme = navigation_theme_config();
        if (active.get() == target) {
            const auto tokens = ryn::resolve_theme(parent_theme.get());
            const auto primary = tokens.map().color_primary;
            theme.button.tokens.text_background = ryn::Color(primary.red(), primary.green(), primary.blue(), 0.08F);
            theme.button.tokens.text_color = tokens.map().color_primary;
            theme.alias.color_text = tokens.map().color_primary;
        }
        return theme;
    });
    ryn::Theme(ryn::ThemeProps{}.config(config), ryn::ThemeContent{[state, target, text, nav_width, child] {
                   ryn::Button(ryn::ButtonProps{}
                                   .type(ryn::ButtonType::Text)
                                   .size(ryn::ControlSize::Small)
                                   .layout(ryn::LayoutStyle{}
                                               .width(ryn::bind([nav_width, child] {
                                                   return ryn::dp(nav_width.get().value() - (child ? 28.0F : 16.0F));
                                               }))
                                               .margin_left(ryn::dp(child ? 12.0F : 0.0F))
                                               .height(ryn::dp(36.0F)))
                                   .onClick([state, target] {
                                       if (target.kind == GalleryNavigationTargetKind::component) {
                                           const auto* entry =
                                               find_ant_design_reference_entry(target.component_identity);
                                           if (entry && !gallery_support_filter_matches(state->support_filter.get(),
                                                                                        entry->support_status)) {
                                               state->support_filter.set(GallerySupportFilter::all);
                                               ++state->telemetry.filter_updates;
                                           }
                                       }
                                       state->active_navigation.set(target);
                                       state->navigation_request = target;
                                       ++state->telemetry.navigation_requests;
                                   }),
                               [text, nav_width, child] {
                                   ryn::Text(ryn::TypographyProps{}
                                                 .content(text)
                                                 .ellipsis(ryn::TypographyEllipsis{.rows = 1})
                                                 .layout(ryn::LayoutStyle{}.width(ryn::bind([nav_width, child] {
                                                     return ryn::dp(nav_width.get().value() - (child ? 48.0F : 36.0F));
                                                 }))));
                               });
               }});
    const auto key = target.kind == GalleryNavigationTargetKind::component
                         ? "gallery.nav." + std::string{target.component_identity}
                     : target.kind == GalleryNavigationTargetKind::category
                         ? "gallery.nav.category." + std::to_string(static_cast<int>(target.category))
                         : "gallery.nav.section." + std::to_string(static_cast<int>(target.section));
    remember_target(state, key, true);
}

void filter_button(const std::shared_ptr<GalleryState>& state, ryn::Prop<ryn::String> caption,
                   GallerySupportFilter filter) {
    const auto text = caption;
    ryn::Button(ryn::ButtonProps{}.size(ryn::ControlSize::Small).onClick([state, filter] {
        if (state->support_filter.set(filter)) {
            ++state->telemetry.filter_updates;
        }
    }),
                [text] { ryn::Text(ryn::TextProps{}.content(text)); });
    remember_target(state, "gallery.filter." + std::to_string(static_cast<int>(filter)), true);
}

void navigation_controls(const std::shared_ptr<GalleryState>& state) {
    document_text(gallery_text("gallery.complete.copy.299"), 12.0F, 24.0F, 500, ryn::TextTone::Secondary);
    ryn::Text(ryn::TextProps{}
                  .content(gallery_text("gallery.complete.copy.415"))
                  .tone(ryn::TextTone::Secondary)
                  .layout(ryn::LayoutStyle{}.margin_bottom(ryn::dp(8.0F))));
    for (const auto& section : gallery_document_sections()) {
        navigation_button(state, gallery_field("frame." + std::string{section.identity}, "title"),
                          GalleryNavigationTarget::to_section(section.kind));
    }
    ryn::Text(ryn::TextProps{}
                  .content(gallery_text("gallery.shell.components"))
                  .tone(ryn::TextTone::Secondary)
                  .layout(ryn::LayoutStyle{}.margin_top(ryn::dp(24.0F)).margin_bottom(ryn::dp(8.0F))));
    for (const auto& category : ant_design_reference_categories()) {
        navigation_button(state, gallery_field(category.identity, "name"),
                          GalleryNavigationTarget::to_category(category.category));
        for (const auto& entry : ant_design_reference_entries()) {
            if (entry.category != category.category) {
                continue;
            }
            navigation_button(state, gallery_field(entry.identity, "name"),
                              GalleryNavigationTarget::to_component(entry.identity), true);
        }
    }
    ryn::Text(ryn::TextProps{}
                  .content(gallery_text("gallery.complete.copy.300"))
                  .tone(ryn::TextTone::Secondary)
                  .layout(ryn::LayoutStyle{}.margin_top(ryn::dp(24.0F))));
    filter_button(state, gallery_text("gallery.status.all"), GallerySupportFilter::all);
    filter_button(state, gallery_text("gallery.status.implemented"), GallerySupportFilter::implemented);
    filter_button(state, gallery_text("gallery.status.partial"), GallerySupportFilter::partial);
    filter_button(state, gallery_text("gallery.status.planned"), GallerySupportFilter::planned);
    filter_button(state, gallery_text("gallery.status.web-only"), GallerySupportFilter::web_only);
    filter_button(state, gallery_text("gallery.status.deprecated"), GallerySupportFilter::deprecated);
    filter_button(state, gallery_text("gallery.status.out-of-scope"), GallerySupportFilter::out_of_scope);
}

void scrollbar_surface(std::string_view identity, ReferenceSurfaceRole role,
                       const ryn::Signal<ryn::LogicalLength>& height, const ryn::Signal<bool>& visible) {
    ReferenceSurface(ReferenceSurfaceProps{}.identity(identity).role(role).visible(visible).layout(
                         ryn::LayoutStyle{}.width(ryn::dp(8.0F)).height(height).order(2)),
                     [] {});
}

void design_values(const std::shared_ptr<GalleryState>& state) {
    section_surface(state, gallery_document_sections()[2]);
    ryn::Flex(ryn::FlexProps{}
                  .align(ryn::FlexAlign::Start)
                  .wrap(true)
                  .gap(ryn::dp(GalleryLayoutMetrics::card_gap), ryn::dp(GalleryLayoutMetrics::card_gap))
                  .layout(ryn::LayoutStyle{}.width(state->document_width)),
              [state] {
                  for (const auto& value : gallery_design_values()) {
                      const auto title = gallery_field(value.identity, "name");
                      const auto summary = gallery_field(value.identity, "summary");
                      ReferenceSurface(
                          ReferenceSurfaceProps{}
                              .role(ReferenceSurfaceRole::document_note)
                              .layout(ryn::LayoutStyle{}.width(state->cell_width).margin_bottom(ryn::dp(8.0F))),
                          [state, title, summary] {
                              ++state->telemetry.reference_content_runs;
                              document_text(title, 16.0F, 26.0F, 500);
                              document_text(summary, 14.0F, 24.0F, 400, ryn::TextTone::Secondary);
                          });
                      ++state->telemetry.reference_surfaces;
                  }
              });
}

void reference_cell(const std::shared_ptr<GalleryState>& state, std::string_view test_id, std::string_view caption,
                    ryn::ThemeConfig config, std::optional<ryn::Color> swatch = std::nullopt, bool elevated = false) {
    const auto title = gallery_text(caption);
    const auto identity = utf8(test_id);
    ryn::Theme(ryn::ThemeProps{}.config(std::move(config)),
               ryn::ThemeContent{[state, title, identity, test_id, swatch, elevated] {
                   ++state->telemetry.theme_content_runs;
                   ReferenceSurface(ReferenceSurfaceProps{}
                                        .identity(test_id)
                                        .role(state->register_targets ? ReferenceSurfaceRole::example
                                                                      : ReferenceSurfaceRole::reference)
                                        .status(GallerySupportStatus::implemented)
                                        .swatch(swatch)
                                        .elevated(elevated)
                                        .layout(ryn::LayoutStyle{}.width(state->cell_width)),
                                    [state, title, identity] {
                                        ++state->telemetry.reference_content_runs;
                                        ryn::Text(ryn::TextProps{}.content(title));
                                        ryn::Text(ryn::TextProps{}.content(identity).tone(ryn::TextTone::Secondary));
                                    });
                   ++state->telemetry.reference_surfaces;
               }});
}

void reference_cell(const std::shared_ptr<GalleryState>& state, std::string_view test_id, std::string_view caption,
                    std::optional<ryn::Color> swatch = std::nullopt) {
    reference_cell(state, test_id, caption, ryn::ThemeConfig{}, swatch);
}

void add_palette_cells(const std::shared_ptr<GalleryState>& state) {
    reference_cell(state, "ant.map.colorPrimary", "gallery.complete.copy.302", ryn::Color::rgba8(22, 119, 255));
    reference_cell(state, "ant.map.colorSuccess", "gallery.complete.copy.303", ryn::Color::rgba8(82, 196, 26));
    reference_cell(state, "ant.map.colorWarning", "gallery.complete.copy.304", ryn::Color::rgba8(250, 173, 20));
    reference_cell(state, "ant.map.colorError", "gallery.complete.copy.305", ryn::Color::rgba8(255, 77, 79));
    reference_cell(state, "ant.map.colorInfo", "gallery.complete.copy.306", ryn::Color::rgba8(22, 119, 255));
    reference_cell(state, "ant.map.blue6", "gallery.complete.copy.307", ryn::Color::rgba8(22, 119, 255));
    reference_cell(state, "ant.map.green6", "gallery.complete.copy.308", ryn::Color::rgba8(82, 196, 26));
    reference_cell(state, "ant.map.red5", "gallery.complete.copy.309", ryn::Color::rgba8(255, 77, 79));
    reference_cell(state, "ant.map.gold6", "gallery.complete.copy.310", ryn::Color::rgba8(250, 173, 20));
    reference_cell(state, "ant.map.purple6", "gallery.complete.copy.311", ryn::Color::rgba8(114, 46, 209));
}

void add_scale_cells(const std::shared_ptr<GalleryState>& state) {
    const auto text_cell = [&](std::string_view id, std::string_view caption, float size) {
        ryn::ThemeConfig config;
        config.text.tokens.font_size = ryn::dp(size);
        config.text.tokens.line_height = ryn::dp(size + 8.0F);
        reference_cell(state, id, caption, std::move(config));
    };
    text_cell("ant.map.fontSizeSM", "gallery.complete.copy.312", 12.0F);
    text_cell("ant.map.fontSize", "gallery.complete.copy.313", 14.0F);
    text_cell("ant.map.fontSizeLG", "gallery.complete.copy.314", 16.0F);
    reference_cell(state, "ant.map.sizeXS", "gallery.complete.copy.315");
    reference_cell(state, "ant.map.size", "gallery.complete.copy.316");
    reference_cell(state, "ant.map.sizeLG", "gallery.complete.copy.317");
    reference_cell(state, "ant.map.controlHeightSM", "gallery.complete.copy.318");
    reference_cell(state, "ant.seed.controlHeight", "gallery.complete.copy.319");
    reference_cell(state, "ant.map.controlHeightLG", "gallery.complete.copy.320");
    for (const auto [id, caption, radius] :
         std::array{std::tuple{"ant.map.borderRadiusSM", "gallery.complete.copy.321", 4.0F},
                    std::tuple{"ant.seed.borderRadius", "gallery.complete.copy.322", 6.0F},
                    std::tuple{"ant.map.borderRadiusLG", "gallery.complete.copy.323", 8.0F}}) {
        ryn::ThemeConfig config;
        config.seed.border_radius = ryn::dp(radius);
        reference_cell(state, id, caption, std::move(config));
    }
}

void shadow_cell(const std::shared_ptr<GalleryState>& state, std::string_view id, std::string_view caption,
                 const ryn::ShadowList& shadow) {
    ryn::ThemeConfig config;
    config.alias.box_shadow_tertiary = shadow;
    reference_cell(state, id, caption, std::move(config), std::nullopt, true);
}

void add_shadow_cells(const std::shared_ptr<GalleryState>& state) {
    const auto& shadows = ryn::ant_design_default_shadows();
    shadow_cell(state, "ant.alias.boxShadowTertiary", "gallery.complete.copy.324", shadows.box_shadow_tertiary);
    shadow_cell(state, "ant.alias.boxShadowSecondary", "gallery.complete.copy.325", shadows.box_shadow_secondary);
    shadow_cell(state, "ant.alias.boxShadow", "gallery.complete.copy.326", shadows.box_shadow);
    shadow_cell(state, "ant.component.Button.defaultShadow", "gallery.complete.copy.327", shadows.button_default);
    shadow_cell(state, "ant.component.Button.primaryShadow", "gallery.complete.copy.328", shadows.button_primary);
    shadow_cell(state, "ant.component.Button.dangerShadow", "gallery.complete.copy.329", shadows.button_danger);
    shadow_cell(state, "ant.alias.boxShadowDrawerLeft", "gallery.complete.copy.330", shadows.drawer_left);
    shadow_cell(state, "ant.alias.boxShadowDrawerRight", "gallery.complete.copy.331", shadows.drawer_right);
    shadow_cell(state, "ant.alias.boxShadowDrawerUp", "gallery.complete.copy.332", shadows.drawer_up);
    shadow_cell(state, "ant.alias.boxShadowDrawerDown", "gallery.complete.copy.333", shadows.drawer_down);
    shadow_cell(state, "ant.alias.boxShadowPopoverArrow", "gallery.complete.copy.334", shadows.popover_arrow);
    shadow_cell(state, "ant.alias.dropShadowPopover", "gallery.complete.copy.335", shadows.popover_drop);
    shadow_cell(state, "ant.alias.boxShadowCard", "gallery.complete.copy.336", shadows.card);
    shadow_cell(state, "ant.alias.boxShadowTabsOverflowLeft", "gallery.complete.copy.337", shadows.tabs_overflow_left);
    shadow_cell(state, "ant.alias.boxShadowTabsOverflowRight", "gallery.complete.copy.338",
                shadows.tabs_overflow_right);
    shadow_cell(state, "ant.alias.boxShadowTabsOverflowTop", "gallery.complete.copy.339", shadows.tabs_overflow_top);
    shadow_cell(state, "ant.alias.boxShadowTabsOverflowBottom", "gallery.complete.copy.340",
                shadows.tabs_overflow_bottom);
}

void foundation_tokens(const std::shared_ptr<GalleryState>& state) {
    section_surface(state, gallery_document_sections()[3]);
    ryn::Flex(ryn::FlexProps{}
                  .align(ryn::FlexAlign::Start)
                  .wrap(true)
                  .gap(ryn::dp(GalleryLayoutMetrics::card_gap), ryn::dp(GalleryLayoutMetrics::card_gap))
                  .layout(ryn::LayoutStyle{}.width(state->document_width)),
              [state] {
                  add_palette_cells(state);
                  add_scale_cells(state);
                  add_shadow_cells(state);
              });
}

void add_typography_catalog_samples(const std::shared_ptr<GalleryState>& state) {
    for (auto level : {ryn::TypographyLevel::H1, ryn::TypographyLevel::H2, ryn::TypographyLevel::H3,
                       ryn::TypographyLevel::H4, ryn::TypographyLevel::H5}) {
        ryn::Title(ryn::TitleProps{}.level(level).content(gallery_text("gallery.copy.property.000")));
        remember_target(state, std::string{"gallery.typography.level-"} + std::to_string(static_cast<int>(level)));
    }
    ryn::Text(ryn::TypographyProps{}
                  .content(gallery_text("gallery.copy.property.001"))
                  .strong(true)
                  .italic(true)
                  .type(ryn::TypographyType::Danger));
    remember_target(state, "gallery.typography.emphasis");
    ryn::Text(ryn::TypographyProps{}.content(u8"code: a += b; 中文回退").code(true).type(ryn::TypographyType::Success));
    remember_target(state, "gallery.typography.code");
    ryn::Text(ryn::TypographyProps{}
                  .content(gallery_text("gallery.copy.property.003"))
                  .keyboard(true)
                  .type(ryn::TypographyType::Warning));
    remember_target(state, "gallery.typography.keyboard");
    ryn::Text(ryn::TypographyProps{}
                  .content(gallery_text("gallery.copy.property.004"))
                  .mark(true)
                  .underline(true)
                  .strikethrough(true)
                  .type(ryn::TypographyType::Secondary));
    remember_target(state, "gallery.typography.decoration");
    ryn::Link(ryn::LinkProps{}.content(gallery_text("gallery.copy.property.005")).onClick([state] {
        ++state->telemetry.activations;
    }));
    remember_target(state, "gallery.typography.activation");
    state->telemetry.live_samples += 10;
}

void add_divider_catalog_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Divider();
    remember_target(state, "gallery.divider.basic");
    ryn::Divider(ryn::DividerProps{}.content(gallery_text("gallery.copy.property.006")));
    remember_target(state, "gallery.divider.labeled");
    ryn::Divider(ryn::DividerProps{}.dashed(true));
    remember_target(state, "gallery.divider.dashed");
    state->telemetry.live_samples += 3;
}

void component_entry(const std::shared_ptr<GalleryState>& state, const AntDesignReferenceEntry& entry) {
    const auto name = gallery_field(entry.identity, "name");
    const auto summary = gallery_field(entry.identity, "summary");
    const auto supported = gallery_field(entry.identity, "supported");
    const auto missing = gallery_field(entry.identity, "missing");
    const auto source_note = reference_source_note(entry);
    const auto filter = state->support_filter;
    const auto cell_width = state->cell_width;
    const auto document_width = state->document_width;
    const auto visible = ryn::bind(
        [filter, status = entry.support_status] { return gallery_support_filter_matches(filter.get(), status); });
    const auto width = ryn::bind([filter, cell_width, document_width, status = entry.support_status] {
        return gallery_support_filter_matches(filter.get(), status)
                   ? (status == GallerySupportStatus::partial ? document_width.get() : cell_width.get())
                   : ryn::dp(0.0F);
    });
    const auto height = ryn::bind([filter, status = entry.support_status] {
        return gallery_support_filter_matches(filter.get(), status) ? ryn::auto_length : ryn::dp(0.0F);
    });
    ReferenceSurface(ReferenceSurfaceProps{}
                         .identity(entry.identity)
                         .status(entry.support_status)
                         .visible(visible)
                         .layout(ryn::LayoutStyle{}.width(width).height(height)),
                     [state, name, summary, supported, missing, source_note, identity = entry.identity,
                      status = entry.support_status] {
                         ++state->telemetry.reference_content_runs;
                         document_text(name, 16.0F, 26.0F, 500);
                         ryn::Text(ryn::TextProps{}.content(summary).tone(ryn::TextTone::Secondary));
                         if (status == GallerySupportStatus::partial) {
                             document_text(supported, 12.0F, 20.0F, 400, ryn::TextTone::Secondary);
                             document_text(missing, 12.0F, 20.0F, 400, ryn::TextTone::Secondary);
                         }
                         document_text(source_note, 11.0F, 18.0F, 400, ryn::TextTone::Secondary);
                         if (state->frame_pages.empty() && identity == "ant.component.typography") {
                             add_typography_catalog_samples(state);
                         }
                         if (state->frame_pages.empty() && identity == "ant.component.divider") {
                             add_divider_catalog_samples(state);
                         }
                     });
    ++state->telemetry.component_entries;
    ++state->telemetry.reference_surfaces;
}

void component_overview(const std::shared_ptr<GalleryState>& state) {
    section_surface(state, gallery_document_sections()[4]);
    const auto entries = ant_design_reference_entries();
    for (const auto& category : ant_design_reference_categories()) {
        document_text(gallery_field(category.identity, "name"), 18.0F, 28.0F, 500);
        ryn::Flex(ryn::FlexProps{}
                      .align(ryn::FlexAlign::Start)
                      .wrap(true)
                      .gap(ryn::dp(GalleryLayoutMetrics::card_gap), ryn::dp(GalleryLayoutMetrics::card_gap))
                      .layout(ryn::LayoutStyle{}.width(state->document_width)),
                  [state, entries, category] {
                      for (const auto& entry : entries) {
                          if (entry.category == category.category &&
                              gallery_support_filter_matches(GallerySupportFilter::all, entry.support_status)) {
                              component_entry(state, entry);
                          }
                      }
                  });
    }
}

void themed_button(const std::shared_ptr<GalleryState>& state, std::string_view test_id, std::string_view caption,
                   const ryn::ThemeConfig& config, ryn::ButtonType type = ryn::ButtonType::Default,
                   ryn::ControlSize size = ryn::ControlSize::Small, ryn::Prop<bool> disabled = false,
                   ryn::Prop<bool> loading = false, std::function<void()> on_click = {}) {
    const auto text = gallery_text(caption);
    // Keep the large ThemeConfig-bearing props off the native thread's stack.
    auto theme_props = std::make_unique<ryn::ThemeProps>();
    theme_props->config(config);
    ryn::Theme(std::move(*theme_props),
               ryn::ThemeContent{[state, text, type, size, disabled = std::move(disabled), loading = std::move(loading),
                                  on_click = std::move(on_click)]() mutable {
                   ++state->telemetry.theme_content_runs;
                   auto props = ryn::ButtonProps{}
                                    .type(type)
                                    .size(size)
                                    .disabled(std::move(disabled))
                                    .loading(std::move(loading))
                                    .layout(ryn::LayoutStyle{}.width(state->cell_width).flex_shrink(1.0F));
                   if (on_click) {
                       props.onClick(std::move(on_click));
                   }
                   ryn::Button(std::move(props), [text] { ryn::Text(ryn::TextProps{}.content(text)); });
                   ++state->telemetry.live_samples;
               }});
    remember_target(state, test_id, true);
}

void add_splitter_multiple_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(TextProps{}.content(state->splitter_feedback));
    Splitter(SplitterProps{}
                 .layout(LayoutStyle{}.width(state->document_width).height(dp(220)))
                 .onResizeStart([state](SplitterSizes) {
                     state->splitter_feedback.set(GalleryMessage{"gallery.feedback.047", {}});
                 })
                 .onResize([state](SplitterSizes) {
                     state->splitter_feedback.set(GalleryMessage{"gallery.feedback.048", {}});
                     ++state->telemetry.activations;
                 })
                 .onResizeEnd([state](SplitterSizes) {
                     state->splitter_feedback.set(GalleryMessage{"gallery.feedback.049", {}});
                 }),
             {SplitterPanel{SplitterPanelContent{[] { gallery_label("gallery.copy.sample.000"); }}}
                  .defaultSize(SplitterSize::percent(25))
                  .min(SplitterSize::percent(15))
                  .max(SplitterSize::percent(40)),
              SplitterPanel{SplitterPanelContent{[] { Input(InputProps{}.defaultValue(u8"B · 拖动保持草稿")); }}}
                  .defaultSize(SplitterSize::percent(35))
                  .min(SplitterSize{dp(96)}),
              SplitterPanel{SplitterPanelContent{[] { gallery_label("gallery.copy.sample.001"); }}}});
}

void add_splitter_controlled_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Space(SpaceProps{}.size(dp(8)), [state] {
        Button(ButtonProps{}.onClick([state] { state->splitter_resizable.set(!state->splitter_resizable.get()); }),
               [] { gallery_label("gallery.copy.sample.002"); });
        Button(ButtonProps{}.onClick([state] { state->splitter_percentages.set({35, 65}); }),
               [] { gallery_label("gallery.copy.sample.003"); });
    });
    Splitter(SplitterProps{}
                 .layout(LayoutStyle{}.width(state->document_width).height(dp(220)))
                 .draggerIcon(SplitterDraggerIcon{[] { Icon(IconProps{}.name(IconName::HolderOutlined)); }})
                 .onDraggerDoubleClick([state](std::size_t) {
                     state->splitter_percentages.set({35, 65});
                     ++state->telemetry.activations;
                 })
                 .onResize([state](SplitterSizes values) {
                     const float total = values[0] + values[1];
                     if (total > 0) {
                         state->splitter_percentages.set({values[0] / total * 100, values[1] / total * 100});
                     }
                 }),
             {SplitterPanel{SplitterPanelContent{[] { gallery_label("gallery.copy.sample.004"); }}}
                  .size(bind([state] { return SplitterSize::percent(state->splitter_percentages.get()[0]); }))
                  .min(SplitterSize::percent(20))
                  .resizable(state->splitter_resizable),
              SplitterPanel{SplitterPanelContent{[] { Input(InputProps{}.defaultValue(u8"自定义拖动图标")); }}}
                  .size(bind([state] { return SplitterSize::percent(state->splitter_percentages.get()[1]); }))
                  .min(SplitterSize::percent(20))
                  .resizable(state->splitter_resizable)});
}

void add_splitter_lazy_collapse_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Space(SpaceProps{}.size(dp(8)), [state] {
        Button(ButtonProps{}.onClick([state] { state->splitter_destroy.set(!state->splitter_destroy.get()); }),
               [] { gallery_label("gallery.copy.sample.005"); });
        remember_target(state, "gallery.splitter.destroy-hidden");
        Button(ButtonProps{}.onClick([state] { state->splitter_motion.set(!state->splitter_motion.get()); }),
               [] { gallery_label("gallery.copy.sample.006"); });
        Button(ButtonProps{}.onClick([state] {
            state->splitter_orientation.set(state->splitter_orientation.get() == SplitterOrientation::Horizontal
                                                ? SplitterOrientation::Vertical
                                                : SplitterOrientation::Horizontal);
        }),
               [] { gallery_label("gallery.copy.sample.007"); });
    });
    Splitter(SplitterProps{}
                 .orientation(state->splitter_orientation)
                 .lazy()
                 .destroyOnHidden(state->splitter_destroy)
                 .collapseMotion(state->splitter_motion)
                 .layout(LayoutStyle{}.width(state->document_width).height(dp(260)))
                 .startIcon(SplitterStartIcon{[] { Icon(IconProps{}.name(IconName::MinusOutlined)); }})
                 .endIcon(SplitterEndIcon{[] { Icon(IconProps{}.name(IconName::PlusOutlined)); }})
                 .onCollapse([state](SplitterCollapsed, SplitterSizes) { ++state->telemetry.activations; }),
             {SplitterPanel{SplitterPanelContent{[] {
                  Input(InputProps{}.defaultValue(u8"折叠时按全局策略保留或销毁"));
              }}}.collapsible(SplitterCollapsible{true, true, SplitterIconVisibility::Always}),
              SplitterPanel{SplitterPanelContent{[] { gallery_label("gallery.copy.sample.008"); }}}
                  .collapsible(SplitterCollapsible{true, true, SplitterIconVisibility::Always})
                  .destroyOnHidden(false)});
}

void add_splitter_nested_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Splitter(SplitterProps{}.layout(LayoutStyle{}.width(state->document_width).height(dp(300))),
             {SplitterPanel{SplitterPanelContent{[] { gallery_label("gallery.copy.sample.009"); }}}
                  .defaultSize(SplitterSize::percent(30))
                  .collapsible(),
              SplitterPanel{SplitterPanelContent{[] {
                  Splitter(SplitterProps{}.orientation(SplitterOrientation::Vertical),
                           {SplitterPanel{SplitterPanelContent{[] {
                                gallery_label("gallery.copy.sample.010");
                            }}}.collapsible(),
                            SplitterPanel{SplitterPanelContent{[] {
                                Input(InputProps{}.defaultValue(u8"嵌套下方草稿"));
                            }}}.collapsible()});
              }}}});
}

void add_shell_structure_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    ThemeConfig header_theme;
    header_theme.layout.tokens.header_text = Color::rgba8(255, 255, 255);
    Theme(ThemeProps{}.config(header_theme), ThemeContent{[state] {
              Layout(
                  LayoutProps{}.layout(LayoutStyle{}.width(state->document_width).height(dp(320))), LayoutContent{[] {
                      Header(HeaderProps{}, HeaderContent{[] { gallery_label("gallery.complete.copy.157"); }});
                      Layout(LayoutProps{}, LayoutContent{[] {
                                 Sider(SiderProps{}.width(dp(152)).theme(SiderTheme::Light), SiderContent{[] {
                                           Flex(FlexProps{}.vertical(true).gap(dp(12)), FlexContent{[] {
                                                    gallery_label("gallery.copy.sample.011");
                                                    Button(ButtonProps{},
                                                           [] { gallery_label("gallery.copy.sample.012"); });
                                                    Button(ButtonProps{},
                                                           [] { gallery_label("gallery.copy.sample.013"); });
                                                }});
                                       }});
                                 ContentArea(ContentProps{}, ContentBody{[] {
                                                 Flex(FlexProps{}.vertical(true).gap(dp(12)), FlexContent{[] {
                                                          gallery_label("gallery.copy.sample.014");
                                                          Input(InputProps{}.defaultValue(u8"调整侧栏时保留此草稿"));
                                                      }});
                                             }});
                                 Sider(SiderProps{}.width(dp(120)).collapsedWidth(dp(48)).reverseArrow().collapsible(),
                                       SiderContent{[] { gallery_label("gallery.copy.sample.015"); }});
                             }});
                      Footer(FooterProps{}, FooterContent{[] { gallery_label("gallery.copy.sample.016"); }});
                  }});
          }});
}

void add_shell_responsive_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(TextProps{}.content(state->shell_feedback));
    Layout(LayoutProps{}.layout(LayoutStyle{}.width(state->document_width).height(dp(260))), LayoutContent{[state] {
               Sider(SiderProps{}
                         .width(dp(200))
                         .collapsedWidth(dp(0))
                         .collapsible()
                         .breakpoint(SiderBreakpoint::Lg)
                         .onBreakpoint([state](bool broken) {
                             state->shell_feedback.set(broken ? GalleryMessage{"gallery.feedback.050", {}}
                                                              : GalleryMessage{"gallery.feedback.051", {}});
                         })
                         .onCollapse([state](bool, SiderCollapseType) { ++state->telemetry.activations; }),
                     SiderContent{[] {
                         Flex(FlexProps{}.vertical(true).gap(dp(12)), FlexContent{[] {
                                  gallery_label("gallery.copy.sample.017");
                                  Input(InputProps{}.defaultValue(u8"零宽折叠后仍保留"));
                              }});
                     }});
               ContentArea(ContentProps{}, ContentBody{[] { gallery_label("gallery.complete.copy.020"); }});
           }});
}

void add_shell_controlled_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Space(SpaceProps{}.size(dp(8)), [state] {
        Button(ButtonProps{}.onClick([state] { state->shell_collapsed.set(!state->shell_collapsed.get()); }),
               [] { gallery_label("gallery.copy.sample.018"); });
        Button(ButtonProps{}.onClick([state] { state->shell_light.set(!state->shell_light.get()); }),
               [] { gallery_label("gallery.complete.copy.158"); });
    });
    Layout(LayoutProps{}.layout(LayoutStyle{}.width(state->document_width).height(dp(220))), LayoutContent{[state] {
               Sider(
                   SiderProps{}
                       .width(dp(180))
                       .collapsedWidth(dp(64))
                       .collapsible()
                       .collapsed(state->shell_collapsed)
                       .theme(bind([state] { return state->shell_light.get() ? SiderTheme::Light : SiderTheme::Dark; }))
                       .trigger(SiderTrigger{[] { gallery_label("gallery.copy.sample.019"); }})
                       .onCollapse([state](bool value, SiderCollapseType) {
                           state->shell_collapsed.set(value);
                           ++state->telemetry.activations;
                       }),
                   SiderContent{[] { gallery_label("gallery.copy.sample.020"); }});
               ContentArea(ContentProps{}, ContentBody{[] { gallery_label("gallery.copy.sample.021"); }});
               Sider(SiderProps{}
                         .width(dp(100))
                         .collapsedWidth(dp(32))
                         .collapsed(state->shell_collapsed)
                         .collapsible()
                         .trigger(SiderTriggerMode::Hidden)
                         .theme(SiderTheme::Light),
                     SiderContent{[] { gallery_label("gallery.copy.sample.022"); }});
           }});
}

void add_theme_button_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Space(ryn::SpaceProps{}
                   .wrap(true)
                   .align(ryn::SpaceAlign::Center)
                   .size(ryn::dp(8))
                   .layout(ryn::LayoutStyle{}.width(state->document_width)),
               [state] {
                   themed_button(state, "gallery.theme.default", "gallery.complete.copy.341",
                                 algorithm_config(ryn::ThemeAlgorithm::Default), ryn::ButtonType::Default,
                                 ryn::ControlSize::Middle, false, false, [state] {
                                     state->set_theme(algorithm_config(ryn::ThemeAlgorithm::Default));
                                     ++state->telemetry.activations;
                                 });
                   themed_button(state, "gallery.theme.dark", "gallery.complete.copy.342",
                                 algorithm_config(ryn::ThemeAlgorithm::Dark), ryn::ButtonType::Default,
                                 ryn::ControlSize::Middle, false, false, [state] {
                                     state->set_theme(algorithm_config(ryn::ThemeAlgorithm::Dark));
                                     ++state->telemetry.activations;
                                 });
                   themed_button(state, "gallery.theme.compact", "gallery.complete.copy.343",
                                 algorithm_config(ryn::ThemeAlgorithm::Compact), ryn::ButtonType::Default,
                                 ryn::ControlSize::Middle, false, false, [state] {
                                     state->set_theme(algorithm_config(ryn::ThemeAlgorithm::Compact));
                                     ++state->telemetry.activations;
                                 });
                   auto nested = algorithm_config(ryn::ThemeAlgorithm::Dark);
                   nested.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
                   themed_button(state, "gallery.theme.nested-brand", "gallery.complete.copy.344", nested,
                                 ryn::ButtonType::Primary);
               });
}

void add_button_base_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Space(ryn::SpaceProps{}
                   .wrap(true)
                   .align(ryn::SpaceAlign::Center)
                   .size(ryn::dp(8))
                   .layout(ryn::LayoutStyle{}.width(state->document_width)),
               [state] {
                   const auto& defaults = ryn::detail::default_theme_config();
                   themed_button(state, "gallery.state.default", "gallery.complete.copy.345", defaults);
                   themed_button(state, "gallery.state.primary", "gallery.complete.copy.346", defaults,
                                 ryn::ButtonType::Primary);
                   themed_button(state, "gallery.state.danger", "gallery.complete.copy.347", defaults,
                                 ryn::ButtonType::Danger);
                   themed_button(state, "gallery.state.hover", "gallery.complete.copy.348", defaults);
                   themed_button(state, "gallery.state.active", "gallery.complete.copy.349", defaults);
                   themed_button(state, "gallery.state.focus-visible", "gallery.complete.copy.350", defaults);
                   themed_button(state, "gallery.state.disabled", "gallery.complete.copy.351", defaults,
                                 ryn::ButtonType::Default, ryn::ControlSize::Middle, state->disabled);
                   themed_button(state, "gallery.state.loading", "gallery.complete.copy.352", defaults,
                                 ryn::ButtonType::Primary, ryn::ControlSize::Middle, false, state->loading);
               });
}

void add_input_base_samples(const std::shared_ptr<GalleryState>& state) {
    gallery_label("gallery.copy.sample.023");
    gallery_label("gallery.copy.sample.024");
    gallery_label("gallery.copy.sample.025");
    ryn::Space(ryn::SpaceProps{}
                   .align(ryn::SpaceAlign::Start)
                   .wrap(true)
                   .size(ryn::dp(8.0F))
                   .layout(ryn::LayoutStyle{}.width(state->document_width)),
               [state] {
                   ryn::Theme(ryn::ThemeProps{}, ryn::ThemeContent{[state] {
                                  ++state->telemetry.theme_content_runs;
                                  ryn::Flex(ryn::FlexProps{}.vertical(true).gap(ryn::dp(8)), [state] {
                                      gallery_label("gallery.copy.sample.026");
                                      ryn::Input(ryn::InputProps{}
                                                     .value(state->input_value)
                                                     .placeholder(gallery_text("gallery.copy.property.007"))
                                                     .maxLength(32)
                                                     .status(ryn::bind([value = state->input_value] {
                                                         return value.get().empty() ? ryn::InputStatus::Default
                                                                                    : ryn::InputStatus::Warning;
                                                     }))
                                                     .onChange([state](ryn::String next) {
                                                         ++state->telemetry.input_changes;
                                                         state->input_value.set(std::move(next));
                                                     })
                                                     .onSubmit([state](ryn::String next) {
                                                         ++state->telemetry.input_submits;
                                                         state->input_feedback.set(std::move(next));
                                                     })
                                                     .layout(ryn::LayoutStyle{}.width(state->cell_width)),
                                                 ryn::InputPrefix{[] { gallery_label("gallery.copy.sample.027"); }},
                                                 ryn::InputSuffix{[] { ryn::Text(u8"32"); }});
                                      remember_target(state, "gallery.input.controlled");
                                      ++state->telemetry.live_samples;
                                  });
                              }});
                   ryn::Theme(ryn::ThemeProps{}, ryn::ThemeContent{[state] {
                                  ++state->telemetry.theme_content_runs;
                                  ryn::Flex(ryn::FlexProps{}.vertical(true).gap(ryn::dp(8)), [state] {
                                      gallery_label("gallery.copy.sample.028");
                                      ryn::Input(ryn::InputProps{}
                                                     .defaultValue(state->initial("gallery.input.uncontrolled",
                                                                                  ryn::String{u8"非受控 / Input"}))
                                                     .placeholder(gallery_text("gallery.copy.property.008"))
                                                     .allowClear(true)
                                                     .disabled(state->clear_disabled)
                                                     .onChange([state](ryn::String next) {
                                                         state->remember("gallery.input.uncontrolled", next);
                                                         ++state->telemetry.input_changes;
                                                         state->input_feedback.set(std::move(next));
                                                     })
                                                     .onSubmit([state](ryn::String next) {
                                                         ++state->telemetry.input_submits;
                                                         state->input_feedback.set(std::move(next));
                                                     })
                                                     .layout(ryn::LayoutStyle{}.width(state->cell_width)));
                                      remember_target(state, "gallery.input.uncontrolled");
                                      ++state->telemetry.live_samples;
                                  });
                              }});
               });
    ryn::Text(ryn::TextProps{}.content(state->input_feedback));
}

void add_search_base_samples(const std::shared_ptr<GalleryState>& state) {
    gallery_label("gallery.copy.sample.029");
    gallery_label("gallery.copy.sample.030");
    ryn::Space(
        ryn::SpaceProps{}
            .align(ryn::SpaceAlign::Start)
            .wrap(true)
            .size(ryn::dp(8.0F))
            .layout(ryn::LayoutStyle{}.width(state->document_width)),
        [state] {
            ryn::Search(ryn::SearchProps{}
                            .value(state->search_value)
                            .placeholder(gallery_text("gallery.copy.property.009"))
                            .onChange([state](ryn::String next) { state->search_value.set(std::move(next)); })
                            .onSearch([state](ryn::String next, ryn::SearchSource) {
                                ++state->telemetry.search_submits;
                                state->search_feedback.set(std::move(next));
                            })
                            .layout(ryn::LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.search.controlled");
            ++state->telemetry.live_samples;
            ryn::Search(ryn::SearchProps{}
                            .defaultValue(state->initial("gallery.search.large", ryn::String{u8"RynUI"}))
                            .size(ryn::ControlSize::Large)
                            .enterButton(true)
                            .onSearch([state](ryn::String next, ryn::SearchSource) {
                                ++state->telemetry.search_submits;
                                state->search_feedback.set(std::move(next));
                            })

                            .onChange([state](ryn::String value) { state->remember("gallery.search.large", value); })
                            .layout(ryn::LayoutStyle{}.width(state->cell_width)),
                        ryn::SearchButtonContent{[] { gallery_label("gallery.copy.sample.031"); }});
            remember_target(state, "gallery.search.large");
            ++state->telemetry.live_samples;
            ryn::Search(ryn::SearchProps{}
                            .defaultValue(state->initial("gallery.search.small", ryn::String{u8"Small / 小号"}))
                            .size(ryn::ControlSize::Small)

                            .onChange([state](ryn::String value) { state->remember("gallery.search.small", value); })
                            .layout(ryn::LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.search.small");
            ++state->telemetry.live_samples;
            ryn::Search(ryn::SearchProps{}
                            .defaultValue(state->initial("gallery.search.loading", ryn::String{u8"loading"}))
                            .enterButton(true)
                            .loading(state->loading)

                            .onChange([state](ryn::String value) { state->remember("gallery.search.loading", value); })
                            .layout(ryn::LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.search.loading");
            ++state->telemetry.live_samples;
            ryn::Search(ryn::SearchProps{}
                            .defaultValue(state->initial("gallery.search.disabled", ryn::String{u8"disabled"}))
                            .disabled(state->disabled)

                            .onChange([state](ryn::String value) { state->remember("gallery.search.disabled", value); })
                            .layout(ryn::LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.search.disabled");
            ++state->telemetry.live_samples;
        });
    ryn::Text(ryn::TextProps{}.content(state->search_feedback));
}

void add_password_base_samples(const std::shared_ptr<GalleryState>& state) {
    gallery_label("gallery.copy.sample.032");
    ryn::Space(ryn::SpaceProps{}
                   .align(ryn::SpaceAlign::Start)
                   .wrap(true)
                   .size(ryn::dp(8.0F))
                   .layout(ryn::LayoutStyle{}.width(state->document_width)),
               [state] {
                   ryn::Password(ryn::PasswordProps{}
                                     .defaultValue(u8"RynUI 密码")
                                     .placeholder(gallery_text("gallery.copy.property.010"))
                                     .layout(ryn::LayoutStyle{}.width(state->cell_width)));
                   remember_target(state, "gallery.password.basic");
                   ++state->telemetry.live_samples;
                   ryn::Password(ryn::PasswordProps{}
                                     .defaultValue(u8"禁用密码")
                                     .disabled(true)
                                     .layout(ryn::LayoutStyle{}.width(state->cell_width)));
                   remember_target(state, "gallery.password.disabled");
                   ++state->telemetry.live_samples;
               });
}

void add_switch_base_samples(const std::shared_ptr<GalleryState>& state) {
    gallery_label("gallery.copy.sample.033");
    ryn::Space(ryn::SpaceProps{}
                   .align(ryn::SpaceAlign::Start)
                   .wrap(true)
                   .align(ryn::SpaceAlign::Center)
                   .size(ryn::dp(12.0F))
                   .layout(ryn::LayoutStyle{}.width(state->document_width)),
               [state] {
                   gallery_label("gallery.copy.sample.034");
                   ryn::Switch(ryn::SwitchProps{}.checked(state->switch_checked).onChange([state](bool value) {
                       state->switch_checked.set(value);
                   }));
                   ++state->telemetry.live_samples;
                   gallery_label("gallery.copy.sample.035");
                   ryn::Switch(ryn::SwitchProps{}
                                   .size(ryn::SwitchSize::Small)
                                   .defaultChecked(gallery_initial_value<bool>("gallery.switch.basic-small", true))
                                   .onChange(gallery_value_changed<bool>("gallery.switch.basic-small")));
                   gallery_target("gallery.switch.basic-small");
                   ++state->telemetry.live_samples;
                   gallery_label("gallery.copy.sample.036");
                   ryn::Switch(ryn::SwitchProps{}
                                   .disabled(true)
                                   .defaultChecked(gallery_initial_value<bool>("gallery.switch.basic-disabled", false))
                                   .onChange(gallery_value_changed<bool>("gallery.switch.basic-disabled")));
                   gallery_target("gallery.switch.basic-disabled");
                   ++state->telemetry.live_samples;
                   gallery_label("gallery.copy.sample.037");
                   ryn::Switch(ryn::SwitchProps{}
                                   .loading(state->loading)
                                   .defaultChecked(gallery_initial_value<bool>("gallery.switch.basic-loading", false))
                                   .onChange(gallery_value_changed<bool>("gallery.switch.basic-loading")));
                   gallery_target("gallery.switch.basic-loading");
                   ++state->telemetry.live_samples;
               });
}

void add_checkbox_base_samples(const std::shared_ptr<GalleryState>& state) {
    gallery_label("gallery.copy.sample.038");
    ryn::Space(
        ryn::SpaceProps{}
            .align(ryn::SpaceAlign::Start)
            .wrap(true)
            .align(ryn::SpaceAlign::Center)
            .size(ryn::dp(12.0F))
            .layout(ryn::LayoutStyle{}.width(state->document_width)),
        [state] {
            ryn::Checkbox(ryn::CheckboxProps{}.checked(state->checkbox_checked).onChange([state](bool value) {
                state->checkbox_checked.set(value);
            }),
                          ryn::CheckboxLabel{[] { gallery_label("gallery.copy.sample.039"); }});
            ++state->telemetry.live_samples;
            ryn::Checkbox(ryn::CheckboxProps{}
                              .defaultChecked(gallery_initial_value<bool>("gallery.checkbox.basic-checked", true))
                              .onChange(gallery_value_changed<bool>("gallery.checkbox.basic-checked")),
                          ryn::CheckboxLabel{[] { gallery_label("gallery.copy.sample.040"); }});
            gallery_target("gallery.checkbox.basic-checked");
            ++state->telemetry.live_samples;
            ryn::Checkbox(ryn::CheckboxProps{}
                              .indeterminate(true)
                              .defaultChecked(gallery_initial_value<bool>("gallery.checkbox.basic-mixed", false))
                              .onChange(gallery_value_changed<bool>("gallery.checkbox.basic-mixed")),
                          ryn::CheckboxLabel{[] { gallery_label("gallery.copy.sample.041"); }});
            gallery_target("gallery.checkbox.basic-mixed");
            ++state->telemetry.live_samples;
            ryn::Checkbox(ryn::CheckboxProps{}
                              .disabled(true)
                              .defaultChecked(gallery_initial_value<bool>("gallery.checkbox.basic-disabled", false))
                              .onChange(gallery_value_changed<bool>("gallery.checkbox.basic-disabled")),
                          ryn::CheckboxLabel{[] { gallery_label("gallery.copy.sample.042"); }});
            gallery_target("gallery.checkbox.basic-disabled");
            ++state->telemetry.live_samples;
        });
}

void add_radio_base_samples(const std::shared_ptr<GalleryState>& state) {
    gallery_label("gallery.copy.sample.043");
    ryn::Space(ryn::SpaceProps{}
                   .align(ryn::SpaceAlign::Start)
                   .wrap(true)
                   .align(ryn::SpaceAlign::Center)
                   .size(ryn::dp(12.0F))
                   .layout(ryn::LayoutStyle{}.width(state->document_width)),
               [state] {
                   ryn::Radio(ryn::RadioProps{}
                                  .defaultChecked(gallery_initial_value<bool>("gallery.radio.basic-standalone", true))
                                  .onChange(gallery_value_changed<bool>("gallery.radio.basic-standalone")),
                              ryn::RadioLabel{[] { gallery_label("gallery.copy.sample.044"); }});
                   gallery_target("gallery.radio.basic-standalone");
                   ++state->telemetry.live_samples;
                   ryn::RadioGroup(ryn::RadioGroupProps{}
                                       .options(gallery_option_labels<ryn::RadioOption>(
                                           std::vector<ryn::RadioOption>{
                                               {ryn::String{u8"a"}, ryn::String{u8"甲 / Alpha"}},
                                               {ryn::String{u8"b"}, ryn::String{u8"乙 / Beta"}},
                                               {ryn::String{u8"c"}, ryn::String{u8"禁用 / Disabled"}, true},
                                           },
                                           {{ryn::String{u8"a"}, "gallery.complete.copy.091"},
                                            {ryn::String{u8"b"}, "gallery.complete.copy.092"},
                                            {ryn::String{u8"c"}, "gallery.complete.copy.093"}}))
                                       .value(state->radio_selected)
                                       .onChange([state](const ryn::String& value) {
                                           state->radio_selected.set(std::optional<ryn::String>{value});
                                       }));
                   gallery_target("gallery.radio.controlled");
                   state->telemetry.live_samples += 3;
               });
}

void add_basic_live_samples(const std::shared_ptr<GalleryState>& state) {
    section_surface(state, gallery_document_sections()[5]);
    add_theme_button_samples(state);
    add_button_base_samples(state);
    add_input_base_samples(state);
    add_search_base_samples(state);
    add_password_base_samples(state);
    add_switch_base_samples(state);
    add_checkbox_base_samples(state);
    add_radio_base_samples(state);
}

void add_typography_features(const std::shared_ptr<GalleryState>& state) {
    gallery_label("gallery.copy.sample.045");
    remember_target(state, "gallery.typography.heading");
    const ryn::String full{u8"原始全文保持完整：省略显示时复制仍写入全部内容；点击展开查看全文，再次点击收起。"};
    ryn::Text(ryn::TypographyProps{}
                  .content(full)
                  .ellipsis(ryn::TypographyEllipsis{.expandable = true, .tooltip = full})
                  .copyable(ryn::TypographyCopyable{})
                  .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    remember_target(state, "gallery.typography.ellipsis");
    ryn::Paragraph(ryn::TypographyProps{}
                       .content(u8"多行省略保留字素边界，英文 Latin "
                                u8"和中文均复用同一文本测量通道。\n显式换行也计入行数。\n展开后显示全部行。")
                       .ellipsis(ryn::TypographyEllipsis{.rows = 2, .expandable = true})
                       .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    remember_target(state, "gallery.typography.paragraph-ellipsis");
    ryn::Paragraph(ryn::TypographyProps{}
                       .content(ryn::TypographyContent{{
                           {.content = u8"混合语义："},
                           {.content = u8"Strong ", .strong = true},
                           {.content = u8"Italic ", .italic = true},
                           {.content = u8"code <>&\" ", .code = true},
                           {.content = u8"Ctrl+C ", .keyboard = true},
                           {.content = u8"Mark ", .mark = true},
                           {.content = u8"Underline ", .underline = true},
                           {.content = u8"Deleted\n", .strikethrough = true},
                           {.content = u8"אבג / 中文 e\u0301：缩窄后省略，HTML 与纯文本仍复制完整来源。",
                            .type = ryn::TypographyType::Danger},
                       }})
                       .ellipsis(ryn::TypographyEllipsis{.rows = 2, .expandable = true})
                       .copyable(ryn::TypographyCopyable{.format = ryn::TypographyCopyFormat::Html})
                       .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    remember_target(state, "gallery.typography.rich-content");
    ryn::Paragraph(ryn::TypographyProps{}
                       .content(state->typography_value)
                       .editable(ryn::TypographyEditable{.max_length = 64, .auto_size = {true, 1, 4}})
                       .onEdit([state](ryn::String value) { state->typography_value.set(std::move(value)); })
                       .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    remember_target(state, "gallery.typography.editable-paragraph");
    ryn::Title(ryn::TitleProps{}
                   .level(ryn::TypographyLevel::H3)
                   .content(state->typography_title)
                   .editable(ryn::TypographyEditable{})
                   .onEdit([state](ryn::String value) { state->typography_title.set(std::move(value)); })
                   .layout(ryn::LayoutStyle{}.width(state->document_width)));
    remember_target(state, "gallery.typography.editable-title");
    ryn::Link(ryn::LinkProps{}.content(gallery_text("gallery.complete.copy.015")).onClick([state] {
        ++state->telemetry.activations;
    }));
    remember_target(state, "gallery.typography.link");
    ryn::Link(ryn::LinkProps{}.content(gallery_text("gallery.complete.copy.016")).disabled(true));
    remember_target(state, "gallery.typography.disabled-link");
    state->telemetry.live_samples += 7;
}

void add_divider_base_samples(const std::shared_ptr<GalleryState>& state) {
    gallery_label("gallery.copy.sample.046");
    ryn::Divider(ryn::DividerProps{}
                     .orientation(ryn::DividerOrientation::Left)
                     .content(gallery_text("gallery.complete.copy.407")));
    remember_target(state, "gallery.divider.left");
    ryn::Divider(ryn::DividerProps{}
                     .orientation(ryn::DividerOrientation::Right)
                     .content(gallery_text("gallery.complete.copy.408"))
                     .dashed(true));
    remember_target(state, "gallery.divider.right");
    ryn::Divider(ryn::DividerProps{}.content(gallery_text("gallery.complete.copy.409")).plain(true));
    remember_target(state, "gallery.divider.plain");
    ryn::Divider(ryn::DividerProps{}
                     .content(gallery_text("gallery.copy.property.011"))
                     .orientation(ryn::DividerOrientation::Left)
                     .orientationMargin(ryn::DividerOrientationMargin::none()));
    remember_target(state, "gallery.divider.no-margin");
    ryn::Space(ryn::SpaceProps{}.align(ryn::SpaceAlign::Center), [state] {
        gallery_label("gallery.copy.sample.047");
        ryn::Divider(ryn::DividerProps{}.type(ryn::DividerType::Vertical));
        remember_target(state, "gallery.divider.vertical");
        gallery_label("gallery.copy.sample.048");
    });
    state->telemetry.live_samples += 5;
}

void add_tooltip_samples(const std::shared_ptr<GalleryState>& state) {
    gallery_label("gallery.copy.sample.049");
    if (state->page_component == "ant.component.tooltip") {
        ryn::Tooltip(ryn::TooltipProps{}
                         .title(gallery_text("gallery.copy.property.012"))
                         .triggers(ryn::TooltipTriggers{true, true, true, true})
                         .direction(ryn::TextDirection::RightToLeft)
                         .placement(ryn::TooltipPlacement::BottomLeft)
                         .animated(),
                     ryn::TooltipTrigger{[] {
                         ryn::Button(ryn::ButtonProps{}, [] { gallery_label("gallery.copy.sample.050"); });
                         gallery_target("gallery.tooltip.rtl-combined");
                     }});
        ++state->telemetry.live_samples;
    }
    ryn::Space(ryn::SpaceProps{}.align(ryn::SpaceAlign::Start).wrap(true), [] {
        ryn::Tooltip(ryn::TooltipProps{}.title(gallery_text("gallery.copy.property.013")), ryn::TooltipTrigger{[] {
                         ryn::Button(ryn::ButtonProps{},
                                     ryn::ButtonContent{[] { gallery_label("gallery.copy.sample.051"); }});
                     }});
        ryn::Tooltip(ryn::TooltipProps{}
                         .title(gallery_text("gallery.copy.property.014"))
                         .placement(ryn::TooltipPlacement::Right),
                     ryn::TooltipTrigger{[] {
                         ryn::Button(ryn::ButtonProps{}.disabled(true),
                                     ryn::ButtonContent{[] { gallery_label("gallery.copy.sample.052"); }});
                     }});
    });
    state->telemetry.live_samples += 2;
    ryn::Space(ryn::SpaceProps{}.align(ryn::SpaceAlign::Start).wrap(true), [] {
        ryn::Tooltip(ryn::TooltipProps{}.trigger(ryn::TooltipTriggerMode::Click), ryn::TooltipTrigger{[] {
                         ryn::Button(ryn::ButtonProps{},
                                     ryn::ButtonContent{[] { gallery_label("gallery.copy.sample.053"); }});
                     }},
                     ryn::TooltipTitle{[] {
                         ryn::Flex(ryn::FlexProps{}.align(ryn::FlexAlign::Start).vertical(true), ryn::FlexContent{[] {
                                       gallery_label("gallery.copy.sample.054");
                                       ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CheckOutlined));
                                   }});
                     }});
        ryn::Tooltip(ryn::TooltipProps{}
                         .title(gallery_text("gallery.copy.property.015"))
                         .trigger(ryn::TooltipTriggerMode::ContextMenu),
                     ryn::TooltipTrigger{[] {
                         ryn::Button(ryn::ButtonProps{},
                                     ryn::ButtonContent{[] { gallery_label("gallery.copy.sample.055"); }});
                     }});
        ryn::Tooltip(ryn::TooltipProps{}
                         .title(gallery_text("gallery.copy.property.016"))
                         .triggers(ryn::TooltipTriggers{true, true, true, false})
                         .placement(ryn::TooltipPlacement::BottomLeft)
                         .pointAtCenter(true),
                     ryn::TooltipTrigger{[] {
                         ryn::Button(ryn::ButtonProps{},
                                     ryn::ButtonContent{[] { gallery_label("gallery.copy.sample.056"); }});
                     }});
    });
    state->telemetry.live_samples += 3;
}

void add_slider_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(ryn::TextProps{}.content(state->selection_focus));
    gallery_label("gallery.copy.sample.057");
    ryn::Slider(ryn::SliderProps{}
                    .value(state->slider_value)
                    .onFocus([state] { state->selection_focus.set(GalleryMessage{"gallery.feedback.052", {}}); })
                    .onBlur([state] { state->selection_focus.set(GalleryMessage{"gallery.feedback.053", {}}); })
                    .marks(ryn::bind([tr = ryn::useI18n()] {
                        return ryn::SliderMarks{{0, tr.translate(gallery_message("gallery.complete.copy.425"))},
                                                {50, tr.translate(gallery_message("gallery.complete.copy.426"))},
                                                {100, tr.translate(gallery_message("gallery.complete.copy.427"))}};
                    }))
                    .onChange([state](double value) { state->slider_value.set(value); })
                    .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    gallery_target("gallery.slider.controlled");
    ryn::RangeSlider(
        ryn::RangeSliderProps{}
            .value(state->slider_range)
            .marks(ryn::SliderMarks{{20, ryn::String{u8"20"}}, {50, ryn::String{u8"50"}}, {80, ryn::String{u8"80"}}})
            .marksOnly(true)
            .dots(true)
            .onChange([state](ryn::SliderRange value) { state->slider_range.set(value); })
            .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::Slider(ryn::SliderProps{}
                    .defaultValue(gallery_initial_value<double>("gallery.slider.reverse-step", double{35}))
                    .onChange(gallery_value_changed<double>("gallery.slider.reverse-step"))
                    .limits(ryn::SliderLimits{0, 100, 10})
                    .dots(true)
                    .included(false)
                    .reverse(true)
                    .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    gallery_target("gallery.slider.reverse-step");
    ryn::Slider(ryn::SliderProps{}
                    .defaultValue(gallery_initial_value<double>("gallery.slider.disabled", double{60}))
                    .onChange(gallery_value_changed<double>("gallery.slider.disabled"))
                    .disabled(true)
                    .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    gallery_target("gallery.slider.disabled");
    ryn::Slider(ryn::SliderProps{}
                    .defaultValue(gallery_initial_value<double>("gallery.slider.vertical", double{40}))
                    .onChange(gallery_value_changed<double>("gallery.slider.vertical"))
                    .orientation(ryn::SliderOrientation::Vertical)
                    .layout(ryn::LayoutStyle{}.height(ryn::dp(120))));
    gallery_target("gallery.slider.vertical");
    state->telemetry.live_samples += 5;
    ryn::RangeSlider(ryn::RangeSliderProps{}
                         .defaultValue(gallery_initial_value<ryn::SliderRange>("gallery.slider.range-track",
                                                                               ryn::SliderRange{20, 80}))
                         .onChange(gallery_value_changed<ryn::SliderRange>("gallery.slider.range-track"))
                         .draggableTrack(true)
                         .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    gallery_target("gallery.slider.range-track");
    ryn::MultiSlider(ryn::MultiSliderProps{}
                         .defaultValue(gallery_initial_value<ryn::SliderValues>("gallery.slider.multi-edit",
                                                                                ryn::SliderValues{20, 50, 80}))
                         .onChange(gallery_value_changed<ryn::SliderValues>("gallery.slider.multi-edit"))
                         .rangeOptions(ryn::SliderRangeOptions{false, true, 0, 6})
                         .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    gallery_target("gallery.slider.multi-edit");
    ryn::MultiSlider(ryn::MultiSliderProps{}
                         .defaultValue(gallery_initial_value<ryn::SliderValues>("gallery.slider.multi-disabled",
                                                                                ryn::SliderValues{20, 50, 80}))
                         .onChange(gallery_value_changed<ryn::SliderValues>("gallery.slider.multi-disabled"))
                         .handleDisabled(ryn::SliderDisabledHandles{false, true, false})
                         .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    gallery_target("gallery.slider.multi-disabled");
    state->telemetry.live_samples += 3;
}

void add_typography_layout_slider_samples(const std::shared_ptr<GalleryState>& state) {
    add_typography_features(state);
    add_divider_base_samples(state);
    add_tooltip_samples(state);
    add_slider_samples(state);
}

void add_button_variant_samples(const std::shared_ptr<GalleryState>& state) {
    gallery_label("gallery.copy.sample.058");
    ryn::Space(
        ryn::SpaceProps{}
            .align(ryn::SpaceAlign::Start)
            .wrap(true)
            .layout(ryn::LayoutStyle{}.width(state->document_width)),
        [state] {
            constexpr std::array colors{ryn::ButtonColor::Blue,    ryn::ButtonColor::Purple,   ryn::ButtonColor::Cyan,
                                        ryn::ButtonColor::Green,   ryn::ButtonColor::Magenta,  ryn::ButtonColor::Pink,
                                        ryn::ButtonColor::Red,     ryn::ButtonColor::Orange,   ryn::ButtonColor::Yellow,
                                        ryn::ButtonColor::Volcano, ryn::ButtonColor::Geekblue, ryn::ButtonColor::Lime,
                                        ryn::ButtonColor::Gold};
            constexpr std::array<std::string_view, 13> names{"blue",     "purple", "cyan",   "green",  "magenta",
                                                             "pink",     "red",    "orange", "yellow", "volcano",
                                                             "geekblue", "lime",   "gold"};
            for (std::size_t index = 0; index < colors.size(); ++index) {
                const auto caption = label(std::string{"gallery.button."} + std::string{names[index]}, names[index]);
                ryn::Button(ryn::ButtonProps{}.color(colors[index]).variant(ryn::ButtonVariant::Solid),
                            [caption] { ryn::Text(caption); });
                remember_target(state, std::string{"gallery.button."} + std::string{names[index]});
            }
            state->telemetry.live_samples += colors.size();
        });
    ryn::Space(
        ryn::SpaceProps{}
            .align(ryn::SpaceAlign::Start)
            .wrap(true)
            .layout(ryn::LayoutStyle{}.width(state->document_width)),
        [state] {
            constexpr std::array variants{ryn::ButtonVariant::Outlined, ryn::ButtonVariant::Dashed,
                                          ryn::ButtonVariant::Solid,    ryn::ButtonVariant::Filled,
                                          ryn::ButtonVariant::Text,     ryn::ButtonVariant::Link};
            constexpr std::array<std::string_view, 6> names{"outlined", "dashed", "solid", "filled", "text", "link"};
            for (std::size_t index = 0; index < variants.size(); ++index) {
                const auto caption = label(std::string{"gallery.button."} + std::string{names[index]}, names[index]);
                ryn::Button(ryn::ButtonProps{}.color(ryn::ButtonColor::Blue).variant(variants[index]),
                            [caption] { ryn::Text(caption); });
                remember_target(state, std::string{"gallery.button."} + std::string{names[index]});
            }
            state->telemetry.live_samples += variants.size();
        });
    ryn::Space(ryn::SpaceProps{}
                   .align(ryn::SpaceAlign::Start)
                   .wrap(true)
                   .layout(ryn::LayoutStyle{}.width(state->document_width)),
               [state] {
                   ryn::Button(ryn::ButtonProps{}
                                   .ref(state->button_ref)
                                   .loading(state->button_loading)
                                   .loadingDelay(ryn::Duration::milliseconds(150))
                                   .iconPlacement(ryn::ButtonIconPlacement::End),
                               ryn::ButtonContent{[] { gallery_label("gallery.copy.sample.059"); }},
                               ryn::ButtonIcon{[] { ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CheckOutlined)); }},
                               ryn::ButtonLoadingIcon{[] { ryn::Text(u8"…"); }});
                   remember_target(state, "gallery.button.icon-end");
                   ryn::Button(ryn::ButtonProps{}.shape(ryn::ButtonShape::Circle),
                               ryn::ButtonSlots{.icon = ryn::ButtonIcon{[] {
                                                    ryn::Icon(ryn::IconProps{}.name(ryn::IconName::SearchOutlined));
                                                }}});
                   remember_target(state, "gallery.button.circle-icon");
                   ryn::Button(ryn::ButtonProps{}.shape(ryn::ButtonShape::Square),
                               [] { gallery_label("gallery.copy.sample.060"); });
                   remember_target(state, "gallery.button.square");
                   ryn::Button(
                       ryn::ButtonProps{}.color(ryn::ButtonColor::Blue).variant(ryn::ButtonVariant::Dashed).ghost(true),
                       [] { gallery_label("gallery.copy.sample.061"); });
                   remember_target(state, "gallery.button.ghost");
                   ryn::Button(ryn::ButtonProps{}.onClick([state] {
                       state->button_loading.set(!state->button_loading.get());
                       static_cast<void>(state->button_ref.focus());
                       ++state->telemetry.activations;
                   }),
                               [] { gallery_label("gallery.copy.sample.062"); });
                   remember_target(state, "gallery.button.loading-focus");
                   state->telemetry.live_samples += 5;
               });
    ryn::Button(ryn::ButtonProps{}.block(true).shape(ryn::ButtonShape::Round),
                [] { gallery_label("gallery.copy.sample.063"); });
    remember_target(state, "gallery.button.round-block");
    ++state->telemetry.live_samples;
}

void add_divider_variant_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Divider(ryn::DividerProps{}
                     .variant(ryn::DividerVariant::Dotted)
                     .size(ryn::ControlSize::Small)
                     .orientation(ryn::DividerOrientation::Start)
                     .direction(ryn::DividerDirection::RightToLeft)
                     .content(gallery_text("gallery.copy.property.017")));
    remember_target(state, "gallery.divider.small-dotted-rtl");
    ryn::Divider(ryn::DividerProps{}
                     .variant(ryn::DividerVariant::Dashed)
                     .size(ryn::ControlSize::Middle)
                     .orientation(ryn::DividerOrientation::End)
                     .orientationMargin(ryn::DividerOrientationMargin::length(ryn::dp(20)))
                     .content(gallery_text("gallery.copy.property.018")));
    remember_target(state, "gallery.divider.middle-length");
    ryn::Space(ryn::SpaceProps{}.align(ryn::SpaceAlign::Center), [state] {
        gallery_label("gallery.copy.sample.064");
        ryn::Divider(ryn::DividerProps{}.type(ryn::DividerType::Vertical).variant(ryn::DividerVariant::Dotted));
        remember_target(state, "gallery.divider.vertical-dotted");
        gallery_label("gallery.complete.copy.159");
    });
    state->telemetry.live_samples += 3;
}

void add_button_and_divider_variant_samples(const std::shared_ptr<GalleryState>& state) {
    add_button_variant_samples(state);
    add_divider_variant_samples(state);
}

// Keep individual sample scopes bounded under the Windows Debug default stack.
void add_live_samples(const std::shared_ptr<GalleryState>& state) {
    add_basic_live_samples(state);
    add_typography_layout_slider_samples(state);
    add_button_and_divider_variant_samples(state);
}

void add_switch_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(ryn::TextProps{}.content(state->selection_focus));
    gallery_label("gallery.copy.sample.065");
    ryn::Space(
        ryn::SpaceProps{}.align(ryn::SpaceAlign::Start).wrap(true).align(ryn::SpaceAlign::Center).size(ryn::dp(12)),
        [state] {
            gallery_label("gallery.copy.sample.066");
            ryn::Switch(
                ryn::SwitchProps{}
                    .checked(state->switch_checked)
                    .ref(state->switch_ref)
                    .onFocus([state] { state->selection_focus.set(GalleryMessage{"gallery.feedback.054", {}}); })
                    .onBlur([state] { state->selection_focus.set(GalleryMessage{"gallery.feedback.055", {}}); })
                    .onChange([state](bool value) { state->switch_checked.set(value); }),
                ryn::SwitchSlots{ryn::SwitchCheckedContent{[] { gallery_label("gallery.copy.sample.067"); }},
                                 ryn::SwitchUncheckedContent{[] { gallery_label("gallery.copy.sample.068"); }}});
            gallery_target("gallery.switch.controlled");
            gallery_label("gallery.copy.sample.069");
            ryn::Switch(ryn::SwitchProps{}
                            .size(ryn::SwitchSize::Small)
                            .direction(ryn::SwitchDirection::RightToLeft)
                            .defaultChecked(gallery_initial_value<bool>("gallery.switch.small-icon-rtl", false))
                            .onChange(gallery_value_changed<bool>("gallery.switch.small-icon-rtl")),
                        ryn::SwitchSlots{ryn::SwitchCheckedContent{
                                             [] { ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CheckOutlined)); }},
                                         ryn::SwitchUncheckedContent{
                                             [] { ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CloseOutlined)); }}});
            gallery_target("gallery.switch.small-icon-rtl");
            ryn::ThemeConfig custom;
            custom.switch_.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
            custom.switch_.algorithm = true;
            ryn::Theme(ryn::ThemeProps{}.config(custom), ryn::ThemeContent{[state] {
                           ++state->telemetry.theme_content_runs;
                           gallery_label("gallery.copy.sample.070");
                           ryn::Switch(
                               ryn::SwitchProps{}
                                   .defaultChecked(gallery_initial_value<bool>("gallery.switch.component-theme", true))
                                   .onChange(gallery_value_changed<bool>("gallery.switch.component-theme")),
                               ryn::SwitchSlots{
                                   ryn::SwitchCheckedContent{[] { gallery_label("gallery.copy.sample.071"); }},
                                   ryn::SwitchUncheckedContent{[] { gallery_label("gallery.copy.sample.072"); }}});
                           gallery_target("gallery.switch.component-theme");
                       }});
            ryn::Button(ryn::ButtonProps{}.onClick([state] {
                static_cast<void>(state->switch_ref.focus());
                ++state->telemetry.activations;
            }),
                        [] { gallery_label("gallery.copy.sample.073"); });
            gallery_target("gallery.switch.focus");
            state->telemetry.live_samples += 4;
        });
    ryn::Flex(ryn::FlexProps{}.align(ryn::FlexAlign::Start).layout(ryn::LayoutStyle{}.height(ryn::dp(48.0F))),
              ryn::FlexContent{[] {}});
}

void add_checkbox_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(ryn::TextProps{}.content(state->selection_focus));
    gallery_label("gallery.copy.sample.074");
    gallery_label("gallery.copy.sample.075");
    ryn::CheckboxGroup(
        ryn::CheckboxGroupProps{}
            .options(gallery_option_labels<ryn::CheckboxOption>(
                std::vector<ryn::CheckboxOption>{{ryn::String{u8"desktop"}, ryn::String{u8"桌面"}},
                                                 {1.0, ryn::String{u8"数字"}},
                                                 {true, ryn::String{u8"禁用布尔"}, true}},
                {{ryn::String{u8"desktop"}, "gallery.complete.copy.094"},
                 {1.0, "gallery.complete.copy.095"},
                 {true, "gallery.complete.copy.096"}}))
            .value(state->checkbox_values)
            .onChange([state](const ryn::CheckboxValues& values) { state->checkbox_values.set(values); }));
    gallery_target("gallery.checkbox.controlled");
    gallery_label("gallery.copy.sample.076");
    ryn::CheckboxGroup(ryn::CheckboxGroupProps{}
                           .options(ryn::bind([state, tr = ryn::useI18n()] {
                               auto result = state->checkbox_options.get();
                               for (auto& option : result) {
                                   if (const auto* text = std::get_if<ryn::String>(&option.value);
                                       text && *text == ryn::String{u8"b"} && state->checkbox_options_expanded.get()) {
                                       option.label = tr.translate(gallery_message("gallery.complete.copy.514"));
                                   } else if (const auto* number = std::get_if<double>(&option.value);
                                              number && *number == 2.0) {
                                       option.label = tr.translate(gallery_message("gallery.complete.copy.515"));
                                   }
                               }
                               return result;
                           }))
                           .defaultValue(gallery_initial_value<ryn::CheckboxValues>(
                               "gallery.checkbox.group-dynamic", ryn::CheckboxValues{ryn::String{u8"a"}}))
                           .onChange(gallery_value_changed<ryn::CheckboxValues>("gallery.checkbox.group-dynamic")));
    gallery_target("gallery.checkbox.dynamic");
    ryn::Button(ryn::ButtonProps{}.onClick([state] {
        state->checkbox_options_expanded.set(!state->checkbox_options_expanded.get());
        if (state->checkbox_options_expanded.get()) {
            state->checkbox_options.set({{ryn::String{u8"b"}, ryn::String{u8"B · retained"}},
                                         {ryn::String{u8"a"}, ryn::String{u8"A"}},
                                         {2.0, ryn::String{u8"新增数字"}}});
        } else {
            state->checkbox_options.set(
                {{ryn::String{u8"a"}, ryn::String{u8"A"}}, {ryn::String{u8"b"}, ryn::String{u8"B"}}});
        }
        ++state->telemetry.activations;
    }),
                [] { gallery_label("gallery.copy.sample.077"); });
    gallery_target("gallery.checkbox.options-update");
    ryn::Checkbox(ryn::CheckboxProps{}
                      .ref(state->checkbox_ref)
                      .onFocus([state] { state->selection_focus.set(GalleryMessage{"gallery.feedback.056", {}}); })
                      .onBlur([state] { state->selection_focus.set(GalleryMessage{"gallery.feedback.057", {}}); })
                      .indeterminate(true)
                      .direction(ryn::CheckboxDirection::RightToLeft)
                      .defaultChecked(gallery_initial_value<bool>("gallery.checkbox.rich-rtl-ref", false))
                      .onChange(gallery_value_changed<bool>("gallery.checkbox.rich-rtl-ref")),
                  ryn::CheckboxLabel{[] {
                      ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CheckOutlined));
                      gallery_label("gallery.copy.sample.078");
                  }});
    gallery_target("gallery.checkbox.rich-rtl-ref");
    ryn::ThemeConfig custom;
    custom.checkbox.algorithm = true;
    custom.checkbox.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
    custom.checkbox.tokens.size = ryn::dp(20);
    ryn::Theme(ryn::ThemeProps{}.config(custom), ryn::ThemeContent{[state] {
                   ++state->telemetry.theme_content_runs;
                   ryn::Checkbox(
                       ryn::CheckboxProps{}
                           .defaultChecked(gallery_initial_value<bool>("gallery.checkbox.component-theme", true))
                           .onChange(gallery_value_changed<bool>("gallery.checkbox.component-theme")),
                       ryn::CheckboxLabel{[] { gallery_label("gallery.copy.sample.079"); }});
                   gallery_target("gallery.checkbox.component-theme");
               }});
    ryn::Button(ryn::ButtonProps{}.onClick([state] {
        static_cast<void>(state->checkbox_ref.focus());
        ++state->telemetry.activations;
    }),
                [] { gallery_label("gallery.copy.sample.080"); });
    gallery_target("gallery.checkbox.focus");
    state->telemetry.live_samples += 6;
}

void add_radio_themed_sample(const std::shared_ptr<GalleryState>& state) {
    ryn::ThemeConfig custom;
    custom.radio.algorithm = true;
    custom.radio.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
    ryn::Theme(ryn::ThemeProps{}.config(custom), ryn::ThemeContent{[state] {
                   ++state->telemetry.theme_content_runs;
                   gallery_label("gallery.copy.sample.081");
                   ryn::RadioButton(
                       ryn::RadioProps{}
                           .defaultChecked(gallery_initial_value<bool>("gallery.radio.ref", true))
                           .onChange(gallery_value_changed<bool>("gallery.radio.ref"))
                           .ref(state->radio_ref)
                           .onFocus([state] { state->selection_focus.set(GalleryMessage{"gallery.feedback.058", {}}); })
                           .onBlur([state] { state->selection_focus.set(GalleryMessage{"gallery.feedback.059", {}}); }),
                       ryn::RadioLabel{[] { gallery_label("gallery.copy.sample.082"); }});
                   gallery_target("gallery.radio.ref");
               }});
}

void add_radio_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(ryn::TextProps{}.content(state->selection_focus));
    gallery_label("gallery.copy.sample.083");
    gallery_label("gallery.copy.sample.084");
    ryn::RadioGroup(ryn::RadioGroupProps{}
                        .options(ryn::bind([state, tr = ryn::useI18n()] {
                            auto result = state->radio_options.get();
                            for (auto& option : result) {
                                if (const auto* text = std::get_if<ryn::String>(&option.value);
                                    text && *text == ryn::String{u8"b"} && state->radio_options_expanded.get()) {
                                    option.label = tr.translate(gallery_message("gallery.complete.copy.516"));
                                } else if (const auto* number = std::get_if<double>(&option.value);
                                           number && *number == 2.0) {
                                    option.label = tr.translate(gallery_message("gallery.complete.copy.517"));
                                }
                            }
                            return result;
                        }))
                        .defaultValue(gallery_initial_value<ryn::RadioValue>("gallery.radio.dynamic-outline",
                                                                             ryn::RadioValue{ryn::String{u8"a"}}))
                        .onChange(gallery_value_changed<ryn::RadioValue>("gallery.radio.dynamic-outline"))
                        .optionType(ryn::RadioOptionType::Button));
    gallery_target("gallery.radio.dynamic-outline");
    ryn::Button(ryn::ButtonProps{}.onClick([state] {
        state->radio_options_expanded.set(!state->radio_options_expanded.get());
        if (state->radio_options_expanded.get()) {
            state->radio_options.set({{ryn::String{u8"b"}, ryn::String{u8"B · retained"}},
                                      {ryn::String{u8"a"}, ryn::String{u8"A"}},
                                      {2.0, ryn::String{u8"新增数字"}}});
        } else {
            state->radio_options.set(
                {{ryn::String{u8"a"}, ryn::String{u8"A"}}, {ryn::String{u8"b"}, ryn::String{u8"B"}}});
        }
        ++state->telemetry.activations;
    }),
                [] { gallery_label("gallery.copy.sample.085"); });
    gallery_target("gallery.radio.options-update");
    gallery_label("gallery.copy.sample.086");
    ryn::RadioGroup(ryn::RadioGroupProps{}
                        .options(gallery_option_labels<ryn::RadioOption>(
                            std::vector<ryn::RadioOption>{{ryn::String{u8"desktop"}, ryn::String{u8"桌面"}},
                                                          {1.0, ryn::String{u8"数字"}},
                                                          {true, ryn::String{u8"禁用布尔"}, true}},
                            {{ryn::String{u8"desktop"}, "gallery.complete.copy.097"},
                             {1.0, "gallery.complete.copy.098"},
                             {true, "gallery.complete.copy.099"}}))
                        .defaultValue(gallery_initial_value<ryn::RadioValue>("gallery.radio.solid-large-block",
                                                                             ryn::RadioValue{1.0}))
                        .onChange(gallery_value_changed<ryn::RadioValue>("gallery.radio.solid-large-block"))
                        .optionType(ryn::RadioOptionType::Button)
                        .buttonStyle(ryn::RadioButtonStyle::Solid)
                        .size(ryn::RadioSize::Large)
                        .block(true)
                        .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    gallery_target("gallery.radio.solid-large-block");
    gallery_label("gallery.copy.sample.087");
    ryn::RadioGroup(
        ryn::RadioGroupProps{}
            .options(gallery_option_labels<ryn::RadioOption>(
                std::vector<ryn::RadioOption>{
                    {false, ryn::String{u8"false"}}, {true, ryn::String{u8"true"}}, {3.0, ryn::String{u8"数字"}}},
                {{3.0, "gallery.complete.copy.100"}}))
            .defaultValue(
                gallery_initial_value<ryn::RadioValue>("gallery.radio.vertical-small-rtl", ryn::RadioValue{true}))
            .onChange(gallery_value_changed<ryn::RadioValue>("gallery.radio.vertical-small-rtl"))
            .optionType(ryn::RadioOptionType::Button)
            .size(ryn::RadioSize::Small)
            .orientation(ryn::RadioGroupOrientation::Vertical)
            .direction(ryn::RadioDirection::RightToLeft));
    gallery_target("gallery.radio.vertical-small-rtl");
    add_radio_themed_sample(state);
    ryn::Button(ryn::ButtonProps{}.onClick([state] {
        static_cast<void>(state->radio_ref.focus());
        ++state->telemetry.activations;
    }),
                [] { gallery_label("gallery.copy.sample.088"); });
    gallery_target("gallery.radio.focus");
    state->telemetry.live_samples += 6;
}

void add_flex_samples(const std::shared_ptr<GalleryState>& state) {
    gallery_label("gallery.copy.sample.089");
    gallery_label("gallery.copy.sample.090");
    ryn::Flex(ryn::FlexProps{}.align(ryn::FlexAlign::Baseline).gap(ryn::dp(8)), ryn::FlexContent{[state] {
                  gallery_label("gallery.copy.sample.091");
                  ryn::ThemeConfig large;
                  large.text.tokens.font_size = ryn::dp(28);
                  large.text.tokens.line_height = ryn::dp(40);
                  ryn::Theme(ryn::ThemeProps{}.config(large), ryn::ThemeContent{[state] {
                                 ++state->telemetry.theme_content_runs;
                                 gallery_label("gallery.copy.sample.092");
                             }});
                  ryn::Button(ryn::ButtonProps{}, [] { gallery_label("gallery.copy.sample.093"); });
              }});
    remember_target(state, "gallery.flex.baseline");
    gallery_label("gallery.copy.sample.094");
    ryn::Flex(ryn::FlexProps{}
                  .align(ryn::FlexAlign::Start)
                  .wrap(ryn::FlexWrap::WrapReverse)
                  .gap(ryn::dp(8))
                  .layout(ryn::LayoutStyle{}.width(ryn::dp(180)).height(ryn::dp(130))),
              ryn::FlexContent{[] {
                  for (const auto label :
                       {"gallery.complete.copy.428", "gallery.complete.copy.429", "gallery.complete.copy.430"}) {
                      ryn::Button(ryn::ButtonProps{}.layout(ryn::LayoutStyle{}.width(ryn::dp(110))),
                                  [label] { gallery_label(label); });
                  }
              }});
    remember_target(state, "gallery.flex.wrap-reverse");
    gallery_label("gallery.copy.sample.095");
    ryn::Flex(ryn::FlexProps{}
                  .align(ryn::FlexAlign::Start)
                  .vertical(true)
                  .wrap(true)
                  .direction(ryn::FlexDirection::RightToLeft)
                  .gap(ryn::dp(8))
                  .layout(ryn::LayoutStyle{}.width(ryn::dp(220)).height(ryn::dp(80))),
              ryn::FlexContent{[] {
                  for (const auto label :
                       {"gallery.complete.copy.431", "gallery.complete.copy.432", "gallery.complete.copy.433"}) {
                      ryn::Button(ryn::ButtonProps{}, [label] { gallery_label(label); });
                  }
              }});
    remember_target(state, "gallery.flex.vertical-rtl");
    gallery_label("gallery.copy.sample.096");
    ryn::Flex(ryn::FlexProps{}.gap(ryn::dp(8)).layout(ryn::LayoutStyle{}.height(ryn::dp(64))), ryn::FlexContent{[] {
                  gallery_label("gallery.copy.sample.097");
                  ryn::Text(ryn::TextProps{}
                                .content(gallery_text("gallery.copy.property.019"))
                                .layout(ryn::LayoutStyle{}.height(ryn::dp(22))));
              }});
    remember_target(state, "gallery.flex.default-stretch");
    state->telemetry.live_samples += 4;
}

void add_grid_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.098");
    gallery_label("gallery.copy.sample.099");
    GridGutter gutter{Responsive<LogicalLength>{dp(8)}.at(Breakpoint::Md, dp(24)), Responsive<LogicalLength>{dp(12)}};
    Row(RowProps{}.gutter(gutter).layout(LayoutStyle{}.width(state->document_width)), RowContent{[] {
            for (int index = 0; index < 4; ++index) {
                const auto options = Responsive<ColOptions>{ColOptions{.span = 24}}
                                         .at(Breakpoint::Sm, ColOptions{.span = 12})
                                         .at(Breakpoint::Md, ColOptions{.span = 8})
                                         .at(Breakpoint::Lg, ColOptions{.span = 6});
                Col(ColProps{}.responsive(options), ColContent{[index] {
                        Card(CardProps{}.title(
                                 gallery_text("gallery.complete.copy.493",
                                              MessageArgs{{u8"index", static_cast<std::uint64_t>(index + 1)}})),
                             CardContent{[] { Input(InputProps{}.defaultValue(u8"窗口变化保留草稿")); }});
                    }});
            }
        }});
    remember_target(state, "gallery.grid.responsive");
    gallery_label("gallery.copy.sample.100");
    Row(RowProps{}.gutter(dp(16), dp(16)).align(RowAlign::Middle).layout(LayoutStyle{}.width(state->document_width)),
        RowContent{[] {
            Col(ColProps{}.span(8).order(1).push(1),
                ColContent{[] { Button(ButtonProps{}, [] { gallery_label("gallery.complete.copy.160"); }); }});
            Col(ColProps{}.span(8).offset(8).order(0).pull(1),
                ColContent{[] { Button(ButtonProps{}, [] { gallery_label("gallery.complete.copy.161"); }); }});
            Col(ColProps{}.span(12), ColContent{[] { gallery_label("gallery.copy.sample.101"); }});
        }});
    remember_target(state, "gallery.grid.offset");
    gallery_label("gallery.copy.sample.102");
    Row(RowProps{}.wrap(false).gutter(dp(8)).layout(LayoutStyle{}.width(state->document_width)), RowContent{[] {
            Col(ColProps{}.flex(GridFlex::fixed(dp(120))), ColContent{[] { Text(u8"120 dp"); }});
            Col(ColProps{}.flex(GridFlex::fill()), ColContent{[] { Input(InputProps{}.defaultValue(u8"flex 1")); }});
            Col(ColProps{}.flex(GridFlex::fill(2)), ColContent{[] { Input(InputProps{}.defaultValue(u8"flex 2")); }});
        }});
    remember_target(state, "gallery.grid.flex");
    gallery_label("gallery.copy.sample.103");
    Signal<FlexDirection> direction{FlexDirection::RightToLeft};
    Signal<int> span{8};
    Space(SpaceProps{}, SpaceContent{[direction, span] {
              Button(ButtonProps{}.onClick([direction] {
                  direction.set(direction.get() == FlexDirection::RightToLeft ? FlexDirection::LeftToRight
                                                                              : FlexDirection::RightToLeft);
              }),
                     [] { gallery_label("gallery.copy.sample.104"); });
              Button(ButtonProps{}.onClick([span] { span.set(span.get() == 0 ? 8 : 0); }),
                     [] { gallery_label("gallery.copy.sample.105"); });
          }});
    Row(RowProps{}
            .direction(direction)
            .gutter(dp(16))
            .justify(RowJustify::SpaceBetween)
            .layout(LayoutStyle{}.width(state->document_width)),
        RowContent{[span] {
            Col(ColProps{}.span(span), ColContent{[] { Input(InputProps{}.defaultValue(u8"隐藏后保持草稿")); }});
            Col(ColProps{}.span(8), ColContent{[] { gallery_label("gallery.complete.copy.162"); }});
        }});
    remember_target(state, "gallery.grid.rtl");
    state->telemetry.live_samples += 4;
}

void add_masonry_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.106");
    gallery_label("gallery.copy.sample.107");
    std::vector<MasonryItem<>> cards;
    for (int index = 0; index < 6; ++index) {
        cards.push_back(MasonryItem<>{index}.children(MasonryContent{[index] {
            Card(CardProps{}.title(gallery_text("gallery.complete.copy.494",
                                                MessageArgs{{u8"index", static_cast<std::uint64_t>(index + 1)}})),
                 CardContent{[index] {
                     Flex(FlexProps{}.vertical(true).gap(SpaceSize::Small), [index] {
                         Text(TextProps{}.content(index % 2 == 0 ? gallery_text("gallery.complete.copy.387")
                                                                 : gallery_text("gallery.complete.copy.388")));
                         Input(InputProps{}.defaultValue(u8"缩放后保留草稿"));
                     });
                 }});
        }}));
    }
    Masonry(MasonryProps<>{}
                .items(cards)
                .columns(Responsive<int>{1}.at(Breakpoint::Sm, 2).at(Breakpoint::Lg, 3))
                .gutter(MasonryGutter{Responsive<LogicalLength>{dp(8)}.at(Breakpoint::Md, dp(16)),
                                      Responsive<LogicalLength>{dp(12)}})
                .layout(LayoutStyle{}.width(state->document_width)));
    remember_target(state, "gallery.masonry.measured");
    gallery_label("gallery.copy.sample.108");
    Signal<LogicalLength> height{dp(72)};
    auto dynamic = MasonryItem<>{u8"dynamic"}.children(MasonryContent{[height] {
        Card(CardProps{}.title(gallery_text("gallery.copy.property.020")).layout(LayoutStyle{}.height(height)),
             CardContent{[] { gallery_label("gallery.copy.sample.109"); }});
    }});
    auto pinned = MasonryItem<>{u8"pinned"}.column(1).height(dp(108)).children(MasonryContent{[] {
        Card(CardProps{}.title(gallery_text("gallery.copy.property.021")),
             CardContent{[] { Input(InputProps{}.defaultValue(u8"稳定 key")); }});
    }});
    Signal<std::vector<MasonryItem<>>> items{std::vector{dynamic, pinned}};
    GalleryFeedback allocation{GalleryMessage{"gallery.complete.copy.241", {}}, useI18n()};
    Space(SpaceProps{}.wrap(true), SpaceContent{[state, height, items] {
              Button(ButtonProps{}.onClick([height] { height.set(dp(height.get().value() < 100 ? 156 : 72)); }),
                     [] { gallery_label("gallery.copy.sample.110"); });
              remember_target(state, "gallery.masonry.resize");
              Button(ButtonProps{}.onClick([items] {
                  auto next = items.get();
                  const auto added =
                      std::ranges::find_if(next, [](const auto& item) { return item.key() == MasonryKey{u8"added"}; });
                  if (added == next.end()) {
                      next.push_back(MasonryItem<>{u8"added"}.children(MasonryContent{[] {
                          Card(CardProps{}.title(gallery_text("gallery.copy.property.022")),
                               CardContent{[] { Input(InputProps{}.defaultValue(u8"独立挂载")); }});
                      }}));
                  } else {
                      next.erase(added);
                  }
                  items.set(std::move(next));
              }),
                     [] { gallery_label("gallery.copy.sample.111"); });
              remember_target(state, "gallery.masonry.toggle");
              Button(ButtonProps{}.onClick([items] {
                  auto next = items.get();
                  std::reverse(next.begin(), next.end());
                  items.set(std::move(next));
              }),
                     [] { gallery_label("gallery.copy.sample.112"); });
              remember_target(state, "gallery.masonry.reverse");
          }});
    Text(TextProps{}.content(allocation));
    Masonry(MasonryProps<>{}
                .items(items)
                .columns(Responsive<int>{1}.at(Breakpoint::Md, 2))
                .gutter(dp(12))
                .fresh()
                .direction(FlexDirection::RightToLeft)
                .onLayoutChange([allocation](const auto& assignments) {
                    std::string text;
                    for (const auto& entry : assignments) {
                        std::visit(
                            [&](const auto& key) {
                                using Key = std::decay_t<decltype(key)>;
                                if constexpr (std::is_same_v<Key, String>) {
                                    text += key.bytes();
                                } else {
                                    text += std::to_string(key);
                                }
                            },
                            entry.key.value());
                        text += "=" + std::to_string(entry.column) + " ";
                    }
                    allocation.set(
                        GalleryMessage{"gallery.complete.copy.446", {{u8"columns", String::from_utf8(text).value()}}});
                })
                .layout(LayoutStyle{}.width(state->document_width)));
    remember_target(state, "gallery.masonry.dynamic");
    state->telemetry.live_samples += 2;
}

void add_space_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.complete.copy.163");
    gallery_label("gallery.copy.sample.113");
    Space(SpaceProps{}
              .wrap(true)
              .separator(SpaceSeparator{[] {
                  Text(u8"/");
                  Button(ButtonProps{}.type(ButtonType::Text).size(ControlSize::Small),
                         [] { gallery_label("gallery.copy.sample.114"); });
              }})
              .layout(LayoutStyle{}.width(state->document_width)),
          SpaceContent{[] {
              for (const auto label :
                   {"gallery.complete.copy.434", "gallery.complete.copy.435", "gallery.complete.copy.436"}) {
                  Button(ButtonProps{},
                         [label] { Text(String::from_utf8(reinterpret_cast<const char*>(label)).value()); });
              }
          }});
    remember_target(state, "gallery.space.separator");
    gallery_label("gallery.copy.sample.115");
    Space(SpaceProps{}.align(SpaceAlign::Baseline), SpaceContent{[] {
              gallery_label("gallery.copy.sample.116");
              Button(ButtonProps{}.size(ControlSize::Large), [] { gallery_label("gallery.copy.sample.117"); });
              Input(InputProps{}
                        .size(ControlSize::Large)
                        .defaultValue(gallery_initial_text("gallery.space.baseline.input", u8"Ag 输入"))
                        .onChange(gallery_value_changed<ryn::String>("gallery.space.baseline.input"))
                        .layout(LayoutStyle{}.width(dp(150))));
              gallery_target("gallery.space.baseline.input");
          }});
    remember_target(state, "gallery.space.baseline");
    for (const auto size : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
        Text(size == ControlSize::Small   ? String{u8"gallery.space.compact.small"}
             : size == ControlSize::Large ? String{u8"gallery.space.compact.large"}
                                          : String{u8"gallery.space.compact.middle"});
        SpaceCompact(
            SpaceCompactProps{}.size(size), SpaceCompactContent{[] {
                Button(ButtonProps{}.variant(ButtonVariant::Dashed), [] { gallery_label("gallery.copy.sample.118"); });
                Button(ButtonProps{}.type(ButtonType::Primary), [] { gallery_label("gallery.copy.sample.119"); });
            }});
        remember_target(state, size == ControlSize::Small   ? "gallery.space.compact.small"
                               : size == ControlSize::Large ? "gallery.space.compact.large"
                                                            : "gallery.space.compact.middle");
    }
    gallery_label("gallery.copy.sample.120");
    SpaceCompact(SpaceCompactProps{}
                     .orientation(SpaceOrientation::Vertical)
                     .direction(FlexDirection::RightToLeft)
                     .block(true)
                     .layout(LayoutStyle{}.width(state->cell_width)),
                 SpaceCompactContent{[] {
                     Button(ButtonProps{}, [] { gallery_label("gallery.copy.sample.121"); });
                     Button(ButtonProps{}.danger(true), [] { gallery_label("gallery.copy.sample.122"); });
                 }});
    remember_target(state, "gallery.space.compact.vertical-rtl");
    gallery_label("gallery.copy.sample.123");
    SpaceCompact(SpaceCompactProps{}
                     .size(ControlSize::Small)
                     .orientation(bind([narrow = state->narrow_layout] {
                         return narrow.get() ? SpaceOrientation::Vertical : SpaceOrientation::Horizontal;
                     }))
                     .block(true)
                     .layout(LayoutStyle{}.width(state->document_width)),
                 SpaceCompactContent{[] {
                     SpaceAddon(SpaceAddonProps{}, SpaceAddonContent{[] { Text(u8"https://"); }});
                     Input(InputProps{}
                               .defaultValue(gallery_initial_text("gallery.space.compact.mixed.input", u8"输入"))
                               .onChange(gallery_value_changed<ryn::String>("gallery.space.compact.mixed.input"))
                               .layout(LayoutStyle{}.width(dp(120))));
                     gallery_target("gallery.space.compact.mixed.input");
                     Password(PasswordProps{}.defaultValue(u8"秘密").layout(LayoutStyle{}.width(dp(100))));
                     Search(SearchProps{}
                                .defaultValue(gallery_initial_text("gallery.space.compact.mixed.search", u8"搜索"))
                                .onChange(gallery_value_changed<ryn::String>("gallery.space.compact.mixed.search"))
                                .layout(LayoutStyle{}.flex_grow(1).min_width(dp(0))));
                     gallery_target("gallery.space.compact.mixed.search");
                     RadioButton(RadioProps{}, RadioLabel{[] { gallery_label("gallery.copy.sample.124"); }});
                     SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[] {
                                      Button(ButtonProps{}, [] { gallery_label("gallery.copy.sample.125"); });
                                      Button(ButtonProps{}.disabled(true),
                                             [] { gallery_label("gallery.copy.sample.126"); });
                                  }});
                 }});
    remember_target(state, "gallery.space.compact.mixed");
    gallery_label("gallery.copy.sample.127");
    Space(SpaceProps{}.wrap(true).layout(LayoutStyle{}.width(state->document_width)), SpaceContent{[] {
              for (const auto variant :
                   {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
                  SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[variant] {
                                   SpaceAddon(SpaceAddonProps{}
                                                  .variant(variant)
                                                  .status(variant == InputVariant::Filled ? InputStatus::Error
                                                                                          : InputStatus::Warning)
                                                  .disabled(variant == InputVariant::Outlined),
                                              SpaceAddonContent{[] { gallery_label("gallery.copy.sample.128"); }});
                                   Button(ButtonProps{}, [] { gallery_label("gallery.copy.sample.129"); });
                               }});
              }
          }});
    remember_target(state, "gallery.space.addon.variants");
    state->telemetry.live_samples += 11;
}

void add_icon_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.130");
    ThemeConfig large;
    large.text.tokens.font_size = dp(32);
    large.text.tokens.line_height = dp(40);
    Theme(ThemeProps{}.config(large), ThemeContent{[state] {
              ++state->telemetry.theme_content_runs;
              Space(SpaceProps{}.wrap(true).size(dp(20)).layout(LayoutStyle{}.width(state->document_width)),
                    SpaceContent{[state] {
                        Icon(IconProps{}.name(IconName::HomeOutlined));
                        remember_target(state, "gallery.icon.outlined");
                        Icon(IconProps{}.name(IconName::HomeFilled));
                        remember_target(state, "gallery.icon.filled");
                        Icon(IconProps{}.name(IconName::HeartTwoTone));
                        remember_target(state, "gallery.icon.two-tone");
                        Icon(IconProps{}
                                 .name(IconName::WalletTwoTone)
                                 .twoToneColor(
                                     IconTwoToneColor{Color::rgba8(114, 46, 209), Color::rgba8(239, 219, 255)}));
                        remember_target(state, "gallery.icon.four-layers");
                        Icon(IconProps{}.source(state->icon_source).rotate(state->icon_angle));
                        remember_target(state, "gallery.icon.retained-source");
                        Icon(IconProps{}.name(IconName::LoadingOutlined).spin(state->icon_spin));
                        remember_target(state, "gallery.icon.spin");
                        Icon(IconProps{}.source(icon_vector_sample()));
                        remember_target(state, "gallery.icon.custom-vector");
                        Icon(IconProps{}.source(icon_wide_sample()));
                        remember_target(state, "gallery.icon.wide-vector");
                    }});
          }});
    Space(SpaceProps{}.wrap(true), SpaceContent{[state] {
              Button(ButtonProps{}.onClick([state] { state->icon_spin.set(!state->icon_spin.get()); }),
                     ButtonContent{[] { gallery_label("gallery.copy.sample.131"); }});
              remember_target(state, "gallery.icon.toggle-spin");
              Button(ButtonProps{}.onClick(
                         [state] { state->icon_angle.set(std::fmod(state->icon_angle.get() + 45, 360.0F)); }),
                     ButtonContent{[] { gallery_label("gallery.copy.sample.132"); }});
              remember_target(state, "gallery.icon.rotate");
              Button(ButtonProps{}.onClick([state] {
                  const auto step = ++state->icon_source_step % 3;
                  state->icon_source.set(step == 0   ? IconSource{IconName::HeartTwoTone}
                                         : step == 1 ? IconSource{IconName::WalletTwoTone}
                                                     : icon_vector_sample());
              }),
                     ButtonContent{[] { gallery_label("gallery.copy.sample.133"); }});
              remember_target(state, "gallery.icon.change-source");
          }});
    state->telemetry.live_samples += 8;
}

void add_tabs_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.134");
    const auto sample = [&](const char8_t* caption, TabsProps props, std::vector<TabItem> items) {
        const auto label = String::from_utf8(std::u8string_view{caption}).value();
        const auto key = label.bytes().substr(0, label.bytes().find(" ·"));
        if (state->register_targets && key != "gallery.tabs.controlled-retained" && key != "gallery.tabs.unknown-key") {
            const auto initial = key == "gallery.tabs.overflow" ? String{u8"1"}
                                 : items.empty()                ? String{}
                                                                : String{u8"draft"};
            const auto draft_key = std::string{key} + "/active-key";
            props.defaultActiveKey(gallery_initial_value(draft_key, initial))
                .onChange(gallery_value_changed<String>(draft_key));
        }
        Text(TextProps{}.content(gallery_field(key, "caption")));
        Tabs(props.layout(LayoutStyle{}.width(state->cell_width)), std::move(items));
        const auto identity = String::from_utf8(std::u8string_view{caption}).value();
        remember_target(state, identity.bytes().substr(0, identity.bytes().find(" ·")));
        ++state->telemetry.live_samples;
    };
    const auto basic = [] {
        return std::vector<TabItem>{
            TabItem{u8"draft", ryn::String{}, TabContent{[] { gallery_label("gallery.copy.sample.135"); }}}.label(
                gallery_text("gallery.complete.copy.032")),
            TabItem{u8"settings", ryn::String{}, TabContent{[] { gallery_label("gallery.complete.copy.164"); }}}.label(
                gallery_text("gallery.complete.copy.033"))};
    };
    sample(
        u8"gallery.tabs.controlled-retained", TabsProps{}.activeKey(state->tabs_active).onChange([state](String key) {
            state->tabs_active.set(std::move(key));
            ++state->telemetry.activations;
        }),
        {TabItem{
             u8"draft", ryn::String{}, TabContent{[] {
                 Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[] {
                          Input(InputProps{}
                                    .defaultValue(gallery_initial_text("gallery.tabs.draft", u8"切换后保留 / Draft"))
                                    .onChange(gallery_value_changed<ryn::String>("gallery.tabs.draft"))
                                    .layout(LayoutStyle{}.width(dp(140))));
                          gallery_target("gallery.tabs.draft");
                          Select(gallery_select_default(
                                     "gallery.tabs.draft-role",
                                     SelectProps{}
                                         .options(gallery_option_labels<ryn::SelectOption>(
                                             std::vector<ryn::SelectOption>{{1, u8"桌面"}, {2, u8"服务"}},
                                             {{1, "gallery.complete.copy.101"}, {2, "gallery.complete.copy.102"}}))

                                         .layout(LayoutStyle{}.width(dp(140))),
                                     ryn::SelectValue{1})
                                     .onChange(gallery_value_changed<ryn::SelectSelection>("gallery.tabs.draft-role")));
                          gallery_target("gallery.tabs.draft-role");
                      }});
             }}}
             .label(gallery_text("gallery.complete.copy.034")),
         TabItem{u8"settings", ryn::String{}, TabContent{[] {
                     Input(InputProps{}
                               .defaultValue(gallery_initial_text("gallery.tabs.settings-draft", u8"另一份草稿"))
                               .onChange(gallery_value_changed<ryn::String>("gallery.tabs.settings-draft"))
                               .layout(LayoutStyle{}.width(dp(140))));
                     gallery_target("gallery.tabs.settings-draft");
                 }}}
             .label(gallery_text("gallery.complete.copy.035")),
         TabItem{u8"locked", ryn::String{}, TabContent{[] { gallery_label("gallery.complete.copy.165"); }}}
             .label(gallery_text("gallery.complete.copy.036"))
             .disabled()});
    auto disabled = basic();
    disabled.push_back(
        TabItem{u8"locked", ryn::String{}, TabContent{[] { gallery_label("gallery.complete.copy.165"); }}}
            .label(gallery_text("gallery.complete.copy.037"))
            .disabled());
    sample(u8"gallery.tabs.disabled", TabsProps{}, std::move(disabled));
    auto dynamic = basic();
    dynamic[1].label(state->tabs_label).disabled(state->tabs_disabled);
    sample(u8"gallery.tabs.dynamic-label", TabsProps{}, std::move(dynamic));
    sample(u8"gallery.tabs.card", TabsProps{}.type(TabsType::Card), basic());
    sample(u8"gallery.tabs.small", TabsProps{}.size(ControlSize::Small), basic());
    sample(u8"gallery.tabs.large", TabsProps{}.size(ControlSize::Large), basic());
    sample(u8"gallery.tabs.bottom", TabsProps{}.placement(TabsPlacement::Bottom), basic());
    sample(u8"gallery.tabs.bottom-card", TabsProps{}.type(TabsType::Card).placement(TabsPlacement::Bottom), basic());
    sample(u8"gallery.tabs.centered", TabsProps{}.centered(), basic());
    std::vector<TabItem> many;
    for (int i = 1; i <= 8; ++i) {
        auto key = String::from_utf8(std::to_string(i)).value();
        many.push_back(TabItem{key, String{}, TabContent{[key] { Text(key); }}}.label(
            gallery_text("gallery.complete.copy.447", MessageArgs{{u8"index", static_cast<std::uint64_t>(i)}})));
    }
    sample(u8"gallery.tabs.overflow", TabsProps{}, std::move(many));
    sample(u8"gallery.tabs.empty", TabsProps{}, {});
    sample(u8"gallery.tabs.unknown-key", TabsProps{}.activeKey(u8"unknown"), basic());
    ThemeConfig custom;
    custom.tabs.algorithm = true;
    custom.tabs.seed.color_primary = Color::rgba8(114, 46, 209);
    custom.tabs.tokens.horizontal_gutter = dp(16);
    Theme(ThemeProps{}.config(custom), ThemeContent{[state, basic] {
              ++state->telemetry.theme_content_runs;
              gallery_label("gallery.copy.sample.136");
              Tabs(TabsProps{}.layout(LayoutStyle{}.width(state->cell_width)), basic());
              remember_target(state, "gallery.tabs.component-theme");
              ++state->telemetry.live_samples;
          }});
    Button(ButtonProps{}.onClick([state] {
        const bool disabled = !state->tabs_disabled.get();
        state->tabs_disabled.set(disabled);
        state->tabs_label.set(disabled ? GalleryMessage{"gallery.complete.copy.140", {}}
                                       : GalleryMessage{"gallery.complete.copy.122", {}});
        ++state->telemetry.activations;
    }),
           [] { gallery_label("gallery.copy.sample.137"); });
    remember_target(state, "gallery.tabs.toggle-label");
    ++state->telemetry.live_samples;
    gallery_label("gallery.copy.sample.138");
    auto editable_items = Signal<std::vector<TabItem>>{std::vector<TabItem>{
        TabItem{u8"draft", ryn::String{}, TabContent{[] { Input(InputProps{}.defaultValue(u8"重排后保留")); }}}
            .label(gallery_text("gallery.complete.copy.038"))
            .richLabel(TabLabel{[] { gallery_label("gallery.copy.sample.139"); }})
            .closable(false),
        TabItem{u8"details", ryn::String{}, TabContent{[] { Input(InputProps{}.defaultValue(u8"按需挂载内容")); }}}
            .label(gallery_text("gallery.complete.copy.039"))
            .destroyOnHidden(true)}};
    auto serial = std::make_shared<std::size_t>(0);
    Tabs(TabsProps{}
             .type(TabsType::EditableCard)
             .lazy()
             .layout(LayoutStyle{}.width(state->document_width))
             .extraStart(TabsExtra{[] { gallery_label("gallery.copy.sample.140"); }})
             .extraEnd(TabsExtra{[state, editable_items] {
                 Button(ButtonProps{}.onClick([editable_items] {
                     auto values = editable_items.get();
                     std::reverse(values.begin(), values.end());
                     editable_items.set(std::move(values));
                 }),
                        [] { gallery_label("gallery.copy.sample.141"); });
                 remember_target(state, "gallery.tabs.reorder");
             }})
             .onEdit([editable_items, serial, tr = useI18n()](String key, TabsEditAction action) {
                 auto values = editable_items.get();
                 if (action == TabsEditAction::Add) {
                     const auto key = utf8("new-" + std::to_string(++*serial));
                     values.push_back(TabItem{
                         key, String{}, TabContent{[] {
                             Input(InputProps{}.defaultValue(u8"新页草稿"));
                         }}}.label(tr.text(gallery_message("gallery.complete.copy.521"),
                                           MessageArgs{{u8"index", static_cast<std::uint64_t>(*serial)}})));
                 } else {
                     std::erase_if(values, [&](const auto& item) { return item.key() == key; });
                 }
                 editable_items.set(std::move(values));
             }),
         editable_items);
    remember_target(state, "gallery.tabs.editable");
    for (auto placement : {TabsPlacement::Left, TabsPlacement::Right}) {
        Text(TextProps{}.content(placement == TabsPlacement::Left ? gallery_text("gallery.complete.copy.448")
                                                                  : gallery_text("gallery.complete.copy.449")));
        Tabs(TabsProps{}
                 .placement(placement)
                 .rtl(placement == TabsPlacement::Left)
                 .indicator(TabsIndicator{20, TabsIndicatorAlign::Center})
                 .layout(LayoutStyle{}.width(state->cell_width).height(dp(200))),
             basic());
        remember_target(state, placement == TabsPlacement::Left ? "gallery.tabs.left" : "gallery.tabs.right");
    }
    state->telemetry.live_samples += 3;
}

void add_modal_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.142");
    Text(TextProps{}.content(state->modal_feedback));
    Text(TextProps{}.content(state->modal_lifecycle));
    const auto sample = [state](int index, std::string_view identity, String, ModalProps props, ModalContent content,
                                ModalSlots slots = {}) {
        const auto title = gallery_field(identity, "caption");
        Button(ButtonProps{}.onClick([state, index] {
            state->modal_active.set(index);
            ++state->telemetry.activations;
        }),
               ButtonContent{[title] { Text(TextProps{}.content(title)); }});
        remember_target(state, identity);
        props.open(Binding<bool>{[state, index] { return state->modal_active.get() == index; }})
            .title(title)
            .onOk([state, index] {
                ++state->telemetry.activations;
                state->modal_feedback.set(index == 2 ? GalleryMessage{"gallery.feedback.060", {}}
                                                     : GalleryMessage{"gallery.feedback.061", {}});
                if (index != 2) {
                    state->modal_active.set(-1);
                }
            })
            .onCancel([state, index](ModalCancelReason reason) {
                ++state->telemetry.activations;
                const auto message =
                    reason == ModalCancelReason::CancelButton  ? GalleryMessage{"gallery.complete.copy.401", {}}
                    : reason == ModalCancelReason::CloseButton ? GalleryMessage{"gallery.complete.copy.402", {}}
                    : reason == ModalCancelReason::Mask        ? GalleryMessage{"gallery.complete.copy.403", {}}
                                                               : GalleryMessage{"gallery.complete.copy.404", {}};
                state->modal_feedback.set(message);
                if (index != 2) {
                    state->modal_nested.set(false);
                    state->modal_active.set(-1);
                }
            })
            .afterOpenChange([state](bool shown) {
                state->modal_lifecycle.set(shown ? GalleryMessage{"gallery.feedback.062", {}}
                                                 : GalleryMessage{"gallery.feedback.063", {}});
            })
            .afterClose([state] { state->modal_lifecycle.set(GalleryMessage{"gallery.feedback.064", {}}); });
        Modal(std::move(props), std::move(content), std::move(slots));
        remember_target(state, std::string{identity} + ".dialog");
        state->telemetry.live_samples += 2; // Dialog and its page trigger.
    };
    sample(0, "gallery.modal.basic", String{}, ModalProps{}, ModalContent{[] {
               gallery_label("gallery.copy.sample.143");
               gallery_label("gallery.copy.sample.144");
           }});
    sample(
        1, "gallery.modal.retained-edit", String{}, ModalProps{}, ModalContent{[] {
            gallery_label("gallery.copy.sample.145");
            Input(InputProps{}
                      .defaultValue(gallery_initial_text("gallery.modal.account", u8"RynUI account"))
                      .onChange(gallery_value_changed<ryn::String>("gallery.modal.account")));
            gallery_target("gallery.modal.account");
            TextArea(
                TextAreaProps{}
                    .rows(3)
                    .defaultValue(gallery_initial_text("gallery.modal.notes", u8"保留的多行备注\n中文 / English 🙂"))
                    .onChange(gallery_value_changed<ryn::String>("gallery.modal.notes")));
            gallery_target("gallery.modal.notes");
            Select(
                gallery_select_default(
                    "gallery.modal.account-role",
                    SelectProps{}
                        .options(gallery_option_labels<ryn::SelectOption>(
                            std::vector<ryn::SelectOption>{{u8"desktop", u8"桌面"}, {u8"service", u8"服务"}},
                            {{u8"desktop", "gallery.complete.copy.103"}, {u8"service", "gallery.complete.copy.104"}}))

                        .showSearch()
                        .allowClear(),
                    ryn::SelectValue{u8"desktop"})
                    .onChange(gallery_value_changed<ryn::SelectSelection>("gallery.modal.account-role")));
            gallery_target("gallery.modal.account-role");
            Tooltip(TooltipProps{}.title(gallery_text("gallery.copy.property.023")).trigger(TooltipTriggerMode::Click),
                    TooltipTrigger{[] { Button(ButtonProps{}, [] { gallery_label("gallery.copy.sample.146"); }); }});
        }});
    sample(2, "gallery.modal.controlled-refusal", String{}, ModalProps{}, ModalContent{[state] {
               gallery_label("gallery.copy.sample.147");
               Text(TextProps{}.content(state->modal_feedback));
               Button(ButtonProps{}.onClick([state] { state->modal_active.set(-1); }),
                      [] { gallery_label("gallery.copy.sample.148"); });
           }});
    sample(3, "gallery.modal.confirm-loading", String{}, ModalProps{}.confirmLoading(state->modal_loading),
           ModalContent{[state] {
               Flex(
                   FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state] {
                       gallery_label("gallery.copy.sample.149");
                       Button(
                           ButtonProps{}
                               .layout(
                                   LayoutStyle{}.margin_left(dp(4)).margin_top(dp(4)).margin_right(dp(4)).margin_bottom(
                                       dp(4)))
                               .onClick([state] { state->modal_loading.set(!state->modal_loading.get()); }),
                           [] { gallery_label("gallery.copy.sample.150"); });
                   }});
           }});
    sample(4, "gallery.modal.custom-footer", String{}, ModalProps{}.footer(ModalFooterMode::None),
           ModalContent{[] { gallery_label("gallery.copy.sample.151"); }},
           ModalSlots{.title = ModalTitle{[] {
                          Flex(FlexProps{}.gap(dp(8)).align(FlexAlign::Center), FlexContent{[] {
                                   Icon(IconProps{}.name(IconName::EditOutlined));
                                   gallery_label("gallery.copy.sample.152");
                               }});
                      }},
                      .footer = ModalFooter{[state] {
                          Flex(FlexProps{}.gap(dp(8)).justify(FlexJustify::End), FlexContent{[state] {
                                   Button(ButtonProps{}.onClick([state] { state->modal_active.set(-1); }),
                                          [] { gallery_label("gallery.copy.sample.153"); });
                                   Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                                       state->modal_feedback.set(GalleryMessage{"gallery.feedback.065", {}});
                                       state->modal_active.set(-1);
                                   }),
                                          [] { gallery_label("gallery.copy.sample.154"); });
                               }});
                      }}});
    sample(5, "gallery.modal.no-footer", String{}, ModalProps{}.footer(ModalFooterMode::None),
           ModalContent{[] { gallery_label("gallery.copy.sample.155"); }});
    sample(6, "gallery.modal.maskless", String{}, ModalProps{}.mask(ModalMask{false, true}),
           ModalContent{[] { gallery_label("gallery.copy.sample.156"); }});
    sample(7, "gallery.modal.centered", String{}, ModalProps{}.centered(),
           ModalContent{[] { gallery_label("gallery.copy.sample.157"); }});
    sample(
        8, "gallery.modal.long-body", String{}, ModalProps{}, ModalContent{[state] {
            Button(ButtonProps{}.onClick([state] { state->modal_active.set(-1); }),
                   [] { gallery_label("gallery.copy.sample.158"); });
            gallery_label("gallery.copy.sample.159");
            for (int row = 1; row <= 28; ++row) {
                Text(TextProps{}.content(gallery_text("gallery.complete.copy.397",
                                                      MessageArgs{{u8"index", static_cast<std::uint64_t>(row)}})));
            }
            Input(InputProps{}
                      .defaultValue(gallery_initial_text("gallery.modal.end-draft", u8"末尾保留的输入"))
                      .onChange(gallery_value_changed<ryn::String>("gallery.modal.end-draft")));
            gallery_target("gallery.modal.end-draft");
            Select(SelectProps{}.options(gallery_option_labels<ryn::SelectOption>(
                std::vector<ryn::SelectOption>{{1, u8"末尾选项一"}, {2, u8"末尾选项二"}},
                {{1, "gallery.complete.copy.105"}, {2, "gallery.complete.copy.106"}})));
            Tabs(
                TabsProps{},
                {TabItem{u8"first", ryn::String{}, TabContent{[] { gallery_label("gallery.copy.sample.160"); }}}.label(
                     gallery_text("gallery.complete.copy.040")),
                 TabItem{u8"second", ryn::String{}, TabContent{[] { gallery_label("gallery.copy.sample.161"); }}}.label(
                     gallery_text("gallery.complete.copy.041"))});
        }});
    sample(9, "gallery.modal.nested", String{}, ModalProps{}, ModalContent{[state] {
               Input(InputProps{}
                         .defaultValue(gallery_initial_text("gallery.modal.parent-draft", u8"父层保留的草稿"))
                         .onChange(gallery_value_changed<ryn::String>("gallery.modal.parent-draft")));
               gallery_target("gallery.modal.parent-draft");
               Button(ButtonProps{}.onClick([state] { state->modal_nested.set(true); }),
                      [] { gallery_label("gallery.copy.sample.162"); });
               Modal(ModalProps{}
                         .open(state->modal_nested)
                         .title(gallery_text("gallery.copy.property.024"))
                         .onOk([state] { state->modal_nested.set(false); })
                         .onCancel([state](ModalCancelReason) { state->modal_nested.set(false); }),
                     ModalContent{[] {
                         gallery_label("gallery.copy.sample.163");
                         Input(InputProps{}
                                   .defaultValue(gallery_initial_text("gallery.modal.child-draft", u8"子层保留的草稿"))
                                   .onChange(gallery_value_changed<ryn::String>("gallery.modal.child-draft")));
                         gallery_target("gallery.modal.child-draft");
                     }});
               ++state->telemetry.live_samples;
           }});
    ThemeConfig custom;
    custom.modal.algorithm = true;
    custom.modal.seed.color_primary = Color::rgba8(114, 46, 209);
    custom.modal.tokens.border_radius = dp(12);
    custom.button.algorithm = true;
    custom.button.seed.color_primary = Color::rgba8(114, 46, 209);
    Theme(ThemeProps{}.config(custom), ThemeContent{[state, sample] {
              ++state->telemetry.theme_content_runs;
              sample(10, "gallery.modal.component-theme", String{}, ModalProps{},
                     ModalContent{[] { gallery_label("gallery.copy.sample.164"); }});
          }});
}

void add_avatar_image_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    const std::array<std::byte, 16> rgba{std::byte{255}, std::byte{60},  std::byte{30},  std::byte{255},
                                         std::byte{30},  std::byte{200}, std::byte{80},  std::byte{255},
                                         std::byte{40},  std::byte{90},  std::byte{255}, std::byte{255},
                                         std::byte{255}, std::byte{220}, std::byte{40},  std::byte{255}};
    const auto pixels = ImagePixels::copy(2, 2, 8, rgba);

    struct Request {
        ImageCompletion complete;
    };

    const auto request = std::make_shared<Request>();
    const auto loader = std::make_shared<ImageLoader>();
    loader->start = [request](ImageCompletion complete) {
        request->complete = std::move(complete);
        return [request] {
            request->complete = {};
        };
    };
    Signal<ImageSource> source{ImageSource{loader}};
    Signal<AvatarShape> image_shape{AvatarShape::Circle};
    GalleryFeedback feedback{GalleryMessage{"gallery.feedback.034", {}}, useI18n()};
    Signal<std::vector<AvatarGroupItem>> members{std::vector{AvatarGroupItem{u8"a", u8"A"}.source(ImageSource{pixels}),
                                                             AvatarGroupItem{u8"b", u8"B"},
                                                             AvatarGroupItem{u8"c", u8"C"}}};
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[=] {
             gallery_label("gallery.copy.sample.165");
             Text(TextProps{}.content(feedback));
             Avatar(AvatarProps{}
                        .source(source)
                        .text(gallery_text("gallery.complete.copy.011"))
                        .shape(image_shape)
                        .size(Responsive<float>{32}.at(Breakpoint::Md, 56).at(Breakpoint::Lg, 80))
                        .onLoad([feedback] { feedback.set(GalleryMessage{"gallery.feedback.066", {}}); })
                        .onError([feedback](const ImageError& error) {
                            feedback.set(GalleryMessage{"gallery.event.000", {{u8"v0", utf8(error.message)}}});
                            return true;
                        }));
             Button(ButtonProps{}.onClick([=] {
                 const auto complete = request->complete;
                 if (complete) {
                     complete({pixels, {}});
                 }
             }),
                    [] { gallery_label("gallery.copy.sample.166"); });
             remember_target(state, "gallery.avatar.image-success");
             Button(ButtonProps{}.onClick([=] {
                 image_shape.set(image_shape.get() == AvatarShape::Circle ? AvatarShape::Square : AvatarShape::Circle);
             }),
                    [] { gallery_label("gallery.copy.sample.167"); });
             remember_target(state, "gallery.avatar.image-shape");
             Button(ButtonProps{}.onClick([=] {
                 const auto complete = request->complete;
                 if (complete) {
                     complete({{}, {ImageErrorCode::loader_failure, "显式模拟失败"}});
                 }
             }),
                    [] { gallery_label("gallery.copy.sample.168"); });
             remember_target(state, "gallery.avatar.image-error");
             Button(ButtonProps{}.onClick([=] {
                 source.set(ImageSource{});
                 source.set(ImageSource{loader});
                 feedback.set(GalleryMessage{"gallery.feedback.067", {}});
             }),
                    [] { gallery_label("gallery.copy.sample.169"); });
             remember_target(state, "gallery.avatar.image-retry");
             AvatarGroup(AvatarGroupProps{}
                             .items(members)
                             .size(Responsive<float>{24}.at(Breakpoint::Md, 40))
                             .max(AvatarGroupMax{1, PopoverTriggerMode::Click, PopoverPlacement::Bottom}));
             Button(ButtonProps{}.onClick([=] {
                 auto next = members.get();
                 std::rotate(next.begin(), next.begin() + 1, next.end());
                 members.set(next);
                 feedback.set(GalleryMessage{"gallery.feedback.068", {}});
             }),
                    [] { gallery_label("gallery.copy.sample.170"); });
             remember_target(state, "gallery.avatar.keyed-reorder");
         }});
    ++state->telemetry.live_samples;
}

void add_badge_native_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<int> count{8};
    Signal<bool> processing{true};
    Signal<BadgeCountValue> arbitrary{BadgeCountValue{String{u8"NEW"}}};
    GalleryFeedback feedback{GalleryMessage{"gallery.feedback.035", {}}, useI18n()};
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[=] {
             gallery_label("gallery.copy.sample.171");
             Text(TextProps{}.content(feedback));
             Badge(BadgeProps{}.count(count).showZero(), BadgeContent{[=] {
                       Button(ButtonProps{}.onClick([=] { feedback.set(GalleryMessage{"gallery.feedback.069", {}}); }),
                              [] { gallery_label("gallery.copy.sample.172"); });
                   }});
             Button(ButtonProps{}.onClick([=] {
                 count.set(count.get() + 1);
                 feedback.set(GalleryMessage{"gallery.event.001", {{u8"v0", utf8(std::to_string(count.get()))}}});
             }),
                    [] { gallery_label("gallery.copy.sample.173"); });
             remember_target(state, "gallery.badge.native-increment");
             Button(ButtonProps{}.onClick([=] { count.set(std::max(0, count.get() - 3)); }),
                    [] { gallery_label("gallery.copy.sample.174"); });
             remember_target(state, "gallery.badge.native-decrement");
             Badge(BadgeProps{}
                       .status(Status::Processing)
                       .visible(processing)
                       .text(gallery_text("gallery.complete.copy.012")));
             Button(ButtonProps{}.onClick([=] { processing.set(!processing.get()); }),
                    [] { gallery_label("gallery.copy.sample.175"); });
             remember_target(state, "gallery.badge.processing-toggle");
             Badge(BadgeProps{}.countValue(arbitrary));
             const auto rich_count = std::make_shared<BadgeRenderer>();
             rich_count->render = [] {
                 Icon(IconProps{}.name(IconName::ClockCircleOutlined));
             };
             Badge(BadgeProps{}.countValue(BadgeCountValue{std::shared_ptr<const BadgeRenderer>{rich_count}}));
             Input(InputProps{}.defaultValue(u8"NEW").onChange(
                 [=](String value) { arbitrary.set(BadgeCountValue{value}); }));
             remember_target(state, "gallery.badge.arbitrary-input");
             BadgeRibbon(RibbonProps{}.text(gallery_text("gallery.complete.copy.013")), BadgeContent{[=] {
                             Button(ButtonProps{}.layout(LayoutStyle{}.width(dp(220)).height(dp(70))).onClick([=] {
                                 feedback.set(GalleryMessage{"gallery.feedback.070", {}});
                             }),
                                    [] { gallery_label("gallery.copy.sample.176"); });
                         }});
         }});
    ++state->telemetry.live_samples;
}

void add_tag_native_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<bool> allow_close{false};
    Signal<bool> allow_reorder{false};
    Signal<bool> checked{false};
    GalleryFeedback feedback{GalleryMessage{"gallery.feedback.036", {}}, useI18n()};
    const auto rich = std::make_shared<TagItemRenderer>();
    rich->render = [] {
        Input(InputProps{}.defaultValue(u8"重排保留的草稿").layout(LayoutStyle{}.width(dp(160))));
    };
    Signal<std::vector<TagGroupItem>> items{
        std::vector{TagGroupItem{u8"draft", u8"草稿", TagColor{PresetColor::Blue}, false, false, rich},
                    TagGroupItem{u8"b", u8"Beta"}, TagGroupItem{u8"c", u8"Gamma"}}};
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[=] {
             gallery_label("gallery.copy.sample.177");
             Text(TextProps{}.content(feedback));
             Tag(TagProps{}.closable().onClose(std::function<void(TagCloseEvent&)>{[=](TagCloseEvent& event) {
                 if (!allow_close.get()) {
                     event.preventDefault();
                     feedback.set(GalleryMessage{"gallery.feedback.071", {}});
                 } else {
                     feedback.set(GalleryMessage{"gallery.feedback.072", {}});
                 }
             }}),
                 TagContent{[] { gallery_label("gallery.copy.sample.178"); }});
             Button(ButtonProps{}.onClick([=] {
                 allow_close.set(true);
                 feedback.set(GalleryMessage{"gallery.feedback.073", {}});
             }),
                    [] { gallery_label("gallery.copy.sample.179"); });
             remember_target(state, "gallery.tag.allow-close");
             CheckableTag(CheckableTagProps{}.checked(checked).onChange([=](bool next) {
                 checked.set(next);
                 feedback.set(next ? GalleryMessage{"gallery.feedback.074", {}}
                                   : GalleryMessage{"gallery.feedback.075", {}});
             }),
                          TagContent{[] { gallery_label("gallery.copy.sample.180"); }});
             CheckableTagGroup(TagGroupProps{}
                                   .items(gallery_record_labels<TagGroupItem>(
                                       std::vector{TagGroupItem{u8"one", String{}}, TagGroupItem{u8"two", String{}}},
                                       {{u8"draft", "gallery.complete.copy.422"},
                                        {u8"one", "gallery.complete.copy.423"},
                                        {u8"two", "gallery.complete.copy.424"}},
                                       &TagGroupItem::text))
                                   .defaultValue({String{u8"one"}})
                                   .onChange([=](TagValues value) {
                                       feedback.set(GalleryMessage{
                                           "gallery.event.002", {{u8"v0", static_cast<std::uint64_t>(value.size())}}});
                                   }));
             TagGroup(TagGroupProps{}
                          .items(gallery_record_labels<TagGroupItem>(items,
                                                                     {{u8"draft", "gallery.complete.copy.422"},
                                                                      {u8"one", "gallery.complete.copy.423"},
                                                                      {u8"two", "gallery.complete.copy.424"}},
                                                                     &TagGroupItem::text))
                          .draggable()
                          .onReorder([=](TagValues order) {
                              if (!allow_reorder.get()) {
                                  feedback.set(GalleryMessage{"gallery.feedback.076", {}});
                                  return;
                              }
                              std::vector<TagGroupItem> next;
                              for (const auto& key : order) {
                                  for (const auto& row : items.get()) {
                                      if (row.key == key) {
                                          next.push_back(row);
                                          break;
                                      }
                                  }
                              }
                              items.set(next);
                              feedback.set(GalleryMessage{"gallery.feedback.077", {}});
                          }));
             Button(ButtonProps{}.onClick([=] {
                 allow_reorder.set(!allow_reorder.get());
                 feedback.set(allow_reorder.get() ? GalleryMessage{"gallery.feedback.078", {}}
                                                  : GalleryMessage{"gallery.feedback.079", {}});
             }),
                    [] { gallery_label("gallery.copy.sample.181"); });
             remember_target(state, "gallery.tag.allow-reorder");
         }});
    ++state->telemetry.live_samples;
}

void add_tag_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.182");
    Text(TextProps{}.content(state->common_feedback));
    Space(SpaceProps{}.wrap(true).size(dp(12)), SpaceContent{[state] {
              Tag(TagProps{}, TagContent{[] { gallery_label("gallery.copy.sample.183"); }});
              Tag(TagProps{}.color(Status::Success), TagContent{[] { gallery_label("gallery.copy.sample.184"); }});
              Tag(TagProps{}.color(PresetColor::Blue).closable().visible(state->tag_visible).onClose([state] {
                  state->tag_visible.set(false);
              }),
                  TagContent{[] { gallery_label("gallery.copy.sample.185"); }});
              CheckableTag(CheckableTagProps{}.checked(state->tag_checked).onChange([state](bool checked) {
                  state->tag_checked.set(checked);
              }),
                           TagContent{[] { gallery_label("gallery.copy.sample.186"); }});
              Button(ButtonProps{}.onClick([state] { state->tag_visible.set(true); }),
                     [] { gallery_label("gallery.copy.sample.187"); });
              gallery_target("gallery.tag.restore");
          }});
    state->telemetry.live_samples += 4;
}

void add_badge_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    if (state->register_targets) {
        Text(TextProps{}.content(state->common_feedback));
    }
    Space(SpaceProps{}.wrap(true).size(dp(20)), SpaceContent{[state] {
              Badge(BadgeProps{}.count(state->badge_count), BadgeContent{[state] {
                        Button(ButtonProps{}.onClick([state] { state->badge_count.set(0); }),
                               [] { gallery_label("gallery.copy.sample.188"); });
                        gallery_target("gallery.badge.clear");
                    }});
              Badge(BadgeProps{}.count(120));
              Badge(BadgeProps{}.count(0).showZero());
              Badge(BadgeProps{}.dot(),
                    BadgeContent{[] { Button(ButtonProps{}, [] { gallery_label("gallery.copy.sample.189"); }); }});
              Badge(BadgeProps{}.status(Status::Success).text(gallery_text("gallery.complete.copy.014")));
              Button(ButtonProps{}.onClick([state] { state->badge_count.set(state->badge_count.get() + 1); }),
                     [] { gallery_label("gallery.copy.sample.190"); });
              gallery_target("gallery.badge.increment");
          }});
    state->telemetry.live_samples += 5;
}

void add_drawer_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    if (state->register_targets) {
        Text(TextProps{}.content(state->common_feedback));
    }
    gallery_label("gallery.copy.sample.191");
    Space(SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
              for (auto placement :
                   {DrawerPlacement::Right, DrawerPlacement::Left, DrawerPlacement::Top, DrawerPlacement::Bottom}) {
                  const std::string_view label = placement == DrawerPlacement::Right  ? "gallery.drawer.right"
                                                 : placement == DrawerPlacement::Left ? "gallery.drawer.left"
                                                 : placement == DrawerPlacement::Top  ? "gallery.drawer.top"
                                                                                      : "gallery.drawer.bottom";
                  Button(ButtonProps{}.onClick([state, placement] {
                      state->drawer_placement.set(placement);
                      state->drawer_open.set(true);
                  }),
                         [label] { Text(TextProps{}.content(gallery_text(label))); });
                  remember_target(state, label);
              }
          }});
    Button(ButtonProps{}.onClick([state] { state->drawer_destroy.set(!state->drawer_destroy.get()); }),
           [] { gallery_label("gallery.copy.sample.192"); });
    gallery_target("gallery.drawer.destroy-policy");
    Drawer(DrawerProps{}
               .open(state->drawer_open)
               .placement(state->drawer_placement)
               .width(state->drawer_width)
               .height(state->drawer_height)
               .loading(state->drawer_loading)
               .lazy()
               .destroyOnHidden(state->drawer_destroy)
               .rtl(state->drawer_rtl)
               .resizable(DrawerResize{})
               .onResize([state](float size) {
                   const auto edge = state->drawer_placement.get();
                   if (edge == DrawerPlacement::Left || edge == DrawerPlacement::Right) {
                       state->drawer_width.set(size);
                   } else {
                       state->drawer_height.set(size);
                   }
               })
               .title(gallery_text("gallery.copy.property.025"))
               .onCancel([state](DrawerCancelReason) { state->drawer_open.set(false); }),
           DrawerContent{[state] {
               GalleryDraftBuild draft_build{state};
               Input(InputProps{}
                         .defaultValue(gallery_initial_text("gallery.drawer.draft", u8"保留的抽屉草稿"))
                         .onChange(gallery_value_changed<ryn::String>("gallery.drawer.draft")));
               remember_target(state, "gallery.drawer.draft");
               TextArea(TextAreaProps{}
                            .defaultValue(gallery_initial_text("gallery.drawer.notes",
                                                               u8"抽屉中的多行备注\n关闭后重新打开仍然保留"))
                            .onChange(gallery_value_changed<ryn::String>("gallery.drawer.notes")));
               gallery_target("gallery.drawer.notes");
               Select(gallery_select_default("gallery.drawer.role",
                                             SelectProps{}.options(gallery_option_labels<ryn::SelectOption>(
                                                 std::vector<ryn::SelectOption>{{1, u8"桌面"}, {2, u8"服务"}},
                                                 {{1, "gallery.complete.copy.107"}, {2, "gallery.complete.copy.108"}})),
                                             ryn::SelectValue{1})
                          .onChange(gallery_value_changed<ryn::SelectSelection>("gallery.drawer.role")));
               gallery_target("gallery.drawer.role");
               Button(ButtonProps{}.onClick([state] { state->drawer_child_open.set(true); }),
                      [] { gallery_label("gallery.copy.sample.193"); });
               gallery_target("gallery.drawer.nested");
               Drawer(DrawerProps{}
                          .open(state->drawer_child_open)
                          .title(gallery_text("gallery.copy.property.026"))
                          .width(280)
                          .push(DrawerPush{true, 80})
                          .onCancel([state](DrawerCancelReason) { state->drawer_child_open.set(false); }),
                      DrawerContent{[] { Input(InputProps{}.defaultValue(u8"嵌套草稿")); }});
               Popconfirm(PopconfirmProps{}.title(gallery_text("gallery.copy.property.027")).onConfirm([state] {
                   state->common_feedback.set(GalleryMessage{"gallery.feedback.080", {}});
               }),
                          PopconfirmTrigger{[] {
                              Button(ButtonProps{}, [] { gallery_label("gallery.copy.sample.194"); });
                              gallery_target("gallery.drawer.confirm");
                          }});
               for (int row = 1; row <= 20; ++row) {
                   Text(TextProps{}.content(gallery_text("gallery.complete.copy.398",
                                                         MessageArgs{{u8"index", static_cast<std::uint64_t>(row)}})));
               }
               Input(InputProps{}
                         .defaultValue(gallery_initial_text("gallery.drawer.end", u8"末尾编辑"))
                         .onChange(gallery_value_changed<ryn::String>("gallery.drawer.end")));
               gallery_target("gallery.drawer.end");
           }},
           {.footer = DrawerFooter{[state] {
                GalleryDraftBuild draft_build{state};
                Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] { state->drawer_open.set(false); }),
                       [] { gallery_label("gallery.copy.sample.195"); });
                gallery_target("gallery.drawer.done");
            }},
            .extra = DrawerExtra{[state] {
                GalleryDraftBuild draft_build{state};
                Space(SpaceProps{}.size(dp(8)), SpaceContent{[state] {
                          Button(ButtonProps{}.onClick(
                                     [state] { state->drawer_loading.set(!state->drawer_loading.get()); }),
                                 [] { gallery_label("gallery.copy.sample.196"); });
                          gallery_target("gallery.drawer.loading");
                          Button(ButtonProps{}.onClick([state] { state->drawer_rtl.set(!state->drawer_rtl.get()); }),
                                 [] { Text(u8"RTL"); });
                          gallery_target("gallery.drawer.rtl");
                      }});
            }}});
    state->telemetry.live_samples += 1;
}

void add_popconfirm_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    if (state->register_targets) {
        Text(TextProps{}.content(state->common_feedback));
    }
    gallery_label("gallery.copy.sample.197");
    Popconfirm(PopconfirmProps{}
                   .open(state->popconfirm_open)
                   .confirmLoading(state->popconfirm_loading)
                   .title(gallery_text("gallery.copy.property.028"))
                   .onOpenChange([state](bool open) {
                       state->popconfirm_open.set(open);
                       if (!open) {
                           state->popconfirm_loading.set(false);
                       }
                   })
                   .onConfirm([state] { state->popconfirm_loading.set(true); })
                   .onCancel([state] { state->common_feedback.set(GalleryMessage{"gallery.feedback.081", {}}); }),
               PopconfirmTrigger{[] {
                   Button(ButtonProps{}, [] { gallery_label("gallery.copy.sample.198"); });
                   gallery_target("gallery.popconfirm.controlled");
               }},
               {.description = PopconfirmDescription{[state] {
                    Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state] {
                             gallery_label("gallery.copy.sample.199");
                             Button(ButtonProps{}.onClick([state] {
                                 state->popconfirm_loading.set(false);
                                 state->popconfirm_open.set(false);
                                 state->common_feedback.set(GalleryMessage{"gallery.feedback.082", {}});
                             }),
                                    [] { gallery_label("gallery.copy.sample.200"); });
                             gallery_target("gallery.popconfirm.finish");
                         }});
                }}});
    Popconfirm(PopconfirmProps{}.title(gallery_text("gallery.copy.property.029")).showCancel(false).onConfirm([state] {
        state->badge_count.set(state->badge_count.get() + 1);
    }),
               PopconfirmTrigger{[] {
                   Button(ButtonProps{}, [] { gallery_label("gallery.copy.sample.201"); });
                   gallery_target("gallery.popconfirm.single");
               }});
    state->telemetry.live_samples += 2;
}

void add_alert_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    if (state->register_targets) {
        Text(TextProps{}.content(state->common_feedback));
    }
    gallery_label("gallery.copy.sample.202");
    Alert(AlertProps{}
              .status(Status::Warning)
              .message(gallery_text("gallery.copy.property.030"))
              .description(gallery_text("gallery.copy.property.031"))
              .showIcon()
              .visible(state->alert_visible)
              .closable()
              .motion()
              .onClose([state] { state->alert_visible.set(false); }),
          {.action = AlertAction{[state] {
               Button(ButtonProps{}.onClick(
                          [state] { state->common_feedback.set(GalleryMessage{"gallery.feedback.083", {}}); }),
                      [] { gallery_label("gallery.copy.sample.203"); });
               gallery_target("gallery.alert.retry");
           }}});
    Alert(AlertProps{}.status(Status::Success).message(gallery_text("gallery.copy.property.032")).showIcon());
    Alert(AlertProps{}.status(Status::Error).message(gallery_text("gallery.copy.property.033")).showIcon());
    Alert(AlertProps{}
              .banner()
              .marquee(AlertMarquee{})
              .message(gallery_text("gallery.copy.property.034"))
              .layout(LayoutStyle{}.width(state->document_width)));
    Button(ButtonProps{}.onClick([state] { state->alert_visible.set(true); }),
           [] { gallery_label("gallery.copy.sample.204"); });
    gallery_target("gallery.alert.restore");
    state->telemetry.live_samples += 4;
}

void add_spin_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<bool> automatic{false};
    Signal<bool> delayed{false};
    Signal<bool> fullscreen{false};
    if (state->register_targets) {
        Text(TextProps{}.content(state->common_feedback));
    }
    Button(ButtonProps{}.onClick([state] { state->feedback_loading.set(!state->feedback_loading.get()); }),
           [] { gallery_label("gallery.copy.sample.205"); });
    gallery_target("gallery.spin.toggle");
    Button(ButtonProps{}.onClick([automatic]() mutable { automatic.set(!automatic.get()); }),
           [] { gallery_label("gallery.copy.sample.206"); });
    gallery_target("gallery.spin.percent");
    Button(ButtonProps{}.onClick([delayed]() mutable { delayed.set(!delayed.get()); }),
           [] { gallery_label("gallery.copy.sample.207"); });
    gallery_target("gallery.spin.delay");
    Button(ButtonProps{}.onClick([fullscreen]() mutable { fullscreen.set(true); }),
           [] { gallery_label("gallery.copy.sample.208"); });
    gallery_target("gallery.spin.fullscreen");
    Spin(SpinProps{}
             .spinning(state->feedback_loading)
             .description(gallery_text("gallery.copy.property.035"))
             .percent(
                 bind([automatic] { return automatic.get() ? SpinPercent{SpinAutomaticPercent{}} : SpinPercent{}; }))
             .delay(bind([delayed] { return std::chrono::milliseconds{delayed.get() ? 500 : 0}; })),
         SpinContent{[state] {
             // Keep a clear loading region between the retained editor and action.
             Flex(FlexProps{}.vertical(true).gap(dp(64)), FlexContent{[state] {
                      Input(InputProps{}
                                .defaultValue(gallery_initial_text("gallery.spin.draft", u8"加载前后保留内容"))
                                .onChange(gallery_value_changed<ryn::String>("gallery.spin.draft")));
                      remember_target(state, "gallery.spin.draft");
                      Button(ButtonProps{}.onClick(
                                 [state] { state->common_feedback.set(GalleryMessage{"gallery.feedback.084", {}}); }),
                             [] { gallery_label("gallery.copy.sample.209"); });
                      gallery_target("gallery.spin.action");
                  }});
         }});
    Spin(SpinProps{}.spinning(fullscreen).fullscreen(true),
         SpinSlots{.indicator = SpinIndicator{[fullscreen] {
                       Flex(FlexProps{}.vertical(true).gap(dp(16)), FlexContent{[fullscreen] {
                                Icon(IconProps{}.name(IconName::LoadingOutlined).spin(true));
                                Button(ButtonProps{}.onClick([fullscreen]() mutable { fullscreen.set(false); }),
                                       [] { gallery_label("gallery.copy.sample.210"); });
                                gallery_target("gallery.spin.dismiss");
                            }});
                   }}});
    state->telemetry.live_samples += 1;
}

void add_progress_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    if (state->register_targets) {
        Text(TextProps{}.content(state->common_feedback));
    }
    Button(ButtonProps{}.onClick([state] {
        state->feedback_percent.set(state->feedback_percent.get() >= 100 ? 0 : state->feedback_percent.get() + 25);
    }),
           [] { gallery_label("gallery.copy.sample.211"); });
    gallery_target("gallery.progress.advance");
    Button(ButtonProps{}.onClick([state] {
        state->feedback_status.set(state->feedback_status.get() == ProgressStatus::Exception
                                       ? ProgressStatus::Normal
                                       : ProgressStatus::Exception);
    }),
           [] { gallery_label("gallery.copy.sample.212"); });
    gallery_target("gallery.progress.status");
    Progress(ProgressProps{}.percent(state->feedback_percent).status(state->feedback_status));
    Space(SpaceProps{}.size(dp(24)), SpaceContent{[state] {
              Progress(ProgressProps{}
                           .type(ProgressType::Circle)
                           .percent(state->feedback_percent)
                           .status(state->feedback_status));
              Progress(ProgressProps{}
                           .type(ProgressType::Dashboard)
                           .percent(state->feedback_percent)
                           .status(state->feedback_status));
          }});
    state->telemetry.live_samples += 3;
}

void add_common_component_samples(const std::shared_ptr<GalleryState>& state) {
    const auto before = state->telemetry.live_samples;
    add_tag_samples(state);
    add_badge_samples(state);
    add_drawer_samples(state);
    add_popconfirm_samples(state);
    add_alert_samples(state);
    add_spin_samples(state);
    add_progress_samples(state);
    state->telemetry.live_samples = before + 20; // Historical combined fixture counter.
}

void add_table_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.213");
    Text(TextProps{}.content(state->data_feedback));
    Text(TextProps{}.content(bind([state] {
        return state->i18n.translate(
            gallery_message("gallery.snapshot.000"),
            MessageArgs{{u8"v0", utf8(std::to_string(state->table_page.get()))},
                        {u8"v1", static_cast<std::uint64_t>(state->table_selected.get().size())}});
    })));
    Space(SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
              Button(ButtonProps{}.onClick([state] {
                  auto rows = state->records.get();
                  if (!rows.empty()) {
                      rows.front().name = String{u8"已更新任务"};
                      state->records.set(std::move(rows));
                  }
              }),
                     [] { gallery_label("gallery.copy.sample.214"); });
              gallery_target("gallery.table.update");
              Button(ButtonProps{}.onClick([state] {
                  auto rows = state->records.get();
                  std::ranges::reverse(rows);
                  state->records.set(std::move(rows));
              }),
                     [] { gallery_label("gallery.copy.sample.215"); });
              gallery_target("gallery.table.reverse");
              Button(ButtonProps{}.onClick([state] {
                  auto rows = state->records.get();
                  if (!rows.empty()) {
                      const auto removed = rows.back().key;
                      rows.pop_back();
                      auto selected = state->table_selected.get();
                      std::erase(selected, removed);
                      state->table_selected.set(std::move(selected));
                      state->records.set(std::move(rows));
                      state->table_page.set(1);
                  }
              }),
                     [] { gallery_label("gallery.copy.sample.216"); });
              gallery_target("gallery.table.remove");
              Button(ButtonProps{}.onClick([state] {
                  state->records.set(gallery_records());
                  state->table_page.set(1);
                  state->table_sort.set(TableSort{});
                  state->table_selected.set({});
              }),
                     [] { gallery_label("gallery.copy.sample.217"); });
              gallery_target("gallery.table.restore");
              Button(ButtonProps{}.onClick([state] { state->table_loading.set(!state->table_loading.get()); }),
                     [] { gallery_label("gallery.copy.sample.218"); });
              gallery_target("gallery.table.loading");
          }});
    Table(TableProps<GalleryRecord>{}
              .dataSource(state->records)
              .rowKey([](const GalleryRecord& row) { return row.key; })
              .columns(
                  {TableColumn<GalleryRecord>{u8"name", ryn::String{},
                                              [](TableCell<GalleryRecord> cell) {
                                                  Text(TextProps{}.content(bind([cell] { return cell.value().name; })));
                                              }}
                       .title(gallery_text("gallery.complete.copy.042"))
                       .width(220),
                   TableColumn<GalleryRecord>{u8"quantity", ryn::String{},
                                              [state](TableCell<GalleryRecord> cell) {
                                                  InputNumber(
                                                      InputNumberProps{}
                                                          .value(bind([cell] { return cell.value().quantity; }))
                                                          .min(0)
                                                          .max(1000)
                                                          .precision(0)
                                                          .disabled(bind([cell] { return cell.value().locked; }))
                                                          .onChange([state, key = cell.key()](InputNumberValue value) {
                                                              auto rows = state->records.get();
                                                              const auto found =
                                                                  std::ranges::find(rows, key, &GalleryRecord::key);
                                                              if (found != rows.end()) {
                                                                  found->quantity = value;
                                                                  state->records.set(std::move(rows));
                                                              }
                                                          }));
                                              }}
                       .title(gallery_text("gallery.complete.copy.043"))
                       .width(160)
                       .sorter([](const GalleryRecord& a, const GalleryRecord& b) {
                           return a.quantity.value_or(0) < b.quantity.value_or(0);
                       }),
                   TableColumn<GalleryRecord>{u8"status", ryn::String{},
                                              [](TableCell<GalleryRecord> cell) {
                                                  Tag(TagProps{}.color(bind([cell, tr = useI18n()] {
                                                      return TagColor{cell.value().locked ? Status::Warning
                                                                                          : Status::Success};
                                                  })),
                                                      TagContent{[cell] {
                                                          Text(TextProps{}.content(bind([cell, tr = useI18n()] {
                                                              return tr.translate(gallery_message(
                                                                  cell.value().locked ? "gallery.complete.copy.382"
                                                                                      : "gallery.complete.copy.383"));
                                                          })));
                                                      }});
                                              }}
                       .title(gallery_text("gallery.complete.copy.044"))})
              .sort(state->table_sort)
              .onSortChange([state](TableSort sort) {
                  state->table_sort.set(std::move(sort));
                  state->table_page.set(1);
              })
              .rowSelection({state->table_selected, [state](auto keys) { state->table_selected.set(std::move(keys)); },
                             [](const GalleryRecord& row) { return row.locked; }})
              .current(state->table_page)
              .pageSize(std::size_t{4})
              .scrollY(180)
              .loading(state->table_loading)
              .bordered(),
          TableEmpty{[] { gallery_label("gallery.copy.sample.219"); }});
    remember_target(state, "gallery.table.editable");
    ++state->telemetry.live_samples;
}

void add_pagination_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    const auto identity =
        state->page_component == "ant.component.table" ? "gallery.table.pagination" : "gallery.pagination.controlled";
    gallery_label("gallery.complete.copy.386");
    Pagination(PaginationProps{}
                   .total(bind([state] { return state->records.get().size(); }))
                   .current(state->table_page)
                   .pageSize(std::size_t{4})
                   .hideOnSinglePage()
                   .onChange([state](std::size_t page, std::size_t) { state->table_page.set(page); }));
    remember_target(state, identity);
    ++state->telemetry.live_samples;
    gallery_label("gallery.copy.sample.220");
    Pagination(
        PaginationProps{}
            .total(std::size_t{247})
            .defaultPageSize(20)
            .onChange([state](std::size_t page, std::size_t size) {
                state->data_feedback.set(GalleryMessage{
                    "gallery.event.003", {{u8"v0", utf8(std::to_string(page))}, {u8"v1", utf8(std::to_string(size))}}});
            })
            .showSizeChanger()
            .showQuickJumper()
            .responsive()
            .align(PaginationAlign::End)
            .layout(LayoutStyle{}.width(state->document_width))
            .showTotal([](std::size_t total, PaginationRange range) {
                return utf8(std::to_string(range.first) + "–" + std::to_string(range.last) + " / " +
                            std::to_string(total));
            }));
    remember_target(state, "gallery.pagination.advanced");
    Text(TextProps{}.content(state->data_feedback));
    gallery_label("gallery.copy.sample.221");
    Pagination(PaginationProps{}.total(std::size_t{105}).simple().showSizeChanger(false));
    remember_target(state, "gallery.pagination.simple");
    gallery_label("gallery.copy.sample.222");
    Pagination(PaginationProps{}.total(std::size_t{50}).itemRender([](PaginationItemContext item) {
        Text(TextProps{}.content(bind([type = item.type, label = item.label] {
            switch (type.get()) {
            case PaginationItemType::Previous:
                return String{u8"‹"};
            case PaginationItemType::Next:
                return String{u8"›"};
            case PaginationItemType::JumpPrevious:
            case PaginationItemType::JumpNext:
                return String{u8"…"};
            case PaginationItemType::Page:
                return label.get();
            }
            return String{};
        })));
    }));
    state->telemetry.live_samples += 3;
}

std::shared_ptr<const ryn::EncodedImage> media_fixture_source() {
    return ryn::EncodedImage::copy(std::as_bytes(std::span{media_fixture_png}));
}

std::shared_ptr<const ryn::NativeFile> media_fixture_file(ryn::String name = ryn::String{u8"fixture.png"}) {
    auto file = std::make_shared<ryn::NativeFile>();
    file->name = std::move(name);
    file->mime_type = u8"image/png";
    const auto source = media_fixture_source();
    file->bytes = std::make_shared<const std::vector<std::byte>>(source->bytes().begin(), source->bytes().end());
    file->size = file->bytes->size();
    return file;
}

void add_image_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;

    struct Pending final {
        ImageCompletion done;
        GalleryFeedback message{GalleryMessage{"gallery.complete.copy.238", {}}, useI18n()};
    };

    const auto pending = std::make_shared<Pending>();
    const ImageSource encoded{media_fixture_source()};
    Signal<ImageSource> source{encoded};
    Signal<bool> disabled{false};
    Signal<bool> visible{true};
    Signal<bool> rtl{false};
    Signal<bool> accept{true};
    Signal<bool> open{false};
    Signal<ImageFit> fit{ImageFit::Contain};
    const auto loader = std::make_shared<ImageLoader>();
    loader->start = [pending](ImageCompletion done) {
        pending->done = std::move(done);
        pending->message.set(GalleryMessage{"gallery.complete.copy.243", {}});
        return [pending] {
            pending->message.set(GalleryMessage{"gallery.complete.copy.244", {}});
        };
    };
    ImageRef reference;
    gallery_label("gallery.complete.copy.166");
    Flex(FlexProps{}.wrap(true).gap(dp(8)), FlexContent{[=] {
             Button(ButtonProps{}.onClick([=] { source.set(ImageSource{std::make_shared<ImageLoader>(*loader)}); }),
                    [] { gallery_label("gallery.complete.copy.167"); });
             remember_target(state, "gallery.image.load");
             Button(ButtonProps{}.onClick([=] {
                 if (pending->done) {
                     const auto accepted = pending->done(decode_image(*media_fixture_source()));
                     pending->message.set(accepted ? GalleryMessage{"gallery.complete.copy.253", {}}
                                                   : GalleryMessage{"gallery.complete.copy.254", {}});
                 }
             }),
                    [] { gallery_label("gallery.complete.copy.168"); });
             remember_target(state, "gallery.image.deliver");
             Button(ButtonProps{}.onClick([=] {
                 if (pending->done) {
                     static_cast<void>(
                         pending->done({{}, {ImageErrorCode::loader_failure, "Synthetic application source failure"}}));
                 }
             }),
                    [] { gallery_label("gallery.complete.copy.169"); });
             remember_target(state, "gallery.image.failure");
             Button(ButtonProps{}.onClick([=] { visible.set(!visible.get()); }),
                    [] { gallery_label("gallery.complete.copy.170"); });
             Button(ButtonProps{}.onClick([=] { disabled.set(!disabled.get()); }),
                    [] { gallery_label("gallery.complete.copy.171"); });
             Button(ButtonProps{}.onClick(
                        [=] { fit.set(fit.get() == ImageFit::Contain ? ImageFit::Cover : ImageFit::Contain); }),
                    [] { gallery_label("gallery.complete.copy.172"); });
             Button(ButtonProps{}.onClick([=] { accept.set(!accept.get()); }),
                    [] { gallery_label("gallery.complete.copy.173"); });
             Button(ButtonProps{}.onClick([=] { static_cast<void>(reference.preview()); }),
                    [] { gallery_label("gallery.complete.copy.174"); });
             remember_target(state, "gallery.image.preview");
         }});
    Text(TextProps{}.content(pending->message));
    Image(ImageProps{}
              .source(source)
              .alt(gallery_text("gallery.complete.copy.503"))
              .visible(visible)
              .disabled(disabled)
              .fit(fit)
              .open(open)
              .ref(reference)
              .onOpenChange([=](bool value) {
                  if (accept.get()) {
                      open.set(value);
                  }
              })
              .onLoad([pending] { pending->message.set(GalleryMessage{"gallery.complete.copy.245", {}}); })
              .onError([pending](const ImageError& error) {
                  pending->message.set(GalleryMessage{"gallery.complete.copy.416",
                                                      {{u8"error", String::from_utf8(error.message).value()}}});
              })
              .layout(LayoutStyle{}.width(dp(240)).height(dp(160))),
          {.placeholder = ImagePlaceholder{[] { gallery_label("gallery.complete.copy.175"); }},
           .error = ImageErrorContent{[] { gallery_label("gallery.complete.copy.176"); }}});
    remember_target(state, "gallery.image.primary");
    const std::array<std::byte, 4> malformed{};
    gallery_label("gallery.complete.copy.177");
    Image(ImageProps{}
              .source(ImageSource{EncodedImage::copy(malformed)})
              .fallback(encoded)
              .alt(gallery_text("gallery.complete.copy.504"))
              .layout(LayoutStyle{}.width(dp(120)).height(dp(80))));
    Signal<std::vector<ImagePreviewItem>> items{{{String{u8"alpha"}, encoded, String{u8"Alpha fixture"}},
                                                 {String{u8"second"}, encoded, String{u8"Second fixture"}}}};
    Signal<std::size_t> current{0};
    Signal<bool> grouped{false};
    auto actions = std::make_shared<ImageActionsRenderer>();
    actions->render = [](const ImagePreviewActions& api) {
        Input(InputProps{}.defaultValue(u8"Retained toolbar draft"));
        Flex(FlexProps{}.wrap(true).gap(dp(4)), FlexContent{[api] {
                 for (const auto action :
                      {ImageAction::Previous, ImageAction::Next, ImageAction::ZoomIn, ImageAction::ZoomOut,
                       ImageAction::RotateRight, ImageAction::FlipX, ImageAction::Reset, ImageAction::Close}) {
                     Button(ButtonProps{}.onClick([api, action] { api.invoke(action); }), [action] {
                         const auto label = action == ImageAction::Previous      ? "gallery.complete.copy.505"
                                            : action == ImageAction::Next        ? "gallery.complete.copy.506"
                                            : action == ImageAction::ZoomIn      ? "gallery.complete.copy.507"
                                            : action == ImageAction::ZoomOut     ? "gallery.complete.copy.508"
                                            : action == ImageAction::RotateRight ? "gallery.complete.copy.509"
                                            : action == ImageAction::FlipX       ? "gallery.complete.copy.510"
                                            : action == ImageAction::Reset       ? "gallery.complete.copy.511"
                                                                                 : "gallery.complete.copy.512";
                         gallery_label(label);
                     });
                 }
             }});
    };
    Button(ButtonProps{}.onClick([=] {
        auto value = items.get();
        std::reverse(value.begin(), value.end());
        items.set(std::move(value));
    }),
           [] { gallery_label("gallery.complete.copy.178"); });
    remember_target(state, "gallery.image.reorder");
    Button(ButtonProps{}.onClick([=] { rtl.set(!rtl.get()); }), [] { gallery_label("gallery.complete.copy.179"); });
    PreviewGroup(PreviewGroupProps{}
                     .items(gallery_record_labels<ImagePreviewItem>(
                         items, {{u8"alpha", "gallery.complete.copy.518"}, {u8"second", "gallery.complete.copy.519"}},
                         &ImagePreviewItem::alt))
                     .current(current)
                     .open(grouped)
                     .rtl(rtl)
                     .actionsRender(std::shared_ptr<const ImageActionsRenderer>{actions})
                     .onChange([=](std::size_t value, std::size_t) { current.set(value); })
                     .onOpenChange([=](bool value, std::size_t) { grouped.set(value); }));
    remember_target(state, "gallery.image.group");
    state->telemetry.live_samples += 3;
}

void add_upload_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;

    struct Demo final {
        struct Work final {
            UploadRequest request;
            bool active{true};
            float percent{};
        };

        std::vector<Work> jobs;
        std::vector<UploadRemoveCompletion> removals;
        GalleryFeedback feedback{GalleryMessage{"gallery.feedback.037", {}}, useI18n()};
    };

    const auto demo = std::make_shared<Demo>();
    Signal<UploadFiles> files{UploadFiles{}};
    Signal<UploadListType> list{UploadListType::Picture};
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    Signal<bool> rtl{false};
    Signal<bool> accept{true};
    Signal<std::shared_ptr<const UploadItemRenderer>> renderer{std::shared_ptr<const UploadItemRenderer>{}};
    const auto rich = std::make_shared<UploadItemRenderer>();
    rich->render = [](Signal<UploadFile> file, const UploadItemActions& actions) {
        Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[file, actions] {
                 Text(TextProps{}.content(bind([file] { return file.get().name; })));
                 Input(InputProps{}.defaultValue(u8"Retained file item draft"));
                 Progress(ProgressProps{}.percent(bind([file] { return file.get().percent; })));
                 Button(ButtonProps{}.onClick(actions.preview), [] { gallery_label("gallery.complete.copy.180"); });
                 Button(ButtonProps{}.onClick(actions.remove), [] { gallery_label("gallery.complete.copy.181"); });
                 Button(ButtonProps{}.onClick(actions.cancel), [] { gallery_label("gallery.complete.copy.182"); });
             }});
    };
    UploadRef reference;
    gallery_label("gallery.complete.copy.183");
    Flex(FlexProps{}.wrap(true).gap(dp(8)), FlexContent{[=] {
             Button(ButtonProps{}.onClick([=] { static_cast<void>(reference.select({media_fixture_file()})); }),
                    [] { gallery_label("gallery.complete.copy.184"); });
             remember_target(state, "gallery.upload.fixture");
             Button(ButtonProps{}.onClick(
                        [=] { static_cast<void>(reference.select({media_fixture_file(u8"ignored.png")})); }),
                    [] { gallery_label("gallery.complete.copy.185"); });
             remember_target(state, "gallery.upload.ignore");
             Button(ButtonProps{}.onClick([=] {
                 for (auto& job : demo->jobs) {
                     if (job.active) {
                         job.percent = std::min(99.F, job.percent + 20);
                         static_cast<void>(job.request.progress(job.percent));
                     }
                 }
             }),
                    [] { gallery_label("gallery.complete.copy.186"); });
             remember_target(state, "gallery.upload.progress");
             Button(ButtonProps{}.onClick([=] {
                 for (auto& job : demo->jobs) {
                     if (job.active) {
                         job.active = false;
                         static_cast<void>(job.request.success({String{u8"Synthetic fixture result"}, {}}));
                     }
                 }
             }),
                    [] { gallery_label("gallery.complete.copy.187"); });
             remember_target(state, "gallery.upload.success");
             Button(ButtonProps{}.onClick([=] {
                 for (auto& job : demo->jobs) {
                     if (job.active) {
                         job.active = false;
                         static_cast<void>(
                             job.request.failure(String{u8"Synthetic transport failure; retry available"}));
                     }
                 }
             }),
                    [] { gallery_label("gallery.complete.copy.169"); });
             remember_target(state, "gallery.upload.failure");
             Button(ButtonProps{}.onClick([=] {
                 const auto callbacks = std::move(demo->removals);
                 demo->removals.clear();
                 for (const auto& done : callbacks) {
                     static_cast<void>(done(true));
                 }
             }),
                    [] { gallery_label("gallery.complete.copy.188"); });
             remember_target(state, "gallery.upload.remove");
             Button(ButtonProps{}.onClick(
                        [=] { list.set(static_cast<UploadListType>((static_cast<int>(list.get()) + 1) % 4)); }),
                    [] { gallery_label("gallery.complete.copy.189"); });
             remember_target(state, "gallery.upload.list");
             Button(ButtonProps{}.onClick([=] { renderer.set(renderer.get() ? nullptr : rich); }),
                    [] { gallery_label("gallery.complete.copy.190"); });
             remember_target(state, "gallery.upload.custom");
             Button(ButtonProps{}.onClick([=] { read_only.set(!read_only.get()); }),
                    [] { gallery_label("gallery.complete.copy.191"); });
             Button(ButtonProps{}.onClick([=] { disabled.set(!disabled.get()); }),
                    [] { gallery_label("gallery.complete.copy.192"); });
             Button(ButtonProps{}.onClick([=] { accept.set(!accept.get()); }),
                    [] { gallery_label("gallery.complete.copy.193"); });
             Button(ButtonProps{}.onClick([=] { rtl.set(!rtl.get()); }),
                    [] { gallery_label("gallery.complete.copy.179"); });
         }});
    Text(TextProps{}.content(demo->feedback));
    Upload(UploadProps{}
               .fileList(files)
               .listType(list)
               .type(UploadType::Drag)
               .accept(u8".png,.jpg,.jpeg,.bmp")
               .multiple()
               .pastable()
               .maxCount(3)
               .disabled(disabled)
               .readOnly(read_only)
               .rtl(rtl)
               .itemRender(renderer)
               .ref(reference)
               .beforeUpload([demo](const UploadFile& file, const UploadFiles&) {
                   if (file.name == String{u8"ignored.png"}) {
                       demo->feedback.set(GalleryMessage{"gallery.feedback.085", {}});
                   }
                   return UploadBeforeResult{file.name == String{u8"ignored.png"} ? UploadBeforeDecision::Ignore
                                                                                  : UploadBeforeDecision::Upload,
                                             {}};
               })
               .customRequest([demo](const UploadRequest& request) {
                   const auto index = demo->jobs.size();
                   demo->jobs.push_back({request, true});
                   demo->feedback.set(GalleryMessage{"gallery.feedback.086", {}});
                   return [weak = std::weak_ptr<Demo>(demo), index] {
                       if (const auto demo = weak.lock(); demo && index < demo->jobs.size()) {
                           demo->jobs[index].active = false;
                           demo->feedback.set(GalleryMessage{"gallery.feedback.087", {}});
                       }
                   };
               })
               .onChange([=](const UploadChange& change) {
                   if (accept.get()) {
                       files.set(change.file_list);
                   }
                   switch (change.reason) {
                   case UploadChangeReason::Selection:
                       demo->feedback.set(
                           GalleryMessage{"gallery.complete.copy.257",
                                          {{u8"name", change.file.name},
                                           {u8"count", static_cast<std::uint64_t>(change.file_list.size())}}});
                       break;
                   case UploadChangeReason::Start:
                       demo->feedback.set(
                           GalleryMessage{"gallery.complete.copy.258",
                                          {{u8"name", change.file.name},
                                           {u8"count", static_cast<std::uint64_t>(change.file_list.size())}}});
                       break;
                   case UploadChangeReason::Progress:
                       demo->feedback.set(
                           GalleryMessage{"gallery.complete.copy.259",
                                          {{u8"name", change.file.name},
                                           {u8"count", static_cast<std::uint64_t>(change.file_list.size())}}});
                       break;
                   case UploadChangeReason::Success:
                       demo->feedback.set(
                           GalleryMessage{"gallery.complete.copy.260",
                                          {{u8"name", change.file.name},
                                           {u8"count", static_cast<std::uint64_t>(change.file_list.size())}}});
                       break;
                   case UploadChangeReason::Failure:
                       demo->feedback.set(
                           GalleryMessage{"gallery.complete.copy.261",
                                          {{u8"name", change.file.name},
                                           {u8"count", static_cast<std::uint64_t>(change.file_list.size())}}});
                       break;
                   case UploadChangeReason::Remove:
                       demo->feedback.set(
                           GalleryMessage{"gallery.complete.copy.262",
                                          {{u8"name", change.file.name},
                                           {u8"count", static_cast<std::uint64_t>(change.file_list.size())}}});
                       break;
                   default:
                       demo->feedback.set(
                           GalleryMessage{"gallery.complete.copy.263",
                                          {{u8"name", change.file.name},
                                           {u8"count", static_cast<std::uint64_t>(change.file_list.size())}}});
                       break;
                   }
               })
               .onRemove([demo](const UploadFile&, UploadRemoveCompletion done) {
                   demo->removals.push_back(std::move(done));
                   demo->feedback.set(GalleryMessage{"gallery.feedback.088", {}});
                   return UploadCancel{};
               })
               .onDownload([demo](const UploadFile& file) {
                   demo->feedback.set(GalleryMessage{"gallery.complete.copy.264", {{u8"name", file.name}}});
               })
               .onSelectionError([demo](FileServiceError, String message) {
                   demo->feedback.set(GalleryMessage{"gallery.complete.copy.419", {{u8"error", std::move(message)}}});
               })
               .layout(LayoutStyle{}.width(dp(640))));
    remember_target(state, "gallery.upload.primary");
    state->telemetry.live_samples += 1;
}

void add_transfer_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    std::vector<TransferItem> items;
    for (int i = 0; i < 24; ++i) {
        items.push_back({static_cast<double>(i), utf8("Item " + std::to_string(i)), String{}, i == 2});
    }
    std::vector<std::pair<TransferKey, GalleryMessage>> item_labels;
    for (std::uint64_t i = 0; i < items.size(); ++i) {
        item_labels.push_back({static_cast<double>(i), GalleryMessage{"gallery.complete.copy.372", {{u8"index", i}}}});
    }
    const auto localized_items = gallery_transfer_labels(items, std::move(item_labels));
    Signal<TransferKeys> target{TransferKeys{1.0, 3.0}};
    Signal<TransferKeys> selected{TransferKeys{}};
    Signal<bool> one_way{false};
    Signal<bool> accept{true};
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    Signal<float> height{160};
    Signal<float> width{240};
    Signal<float> spacing{2};
    Signal<bool> custom_locale{false};
    const auto locale = bind([custom_locale, tr = useI18n()] {
        auto value = LocaleCatalog::builtin().pack(tr.locale().effective).data().transfer;
        if (custom_locale.get()) {
            value.titles = {tr.translate(gallery_message("gallery.complete.copy.370")),
                            tr.translate(gallery_message("gallery.complete.copy.371"))};
        }
        return value;
    });
    ThemeConfig initial;
    initial.transfer.tokens.header = Color::rgba8(230, 244, 255);
    initial.transfer.tokens.footer = Color::rgba8(246, 255, 237);
    initial.transfer.tokens.selected = Color::rgba8(186, 224, 255);
    initial.transfer.tokens.actions_width = dp(80);
    initial.transfer.tokens.item_height = dp(36);
    Signal<ThemeConfig> tokens{initial};
    Text(TextProps{}.content(state->data_feedback));
    gallery_label("gallery.copy.sample.223");
    Theme(
        ThemeProps{}.config(tokens), ThemeContent{[=] {
            Transfer(
                TransferProps{}
                    .dataSource(localized_items)
                    .targetKeys(target)
                    .selectedKeys(selected)
                    .showSearch(true)
                    .oneWay(one_way)
                    .direction(direction)
                    .locale(locale)
                    .operations(bind([locale] { return locale.get().actions; }))
                    .listHeight(height)
                    .listWidth(width)
                    .rowSpacing(spacing)
                    .pagination(TransferPagination{true, 5, true, true, false, {5, 10, 20}})
                    .onChange([state, target, accept](TransferChangeInfo event) {
                        if (!accept.get()) {
                            state->data_feedback.set(GalleryMessage{"gallery.feedback.089", {}});
                            return;
                        }
                        target.set(event.target_keys);
                        state->data_feedback.set(
                            (event.direction == TransferDirection::Right
                                 ? GalleryMessage{"gallery.event.004.yes",
                                                  {{u8"v1", static_cast<std::uint64_t>(event.moved_keys.size())}}}
                                 : GalleryMessage{"gallery.event.004.no",
                                                  {{u8"v1", static_cast<std::uint64_t>(event.moved_keys.size())}}}));
                    })
                    .onSelectChange([selected](TransferSelectInfo event) {
                        event.source_selected_keys.insert(event.source_selected_keys.end(),
                                                          event.target_selected_keys.begin(),
                                                          event.target_selected_keys.end());
                        selected.set(event.source_selected_keys);
                    })
                    .onSearch([state](TransferDirection side, String query) {
                        state->data_feedback.set(
                            (side == TransferDirection::Right
                                 ? GalleryMessage{"gallery.event.005.yes", {{u8"v1", utf8(std::string{query.bytes()})}}}
                                 : GalleryMessage{"gallery.event.005.no",
                                                  {{u8"v1", utf8(std::string{query.bytes()})}}}));
                    })
                    .onScroll([state](TransferScrollInfo event) {
                        state->data_feedback.set(
                            GalleryMessage{"gallery.event.006", {{u8"v0", utf8(std::to_string(event.offset))}}});
                    })
                    .footer(TransferDirection::Left, TransferFooter{[] { gallery_label("gallery.copy.sample.224"); }})
                    .layout(LayoutStyle{}.width(bind([width] { return dp(width.get() * 2 + 96); }))));
        }});
    remember_target(state, "gallery.transfer.lists");
    Button(ButtonProps{}.onClick([accept] { accept.set(!accept.get()); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.225"); }});
    remember_target(state, "gallery.transfer.accept");
    Button(ButtonProps{}.onClick([one_way] { one_way.set(!one_way.get()); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.226"); }});
    remember_target(state, "gallery.transfer.one-way");
    Button(ButtonProps{}.onClick([direction] {
        direction.set(direction.get() == TextDirection::RightToLeft ? TextDirection::LeftToRight
                                                                    : TextDirection::RightToLeft);
    }),
           ButtonContent{[] { Text(u8"RTL / LTR"); }});
    remember_target(state, "gallery.transfer.direction");
    Button(ButtonProps{}.onClick([custom_locale] { custom_locale.set(!custom_locale.get()); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.227"); }});
    remember_target(state, "gallery.transfer.locale");
    Button(ButtonProps{}.onClick([height, width, spacing] {
        height.set(height.get() == 160 ? 96 : 160);
        width.set(width.get() == 240 ? 200 : 240);
        spacing.set(spacing.get() == 2 ? 7 : 2);
    }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.228"); }});
    remember_target(state, "gallery.transfer.geometry");
    Button(ButtonProps{}.onClick([tokens, initial] {
        auto next = initial;
        if (tokens.get() == initial) {
            next.transfer.tokens.header = Color::rgba8(255, 241, 184);
            next.transfer.tokens.item_height = dp(44);
            next.transfer.tokens.radius = dp(12);
            next.transfer.tokens.font_size = dp(16);
            next.transfer.tokens.line_height = dp(25);
        }
        tokens.set(next);
    }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.229"); }});
    remember_target(state, "gallery.transfer.tokens");
    auto rich = TransferItem{String{u8"draft"}, u8"Retained input draft"};
    rich.content = TransferItemContent{[] { Input(InputProps{}.defaultValue(String{u8"移动和重排时保留此草稿"})); }};
    Transfer(
        TransferProps{}
            .dataSource(gallery_transfer_labels(std::vector<TransferItem>{rich, {String{u8"plain"}, u8"Plain item"}},
                                                {{String{u8"draft"}, GalleryMessage{"gallery.complete.copy.373", {}}},
                                                 {String{u8"plain"}, GalleryMessage{"gallery.complete.copy.374", {}}}}))
            .defaultSelectedKeys({String{u8"draft"}})
            .listHeight(100)
            .locale(locale)
            .onChange([state](TransferChangeInfo event) {
                state->data_feedback.set(GalleryMessage{
                    "gallery.event.007", {{u8"v0", static_cast<std::uint64_t>(event.moved_keys.size())}}});
            })
            .layout(LayoutStyle{}.width(dp(560))));
    remember_target(state, "gallery.transfer.rich");
    gallery_label("gallery.copy.sample.230");
    Transfer(
        TransferProps{}
            .dataSource(localized_items)
            .listHeight(180)
            .locale(locale)
            .showSearch(true)
            .renderList([](TransferListContext context) {
                return TransferListContent{[context] {
                    const auto item_key = [](const TransferKey& key) {
                        return std::holds_alternative<String>(key) ? std::get<String>(key)
                                                                   : utf8(std::to_string(std::get<double>(key)));
                    };
                    Table(TableProps<TransferItem>{}
                              .dataSource(context.filtered_items)
                              .rowKey([item_key](const TransferItem& item) { return item_key(item.key); })
                              .columns({TableColumn<TransferItem>{
                                  u8"item", ryn::String{},
                                  [](TableCell<TransferItem> cell) {
                                      Text(TextProps{}.content(bind([cell] { return cell.value().title; })));
                                  }}.title(gallery_text("gallery.complete.copy.045"))})
                              .rowSelection(
                                  {bind([context, item_key] {
                                       std::vector<String> keys;
                                       for (const auto& key : ryn::detail::read_prop(context.selected_keys)) {
                                           keys.push_back(item_key(key));
                                       }
                                       return keys;
                                   }),
                                   [context, item_key](std::vector<String> selected) {
                                       const auto before = ryn::detail::read_prop(context.selected_keys);
                                       TransferKeys add;
                                       TransferKeys remove;
                                       for (const auto& item : ryn::detail::read_prop(context.filtered_items)) {
                                           if (item.disabled) {
                                               continue;
                                           }
                                           const bool checked =
                                               std::ranges::find(selected, item_key(item.key)) != selected.end();
                                           const bool was_checked = std::ranges::find(before, item.key) != before.end();
                                           if (checked && !was_checked) {
                                               add.push_back(item.key);
                                           }
                                           if (!checked && was_checked) {
                                               remove.push_back(item.key);
                                           }
                                       }
                                       if (!remove.empty()) {
                                           context.on_item_select_all(std::move(remove), false);
                                       }
                                       if (!add.empty()) {
                                           context.on_item_select_all(std::move(add), true);
                                       }
                                   },
                                   [context](const TransferItem& item) {
                                       return item.disabled || ryn::detail::read_prop(context.disabled);
                                   }})
                              .pageSize(std::size_t{100})
                              .showHeader(false)
                              .scrollY(140)
                              .size(ControlSize::Small));
                }};
            })
            .selectAllLabel(TransferDirection::Left,
                            [](TransferSelectAllContext context) {
                                return TransferSelectAllLabel{[context] { Text(TextProps{}.content(context.label)); }};
                            })
            .action(TransferDirection::Right,
                    [](TransferActionContext context) {
                        return TransferAction{[context] {
                            Button(ButtonProps{}.disabled(context.disabled).onClick(context.activate),
                                   ButtonContent{[] { Text(u8"+"); }});
                        }};
                    })
            .footer([](TransferListContext context) {
                return TransferFooter{[context] {
                    TextArea(TextAreaProps{}
                                 .disabled(context.disabled)
                                 .defaultValue(String{u8"保留的 footer 草稿"})
                                 .layout(LayoutStyle{}.height(dp(32))));
                    if (context.direction == TransferDirection::Left) {
                        gallery_target("gallery.transfer.custom.footer");
                    }
                }};
            })
            .onChange([state](TransferChangeInfo event) {
                state->data_feedback.set(GalleryMessage{
                    "gallery.event.008", {{u8"v0", static_cast<std::uint64_t>(event.moved_keys.size())}}});
            })
            .layout(LayoutStyle{}.width(dp(560))));
    remember_target(state, "gallery.transfer.custom");
    ++state->telemetry.live_samples;
}

void add_cascader_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<CascaderPaths> selected{CascaderPaths{}};
    const auto options = gallery_cascader_labels(
        std::vector<CascaderOption>{
            {u8"cn", String{u8"中国"}, u8"中国", {{u8"hz", 1.0, u8"杭州"}, {u8"nj", 2.0, u8"南京"}}},
            {u8"eu", String{u8"Europe"}, u8"Europe", {{u8"paris", String{u8"Paris"}, u8"Paris"}}},
            {u8"off", String{u8"off"}, u8"Disabled", {}, true}},
        {{u8"cn", GalleryMessage{"gallery.complete.copy.451", {}}},
         {u8"hz", GalleryMessage{"gallery.complete.copy.452", {}}},
         {u8"nj", GalleryMessage{"gallery.complete.copy.453", {}}},
         {u8"eu", GalleryMessage{"gallery.complete.copy.454", {}}},
         {u8"paris", GalleryMessage{"gallery.complete.copy.455", {}}},
         {u8"off", GalleryMessage{"gallery.complete.copy.456", {}}}});
    Text(TextProps{}.content(state->data_feedback));
    Cascader(CascaderProps{}
                 .options(options)
                 .value(selected)
                 .showSearch(true)
                 .placeholder(gallery_text("gallery.copy.property.036"))
                 .popupContent(CascaderPopupContent{[state] {
                     Popover(PopoverProps{}.trigger(PopoverTriggerMode::Click), PopoverTrigger{[state] {
                                 Button(ButtonProps{}, ButtonContent{[] { gallery_label("gallery.copy.sample.231"); }});
                                 remember_target(state, "gallery.cascader.child-trigger");
                             }},
                             PopoverSlots{{}, PopoverContent{[state] {
                                              Input(
                                                  InputProps{}.placeholder(gallery_text("gallery.copy.property.037")));
                                              remember_target(state, "gallery.cascader.child-editor");
                                          }}});
                 }})
                 .onChange([state, selected](CascaderPaths paths) {
                     selected.set(paths);
                     state->data_feedback.set(
                         GalleryMessage{"gallery.event.009", {{u8"v0", static_cast<std::uint64_t>(paths.size())}}});
                 })
                 .layout(LayoutStyle{}.width(dp(400))));
    remember_target(state, "gallery.cascader.selector");
    CascaderPanel(CascaderProps{}
                      .options(options)
                      .multiple(true)
                      .showCheckedStrategy(CascaderCheckedStrategy::ShowChild)
                      .onChange([state](CascaderPaths paths) {
                          state->data_feedback.set(GalleryMessage{
                              "gallery.event.010", {{u8"v0", static_cast<std::uint64_t>(paths.size())}}});
                      })
                      .layout(LayoutStyle{}.width(dp(540)).height(dp(240))));
    remember_target(state, "gallery.cascader.panel");
    const auto load_attempts = std::make_shared<unsigned>(0);
    gallery_label("gallery.copy.sample.232");
    Cascader(CascaderProps{}
                 .options(gallery_cascader_labels(
                     std::vector<CascaderOption>{{u8"lazy", String{u8"lazy"}, String{}, {}, false, false, false}},
                     {{u8"lazy", GalleryMessage{"gallery.complete.copy.520", {}}}}))
                 .loadData([state, load_attempts](CascaderLoadRequest request) {
                     if (++*load_attempts == 1) {
                         state->data_feedback.set(GalleryMessage{"gallery.feedback.090", {}});
                         (void)request.complete({{}, String{u8"Local demo error"}});
                         return;
                     }
                     state->data_feedback.set(GalleryMessage{"gallery.feedback.091", {}});
                     static_cast<void>(request.complete({{{u8"loaded", 42.0, u8"Loaded child"}}, {}}));
                 })
                 .layout(LayoutStyle{}.width(dp(400))));
    remember_target(state, "gallery.cascader.lazy");
    gallery_label("gallery.copy.sample.233");
    Signal<CascaderPaths> tags{
        CascaderPaths{{String{u8"中国"}, 1.0}, {String{u8"中国"}, 2.0}, {String{u8"Europe"}, String{u8"Paris"}}}};
    Cascader(CascaderProps{}
                 .options(options)
                 .multiple(true)
                 .value(tags)
                 .maxTagCount(CascaderTagCount::Responsive)
                 .maxTagTextLength(std::size_t{10})
                 .maxTagPlaceholder([tr = useI18n()](const CascaderPaths& values) {
                     return tr.translate(gallery_message("gallery.complete.copy.457"),
                                         {{u8"count", static_cast<std::uint64_t>(values.size())}});
                 })
                 .onChange([state, tags](CascaderPaths value) {
                     tags.set(value);
                     state->data_feedback.set(
                         GalleryMessage{"gallery.event.011", {{u8"v0", static_cast<std::uint64_t>(value.size())}}});
                 })
                 .layout(LayoutStyle{}.width(dp(400))));
    remember_target(state, "gallery.cascader.tags");
    gallery_label("gallery.copy.sample.234");
    ThemeConfig themed;
    themed.cascader.tokens.option_selected_background = Color::rgba8(230, 255, 235);
    themed.cascader.tokens.option_selected_color = Color::rgba8(20, 100, 55);
    themed.cascader.tokens.option_height = dp(36);
    themed.cascader.tokens.column_width = dp(200);
    themed.cascader.tokens.dropdown_height = dp(180);
    themed.cascader.tokens.tag_background = Color::rgba8(230, 255, 235);
    std::vector<CascaderOption> long_options;
    for (int i = 0; i < 30; ++i) {
        const auto label = utf8("Straße / 分组 " + std::to_string(i));
        long_options.push_back({utf8("long-" + std::to_string(i)), double(i), label});
    }
    std::vector<std::pair<String, GalleryMessage>> long_labels;
    for (std::uint64_t i = 0; i < long_options.size(); ++i) {
        long_labels.push_back({long_options[i].key, GalleryMessage{"gallery.complete.copy.458", {{u8"index", i}}}});
    }
    const auto localized_long = gallery_cascader_labels(long_options, std::move(long_labels));
    Theme(ThemeProps{}.config(themed), ThemeContent{[state, localized_long] {
              CascaderPanel(CascaderProps{}
                                .options(localized_long)
                                .showSearch(true)
                                .onChange([state](CascaderPaths value) {
                                    state->data_feedback.set(GalleryMessage{
                                        "gallery.event.012", {{u8"v0", static_cast<std::uint64_t>(value.size())}}});
                                })
                                .layout(LayoutStyle{}.width(dp(400))));
          }});
    remember_target(state, "gallery.cascader.theme-scroll");
    gallery_label("gallery.copy.sample.235");
    Signal<CascaderPaths> rich_tags{CascaderPaths{{String{u8"中国"}, 1.0}}};
    Cascader(CascaderProps{}
                 .options(options)
                 .multiple(true)
                 .value(rich_tags)
                 .tagRender([](CascaderTagInfo info) {
                     return CascaderTagContent{[info] {
                         Flex(FlexProps{}.align(FlexAlign::Center).gap(dp(4)), FlexContent{[info] {
                                  Input(InputProps{}
                                            .defaultValue(String{u8"保留的标签草稿"})
                                            .disabled(info.disabled)
                                            .layout(LayoutStyle{}.width(dp(160))));
                                  Button(ButtonProps{}.disabled(info.disabled).onClick(info.onClose),
                                         ButtonContent{[] { gallery_label("gallery.copy.sample.236"); }});
                              }});
                     }};
                 })
                 .onChange([rich_tags](CascaderPaths value) { rich_tags.set(value); })
                 .layout(LayoutStyle{}.width(dp(440))));
    remember_target(state, "gallery.cascader.custom-tags");
    gallery_label("gallery.copy.sample.237");
    Cascader(CascaderProps{}
                 .triggerContent(CascaderTriggerContent{[] { gallery_label("gallery.copy.sample.238"); }})
                 .emptyContent(CascaderEmptyContent{[] { gallery_label("gallery.copy.sample.239"); }})
                 .layout(LayoutStyle{}.width(dp(240))));
    remember_target(state, "gallery.cascader.custom-trigger");
    ++state->telemetry.live_samples;
}

void add_menu_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.240");
    Text(TextProps{}.content(state->data_feedback));
    Menu(MenuProps{}
             .mode(MenuMode::Inline)
             .items({{u8"overview", gallery_text("gallery.complete.copy.211")},
                     {u8"tasks", state->menu_label},
                     {u8"tools",
                      gallery_text("gallery.complete.copy.212"),
                      false,
                      false,
                      MenuItemKind::SubMenu,
                      {{u8"logs", gallery_text("gallery.complete.copy.213")},
                       {u8"settings", gallery_text("gallery.complete.copy.214")},
                       {u8"unavailable", gallery_text("gallery.complete.copy.215"), true}}},
                     {u8"divider", u8"", false, false, MenuItemKind::Divider},
                     {u8"reset", gallery_text("gallery.complete.copy.216"), false, true}})
             .selectedKeys(state->menu_selected)
             .openKeys(state->menu_open)
             .disabled(state->menu_disabled)
             .onSelect([state](MenuSelectInfo info) { state->menu_selected.set(std::move(info.selected_keys)); })
             .onOpenChange([state](MenuKeys keys) { state->menu_open.set(std::move(keys)); })
             .onClick([state](MenuClickInfo info) {
                 state->data_feedback.set(
                     GalleryMessage{"gallery.event.013", {{u8"v0", utf8(std::string{info.key.bytes()})}}});
             })
             .layout(LayoutStyle{}.width(dp(260)).height(dp(280))));
    remember_target(state, "gallery.menu.inline");
    Space(SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
              Button(ButtonProps{}.onClick([state] {
                  state->menu_label.set(state->menu_label.is("gallery.complete.copy.123")
                                            ? GalleryMessage{"gallery.complete.copy.134", {}}
                                            : GalleryMessage{"gallery.complete.copy.123", {}});
              }),
                     [] { gallery_label("gallery.copy.sample.241"); });
              remember_target(state, "gallery.menu.label");
              Button(ButtonProps{}.onClick([state] { state->menu_disabled.set(!state->menu_disabled.get()); }),
                     [] { gallery_label("gallery.copy.sample.242"); });
              remember_target(state, "gallery.menu.disabled");
          }});
    if (state->page_component == "ant.component.menu") {
        Signal<bool> changed{false};
        Signal<bool> collapsed{false};
        Signal<MenuMode> mode{MenuMode::Horizontal};
        const auto items = bind([changed] {
            std::vector<MenuItem> rows{{u8"home", gallery_text("gallery.complete.copy.217")},
                                       {u8"tools",
                                        gallery_text("gallery.complete.copy.212"),
                                        false,
                                        false,
                                        MenuItemKind::SubMenu,
                                        {{u8"build", gallery_text("gallery.complete.copy.218")},
                                         {u8"test", gallery_text("gallery.complete.copy.219")},
                                         {u8"more",
                                          gallery_text("gallery.complete.copy.220"),
                                          false,
                                          false,
                                          MenuItemKind::SubMenu,
                                          {{u8"logs", gallery_text("gallery.complete.copy.213")}}}}},
                                       {u8"settings", gallery_text("gallery.complete.copy.221")},
                                       {u8"help", gallery_text("gallery.complete.copy.222")}};
            rows[0].icon = MenuIcon{[] { Icon(IconProps{}.name(IconName::HomeOutlined)); }};
            if (changed.get()) {
                std::reverse(rows.begin(), rows.end());
                rows.push_back({u8"new", gallery_text("gallery.complete.copy.223")});
            }
            return rows;
        });
        gallery_label("gallery.copy.sample.243");
        Menu(MenuProps{}
                 .mode(mode)
                 .items(items)
                 .inlineCollapsed(collapsed)
                 .direction(TextDirection::RightToLeft)
                 .theme(MenuTheme::Dark)
                 .onClick([state](MenuClickInfo info) {
                     state->data_feedback.set(
                         GalleryMessage{"gallery.event.014", {{u8"v0", utf8(std::string{info.key.bytes()})}}});
                 })
                 .layout(LayoutStyle{}
                             .width(bind([mode, collapsed] {
                                 return dp(mode.get() == MenuMode::Inline && collapsed.get() ? 80 : 180);
                             }))
                             .height(dp(176))));
        remember_target(state, "gallery.menu.dynamic");
        Space(SpaceProps{}.wrap(true), SpaceContent{[state, mode, changed, collapsed] {
                  Button(ButtonProps{}.onClick([mode] {
                      mode.set(mode.get() == MenuMode::Horizontal ? MenuMode::Vertical
                               : mode.get() == MenuMode::Vertical ? MenuMode::Inline
                                                                  : MenuMode::Horizontal);
                  }),
                         [] { gallery_label("gallery.copy.sample.244"); });
                  remember_target(state, "gallery.menu.mode");
                  Button(ButtonProps{}.onClick([changed] { changed.set(!changed.get()); }),
                         [] { gallery_label("gallery.copy.sample.245"); });
                  remember_target(state, "gallery.menu.topology");
                  Button(ButtonProps{}.onClick([collapsed] { collapsed.set(!collapsed.get()); }),
                         [] { gallery_label("gallery.copy.sample.246"); });
                  remember_target(state, "gallery.menu.collapsed");
              }});
    }
    ++state->telemetry.live_samples;
}

void add_dropdown_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    if (state->register_targets) {
        Text(TextProps{}.content(state->data_feedback));
    }
    Dropdown(DropdownProps{}
                 .open(state->dropdown_open)
                 .onOpenChange([state](DropdownOpenInfo info) { state->dropdown_open.set(info.open); })
                 .trigger(DropdownTriggerMode::ClickContextMenu)
                 .menu(MenuProps{}
                           .items({{u8"refresh", gallery_text("gallery.complete.copy.226")},
                                   {u8"clear-selection", gallery_text("gallery.complete.copy.227")},
                                   {u8"disabled", gallery_text("gallery.complete.copy.228"), true}})
                           .onClick([state](MenuClickInfo info) {
                               if (info.key == String{u8"refresh"}) {
                                   state->records.set(gallery_records());
                                   state->table_page.set(1);
                               } else if (info.key == String{u8"clear-selection"}) {
                                   state->table_selected.set({});
                               }
                               state->data_feedback.set(GalleryMessage{
                                   "gallery.event.015", {{u8"v0", utf8(std::string{info.key.bytes()})}}});
                           })),
             DropdownTrigger{[] { Button(ButtonProps{}, [] { gallery_label("gallery.copy.sample.247"); }); }});
    remember_target(state, "gallery.dropdown.actions");
    if (state->page_component == "ant.component.dropdown") {
        gallery_label("gallery.copy.sample.248");
        DropdownButton(
            {.dropdown = DropdownProps{}
                             .trigger(DropdownTriggerMode::HoverClick)
                             .arrow(DropdownArrow{true, true})
                             .menu(MenuProps{}
                                       .mode(MenuMode::Vertical)
                                       .items({{u8"run", gallery_text("gallery.complete.copy.229")},
                                               {u8"tools",
                                                gallery_text("gallery.complete.copy.224"),
                                                false,
                                                false,
                                                MenuItemKind::SubMenu,
                                                {{u8"logs", gallery_text("gallery.complete.copy.230")},
                                                 {u8"settings", gallery_text("gallery.complete.copy.225")}}}})
                                       .onClick([state](MenuClickInfo info) {
                                           state->data_feedback.set(GalleryMessage{
                                               "gallery.event.016", {{u8"v0", utf8(std::string{info.key.bytes()})}}});
                                       })),
             .button = ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                 state->data_feedback.set(GalleryMessage{"gallery.feedback.092", {}});
             }),
             .trigger_button = ButtonProps{}.type(ButtonType::Primary)},
            {.content = DropdownButtonContent{[] { gallery_label("gallery.copy.sample.249"); }}});
        remember_target(state, "gallery.dropdown.split");
        gallery_label("gallery.copy.sample.250");
        Dropdown(DropdownProps{}
                     .arrow()
                     .menu(MenuProps{}.items({{u8"apply", gallery_text("gallery.complete.copy.231")},
                                              {u8"cancel", gallery_text("gallery.complete.copy.232")}}))
                     .popupRender([](DropdownMenuContent menu) {
                         Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[menu] {
                                  gallery_label("gallery.copy.sample.251");
                                  menu();
                                  Input(InputProps{}.defaultValue(u8"关闭再打开会保留草稿"));
                              }});
                     }),
                 DropdownTrigger{[] { Button(ButtonProps{}, [] { gallery_label("gallery.copy.sample.252"); }); }});
        remember_target(state, "gallery.dropdown.custom");
    }
    ++state->telemetry.live_samples;
}

void add_color_picker_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.253");
    Text(TextProps{}.content(state->data_feedback));
    auto color = std::make_shared<Signal<ColorPickerValue>>(ColorPickerValue{PickerColor{ColorHsb{215, .8, .9, .75}}});
    auto gradient = std::make_shared<Signal<ColorPickerValue>>(
        ColorPickerValue{std::vector<ColorStop>{{String{u8"red"}, PickerColor{ColorRgb{255, 0, 0, .25}}, 0},
                                                {String{u8"blue"}, PickerColor{ColorRgb{0, 0, 255, 1}}, 100}}});
    Space(
        SpaceProps{}.wrap(true).size(dp(24)), SpaceContent{[state, color, gradient] {
            Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state, color] {
                     gallery_label("gallery.copy.sample.254");
                     ColorPicker(
                         ColorPickerProps{}
                             .value(*color)
                             .showText(true)
                             .allowClear(true)
                             .onChange([state, color](ColorPickerValue next, String css) {
                                 color->set(next);
                                 state->data_feedback.set(css);
                             })
                             .onChangeComplete([state](ColorPickerValue next) {
                                 state->data_feedback.set(GalleryMessage{
                                     "gallery.event.017", {{u8"v0", utf8(std::string{next.format().bytes()})}}});
                             })
                             .onFormatChange([state](ColorFormat) {
                                 state->data_feedback.set(GalleryMessage{"gallery.feedback.093", {}});
                             })
                             .onClear([state] { state->data_feedback.set(GalleryMessage{"gallery.feedback.094", {}}); })
                             .presets(gallery_record_labels<ColorPreset>(
                                 std::vector<ColorPreset>{{String{u8"primary"},
                                                           String{u8"常用颜色"},
                                                           {ColorPickerValue{PickerColor{ColorRgb{22, 119, 255, 1}}},
                                                            ColorPickerValue{PickerColor{ColorRgb{82, 196, 26, 1}}},
                                                            ColorPickerValue{PickerColor{ColorRgb{245, 34, 45, .5}}}},
                                                           true}},
                                 {{u8"primary", "gallery.complete.copy.450"}}, &ColorPreset::label)),
                         ColorPickerSlots{
                             .panel_render =
                                 [](ColorPickerPanelParts parts) {
                                     Flex(FlexProps{}.vertical(true).gap(dp(8)), [parts] {
                                         parts.renderPicker();
                                         parts.renderPresets();
                                         Input(InputProps{}.placeholder(gallery_text("gallery.copy.property.038")));
                                         gallery_target("gallery.color-picker.note");
                                     });
                                 },
                             .text_render =
                                 [](ColorPickerTextContext context) { Text(TextProps{}.content(context.text)); },
                             .extra = ColorPickerExtra{[] { gallery_label("gallery.copy.sample.255"); }}});
                     remember_target(state, "gallery.color-picker.solid");
                 }});
            Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state, gradient] {
                     gallery_label("gallery.copy.sample.256");
                     ColorPicker(
                         ColorPickerProps{}
                             .value(*gradient)
                             .modes(std::vector<ColorPickerMode>{ColorPickerMode::Single, ColorPickerMode::Gradient})
                             .showText(false)
                             .onChange([state, gradient](ColorPickerValue next, String css) {
                                 gradient->set(next);
                                 state->data_feedback.set(css);
                             }));
                     remember_target(state, "gallery.color-picker.gradient");
                 }});
            ColorPicker(ColorPickerProps{}
                            .defaultValue(PickerColor{ColorRgb{255, 165, 0, 1}})
                            .disabledAlpha(true)
                            .defaultFormat(ColorFormat::Hsb)
                            .showText(true)
                            .size(ControlSize::Large));
            ColorPicker(ColorPickerProps{}.defaultValue(PickerColor{}).disabled(true).showText(true));
        }});
}

void add_input_number_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    auto exact = std::make_shared<Signal<InputNumberStringValue>>(
        InputNumberStringValue{Decimal{"9007199254740993.00000000000000001"}});
    gallery_label("gallery.copy.sample.257");
    InputNumber(InputNumberProps{}
                    .value(*exact)
                    .step(Decimal{"1e-17"})
                    .changeOnWheel(true)
                    .onStringChange([state, exact](InputNumberStringValue v) {
                        exact->set(v);
                        if (v) {
                            state->data_feedback.set(v->text());
                        } else {
                            state->data_feedback.set(GalleryMessage{"gallery.feedback.095", {}});
                        }
                    })
                    .layout(LayoutStyle{}.width(dp(420))));
    remember_target(state, "gallery.number.exact");
    gallery_label("gallery.copy.sample.258");
    InputNumber(InputNumberProps{}
                    .defaultValue(1234.5)
                    .step(.1)
                    .changeOnWheel(true)
                    .mode(InputNumberMode::Spinner)
                    .formatter([](String n, InputNumberFormatInfo info) {
                        if (info.user_typing) {
                            return info.input;
                        }
                        auto text = std::string{n.bytes()};
                        const auto dot = text.find('.');
                        const auto end = dot == text.npos ? text.size() : dot;
                        const auto begin = !text.empty() && text.front() == '-' ? 1U : 0U;
                        for (auto i = end; i > begin + 3;) {
                            i -= 3;
                            text.insert(i, 1, ',');
                        }
                        return utf8(text);
                    })
                    .parser([](String n) {
                        auto text = std::string{n.bytes()};
                        std::erase(text, ',');
                        return utf8(text);
                    })
                    .onChange([state](InputNumberValue v) {
                        state->data_feedback.set(
                            v ? GalleryMessage{"gallery.complete.copy.496", {{u8"value", utf8(std::to_string(*v))}}}
                              : GalleryMessage{"gallery.complete.copy.497", {}});
                    })
                    .layout(LayoutStyle{}.width(dp(260))));
    remember_target(state, "gallery.number.formatted");
    gallery_label("gallery.copy.sample.259");
    Text(TextProps{}.content(state->data_feedback));
    Space(
        SpaceProps{}.wrap(true).size(dp(16)), SpaceContent{[state] {
            Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[state] {
                     gallery_label("gallery.copy.sample.260");
                     remember_target(state, "gallery.number.controlled", true);
                     InputNumber(InputNumberProps{}
                                     .value(state->number_value)
                                     .min(0)
                                     .max(10)
                                     .step(0.1)
                                     .precision(2)
                                     .ref(state->number_ref)
                                     .onChange([state](InputNumberValue value) { state->number_value.set(value); })
                                     .onSubmit([state](InputNumberValue value) {
                                         state->data_feedback.set(
                                             value ? GalleryMessage{"gallery.complete.copy.405",
                                                                    {{u8"value", utf8(std::to_string(*value))}}}
                                                   : GalleryMessage{"gallery.complete.copy.406", {}});
                                     }));
                 }});
            Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[state] {
                     gallery_label("gallery.copy.sample.261");
                     remember_target(state, "gallery.number.disabled", true);
                     InputNumber(InputNumberProps{}
                                     .defaultValue(gallery_initial_value<ryn::InputNumberValue>(
                                         "gallery.number.disabled", ryn::InputNumberValue{5}))
                                     .onChange(gallery_value_changed<ryn::InputNumberValue>("gallery.number.disabled"))
                                     .disabled(state->number_disabled)
                                     .size(ControlSize::Small));
                 }});
            Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[state] {
                     gallery_label("gallery.copy.sample.262");
                     remember_target(state, "gallery.number.readonly", true);
                     InputNumber(InputNumberProps{}
                                     .defaultValue(gallery_initial_value<ryn::InputNumberValue>(
                                         "gallery.number.readonly", ryn::InputNumberValue{10}))
                                     .onChange(gallery_value_changed<ryn::InputNumberValue>("gallery.number.readonly"))
                                     .readOnly(true)
                                     .controls(false)
                                     .size(ControlSize::Large));
                 }});
        }});
    Space(SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
              Button(ButtonProps{}.onClick([state] { state->number_disabled.set(!state->number_disabled.get()); }),
                     [] { gallery_label("gallery.copy.sample.263"); });
              gallery_target("gallery.number.toggle-disabled");
              Button(ButtonProps{}.onClick([state] { static_cast<void>(state->number_ref.focus()); }),
                     [] { gallery_label("gallery.copy.sample.264"); });
              gallery_target("gallery.number.focus");
          }});
    state->telemetry.live_samples += 3;
}

void add_data_and_selection_samples(const std::shared_ptr<GalleryState>& state) {
    add_table_samples(state);
    add_pagination_samples(state);
    add_menu_samples(state);
    add_dropdown_samples(state);
    add_input_number_samples(state);
}

void add_collapse_contract_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<bool> reverse{false};
    Signal<bool> ghost{false};
    Signal<bool> lazy{true};
    Signal<CollapseKeys> active{CollapseKeys{u8"draft"}};
    gallery_label("gallery.copy.sample.265");
    Collapse(
        CollapseProps{}.activeKeys(active).ghost(ghost).lazy(lazy).onChange(
            [active](CollapseKeys keys) { active.set(std::move(keys)); }),
        bind([reverse] {
            std::vector<CollapsePanel> panels{
                CollapsePanel{u8"draft", CollapseTitle{[] { gallery_label("gallery.copy.sample.266"); }},
                              CollapseBody{[] {
                                  Input(InputProps{}.defaultValue(u8"编辑后重排，草稿仍保留"));
                                  gallery_target("gallery.collapse.dynamic-draft");
                              }}},
                CollapsePanel{u8"lazy", CollapseTitle{[] { gallery_label("gallery.copy.sample.267"); }},
                              CollapseBody{[] { Input(InputProps{}.defaultValue(u8"首次展开才创建；收起后销毁")); }}}
                    .destroyOnHidden(true)};
            if (reverse.get()) {
                std::reverse(panels.begin(), panels.end());
            }
            return panels;
        }));
    gallery_target("gallery.collapse.dynamic");
    Space(SpaceProps{}.wrap(true), SpaceContent{[=] {
              Button(ButtonProps{}.onClick([reverse] { reverse.set(!reverse.get()); }),
                     [] { gallery_label("gallery.copy.sample.268"); });
              gallery_target("gallery.collapse.reorder");
              Button(ButtonProps{}.onClick([ghost] { ghost.set(!ghost.get()); }),
                     [] { gallery_label("gallery.copy.sample.269"); });
              gallery_target("gallery.collapse.ghost");
              Button(ButtonProps{}.onClick(
                         [active] { active.set(active.get().empty() ? CollapseKeys{u8"draft"} : CollapseKeys{}); }),
                     [] { gallery_label("gallery.copy.sample.270"); });
              gallery_target("gallery.collapse.dynamic-toggle");
          }});
    state->telemetry.live_samples += 1;
}

void add_segmented_contract_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<bool> reverse{false};
    Signal<bool> rtl{true};
    Signal<SegmentedValue> selected{SegmentedValue{1.0}};
    gallery_label("gallery.copy.sample.271");
    Text(TextProps{}.content(bind([selected, tr = useI18n()] {
        return selected.get() == SegmentedValue{1.0} ? tr.translate(gallery_message("gallery.complete.copy.389"))
                                                     : tr.translate(gallery_message("gallery.complete.copy.390"));
    })));
    Segmented(
        SegmentedProps{}
            .value(selected)
            .rtl(rtl)
            .shape(SegmentedShape::Round)
            .block()
            .onValueChange([selected](SegmentedValue value) { selected.set(std::move(value)); }),
        bind([reverse] {
            std::vector<SegmentedOption> options{
                SegmentedOption{u8"one", ryn::String{}}.label(gallery_text("gallery.complete.copy.046")).value(1.0),
                SegmentedOption{u8"two", ryn::String{}}.label(gallery_text("gallery.complete.copy.047")).value(2.0)};
            if (reverse.get()) {
                std::reverse(options.begin(), options.end());
            }
            return options;
        }));
    gallery_target("gallery.segmented.dynamic");
    Space(SpaceProps{}, SpaceContent{[=] {
              Button(ButtonProps{}.onClick([reverse] { reverse.set(!reverse.get()); }),
                     [] { gallery_label("gallery.copy.sample.272"); });
              gallery_target("gallery.segmented.reorder");
              Button(ButtonProps{}.onClick([rtl] { rtl.set(!rtl.get()); }),
                     [] { gallery_label("gallery.copy.sample.273"); });
              gallery_target("gallery.segmented.rtl");
          }});
    state->telemetry.live_samples += 1;
}

void add_descriptions_contract_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<bool> reverse{false};
    Signal<bool> rtl{false};
    gallery_label("gallery.copy.sample.274");
    Descriptions(
        DescriptionsProps{}
            .column(Responsive<int>{1}.at(Breakpoint::Sm, 2).at(Breakpoint::Lg, 3))
            .bordered()
            .rtl(rtl)
            .title(DescriptionsTitle{[] { gallery_label("gallery.copy.sample.275"); }})
            .extra(DescriptionsExtra{[reverse] {
                Button(ButtonProps{}.onClick([reverse] { reverse.set(!reverse.get()); }),
                       [] { gallery_label("gallery.copy.sample.276"); });
                gallery_target("gallery.descriptions.reorder");
            }}),
        bind([reverse] {
            std::vector<DescriptionsItem> items{
                DescriptionsItem{u8"draft", DescriptionsLabel{[] { gallery_label("gallery.copy.sample.277"); }},
                                 DescriptionsContent{[] {
                                     Input(InputProps{}.defaultValue(u8"保留的详情草稿"));
                                     gallery_target("gallery.descriptions.dynamic-draft");
                                 }}},
                DescriptionsItem{u8"state", DescriptionsLabel{[] { gallery_label("gallery.copy.sample.278"); }},
                                 DescriptionsContent{[] { gallery_label("gallery.copy.sample.279"); }}},
                DescriptionsItem{u8"fill", DescriptionsLabel{[] { gallery_label("gallery.copy.sample.280"); }},
                                 DescriptionsContent{[] { gallery_label("gallery.copy.sample.281"); }}}
                    .fill()};
            if (reverse.get()) {
                std::reverse(items.begin(), items.end());
            }
            return items;
        }));
    gallery_target("gallery.descriptions.dynamic");
    Button(ButtonProps{}.onClick([rtl] { rtl.set(!rtl.get()); }), [] { gallery_label("gallery.copy.sample.282"); });
    gallery_target("gallery.descriptions.rtl");
    state->telemetry.live_samples += 1;
}

void add_timeline_contract_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<bool> reverse{false};
    Signal<bool> loading{false};
    Signal<TimelineTone> tone{TimelineTone::Green};
    Signal<TimelineOrientation> orientation{TimelineOrientation::Horizontal};
    Signal<TimelineTitleSpan> span{TimelineTitleSpan::fraction(.25F)};
    gallery_label("gallery.copy.sample.283");
    Text(TextProps{}.content(bind([tone, tr = useI18n()] {
        return tone.get() == TimelineTone::Green ? tr.translate(gallery_message("gallery.complete.copy.391"))
                                                 : tr.translate(gallery_message("gallery.complete.copy.392"));
    })));
    Timeline(
        TimelineProps{}.orientation(orientation).titleSpan(span).variant(TimelineVariant::Filled),
        bind([reverse, loading, tone] {
            std::vector<TimelineItem> items{
                TimelineItem{u8"created", gallery_text("gallery.complete.copy.048")}.title(u8"09:00").tone(tone),
                TimelineItem{u8"draft", u8""}.title(u8"09:30").richContent(TimelineContent{[] {
                    Input(InputProps{}.defaultValue(u8"重排后保留时间轴草稿"));
                    gallery_target("gallery.timeline.dynamic-draft");
                }}),
                TimelineItem{u8"pending", gallery_text("gallery.complete.copy.049")}.title(u8"10:00").loading(loading)};
            if (reverse.get()) {
                std::reverse(items.begin(), items.end());
            }
            return items;
        }));
    gallery_target("gallery.timeline.dynamic");
    Space(
        SpaceProps{}.wrap(true), SpaceContent{[=] {
            Button(ButtonProps{}.onClick([reverse] { reverse.set(!reverse.get()); }),
                   [] { gallery_label("gallery.copy.sample.284"); });
            gallery_target("gallery.timeline.reorder");
            Button(ButtonProps{}.onClick([orientation] {
                orientation.set(orientation.get() == TimelineOrientation::Horizontal ? TimelineOrientation::Vertical
                                                                                     : TimelineOrientation::Horizontal);
            }),
                   [] { gallery_label("gallery.copy.sample.285"); });
            gallery_target("gallery.timeline.orientation");
            Button(ButtonProps{}.onClick([loading] { loading.set(!loading.get()); }),
                   [] { gallery_label("gallery.copy.sample.286"); });
            gallery_target("gallery.timeline.loading-motion");
            Button(ButtonProps{}.onClick([tone] {
                tone.set(tone.get() == TimelineTone::Green ? TimelineTone::Red : TimelineTone::Green);
            }),
                   [] { gallery_label("gallery.copy.sample.287"); });
            gallery_target("gallery.timeline.tone-motion");
            Button(ButtonProps{}.onClick([span] {
                span.set(span.get().logical ? TimelineTitleSpan::fraction(.25F) : TimelineTitleSpan::logicalWidth(180));
            }),
                   [] { gallery_label("gallery.copy.sample.288"); });
            gallery_target("gallery.timeline.title-span");
        }});
    state->telemetry.live_samples += 1;
}

void add_collapse_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.289");
    Text(TextProps{}.content(state->panel_feedback));
    Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)), SpaceContent{[state] {
              Button(
                  ButtonProps{}.onClick([state] { state->collapse_accordion.set(!state->collapse_accordion.get()); }),
                  [] { gallery_label("gallery.copy.sample.290"); });
              gallery_target("gallery.collapse.accordion");
              Button(ButtonProps{}.onClick([state] { state->collapse_disabled.set(!state->collapse_disabled.get()); }),
                     [] { gallery_label("gallery.copy.sample.291"); });
              gallery_target("gallery.collapse.disabled");
              Button(ButtonProps{}.onClick([state] { state->collapse_keys.set({}); }),
                     [] { gallery_label("gallery.copy.sample.292"); });
              gallery_target("gallery.collapse.close");
              Button(ButtonProps{}.onClick([state] {
                  state->collapse_keys.set(CollapseKeys{String{u8"general"}});
                  state->collapse_select_open.set(true);
              }),
                     [] { gallery_label("gallery.copy.sample.293"); });
              gallery_target("gallery.collapse.options");
          }});
    Collapse(
        CollapseProps{}
            .activeKeys(state->collapse_keys)
            .accordion(state->collapse_accordion)
            .onChange([state](CollapseKeys keys) { state->collapse_keys.set(std::move(keys)); }),
        {CollapsePanel{
             u8"general", CollapseTitle{[] { gallery_label("gallery.copy.sample.294"); }}, CollapseBody{[state] {
                 Flex(
                     FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state] {
                         Input(InputProps{}
                                   .defaultValue(gallery_initial_text("gallery.collapse.draft", u8"保留的设置草稿"))
                                   .onChange(gallery_value_changed<ryn::String>("gallery.collapse.draft")));
                         gallery_target("gallery.collapse.draft");
                         Select(
                             gallery_select_default(
                                 "gallery.collapse.selection",
                                 SelectProps{}
                                     .options(gallery_option_labels<ryn::SelectOption>(
                                         std::vector<ryn::SelectOption>{{u8"draft", u8"草稿"}, {u8"service", u8"服务"}},
                                         {{u8"draft", "gallery.complete.copy.109"},
                                          {u8"service", "gallery.complete.copy.110"}}))

                                     .open(state->collapse_select_open)
                                     .onOpenChange([state](bool open) { state->collapse_select_open.set(open); }),
                                 ryn::SelectValue{u8"draft"})
                                 .onChange(gallery_value_changed<ryn::SelectSelection>("gallery.collapse.selection")));
                     }});
             }}}
             .extra(CollapseExtra{[state] {
                 Button(ButtonProps{}.onClick(
                            [state] { state->panel_feedback.set(GalleryMessage{"gallery.feedback.096", {}}); }),
                        [] { gallery_label("gallery.copy.sample.295"); });
                 gallery_target("gallery.collapse.help");
             }}),
         CollapsePanel{u8"advanced", CollapseTitle{[] { gallery_label("gallery.copy.sample.296"); }}, CollapseBody{[] {
                           Input(InputProps{}
                                     .defaultValue(gallery_initial_text("gallery.collapse.advanced", u8"高级草稿"))
                                     .onChange(gallery_value_changed<ryn::String>("gallery.collapse.advanced")));
                           gallery_target("gallery.collapse.advanced");
                       }}}
             .disabled(state->collapse_disabled)});
    state->telemetry.live_samples += 1;
    add_collapse_contract_samples(state);
}

void add_segmented_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.297");
    Text(TextProps{}.content(bind([state] {
        return state->i18n.translate(gallery_message("gallery.snapshot.001"),
                                     MessageArgs{{u8"v0", utf8(std::string{state->segment_value.get().bytes()})}});
    })));
    Segmented(SegmentedProps{}
                  .value(state->segment_value)
                  .block()
                  .layout(LayoutStyle{}.width(state->document_width))
                  .onChange([state](String value) { state->segment_value.set(std::move(value)); }),
              {SegmentedOption{u8"day", ryn::String{}}
                   .label(gallery_text("gallery.complete.copy.050"))
                   .label(state->segment_label)
                   .icon(SegmentedIcon{[] { Icon(IconProps{}.name(IconName::SearchOutlined)); }}),
               SegmentedOption{u8"week", ryn::String{}}
                   .label(gallery_text("gallery.complete.copy.051"))
                   .disabled(state->segment_disabled),
               SegmentedOption{u8"month", String{}}.label(gallery_text("gallery.complete.copy.437"))});
    gallery_label("gallery.copy.sample.298");
    Segmented(SegmentedProps{}
                  .value(state->segment_value)
                  .vertical()
                  .size(ControlSize::Small)
                  .layout(LayoutStyle{}.width(dp(240)))
                  .onChange([state](String value) { state->segment_value.set(std::move(value)); }),
              {SegmentedOption{u8"day", String{}}.label(gallery_text("gallery.complete.copy.438")),
               SegmentedOption{u8"week", ryn::String{}}
                   .label(gallery_text("gallery.complete.copy.052"))
                   .disabled(state->segment_disabled),
               SegmentedOption{u8"month", String{}}.label(gallery_text("gallery.complete.copy.437"))});
    gallery_label("gallery.copy.sample.299");
    Segmented(SegmentedProps{}.size(ControlSize::Large),
              {SegmentedOption{u8"list", ryn::String{}}
                   .label(gallery_text("gallery.complete.copy.053"))
                   .content(SegmentedLabel{[] {
                       Flex(FlexProps{}.gap(dp(6)).align(FlexAlign::Center), FlexContent{[] {
                                Icon(IconProps{}.name(IconName::SearchOutlined));
                                gallery_label("gallery.copy.sample.300");
                            }});
                   }}),
               SegmentedOption{u8"detail", String{}}.label(gallery_text("gallery.complete.copy.439"))});
    Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)), SpaceContent{[state] {
              Button(ButtonProps{}.onClick([state] {
                  state->segment_label.set(state->segment_label.is("gallery.complete.copy.124")
                                               ? GalleryMessage{"gallery.complete.copy.135", {}}
                                               : GalleryMessage{"gallery.complete.copy.124", {}});
              }),
                     [] { gallery_label("gallery.copy.sample.301"); });
              gallery_target("gallery.segmented.label");
              Button(ButtonProps{}.onClick([state] { state->segment_disabled.set(!state->segment_disabled.get()); }),
                     [] { gallery_label("gallery.copy.sample.302"); });
              gallery_target("gallery.segmented.disabled");
          }});
    state->telemetry.live_samples += 4;
    state->telemetry.live_samples += 3;
    add_segmented_contract_samples(state);
}

void add_panel_and_selection_samples(const std::shared_ptr<GalleryState>& state) {
    const auto before = state->telemetry.live_samples;
    add_collapse_samples(state);
    add_segmented_samples(state);
    state->telemetry.live_samples = before + 5; // Historical combined fixture counter.
}

void add_empty_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.303");
    remember_target(state, "gallery.empty.default", true);
    Empty(EmptyProps{}.description(state->empty_description).image(state->empty_image),
          {.extra = EmptyExtra{[state] {
               Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                   state->empty_description.set(GalleryMessage{"gallery.complete.copy.141", {}});
               }),
                      [] { gallery_label("gallery.copy.sample.304"); });
               gallery_target("gallery.empty.create");
           }}});
    Button(ButtonProps{}.onClick([state] {
        state->empty_image.set(state->empty_image.get() == EmptyImage::None ? EmptyImage::Default : EmptyImage::None);
    }),
           [] { gallery_label("gallery.copy.sample.305"); });
    gallery_target("gallery.empty.image");
    gallery_label("gallery.copy.sample.306");
    Empty(EmptyProps{}.image(EmptyImage::Compact).description(gallery_text("gallery.copy.property.039")));
    gallery_label("gallery.copy.sample.307");
    Empty(EmptyProps{}.description(gallery_text("gallery.copy.property.040")),
          {.image = EmptyImageContent{[] {
               Icon(IconProps{}
                        .name(IconName::SearchOutlined)
                        .layout(LayoutStyle{}.width(dp(100)).margin_left(dp(50)).margin_right(dp(50))));
           }}});
    state->telemetry.live_samples += 3;
}

void add_result_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<std::shared_ptr<const ResultTitle>> rich_title{nullptr};
    gallery_label("gallery.copy.sample.308");
    remember_target(state, "gallery.result.controlled", true);
    Text(TextProps{}.content(state->result_feedback));
    Button(ButtonProps{}.onClick([rich_title]() mutable {
        rich_title.set(rich_title.get() ? nullptr : std::make_shared<const ResultTitle>(ResultTitle{[] {
            Flex(FlexProps{}.gap(dp(8)).align(FlexAlign::Center), FlexContent{[] {
                     Icon(IconProps{}.name(IconName::CheckCircleFilled));
                     gallery_label("gallery.copy.sample.309");
                 }});
        }}));
    }),
           [] { gallery_label("gallery.copy.sample.310"); });
    gallery_target("gallery.result.topology");
    Result(ResultProps{}
               .status(state->result_status)
               .title(state->result_title)
               .titleRender(rich_title)
               .subtitle(gallery_text("gallery.copy.property.041")),
           {.content = ResultContent{[] {
                gallery_label("gallery.copy.sample.311");
                Input(InputProps{}
                          .defaultValue(gallery_initial_text("gallery.result.draft", u8"保留的结果详情草稿"))
                          .onChange(gallery_value_changed<ryn::String>("gallery.result.draft"))
                          .layout(LayoutStyle{}.width(dp(280))));
                gallery_target("gallery.result.draft");
            }},
            .extra = ResultExtra{[state] {
                Button(ButtonProps{}.onClick([state] {
                    constexpr std::array statuses{ResultStatus{Status::Success},
                                                  ResultStatus{Status::Warning},
                                                  ResultStatus{Status::Error},
                                                  ResultStatus{Status::Default},
                                                  ResultStatus{ResultException::Forbidden},
                                                  ResultStatus{ResultException::NotFound},
                                                  ResultStatus{ResultException::ServerError}};
                    constexpr std::array titles{"gallery.complete.copy.280", "gallery.complete.copy.281",
                                                "gallery.complete.copy.282", "gallery.complete.copy.283",
                                                "gallery.complete.copy.284", "gallery.complete.copy.285",
                                                "gallery.complete.copy.286"};
                    state->result_status_step = (state->result_status_step + 1) % statuses.size();
                    state->result_status.set(statuses[state->result_status_step]);
                    state->result_title.set(GalleryMessage{titles[state->result_status_step], {}});
                }),
                       [] { gallery_label("gallery.copy.sample.312"); });
                gallery_target("gallery.result.status");
                Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                    state->result_feedback.set(GalleryMessage{"gallery.feedback.097", {}});
                }),
                       [] { gallery_label("gallery.copy.sample.313"); });
                gallery_target("gallery.result.action");
            }}});
    gallery_label("gallery.copy.sample.314");
    Result(ResultProps{}
               .status(ResultException::NotFound)
               .title(gallery_text("gallery.copy.property.042"))
               .subtitle(gallery_text("gallery.copy.property.043")));
    gallery_label("gallery.copy.sample.315");
    Result(ResultProps{}
               .title(gallery_text("gallery.copy.property.044"))
               .subtitle(gallery_text("gallery.copy.property.045")),
           {.icon = ResultIcon{[] {
                Flex(FlexProps{}.gap(dp(8)).align(FlexAlign::Center), FlexContent{[] {
                         Icon(IconProps{}.name(IconName::CheckCircleFilled));
                         gallery_label("gallery.complete.copy.194");
                     }});
            }}});
    state->telemetry.live_samples += 3;
}

void add_empty_and_result_samples(const std::shared_ptr<GalleryState>& state) {
    const auto before = state->telemetry.live_samples;
    add_empty_samples(state);
    add_result_samples(state);
    state->telemetry.live_samples = before + 6; // Historical combined fixture counter.
}

void add_form_layout_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    FormRef form;
    FormField<String> name;
    FormField<String> custom{String{u8"custom initial"}};
    Signal<std::size_t> submissions{0};
    FormField<InputNumberStringValue> amount{Decimal{"9007199254740993.0001"}};
    name.required();
    FormPresentation initial;
    initial.layout = FormLayout::Horizontal;
    Signal<FormPresentation> presentation{initial};
    GalleryFeedback result{GalleryMessage{"gallery.complete.copy.151", {}}, useI18n()};
    Signal<float> offset{0};
    gallery_label("gallery.copy.sample.316");
    Text(TextProps{}.content(result));
    Form(FormProps{}
             .ref(form)
             .presentation(presentation)
             .scrollToFirstError()
             .scrollTarget([offset](FormScrollRequest request, auto complete) {
                 offset.set(std::max(0.0F, offset.get() + request.control.bounds.y - request.control.clip.y - 8));
                 complete(true); // Form waits for the next synchronized native geometry before focusing.
             })
             .onFinish([result, submissions](FormResult submitted) {
                 submissions.set(submissions.get() + 1);
                 const auto name = std::get<String>(submitted.values[0].value);
                 const auto amount = std::get<InputNumberStringValue>(submitted.values[1].value);
                 result.set(GalleryMessage{"gallery.complete.copy.266",
                                           {{u8"name", name},
                                            {u8"amount", amount ? amount->text() : String{u8"empty"}},
                                            {u8"custom", std::get<String>(submitted.values[2].value)},
                                            {u8"count", static_cast<std::uint64_t>(submissions.get())}}});
             })
             .onFinishFailed([result](FormResult submitted) {
                 result.set(GalleryMessage{"gallery.complete.copy.155",
                                           {{u8"count", static_cast<std::uint64_t>(submitted.errors.size())}}});
             }),
         FormContent{[=] {
             ReferenceScrollViewport(
                 offset, state->cell_width, 180, ReferenceSurfaceContent{[=] {
                     Text(TextProps{}
                              .content(gallery_text("gallery.copy.property.046"))
                              .layout(LayoutStyle{}.height(dp(200))));
                     FormItem(FormItemProps{}
                                  .field(String{u8"name"}, name)
                                  .label(gallery_text("gallery.copy.property.047"))
                                  .tooltip(gallery_text("gallery.copy.property.048"))
                                  .extra(gallery_text("gallery.complete.copy.272"))
                                  .hasFeedback(),
                              FormItemContent{[=] {
                                  Input(InputProps{}.placeholder(gallery_text("gallery.copy.property.049")));
                                  remember_target(state, "gallery.form.layouts.name");
                              }});
                     FormItem(FormItemProps{}
                                  .field(String{u8"amount"}, amount)
                                  .label(gallery_text("gallery.copy.property.050")),
                              FormItemContent{[=] {
                                  InputNumber(InputNumberProps{});
                                  remember_target(state, "gallery.form.layouts.amount");
                              }});
                     FormItem(FormItemProps{}
                                  .field(String{u8"custom"}, custom)
                                  .label(gallery_text("gallery.complete.copy.273")),
                              FormItemContent{[=] {
                                  FormControl(FormControlAdapter<String>{[=](FormControlBinding<String> binding) {
                                      InputRef ref;
                                      Input(InputProps{}
                                                .value(binding.value())
                                                .onChange(binding.onChange())
                                                .disabled(binding.disabled())
                                                .readOnly(binding.readOnly())
                                                .status(binding.status())
                                                .onBlur(binding.onBlur())
                                                .ref(ref));
                                      binding.control(ref);
                                      remember_target(state, "gallery.form.layouts.custom");
                                  }});
                              }});
                 }});
         }});
    Button(ButtonProps{}.onClick([=] {
        auto next = presentation.get();
        next.layout = next.layout == FormLayout::Horizontal ? FormLayout::Vertical
                      : next.layout == FormLayout::Vertical ? FormLayout::Inline
                                                            : FormLayout::Horizontal;
        presentation.set(next);
    }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.317"); }});
    remember_target(state, "gallery.form.layouts.toggle");
    Button(ButtonProps{}.onClick([=] {
        auto next = presentation.get();
        next.direction =
            next.direction == TextDirection::RightToLeft ? TextDirection::LeftToRight : TextDirection::RightToLeft;
        presentation.set(next);
    }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.318"); }});
    remember_target(state, "gallery.form.layouts.rtl");
    Button(ButtonProps{}.onClick([=] { static_cast<void>(form.submit()); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.319"); }});
    remember_target(state, "gallery.form.layouts.submit");
    Button(ButtonProps{}.onClick([=] { static_cast<void>(form.scrollToField(FormNamePath{u8"name"})); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.320"); }});
    remember_target(state, "gallery.form.layouts.reveal");
    Button(ButtonProps{}.onClick([=] { static_cast<void>(form.scrollToField(FormNamePath{u8"amount"})); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.321"); }});
    remember_target(state, "gallery.form.layouts.reveal-amount");
    Button(ButtonProps{}.onClick([=] { static_cast<void>(form.scrollToField(FormNamePath{u8"custom"})); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.322"); }});
    remember_target(state, "gallery.form.layouts.reveal-custom");
    Button(ButtonProps{}.onClick([=] { static_cast<void>(form.reset()); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.323"); }});
    remember_target(state, "gallery.form.layouts.reset");
    ++state->telemetry.live_samples;
}

void add_form_list_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    FormRef form;
    FormListRef people;
    GalleryFeedback result{GalleryMessage{"gallery.complete.copy.152", {}}, useI18n()};
    gallery_label("gallery.copy.sample.324");
    Text(TextProps{}.content(result));
    Form(FormProps{}
             .ref(form)
             .onFinish([result](FormResult submitted) {
                 std::string text;
                 for (const auto& value : submitted.values) {
                     for (const auto& segment : value.path.segments()) {
                         std::visit(
                             [&](const auto& part) {
                                 if constexpr (std::same_as<std::decay_t<decltype(part)>, String>) {
                                     text += part.bytes();
                                 } else {
                                     text += std::to_string(part);
                                 }
                                 text += '/';
                             },
                             segment.value());
                     }
                     if (const auto* string = std::get_if<String>(&value.value)) {
                         text += string->bytes();
                     }
                     text += "；";
                 }
                 result.set(GalleryMessage{"gallery.complete.copy.154", {{u8"values", utf8(text)}}});
             })
             .onFinishFailed([result](FormResult submitted) {
                 result.set(GalleryMessage{"gallery.complete.copy.156",
                                           {{u8"count", static_cast<std::uint64_t>(submitted.errors.size())}}});
             }),
         FormContent{[=] {
             FormList(
                 FormListProps{}
                     .name(FormNamePath{u8"people"})
                     .ref(people)
                     .initialValue({{}, {}})
                     .rules({[](const FormData::Array& rows, const FormValidationContext&) {
                         if (rows.empty()) {
                             return std::vector<FormIssue>{{translate(LocaleCatalog::builtin(), locales::en_US,
                                                                      gallery_message("gallery.complete.copy.275")),
                                                            false}};
                         }
                         return std::vector<FormIssue>{};
                     }}),
                 [=](FormListField row) {
                     FormField<String> name;
                     FormItem(FormItemProps{}.field(FormNamePath{u8"name"}, name).label(bind([row, tr = useI18n()] {
                         return tr.translate(gallery_message("gallery.complete.copy.274"),
                                             {{u8"index", static_cast<std::uint64_t>(row.index() + 1)}});
                     })),
                              FormItemContent{[=] {
                                  Input(InputProps{});
                                  remember_target(state, "gallery.form.list.name." + std::to_string(row.key()), true);
                              }});
                     FormList(FormListProps{}.name(FormNamePath{u8"notes"}).initialValue({{}}), [=](FormListField) {
                         FormField<String> note;
                         FormItem(FormItemProps{}.field(FormNamePath{u8"text"}, note).noStyle(), FormItemContent{[=] {
                                      TextArea(TextAreaProps{}
                                                   .rows(std::size_t{1})
                                                   .placeholder(gallery_text("gallery.copy.property.051")));
                                      remember_target(state, "gallery.form.list.note." + std::to_string(row.key()),
                                                      true);
                                  }});
                     });
                     Button(ButtonProps{}.onClick([=] {
                         if (row.index() > 0) {
                             static_cast<void>(people.move(row.index(), row.index() - 1));
                         }
                     }),
                            ButtonContent{[] { gallery_label("gallery.copy.sample.325"); }});
                     remember_target(state, "gallery.form.list.move." + std::to_string(row.key()), true);
                     Button(ButtonProps{}.onClick([=] { static_cast<void>(people.remove({row.index()})); }),
                            ButtonContent{[] { gallery_label("gallery.copy.sample.326"); }});
                     remember_target(state, "gallery.form.list.remove." + std::to_string(row.key()), true);
                 });
             FormErrorList(FormErrorListProps{}.errors(bind([people, tr = useI18n()] {
                 auto errors = people.meta().errors;
                 for (auto& error : errors) {
                     if (people.errorSource() == FormIssueSource::RuleValidator) {
                         error = tr.translate(gallery_message("gallery.complete.copy.275"));
                     }
                 }
                 return errors;
             })));
         }});
    Button(ButtonProps{}.onClick([=] { static_cast<void>(people.add()); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.327"); }});
    remember_target(state, "gallery.form.list.add");
    Button(ButtonProps{}.onClick([=] { static_cast<void>(form.submit()); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.328"); }});
    remember_target(state, "gallery.form.list.submit");
    Button(ButtonProps{}.onClick([=] { static_cast<void>(form.reset()); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.329"); }});
    remember_target(state, "gallery.form.list.reset");
    ++state->telemetry.live_samples;
}

void add_form_async_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    FormRef form;
    FormField<String> name;
    FormField<String> confirmation;
    GalleryFeedback result{GalleryMessage{"gallery.complete.copy.153", {}}, useI18n()};
    Signal<std::optional<GalleryMessage>> name_issue{std::optional<GalleryMessage>{}};
    Signal<std::optional<GalleryMessage>> confirmation_issue{std::optional<GalleryMessage>{}};
    const auto posts = std::make_shared<std::vector<std::function<void()>>>();
    name.required().asyncValidator(
        [name_issue](String value, FormValidationContext, FormValidationComplete complete) {
            std::vector<FormIssue> issues;
            name_issue.set(std::nullopt);
            if (value == String{u8"taken"}) {
                issues.push_back(
                    {gallery_validation_issue(name_issue, GalleryMessage{"gallery.complete.copy.276", {}}), false});
            }
            complete(std::move(issues));
            return AsyncOptionsCancel{};
        },
        [posts](auto callback) { posts->push_back(std::move(callback)); });
    FormRule matches;
    matches.validator = [confirmation_issue](const FormValue& value, const FormValidationContext& context) {
        confirmation_issue.set(std::nullopt);
        if (context.get(FormNamePath{u8"name"}) != value) {
            return std::vector<FormIssue>{
                {gallery_validation_issue(confirmation_issue, GalleryMessage{"gallery.complete.copy.277", {}}), false}};
        }
        return std::vector<FormIssue>{};
    };
    confirmation.rules({matches}).validation(FormValidateTrigger::Change, 150, {FormNamePath{u8"name"}});
    gallery_label("gallery.copy.sample.330");
    Text(TextProps{}.content(result));
    Text(TextProps{}.content(bind([name, tr = useI18n()] {
        return tr.translate(
            gallery_message(name.meta().validating ? "gallery.complete.copy.278" : "gallery.complete.copy.279"));
    })));
    Form(FormProps{}
             .ref(form)
             .onFinish([result](FormResult submitted) {
                 result.set(GalleryMessage{"gallery.complete.copy.267",
                                           {{u8"name", std::get<String>(submitted.values[0].value)},
                                            {u8"confirmation", std::get<String>(submitted.values[1].value)}}});
             })
             .onFinishFailed([result](FormResult submitted) {
                 result.set(submitted.out_of_date
                                ? GalleryMessage{"gallery.complete.copy.268", {}}
                                : GalleryMessage{"gallery.complete.copy.269",
                                                 {{u8"count", static_cast<std::uint64_t>(submitted.errors.size())}}});
             }),
         FormContent{[=] {
             gallery_validated_item(FormItemProps{}
                                        .field(String{u8"name"}, name)
                                        .label(gallery_text("gallery.copy.property.052"))
                                        .hasFeedback(),
                                    name, name_issue, gallery_text("gallery.copy.property.052"),
                                    GalleryFeedback{String{}, useI18n()}, FormItemContent{[=] {
                                        Input(InputProps{});
                                        remember_target(state, "gallery.form.async.name");
                                    }},
                                    FormIssueSource::AsyncValidator);
             gallery_validated_item(FormItemProps{}
                                        .field(String{u8"confirmation"}, confirmation)
                                        .label(gallery_text("gallery.copy.property.053")),
                                    confirmation, confirmation_issue, gallery_text("gallery.copy.property.053"),
                                    GalleryFeedback{String{}, useI18n()}, FormItemContent{[=] {
                                        Input(InputProps{});
                                        remember_target(state, "gallery.form.async.confirmation");
                                    }},
                                    FormIssueSource::RuleValidator);
         }});
    Button(ButtonProps{}.onClick([=] { static_cast<void>(form.submitAsync()); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.331"); }});
    remember_target(state, "gallery.form.async.submit");
    Button(ButtonProps{}.onClick([posts] {
        auto pending = std::exchange(*posts, {});
        for (auto& callback : pending) {
            callback();
        }
    }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.332"); }});
    remember_target(state, "gallery.form.async.deliver");
    Button(ButtonProps{}.onClick([=] { static_cast<void>(form.reset()); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.333"); }});
    remember_target(state, "gallery.form.async.reset");
    ++state->telemetry.live_samples;
}

void add_form_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    const auto form_width = bind([state] { return dp(state->document_width.get().value() - 2); });
    Text(TextProps{}.content(gallery_text("gallery.copy.property.054")).layout(LayoutStyle{}.margin_left(dp(1))));
    remember_target(state, "gallery.form.workflow", true);
    Text(
        TextProps{}
            .content(bind([state] {
                return state->i18n.translate(
                    gallery_message("gallery.snapshot.002"),
                    MessageArgs{
                        {u8"v0", utf8(std::string{state->form_feedback.get().bytes()})},
                        {u8"v1",
                         utf8((
                             state->form_disabled.get()
                                 ? std::string{state->i18n.translate(gallery_message("gallery.state.disabled")).bytes()}
                                 : std::string{state->i18n.translate(gallery_message("gallery.state.enabled"))
                                                   .bytes()}))},
                        {u8"v2",
                         utf8((
                             state->form_read_only.get()
                                 ? std::string{state->i18n.translate(gallery_message("gallery.state.readonly")).bytes()}
                                 : std::string{
                                       state->i18n.translate(gallery_message("gallery.state.editable")).bytes()}))}});
            }))
            .layout(LayoutStyle{}.margin_left(dp(1))));
    remember_target(state, "gallery.form.status", true);
    Form(FormProps{}
             .ref(state->form_ref)
             .disabled(state->form_disabled)
             .readOnly(state->form_read_only)
             .layout(LayoutStyle{}.width(form_width).margin_left(dp(1)).margin_right(dp(1)))
             .onFinish([state](FormResult result) {
                 std::string values;
                 for (const auto& field : result.values) {
                     values += " · " + std::string{field.name.bytes()} + "=";
                     std::visit(
                         [&values](const auto& value) {
                             using T = std::decay_t<decltype(value)>;
                             if constexpr (std::same_as<T, String>) {
                                 values += value.bytes();
                             } else if constexpr (std::same_as<T, InputNumberValue>) {
                                 if (value) {
                                     std::array<char, 64> characters{};
                                     const auto formatted = std::to_chars(
                                         characters.data(), characters.data() + characters.size(), *value);
                                     if (formatted.ec != std::errc{}) {
                                         throw std::runtime_error("Form Gallery numeric feedback formatting failed");
                                     }
                                     values.append(characters.data(), formatted.ptr);
                                 } else {
                                     values += "∅";
                                 }
                             } else if constexpr (std::same_as<T, bool>) {
                                 values += value ? "true" : "false";
                             } else if constexpr (std::same_as<T, SelectSelection>) {
                                 if (value) {
                                     std::visit(
                                         [&values](const auto& selected) {
                                             if constexpr (std::same_as<std::decay_t<decltype(selected)>, String>) {
                                                 values += selected.bytes();
                                             } else {
                                                 values += std::to_string(selected);
                                             }
                                         },
                                         value->storage());
                                 }
                             } else if constexpr (std::same_as<T, InputNumberStringValue>) {
                                 values += value ? std::string{value->text().bytes()} : "∅";
                             } else if constexpr (std::same_as<T, SelectValues>) {
                                 values += std::to_string(value.size()) + "×";
                             }
                         },
                         field.value);
                 }
                 state->form_feedback.set(GalleryMessage{"gallery.complete.copy.150", {{u8"values", utf8(values)}}});
             })
             .onFinishFailed([state](FormResult result) {
                 state->form_feedback.set(
                     GalleryMessage{"gallery.event.018", {{u8"v0", static_cast<std::uint64_t>(result.errors.size())}}});
             }),
         FormContent{[state, form_width] {
             FormItem(FormItemProps{}
                          .field(u8"name", state->form_name)
                          .label(gallery_text("gallery.copy.property.055"))
                          .help(state->form_help)
                          .control(state->form_name_ref),
                      FormItemContent{[state, form_width] {
                          Input(InputProps{}
                                    .value(state->form_name.value())
                                    .onChange(state->form_name.onChange())
                                    .status(state->form_name.status())
                                    .disabled(state->form_name.disabled())
                                    .readOnly(state->form_name.readOnly())
                                    .ref(state->form_name_ref)
                                    .layout(LayoutStyle{}.width(form_width)));
                          remember_target(state, "gallery.form.name");
                      }});
             FormItem(FormItemProps{}
                          .field(u8"role", state->form_role)
                          .label(gallery_text("gallery.copy.property.056"))
                          .control(state->form_role_ref),
                      FormItemContent{[state, form_width] {
                          Select(SelectProps{}
                                     .options(gallery_option_labels<ryn::SelectOption>(
                                         std::vector<ryn::SelectOption>{{u8"operator", u8"操作员"},
                                                                        {u8"reviewer", u8"审核员"}},
                                         {{u8"operator", "gallery.complete.copy.111"},
                                          {u8"reviewer", "gallery.complete.copy.112"}}))
                                     .value(state->form_role.value())
                                     .onChange(state->form_role.onChange())
                                     .status(state->form_role.status())
                                     .disabled(state->form_role.uneditable())
                                     .ref(state->form_role_ref)
                                     .layout(LayoutStyle{}.width(form_width)));
                          remember_target(state, "gallery.form.role");
                      }});
             gallery_validated_item(FormItemProps{}
                                        .field(u8"count", state->form_count)
                                        .label(gallery_text("gallery.copy.property.057"))
                                        .help(gallery_text("gallery.copy.property.058"))
                                        .control(state->form_count_ref),
                                    state->form_count, state->count_issue, gallery_text("gallery.copy.property.057"),
                                    state->form_help, FormItemContent{[state, form_width] {
                                        InputNumber(InputNumberProps{}
                                                        .value(state->form_count.value())
                                                        .onChange(state->form_count.onChange())
                                                        .status(state->form_count.status())
                                                        .disabled(state->form_count.disabled())
                                                        .readOnly(state->form_count.readOnly())
                                                        .ref(state->form_count_ref)
                                                        .layout(LayoutStyle{}.width(form_width)));
                                        gallery_target("gallery.form.count");
                                    }});
             FormItem(FormItemProps{}
                          .field(u8"consent", state->form_consent)
                          .label(gallery_text("gallery.copy.property.059"))
                          .control(state->form_consent_ref),
                      FormItemContent{[state] {
                          Checkbox(CheckboxProps{}
                                       .checked(state->form_consent.value())
                                       .onChange(state->form_consent.onChange())
                                       .disabled(state->form_consent.uneditable())
                                       .ref(state->form_consent_ref),
                                   CheckboxLabel{[] { gallery_label("gallery.copy.sample.334"); }});
                          remember_target(state, "gallery.form.consent");
                      }});
         }});
    // These actions stay reachable when Form disables its registered controls.
    Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)), SpaceContent{[state] {
              Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                  if (!state->form_ref.submit()) {
                      state->form_feedback.set(GalleryMessage{"gallery.feedback.098", {}});
                  }
              }),
                     [] { gallery_label("gallery.copy.sample.335"); });
              gallery_target("gallery.form.submit");
              Button(ButtonProps{}.onClick([state] {
                  if (const auto result = state->form_ref.validate()) {
                      state->form_feedback.set(result->valid() ? GalleryMessage{"gallery.feedback.099", {}}
                                                               : GalleryMessage{"gallery.feedback.100", {}});
                  }
              }),
                     [] { gallery_label("gallery.copy.sample.336"); });
              gallery_target("gallery.form.validate");
              Button(ButtonProps{}.onClick([state] {
                  if (state->form_ref.reset()) {
                      state->form_feedback.set(GalleryMessage{"gallery.feedback.101", {}});
                  }
              }),
                     [] { gallery_label("gallery.copy.sample.337"); });
              gallery_target("gallery.form.reset");
              Button(ButtonProps{}.onClick([state] { state->form_disabled.set(!state->form_disabled.get()); }),
                     [] { gallery_label("gallery.copy.sample.338"); });
              gallery_target("gallery.form.disabled");
              Button(ButtonProps{}.onClick([state] { state->form_read_only.set(!state->form_read_only.get()); }),
                     [] { gallery_label("gallery.copy.sample.339"); });
              gallery_target("gallery.form.readonly");
              Button(ButtonProps{}.onClick([state] {
                  state->form_help.set(state->form_help.is("gallery.complete.copy.127")
                                           ? GalleryMessage{"gallery.complete.copy.136", {}}
                                           : GalleryMessage{"gallery.complete.copy.127", {}});
              }),
                     [] { gallery_label("gallery.copy.sample.340"); });
              gallery_target("gallery.form.help");
          }});
    state->telemetry.live_samples += 1;
}

void add_date_picker_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(8))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(gallery_text("gallery.copy.property.060"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.date-picker.controlled", true);
             Text(TextProps{}
                      .content(bind([state] {
                          const auto value = state->date_value.get();
                          return value ? format_iso_date(*value)
                                       : state->i18n.translate(gallery_message("gallery.complete.copy.400"));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.date-picker.value", true);
             DatePicker(DatePickerProps{}
                            .value(state->date_value)
                            .open(state->date_open)
                            .onChange([state](DatePickerValue value) { state->date_value.set(value); })
                            .onOpenChange([state](bool value) { state->date_open.set(value); })
                            .defaultPanelDate(CivilDate{2026, 10, 1})
                            .minDate(CivilDate{2026, 10, 5})
                            .maxDate(CivilDate{2026, 11, 25})
                            .disabledDate([](CivilDate date) { return date.day() == 13; })
                            .allowClear(true)
                            .status(InputStatus::Default)
                            .disabled(false)
                            .inputReadOnly(false)
                            .ref(state->date_ref)
                            .layout(LayoutStyle{}.width(dp(280))),
                        DatePickerFooter{[] {
                            Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[] {
                                     gallery_label("gallery.copy.sample.341");
                                     Input(InputProps{}
                                               .defaultValue(
                                                   gallery_initial_text("gallery.date-picker.note", u8"保留的日期备注"))
                                               .onChange(gallery_value_changed<ryn::String>("gallery.date-picker.note"))
                                               .layout(LayoutStyle{}.width(dp(220))));
                                     gallery_target("gallery.date-picker.note");
                                 }});
                        }});
             Button(ButtonProps{}.onClick([state] {
                 state->date_value.set(CivilDate{2026, 10, 12});
                 state->date_open.set(false);
             }),
                    [] { gallery_label("gallery.copy.sample.342"); });
             gallery_target("gallery.date-picker.restore");
         }});
    Text(TextProps{}.content(gallery_text("gallery.copy.property.061")).layout(LayoutStyle{}.margin_left(dp(1))));
    Text(
        TextProps{}
            .content(bind([state] {
                return state->i18n.translate(
                    gallery_message("gallery.snapshot.003"),
                    MessageArgs{
                        {u8"v0", utf8(std::string{state->date_feedback.get().bytes()})},
                        {u8"v1",
                         utf8((
                             state->date_form_disabled.get()
                                 ? std::string{state->i18n.translate(gallery_message("gallery.state.disabled")).bytes()}
                                 : std::string{state->i18n.translate(gallery_message("gallery.state.enabled"))
                                                   .bytes()}))},
                        {u8"v2",
                         utf8((
                             state->date_form_read_only.get()
                                 ? std::string{state->i18n.translate(gallery_message("gallery.state.readonly")).bytes()}
                                 : std::string{
                                       state->i18n.translate(gallery_message("gallery.state.editable")).bytes()}))}});
            }))
            .layout(LayoutStyle{}.margin_left(dp(1))));
    remember_target(state, "gallery.date-picker.status", true);
    Form(FormProps{}
             .ref(state->date_form_ref)
             .disabled(state->date_form_disabled)
             .readOnly(state->date_form_read_only)
             .layout(LayoutStyle{}
                         .width(bind([state] { return dp(state->document_width.get().value() - 2); }))
                         .margin_left(dp(1))
                         .margin_right(dp(1)))
             .onFinish([state](FormResult result) {
                 state->date_feedback.set(GalleryMessage{
                     "gallery.event.019",
                     {{u8"v0", utf8(std::string{std::get<String>(result.values.front().value).bytes()})}}});
             })
             .onFinishFailed(
                 [state](FormResult) { state->date_feedback.set(GalleryMessage{"gallery.feedback.102", {}}); }),
         FormContent{[state] {
             gallery_validated_item(FormItemProps{}
                                        .field(u8"date", state->date_field)
                                        .label(gallery_text("gallery.copy.property.062"))
                                        .help(state->date_help)
                                        .control(state->date_field_ref),
                                    state->date_field, state->date_issue, gallery_text("gallery.copy.property.062"),
                                    state->date_help, FormItemContent{[state] {
                                        // Capture the registration's onChange, not a later field generation.
                                        const auto change = state->date_field.onChange();
                                        DatePicker(DatePickerProps{}
                                                       .value(bind([field = state->date_field] {
                                                           return parse_iso_date(field.get().bytes());
                                                       }))
                                                       .onChange([change](DatePickerValue value) {
                                                           change(value ? format_iso_date(*value) : String{});
                                                       })
                                                       .defaultPanelDate(CivilDate{2026, 10, 1})
                                                       .minDate(CivilDate{2026, 10, 5})
                                                       .maxDate(CivilDate{2026, 11, 25})
                                                       .disabledDate([](CivilDate date) { return date.day() == 13; })
                                                       .allowClear(true)
                                                       .status(state->date_field.status())
                                                       .disabled(state->date_field.uneditable())
                                                       .inputReadOnly(state->date_field.readOnly())
                                                       .ref(state->date_field_ref)
                                                       .layout(LayoutStyle{}.width(dp(280))));
                                        remember_target(state, "gallery.date-picker.field");
                                    }});
         }});
    Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)), SpaceContent{[state] {
              Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                  if (!state->date_form_ref.submit()) {
                      state->date_feedback.set(GalleryMessage{"gallery.feedback.103", {}});
                  }
              }),
                     [] { gallery_label("gallery.copy.sample.343"); });
              gallery_target("gallery.date-picker.submit");
              Button(ButtonProps{}.onClick([state] {
                  if (state->date_form_ref.reset()) {
                      state->date_feedback.set(GalleryMessage{"gallery.feedback.104", {}});
                  }
              }),
                     [] { gallery_label("gallery.copy.sample.344"); });
              gallery_target("gallery.date-picker.reset");
              Button(
                  ButtonProps{}.onClick([state] { state->date_form_disabled.set(!state->date_form_disabled.get()); }),
                  [] { gallery_label("gallery.copy.sample.345"); });
              gallery_target("gallery.date-picker.disabled");
              Button(
                  ButtonProps{}.onClick([state] { state->date_form_read_only.set(!state->date_form_read_only.get()); }),
                  [] { gallery_label("gallery.copy.sample.346"); });
              gallery_target("gallery.date-picker.readonly");
              Button(ButtonProps{}.onClick([state] {
                  state->date_help.set(state->date_help.is("gallery.complete.copy.128")
                                           ? GalleryMessage{"gallery.complete.copy.137", {}}
                                           : GalleryMessage{"gallery.complete.copy.128", {}});
              }),
                     [] { gallery_label("gallery.copy.sample.347"); });
              gallery_target("gallery.date-picker.help");
          }});
    state->telemetry.live_samples += 2;
}

void add_input_only_features(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Space(
        SpaceProps{}.wrap(true).align(SpaceAlign::Start).layout(LayoutStyle{}.width(state->document_width)),
        SpaceContent{[state] {
            for (const auto variant :
                 {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
                const String id = utf8(variant == InputVariant::Outlined     ? "gallery.input.outlined"
                                       : variant == InputVariant::Filled     ? "gallery.input.filled"
                                       : variant == InputVariant::Borderless ? "gallery.input.borderless"
                                                                             : "gallery.input.underlined");
                Flex(FlexProps{}.vertical(true).gap(dp(4)).layout(LayoutStyle{}.width(state->cell_width)),
                     FlexContent{[variant, id, state] {
                         Text(TextProps{}.content(gallery_field(id.bytes(), "caption")));
                         Input(InputProps{}
                                   .variant(variant)
                                   .defaultValue(state->initial(id.bytes(), ryn::String{u8"中文 / RynUI 🙂"}))
                                   .allowClear(true)
                                   .showCount()
                                   .count(InputCountOptions{20, InputCountUnit::Grapheme})

                                   .onChange([state, id](ryn::String value) { state->remember(id.bytes(), value); })
                                   .layout(LayoutStyle{}.width(state->cell_width)));
                         remember_target(state, id.bytes());
                     }});
            }
            gallery_label("gallery.copy.sample.348");
            Input(InputProps{}
                      .defaultValue(state->initial("gallery.input.grapheme-count", ryn::String{u8"é🙂中文"}))
                      .showCount()
                      .count(InputCountOptions{3, InputCountUnit::Grapheme})
                      .onChange([state](ryn::String value) { state->remember("gallery.input.grapheme-count", value); })
                      .layout(LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.input.grapheme-count");
            gallery_label("gallery.copy.sample.349");
            Input(
                InputProps{}
                    .defaultValue(state->initial("gallery.input.exceed-formatter", ryn::String{u8"RynUI"}))
                    .showCount()
                    .count(InputCountOptions{8, InputCountUnit::Scalar})
                    .countStrategy([](StringView value) { return value.bytes().size(); })
                    .exceedFormatter([](String value, std::size_t) {
                        auto bytes = std::string{value.bytes()};
                        std::erase(bytes, ' ');
                        return utf8(bytes);
                    })
                    .onChange([state](ryn::String value) { state->remember("gallery.input.exceed-formatter", value); })
                    .layout(LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.input.exceed-formatter");
            gallery_label("gallery.copy.sample.350");
            Input(InputProps{}
                      .ref(state->input_ref)
                      .defaultValue(state->initial("gallery.input.focus-hints", ryn::String{u8"desktop@example.com"}))
                      .purpose(InputPurpose::Email)
                      .capitalization(InputCapitalization::None)
                      .autocorrect(false)
                      .onFocus([state] { state->input_feedback.set(GalleryMessage{"gallery.feedback.105", {}}); })
                      .onChange([state](ryn::String value) { state->remember("gallery.input.focus-hints", value); })
                      .layout(LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.input.focus-hints");
            gallery_label("gallery.copy.sample.351");
            Input(InputProps{}
                      .defaultValue(state->initial("gallery.input.clear-vector", ryn::String{u8"自定义清空图标"}))
                      .allowClear(true)
                      .clearDisabled(state->input_clear_disabled)
                      .clearIcon(icon_vector_sample())
                      .onClear([state] { state->input_feedback.set(GalleryMessage{"gallery.feedback.106", {}}); })
                      .onChange([state](ryn::String value) { state->remember("gallery.input.clear-vector", value); })
                      .layout(LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.input.clear-vector");
            Button(
                ButtonProps{}.onClick([state] { static_cast<void>(state->input_ref.focus({InputFocusCursor::All})); }),
                ButtonContent{[] { gallery_label("gallery.copy.sample.352"); }});
            remember_target(state, "gallery.input.focus-all");
            Button(
                ButtonProps{}.onClick([state] { state->input_clear_disabled.set(!state->input_clear_disabled.get()); }),
                ButtonContent{[] { gallery_label("gallery.copy.sample.353"); }});
            remember_target(state, "gallery.input.toggle-clear-disabled");
        }});
    state->telemetry.live_samples += 10;
}

void add_password_features(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Space(SpaceProps{}.wrap(true).align(SpaceAlign::Start).layout(LayoutStyle{}.width(state->document_width)),
          SpaceContent{[state] {
              gallery_label("gallery.copy.sample.354");
              Password(PasswordProps{}
                           .defaultValue(u8"秘密🙂")
                           .action(PasswordAction::Hover)
                           .allowClear(true)
                           .showCount()
                           .iconRender([](bool visible) {
                               return visible ? icon_vector_sample() : IconSource{IconName::EyeInvisibleOutlined};
                           })
                           .layout(LayoutStyle{}.width(state->cell_width)),
                       InputPrefix{[] { Icon(IconProps{}.name(IconName::LockOutlined)); }});
              remember_target(state, "gallery.password.hover-vector");
              gallery_label("gallery.copy.sample.355");
              Password(PasswordProps{}
                           .defaultValue(u8"controlled")
                           .visible(state->password_visible)
                           .visibilityToggle(state->password_toggle)
                           .onVisibleChange([state](bool value) { state->password_visible.set(value); })
                           .layout(LayoutStyle{}.width(state->cell_width)));
              remember_target(state, "gallery.password.controlled-toggle");
              Button(ButtonProps{}.onClick([state] { state->password_toggle.set(!state->password_toggle.get()); }),
                     ButtonContent{[] { gallery_label("gallery.copy.sample.356"); }});
              remember_target(state, "gallery.password.toggle-action");
          }});
    state->telemetry.live_samples += 3;
}

void add_search_features(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Space(SpaceProps{}.wrap(true).align(SpaceAlign::Start).layout(LayoutStyle{}.width(state->document_width)),
          SpaceContent{[state] {
              ThemeConfig dark;
              dark.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
              Theme(ThemeProps{}.config(dark), ThemeContent{[state] {
                        ++state->telemetry.theme_content_runs;
                        Card(CardProps{}.size(CardSize::Small).layout(LayoutStyle{}.width(state->cell_width)),
                             CardContent{[state] {
                                 Flex(FlexProps{}.vertical(true).gap(SpaceSize::Small), [state] {
                                     gallery_label("gallery.copy.sample.357");
                                     Search(SearchProps{}
                                                .variant(InputVariant::Filled)
                                                .size(ControlSize::Small)
                                                .defaultValue(state->initial("gallery.search.filled-vector",
                                                                             ryn::String{u8"RynUI"}))
                                                .searchIcon(icon_vector_sample())
                                                .allowClear(true)
                                                .showCount()
                                                .onSearch([state](String value, SearchSource source) {
                                                    ++state->telemetry.search_submits;
                                                    if (source == SearchSource::Clear) {
                                                        state->search_feedback.set(
                                                            GalleryMessage{"gallery.feedback.107", {}});
                                                    } else {
                                                        state->search_feedback.set(std::move(value));
                                                    }
                                                })

                                                .onChange([state](ryn::String value) {
                                                    state->remember("gallery.search.filled-vector", value);
                                                }));
                                     remember_target(state, "gallery.search.filled-vector");
                                 });
                             }});
                    }});
              gallery_label("gallery.copy.sample.358");
              Search(SearchProps{}
                         .variant(InputVariant::Underlined)
                         .size(ControlSize::Small)
                         .defaultValue(state->initial("gallery.search.underlined-small", ryn::String{u8"Search"}))

                         .onChange(
                             [state](ryn::String value) { state->remember("gallery.search.underlined-small", value); })
                         .layout(LayoutStyle{}.width(state->cell_width)),
                     SearchButtonContent{[] { gallery_label("gallery.copy.sample.359"); }});
              remember_target(state, "gallery.search.underlined-small");
          }});
    state->telemetry.live_samples += 2;
}

void add_input_feature_samples(const std::shared_ptr<GalleryState>& state) {
    add_input_only_features(state);
    add_password_features(state);
    add_search_features(state);
}

void add_text_area_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.360");
    Space(
        SpaceProps{}.wrap(true).align(SpaceAlign::Start).layout(LayoutStyle{}.width(state->document_width)),
        SpaceContent{[state] {
            for (const auto variant :
                 {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
                const auto id = utf8(variant == InputVariant::Outlined     ? "gallery.text-area.outlined"
                                     : variant == InputVariant::Filled     ? "gallery.text-area.filled"
                                     : variant == InputVariant::Borderless ? "gallery.text-area.borderless"
                                                                           : "gallery.text-area.underlined");
                Flex(FlexProps{}.vertical(true).gap(dp(4)).layout(LayoutStyle{}.width(state->cell_width)),
                     FlexContent{[state, id, variant] {
                         Text(TextProps{}.content(gallery_field(id.bytes(), "caption")));
                         TextArea(TextAreaProps{}
                                      .variant(variant)
                                      .rows(3)
                                      .defaultValue(state->initial(
                                          id.bytes(), ryn::String{u8"中文 / RynUI 🙂\n第二行\nEnter 换行"}))
                                      .allowClear(true)
                                      .showCount()
                                      .count(InputCountOptions{40, InputCountUnit::Grapheme})
                                      .onChange([state, id](String value) {
                                          state->remember(id.bytes(), value);
                                          ++state->telemetry.input_changes;
                                          state->input_feedback.set(std::move(value));
                                      })
                                      .layout(LayoutStyle{}.width(state->cell_width)));
                         remember_target(state, id.bytes());
                     }});
            }
            gallery_label("gallery.copy.sample.361");
            TextArea(TextAreaProps{}
                         .defaultValue(state->initial("gallery.text-area.autosize",
                                                      ryn::String{u8"autoSize 随编辑与宽度换行扩展\n保留空行\n\n"}))
                         .autoSize(TextAreaAutoSize{true, 2, 5})
                         .allowClear(true)
                         .showCount()

                         .onChange([state](ryn::String value) { state->remember("gallery.text-area.autosize", value); })
                         .layout(LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.text-area.autosize");
            gallery_label("gallery.copy.sample.362");
            TextArea(
                TextAreaProps{}
                    .defaultValue(state->initial("gallery.text-area.resize-nowrap",
                                                 ryn::String{u8"No wrap: abcdefghijklmnopqrstuvwxyz "
                                                             u8"0123456789\n第二行\n第三行\n第四行\n第五行\n第六行"}))
                    .rows(2)
                    .wrap(false)
                    .resize(TextAreaResize::Both)
                    .onChange([state](ryn::String value) { state->remember("gallery.text-area.resize-nowrap", value); })
                    .layout(LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.text-area.resize-nowrap");
            ThemeConfig dark;
            dark.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
            Theme(ThemeProps{}.config(dark), ThemeContent{[state] {
                      ++state->telemetry.theme_content_runs;
                      gallery_label("gallery.copy.sample.363");
                      TextArea(TextAreaProps{}
                                   .defaultValue(state->initial(
                                       "gallery.text-area.readonly",
                                       ryn::String{u8"只读可以选择、复制和滚动\n中文 / Latin 🙂\n第三行\n第四行"}))
                                   .rows(2)
                                   .readOnly(true)
                                   .variant(InputVariant::Filled)
                                   .size(ControlSize::Small)
                                   .showCount()

                                   .onChange([state](ryn::String value) {
                                       state->remember("gallery.text-area.readonly", value);
                                   })
                                   .layout(LayoutStyle{}.width(state->cell_width)));
                      remember_target(state, "gallery.text-area.readonly");
                  }});
            gallery_label("gallery.copy.sample.364");
            TextArea(TextAreaProps{}
                         .defaultValue(
                             state->initial("gallery.text-area.disabled", ryn::String{u8"禁用输入\n不响应编辑或拖动"}))
                         .rows(2)
                         .disabled(true)
                         .status(InputStatus::Warning)

                         .onChange([state](ryn::String value) { state->remember("gallery.text-area.disabled", value); })
                         .layout(LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.text-area.disabled");
        }});
    state->telemetry.live_samples += 8;
}

void add_otp_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.365");
    Space(SpaceProps{}.wrap(true).align(SpaceAlign::Start).layout(LayoutStyle{}.width(state->document_width)),
          SpaceContent{[state] {
              for (const auto variant :
                   {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
                  const auto id = utf8(variant == InputVariant::Outlined     ? "gallery.otp.outlined"
                                       : variant == InputVariant::Filled     ? "gallery.otp.filled"
                                       : variant == InputVariant::Borderless ? "gallery.otp.borderless"
                                                                             : "gallery.otp.underlined");
                  Flex(FlexProps{}
                           .vertical(true)
                           .align(FlexAlign::Start)
                           .gap(dp(4))
                           .layout(LayoutStyle{}.width(state->cell_width)),
                       FlexContent{[state, id, variant] {
                           Text(TextProps{}.content(gallery_field(id.bytes(), "caption")));
                           OTP(OTPProps{}
                                   .length(4)
                                   .variant(variant)
                                   .defaultValue(u8"12")
                                   .onInput([state](const auto& cells) {
                                       ++state->telemetry.input_changes;
                                       const auto filled = std::ranges::count_if(
                                           cells, [](const String& value) { return !value.empty(); });
                                       state->input_feedback.set(
                                           GalleryMessage{"gallery.event.020",
                                                          {{u8"v0", utf8(std::to_string(filled))},
                                                           {u8"v1", static_cast<std::uint64_t>(cells.size())}}});
                                   })
                                   .onChange([state](String value) { state->input_feedback.set(std::move(value)); }));
                           remember_target(state, id.bytes());
                       }});
              }
              gallery_label("gallery.copy.sample.366");
              OTP(OTPProps{}
                      .value(state->otp_value)
                      .ref(state->otp_ref)
                      .onInput([state](const auto&) { ++state->telemetry.input_changes; })
                      .onChange([state](String value) {
                          state->otp_value.set(value);
                          state->input_feedback.set(std::move(value));
                      }));
              remember_target(state, "gallery.otp.controlled");
              gallery_label("gallery.copy.sample.367");
              OTP(OTPProps{}.defaultValue(u8"rYnUI!").mask(true).size(ControlSize::Large).formatter([](String value) {
                  std::string bytes{value.bytes()};
                  for (auto& byte : bytes) {
                      if (byte >= 'a' && byte <= 'z') {
                          byte = static_cast<char>(byte - 'a' + 'A');
                      }
                  }
                  return String::from_utf8(bytes).value();
              }));
              remember_target(state, "gallery.otp.formatter-mask");
              gallery_label("gallery.copy.sample.368");
              OTP(OTPProps{}.length(4).defaultValue(u8"1234").direction(OTPDirection::RightToLeft),
                  OTPSeparator{[](std::size_t index) -> std::optional<OTPSeparatorContent> {
                      if (index != 1) {
                          return {};
                      }
                      return OTPSeparatorContent{[] { Text(u8"-"); }};
                  }});
              remember_target(state, "gallery.otp.separator-rtl");
              gallery_label("gallery.copy.sample.369");
              OTP(OTPProps{}.length(state->otp_length).defaultValue(u8"12345678"));
              remember_target(state, "gallery.otp.dynamic-length");
              ThemeConfig dark;
              dark.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
              Theme(ThemeProps{}.config(dark), ThemeContent{[state] {
                        ++state->telemetry.theme_content_runs;
                        gallery_label("gallery.copy.sample.370");
                        OTP(OTPProps{}
                                .length(4)
                                .defaultValue(u8"只读验证码")
                                .mask(u8"*")
                                .readOnly(true)
                                .variant(InputVariant::Filled)
                                .size(ControlSize::Small));
                        remember_target(state, "gallery.otp.readonly-mask");
                    }});
              gallery_label("gallery.copy.sample.371");
              OTP(OTPProps{}.length(4).defaultValue(u8"1234").disabled(true).status(InputStatus::Warning));
              remember_target(state, "gallery.otp.disabled");
              Button(ButtonProps{}.onClick([state] { static_cast<void>(state->otp_ref.focus()); }),
                     ButtonContent{[] { gallery_label("gallery.copy.sample.372"); }});
              remember_target(state, "gallery.otp.focus-all");
              Button(ButtonProps{}.onClick([state] { state->otp_length.set(state->otp_length.get() == 6 ? 4 : 6); }),
                     ButtonContent{[] { gallery_label("gallery.copy.sample.373"); }});
              remember_target(state, "gallery.otp.toggle-length");
          }});
    state->telemetry.live_samples += 12;
}

void add_bidi_typography(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.374");
    remember_target(state, "gallery.bidi.heading");
    Text(TextProps{}.content(String{u8"مرحبا (12) אבג RynUI"}).direction(state->text_direction));
    remember_target(state, "gallery.bidi.text");
    Paragraph(TypographyProps{}
                  .content(String{u8"مرحبا 123 (אבג)\nאבג 12 مرحبا 中文"})
                  .direction(state->text_direction)
                  .underline(true)
                  .layout(LayoutStyle{}.width(state->cell_width)));
    remember_target(state, "gallery.bidi.paragraph");
    state->telemetry.live_samples += 2;
}

void add_bidi_input(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.375");
    Input(InputProps{}
              .defaultValue(state->initial("gallery.bidi.input", ryn::String{u8"A אבג 12 مرحبا"}))
              .direction(state->text_direction)

              .onChange([state](ryn::String value) { state->remember("gallery.bidi.input", value); })
              .layout(LayoutStyle{}.width(state->cell_width)));
    remember_target(state, "gallery.bidi.input");
    state->telemetry.live_samples += 1;
}

void add_bidi_text_area(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.376");
    TextArea(TextAreaProps{}
                 .defaultValue(state->initial("gallery.bidi.text-area",
                                              ryn::String{u8"אבג (12) مرحبا\nمرحبا 34 אבג 中文\n\n尾行"}))
                 .direction(state->text_direction)
                 .rows(3)

                 .onChange([state](ryn::String value) { state->remember("gallery.bidi.text-area", value); })
                 .layout(LayoutStyle{}.width(state->cell_width)));
    remember_target(state, "gallery.bidi.text-area");
    state->telemetry.live_samples += 1;
}

void add_bidi_password(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.377");
    Password(PasswordProps{}
                 .defaultValue(u8"אבג مرحبا 👩‍💻")
                 .direction(state->text_direction)
                 .layout(LayoutStyle{}.width(state->cell_width)));
    remember_target(state, "gallery.bidi.password");
    state->telemetry.live_samples += 1;
}

void add_bidi_direction_control(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Button(ButtonProps{}.onClick([state] {
        const auto current = state->text_direction.get();
        state->text_direction.set(current == TextDirection::Auto          ? TextDirection::LeftToRight
                                  : current == TextDirection::LeftToRight ? TextDirection::RightToLeft
                                                                          : TextDirection::Auto);
    }),
           ButtonContent{[state] {
               const auto identity =
                   state->page_component == "ant.component.typography"   ? "gallery.typography.bidi-direction"
                   : state->page_family == GalleryInputFamily::text_area ? "gallery.text-area.bidi-direction"
                   : state->page_family == GalleryInputFamily::password  ? "gallery.password.bidi-direction"
                                                                         : "gallery.bidi.toggle-direction";
               gallery_label("gallery.complete.copy.381");
           }});
    remember_target(state, state->page_component == "ant.component.typography"   ? "gallery.typography.bidi-direction"
                           : state->page_family == GalleryInputFamily::text_area ? "gallery.text-area.bidi-direction"
                           : state->page_family == GalleryInputFamily::password  ? "gallery.password.bidi-direction"
                                                                                 : "gallery.bidi.toggle-direction");
    state->telemetry.live_samples += 1;
}

void add_bidi_samples(const std::shared_ptr<GalleryState>& state) {
    add_bidi_typography(state);
    add_bidi_input(state);
    add_bidi_text_area(state);
    add_bidi_password(state);
    add_bidi_direction_control(state);
}

void add_complete_select_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.378");
    const auto groups = gallery_group_labels<SelectOptionGroup>(
        std::vector<SelectOptionGroup>{{u8"platform", u8"平台", {{u8"desktop", u8"桌面"}, {u8"service", u8"服务"}}},
                                       {u8"priority", u8"优先级", {{1, u8"高"}, {2, u8"低"}, {3, u8"停用", true}}}},
        {{u8"platform", "gallery.complete.copy.356"}, {u8"priority", "gallery.complete.copy.357"}},
        {{u8"desktop", "gallery.complete.copy.358"},
         {u8"service", "gallery.complete.copy.359"},
         {1, "gallery.complete.copy.360"},
         {2, "gallery.complete.copy.361"},
         {3, "gallery.complete.copy.362"}});
    Select(SelectProps{}
               .mode(SelectMode::Multiple)
               .groups(groups)
               .values(state->select_multi)
               .onValuesChange([state](SelectValues values) {
                   state->select_multi_feedback.set(state->select_multi_accept
                                                        ? GalleryMessage{"gallery.feedback.108", {}}
                                                        : GalleryMessage{"gallery.feedback.109", {}});
                   if (state->select_multi_accept) {
                       state->select_multi.set(std::move(values));
                   }
               })
               .maxTagCount(std::size_t{2})
               .allowClear()
               .motion()
               .placeholder(gallery_text("gallery.copy.property.065"))
               .layout(LayoutStyle{}.margin_top(dp(4)).margin_bottom(dp(4)).width(dp(340))));
    remember_target(state, "gallery.select.multiple");
    Button(ButtonProps{}.onClick([state] { state->select_multi_accept = !state->select_multi_accept; }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.379"); }});
    gallery_target("gallery.select.authority");
    Select(SelectProps{}
               .mode(SelectMode::Tags)
               .groups(groups)
               .tokenSeparators({u8",", u8"；"})
               .responsiveMaxTagCount()
               .maxTagTextLength(std::size_t{8})
               .maxCount(std::size_t{12})
               .placeholder(gallery_text("gallery.copy.property.066"))
               .tagRender(SelectTagRenderer{[](SelectRenderInfo info) {
                   Button(ButtonProps{}.size(ControlSize::Small).disabled(info.disabled).onClick(info.remove),
                          ButtonContent{[label = info.label] { Text(TextProps{}.content(label)); }});
               }})
               .onValuesChange([state](SelectValues values) {
                   state->select_multi_feedback.set(
                       GalleryMessage{"gallery.event.021", {{u8"v0", static_cast<std::uint64_t>(values.size())}}});
               })
               .layout(LayoutStyle{}.margin_top(dp(4)).margin_bottom(dp(4)).width(dp(340))));
    remember_target(state, "gallery.select.tags");
    Text(TextProps{}.content(state->select_multi_feedback));
    gallery_label("gallery.copy.sample.380");
    std::vector<SelectOption> many;
    for (int i = 0; i < 10000; ++i) {
        many.push_back({i, utf8("合成选项 " + std::to_string(i))});
    }
    Select(SelectProps{}
               .options(bind([many = std::move(many), tr = useI18n()] {
                   auto result = many;
                   for (std::size_t i = 0; i < result.size(); ++i) {
                       result[i].label = tr.translate(gallery_message("gallery.complete.copy.363"),
                                                      {{u8"index", static_cast<std::uint64_t>(i)}});
                   }
                   return result;
               }))
               .showSearch()
               .listHeight(160)
               .direction(TextDirection::RightToLeft)
               .optionRender(SelectOptionRenderer{[](SelectRenderInfo info) { Text(TextProps{}.content(info.label)); }})
               .popupRender(SelectPopupRenderer{[](std::function<void()> origin) {
                   Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[origin] {
                            origin();
                            Input(InputProps{}.placeholder(gallery_text("gallery.copy.property.067")));
                            gallery_target("gallery.select.popup-note");
                        }});
               }})
               .placeholder(gallery_text("gallery.copy.property.068"))
               .layout(LayoutStyle{}.margin_top(dp(4)).margin_bottom(dp(4)).width(dp(340))),
           SelectPrefix{[state] {
               Button(ButtonProps{}.size(ControlSize::Small).onClick([state] {
                   state->select_multi_feedback.set(GalleryMessage{"gallery.feedback.110", {}});
               }),
                      ButtonContent{[] { Icon(IconProps{}.name(IconName::SearchOutlined)); }});
               gallery_target("gallery.select.prefix");
           }});
    remember_target(state, "gallery.select.virtual");
    gallery_label("gallery.copy.sample.381");
    const auto weak = std::weak_ptr{state};
    SelectSource source{[weak](AsyncOptionsRequest request, SelectSource::Complete complete) {
                            if (auto state = weak.lock()) {
                                state->completion_jobs.push_back([request, complete = std::move(complete)] {
                                    const auto label = utf8(std::string{request.query.bytes()} + " · synthetic");
                                    complete({{{label, label}}, {}});
                                });
                            }
                            return AsyncOptionsCancel{};
                        },
                        [weak](std::function<void()> post) {
                            if (auto state = weak.lock()) {
                                state->owner_posts.push_back(std::move(post));
                            }
                        }};
    Select(SelectProps{}
               .source(source)
               .notFoundContent(gallery_text("gallery.copy.property.069"))
               .popupRender(SelectPopupRenderer{[state](std::function<void()> origin) {
                   Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[state, origin] {
                            origin();
                            Button(ButtonProps{}.onClick([state] {
                                auto jobs = std::move(state->completion_jobs);
                                state->completion_jobs.clear();
                                for (auto& job : jobs) {
                                    job();
                                }
                                auto posts = std::move(state->owner_posts);
                                state->owner_posts.clear();
                                for (auto& post : posts) {
                                    post();
                                }
                                state->select_multi_feedback.set(GalleryMessage{"gallery.feedback.111", {}});
                            }),
                                   ButtonContent{[] { gallery_label("gallery.copy.sample.382"); }});
                            gallery_target("gallery.select.deliver");
                        }});
               }})
               .showSearch()
               .placeholder(gallery_text("gallery.copy.property.070"))
               .layout(LayoutStyle{}.margin_top(dp(4)).margin_bottom(dp(4)).width(dp(340))));

    remember_target(state, "gallery.select.async");
    state->telemetry.live_samples += 4;
}

void add_select_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.383");
    const auto choices = gallery_option_labels<SelectOption>(
        std::vector<SelectOption>{
            {u8"desktop", ryn::String{}}, {u8"service", ryn::String{}}, {u8"unavailable", u8"禁用选项", true}},
        {{u8"desktop", "gallery.complete.copy.353"},
         {u8"service", "gallery.complete.copy.354"},
         {u8"unavailable", "gallery.complete.copy.355"}});
    const auto field = [&](SelectProps props, const char8_t* caption, SelectSelection initial = {}) {
        const auto label = String::from_utf8(std::u8string_view{caption}).value();
        const auto identity = label.bytes().substr(0, label.bytes().find(" ·"));
        if (identity != "gallery.select.controlled-search") {
            const auto value = gallery_initial_value(identity, initial);
            if (value) {
                props.defaultValue(*value);
            }
            props.onChange(gallery_value_changed<SelectSelection>(identity));
        }
        Text(TextProps{}.content(gallery_field(identity, "caption")));
        Select(props.layout(LayoutStyle{}.width(state->cell_width)));
        remember_target(state, identity);
        ++state->telemetry.live_samples;
    };
    field(SelectProps{}
              .options(gallery_option_labels<ryn::SelectOption>(state->select_options,
                                                                {{u8"desktop", "gallery.complete.copy.233"},
                                                                 {u8"service", "gallery.complete.copy.234"},
                                                                 {u8"device", "gallery.complete.copy.235"}}))
              .value(state->select_value)
              .showSearch()
              .allowClear()
              .ref(state->select_ref)
              .placeholder(gallery_text("gallery.copy.property.071"))
              .notFoundContent(gallery_text("gallery.copy.property.072"))
              .onChange([state](SelectSelection value) {
                  state->select_value.set(std::move(value));
                  ++state->telemetry.activations;
              }),
          u8"gallery.select.controlled-search");
    field(
        SelectProps{}.options(gallery_option_labels<ryn::SelectOption>(
            std::vector<ryn::SelectOption>{{1, u8"优先级 1"}, {42, u8"优先级 42"}, {99, u8"禁用", true}},
            {{1, "gallery.complete.copy.113"}, {42, "gallery.complete.copy.114"}, {99, "gallery.complete.copy.115"}})),
        u8"gallery.select.numeric", SelectSelection{SelectValue{42}});
    field(SelectProps{}.options(choices).disabled(true), u8"gallery.select.disabled",
          SelectSelection{SelectValue{u8"desktop"}});
    field(SelectProps{}.options(choices).loading(state->loading).showSearch(), u8"gallery.select.loading");
    field(SelectProps{}.options({}).showSearch().notFoundContent(gallery_text("gallery.copy.property.073")),
          u8"gallery.select.empty");
    field(SelectProps{}.options(choices).open(state->select_open).onOpenChange([state](bool value) {
        state->select_open.set(value);
    }),
          u8"gallery.select.controlled-open");
    for (const auto [variant, caption] :
         std::array{std::pair{InputVariant::Outlined, u8"gallery.select.outlined"},
                    std::pair{InputVariant::Filled, u8"gallery.select.filled"},
                    std::pair{InputVariant::Borderless, u8"gallery.select.borderless"},
                    std::pair{InputVariant::Underlined, u8"gallery.select.underlined"}}) {
        field(SelectProps{}.options(choices).variant(variant).allowClear(), caption,
              SelectSelection{SelectValue{u8"desktop"}});
    }
    for (const auto [size, caption] : std::array{std::pair{ControlSize::Small, u8"gallery.select.small"},
                                                 std::pair{ControlSize::Middle, u8"gallery.select.middle"},
                                                 std::pair{ControlSize::Large, u8"gallery.select.large"}}) {
        field(SelectProps{}.options(choices).size(size), caption, SelectSelection{SelectValue{u8"desktop"}});
    }
    field(SelectProps{}.options(choices).status(InputStatus::Warning), u8"gallery.select.warning",
          SelectSelection{SelectValue{u8"desktop"}});
    field(SelectProps{}.options(choices).status(InputStatus::Error), u8"gallery.select.error",
          SelectSelection{SelectValue{u8"desktop"}});
    ThemeConfig custom;
    custom.select.algorithm = true;
    custom.select.seed.color_primary = Color::rgba8(114, 46, 209);
    custom.select.tokens.option_height = dp(36);
    Theme(ThemeProps{}.config(custom), ThemeContent{[state, choices] {
              ++state->telemetry.theme_content_runs;
              gallery_label("gallery.copy.sample.384");
              Select(gallery_select_default("gallery.select.component-theme",
                                            SelectProps{}
                                                .options(choices)

                                                .showSearch()
                                                .layout(LayoutStyle{}.width(state->cell_width)),
                                            ryn::SelectValue{u8"service"})
                         .onChange(gallery_value_changed<ryn::SelectSelection>("gallery.select.component-theme")),
                     SelectPrefix{[] { Icon(IconProps{}.name(IconName::SearchOutlined)); }});
              gallery_target("gallery.select.component-theme");
              ++state->telemetry.live_samples;
          }});
    Button(ButtonProps{}.onClick([state] { static_cast<void>(state->select_ref.focus()); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.385"); }});
    gallery_target("gallery.select.focus");
    Button(ButtonProps{}.onClick([state] { state->select_open.set(!state->select_open.get()); }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.386"); }});
    gallery_target("gallery.select.toggle-open");
    Button(ButtonProps{}.onClick([state] {
        state->select_options_expanded = !state->select_options_expanded;
        auto options = std::vector<SelectOption>{{u8"desktop", ryn::String{}}, {u8"service", ryn::String{}}};
        if (state->select_options_expanded) {
            options.push_back({u8"device", ryn::String{}});
        }
        state->select_options.set(std::move(options));
    }),
           ButtonContent{[] { gallery_label("gallery.copy.sample.387"); }});
    gallery_target("gallery.select.dynamic-options");
    state->telemetry.live_samples += 3;
    add_complete_select_samples(state);
}

void add_float_button_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.388");
    gallery_label("gallery.copy.sample.389");
    Space(SpaceProps{}, SpaceContent{[state] {
              FloatButton(FloatButtonProps{}
                              .floating(false)
                              .tooltip(gallery_text("gallery.copy.property.074"))
                              .badge(BadgeProps{}.count(3))
                              .onClick([state] {
                                  state->common_feedback.set(GalleryMessage{"gallery.feedback.112", {}});
                                  ++state->telemetry.activations;
                              }));
              FloatButton(
                  FloatButtonProps{}.floating(false).type(FloatButtonType::Primary).shape(FloatButtonShape::Square),
                  {.icon = FloatButtonIcon{[] { Icon(IconProps{}.name(IconName::CustomerServiceOutlined)); }},
                   .content = FloatButtonContent{[] { gallery_label("gallery.copy.sample.390"); }}});
              FloatButton(FloatButtonProps{}.floating(false).disabled(true));
          }});
    Text(TextProps{}.content(state->common_feedback));
    gallery_label("gallery.copy.sample.391");
    FloatButtonGroup(FloatButtonGroupProps{}.floating(false).shape(FloatButtonShape::Square),
                     FloatButtonGroupContent{[] {
                         FloatButton();
                         FloatButton(FloatButtonProps{}.type(FloatButtonType::Primary));
                     }});
    for (const auto placement : {FloatButtonPlacement::Top, FloatButtonPlacement::Bottom, FloatButtonPlacement::Left,
                                 FloatButtonPlacement::Right}) {
        FloatButtonGroup(
            FloatButtonGroupProps{}.floating(false).trigger(FloatButtonTrigger::Click).placement(placement),
            FloatButtonGroupContent{[] {
                FloatButton();
                FloatButton();
            }});
    }
    FloatButtonGroup(FloatButtonGroupProps{}
                         .floating(false)
                         .trigger(FloatButtonTrigger::Hover)
                         .placement(FloatButtonPlacement::Right),
                     FloatButtonGroupContent{[] {
                         FloatButton();
                         FloatButton();
                     }});
    gallery_label("gallery.copy.sample.392");
    FloatButtonGroup(FloatButtonGroupProps{}
                         .floating(false)
                         .trigger(FloatButtonTrigger::Click)
                         .open(state->float_open)
                         .onOpenChange([state](bool value) {
                             state->float_open.set(value);
                             ++state->telemetry.activations;
                         }),
                     FloatButtonGroupContent{[] {
                         FloatButton();
                         FloatButton();
                     }});
    gallery_label("gallery.copy.sample.393");
    Slider(SliderProps{}
               .limits(SliderLimits{0, 840, 1})
               .value(bind([scroll = state->float_scroll] { return double(scroll.get().offset); }))
               .onChange([state](double value) {
                   auto scroll = state->float_scroll.get();
                   scroll.offset = float(value);
                   state->float_scroll.set(scroll);
               })
               .layout(LayoutStyle{}.width(state->cell_width)));
    ReferenceScrollViewport(
        bind([scroll = state->float_scroll] { return scroll.get().offset; }), state->cell_width, 160,
        ReferenceSurfaceContent{[] {
            Flex(FlexProps{}.vertical(true), FlexContent{[] {
                     for (int index = 0; index < 25; ++index) {
                         Text(
                             TextProps{}
                                 .content(gallery_text("gallery.complete.copy.495",
                                                       MessageArgs{{u8"index", static_cast<std::uint64_t>(index + 1)}}))
                                 .layout(LayoutStyle{}.height(dp(40))));
                     }
                 }});
        }});
    FloatButtonBackTop(FloatButtonBackTopProps{}
                           .button(FloatButtonProps{}.floating(false))
                           .scroll(state->float_scroll)
                           .showProgress(true)
                           .visibilityHeight(100)
                           .onScrollTo([state](float offset) {
                               auto scroll = state->float_scroll.get();
                               scroll.offset = offset;
                               state->float_scroll.set(scroll);
                           }));
    gallery_label("gallery.copy.sample.394");
    FloatButton(FloatButtonProps{}
                    .type(FloatButtonType::Primary)
                    .draggable(true)
                    .tooltip(gallery_text("gallery.copy.property.075"))
                    .onClick([state] {
                        ++state->telemetry.activations;
                        state->common_feedback.set(GalleryMessage{"gallery.feedback.113", {}});
                    }));
}

void add_listy_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    std::vector<ListyItem<String>> initial;
    for (int i = 0; i < 12; ++i) {
        initial.emplace_back(String::from_utf8("item-" + std::to_string(i + 1)).value(),
                             String::from_utf8("Account " + std::to_string(i + 1)).value());
    }
    Signal<std::vector<ListyItem<String>>> items{initial};
    Signal<std::size_t> columns{1};
    Signal<bool> paginated{true};
    Signal<bool> disabled{false};
    Signal<ListyDirection> direction{ListyDirection::Vertical};
    ListyRef ref;
    Space(SpaceProps{}.wrap(true), SpaceContent{[=] {
              Button(ButtonProps{}.onClick([=] {
                  auto next = items.get();
                  std::reverse(next.begin(), next.end());
                  items.set(std::move(next));
              }),
                     [] { gallery_label("gallery.copy.sample.395"); });
              remember_target(state, "gallery.listy.reorder");
              Button(ButtonProps{}.onClick([=] { columns.set(columns.get() == 1 ? 2 : 1); }),
                     [] { gallery_label("gallery.copy.sample.396"); });
              remember_target(state, "gallery.listy.grid");
              Button(ButtonProps{}.onClick(
                         [=] { items.set(items.get().empty() ? initial : std::vector<ListyItem<String>>{}); }),
                     [] { gallery_label("gallery.copy.sample.397"); });
              remember_target(state, "gallery.listy.empty");
              Button(ButtonProps{}.onClick([=] { disabled.set(!disabled.get()); }),
                     [] { gallery_label("gallery.copy.sample.398"); });
              remember_target(state, "gallery.listy.disable");
              Button(ButtonProps{}.onClick([=] { paginated.set(!paginated.get()); }),
                     [] { gallery_label("gallery.copy.sample.399"); });
              remember_target(state, "gallery.listy.pagination");
              Button(ButtonProps{}.onClick([=] {
                  direction.set(direction.get() == ListyDirection::Vertical ? ListyDirection::Horizontal
                                                                            : ListyDirection::Vertical);
              }),
                     [] { gallery_label("gallery.copy.sample.400"); });
              remember_target(state, "gallery.listy.direction");
              Button(ButtonProps{}.onClick([=] { (void)ref.scrollTo(100000.0F); }),
                     [] { gallery_label("gallery.copy.sample.401"); });
              remember_target(state, "gallery.listy.scroll-end");
          }});
    gallery_label("gallery.copy.sample.402");
    Listy(ListyProps<String>{}
              .items(items)
              .columns(columns)
              .pagination(paginated)
              .pageSize(4)
              .height(220.0F)
              .direction(direction)
              .disabled(disabled)
              .ref(ref)
              .layout(LayoutStyle{}.width(state->document_width))
              .header(ListyContent{[] { gallery_label("gallery.copy.sample.403"); }})
              .emptyContent(
                  ListyEmpty{[] { Empty(EmptyProps{}.description(gallery_text("gallery.copy.property.076"))); }})
              .onChange([=](auto, auto) { ++state->telemetry.activations; })
              .itemRender([](ListyItemView<String> view) {
                  Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[=] {
                           Text(TextProps{}.content(view.data()));
                           Input(InputProps{}.defaultValue(view.key).layout(LayoutStyle{}.width(dp(140))));
                       }});
              }));
    remember_target(state, "gallery.listy.retained");
    ++state->telemetry.live_samples;
}

void add_empty_completion_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;

    struct Pending final {
        ImageCompletion done;
    };

    const auto pending = std::make_shared<Pending>();
    auto loader = std::make_shared<ImageLoader>();
    loader->start = [pending](ImageCompletion done) {
        pending->done = std::move(done);
        return [pending] {
            pending->done = {};
        };
    };
    Signal<ImageSource> source{ImageSource{loader}};
    EmptyDynamicContent initial;
    initial.description = std::make_shared<const EmptyDescription>([] { gallery_label("gallery.copy.sample.404"); });
    initial.extra = std::make_shared<const EmptyExtra>([] { Input(InputProps{}.defaultValue(u8"填写创建内容")); });
    Signal<EmptyDynamicContent> dynamic{initial};
    gallery_label("gallery.copy.sample.405");
    Space(SpaceProps{}.wrap(true), SpaceContent{[=] {
              Button(ButtonProps{}.onClick([=] {
                  if (pending->done) {
                      (void)pending->done(decode_image(*media_fixture_source()));
                  }
              }),
                     [] { gallery_label("gallery.copy.sample.406"); });
              remember_target(state, "gallery.empty.complete-image");
              Button(ButtonProps{}.onClick([=] {
                  if (pending->done) {
                      (void)pending->done({{}, {ImageErrorCode::loader_failure, "Application rejected image"}});
                  }
              }),
                     [] { gallery_label("gallery.copy.sample.407"); });
              remember_target(state, "gallery.empty.fail-image");
              Button(ButtonProps{}.onClick([=] { source.set(ImageSource{std::make_shared<ImageLoader>(*loader)}); }),
                     [] { gallery_label("gallery.copy.sample.408"); });
              remember_target(state, "gallery.empty.retry-image");
              Button(ButtonProps{}.onClick([=] {
                  auto next = dynamic.get();
                  next.description = next.description ? nullptr : initial.description;
                  dynamic.set(next);
              }),
                     [] { gallery_label("gallery.copy.sample.409"); });
              remember_target(state, "gallery.empty.dynamic-description");
              Button(ButtonProps{}.onClick([=] {
                  auto next = dynamic.get();
                  next.extra = next.extra ? nullptr : initial.extra;
                  dynamic.set(next);
              }),
                     [] { gallery_label("gallery.copy.sample.410"); });
              remember_target(state, "gallery.empty.dynamic-extra");
          }});
    Empty(EmptyProps{}.source(source).description(gallery_text("gallery.copy.property.077")).dynamicContent(dynamic));
    remember_target(state, "gallery.empty.dynamic");
    Empty(EmptyProps{}.illustration().description(gallery_text("gallery.copy.property.078")));
    ++state->telemetry.live_samples;
}

void add_card_completion_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<std::size_t> columns{2};
    Signal<String> active{String{u8"overview"}};
    auto overview =
        TabItem{u8"overview", ryn::String{}, TabContent{[] { gallery_label("gallery.copy.sample.411"); }}}.label(
            gallery_text("gallery.complete.copy.054"));
    auto draft = TabItem{
        u8"draft", ryn::String{}, TabContent{[] {
            Input(InputProps{}.defaultValue(u8"保留的 tab 草稿"));
        }}}.label(gallery_text("gallery.complete.copy.055"));
    Signal<std::vector<TabItem>> tabs{std::vector{overview, draft}};
    gallery_label("gallery.copy.sample.412");
    Space(SpaceProps{}, SpaceContent{[=] {
              Button(ButtonProps{}.onClick([=] { columns.set(columns.get() == 2 ? 1 : 2); }),
                     [] { gallery_label("gallery.copy.sample.413"); });
              remember_target(state, "gallery.card.grid-columns");
              Button(ButtonProps{}.onClick([=] {
                  auto next = tabs.get();
                  std::reverse(next.begin(), next.end());
                  tabs.set(std::move(next));
              }),
                     [] { gallery_label("gallery.copy.sample.414"); });
              remember_target(state, "gallery.card.tabs-reorder");
          }});
    Card(CardProps{}
             .title(gallery_text("gallery.copy.property.079"))
             .tabList(tabs)
             .activeTabKey(active)
             .onTabChange([=](String key) { active.set(std::move(key)); })
             .gridColumns(columns)
             .layout(LayoutStyle{}.width(state->document_width)),
         CardContent{[] {
             for (int i = 0; i < 3; ++i) {
                 CardGrid(
                     CardGridProps{}.hoverable(i != 1), CardGridContent{[i] {
                         Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[i] {
                                  CardMeta(CardMetaProps{}
                                               .title(String::from_utf8("Member " + std::to_string(i + 1)).value())
                                               .description(gallery_text("gallery.copy.property.080")),
                                           {.avatar = CardMetaAvatar{[] { Text(u8"R"); }}});
                                  Input(InputProps{}.defaultValue(u8"Grid 草稿").layout(LayoutStyle{}.width(dp(120))));
                              }});
                     }});
             }
         }},
         {.cover = CardCover{[] {
              Image(ImageProps{}
                        .source(ImageSource{media_fixture_source()})
                        .preview(false)
                        .shape(ImageShape::RoundedRectangle)
                        .layout(LayoutStyle{}.width(dp(160)).height(dp(80))));
          }}});
    remember_target(state, "gallery.card.completion");
    ++state->telemetry.live_samples;
}

void add_card_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.415");
    const auto props = [state] {
        return CardProps{}.layout(LayoutStyle{}.width(state->cell_width));
    };
    gallery_label("gallery.copy.sample.416");
    Card(props(), CardContent{[] { gallery_label("gallery.copy.sample.417"); }});
    gallery_label("gallery.copy.sample.418");
    Card(props().title(gallery_text("gallery.copy.property.081")).loading(state->card_loading), CardContent{[] {
             Input(InputProps{}
                       .defaultValue(gallery_initial_text("gallery.card.draft", u8"保留草稿 / Draft"))
                       .onChange(gallery_value_changed<ryn::String>("gallery.card.draft"))
                       .layout(LayoutStyle{}.width(dp(140))));
             gallery_target("gallery.card.draft");
         }},
         CardSlots{.extra = CardExtra{[state] {
                       Button(ButtonProps{}.onClick([state] { ++state->telemetry.activations; }),
                              [] { gallery_label("gallery.complete.copy.195"); });
                   }}});
    gallery_label("gallery.copy.sample.419");
    Card(props(), CardContent{[] { gallery_label("gallery.copy.sample.420"); }},
         CardSlots{.title = CardTitle{[] { Text(TextProps{}.content(gallery_text("gallery.copy.property.082"))); }},
                   .cover = CardCover{[] { gallery_label("gallery.copy.sample.421"); }},
                   .actions = CardActions{[state] {
                       Space(SpaceProps{}, SpaceContent{[state] {
                                 Button(ButtonProps{}.onClick([state] { ++state->telemetry.activations; }),
                                        [] { gallery_label("gallery.complete.copy.196"); });
                                 Button(ButtonProps{}.onClick([state] { ++state->telemetry.activations; }),
                                        [] { gallery_label("gallery.complete.copy.197"); });
                             }});
                   }}});
    gallery_label("gallery.copy.sample.422");
    Card(props().title(gallery_text("gallery.copy.property.083")).size(CardSize::Small),
         CardContent{[] { gallery_label("gallery.complete.copy.198"); }});
    gallery_label("gallery.copy.sample.423");
    Card(props().title(gallery_text("gallery.copy.property.084")).variant(CardVariant::Borderless).hoverable(),
         CardContent{[state] {
             Button(ButtonProps{}.onClick([state] { ++state->telemetry.activations; }),
                    [] { gallery_label("gallery.complete.copy.199"); });
         }});
    gallery_label("gallery.copy.sample.424");
    Card(props().title(gallery_text("gallery.copy.property.085")).loading(),
         CardContent{[] { gallery_label("gallery.complete.copy.200"); }});
    ThemeConfig custom;
    custom.card.tokens.background = Color::rgba8(238, 246, 255);
    custom.card.tokens.header_background = Color::rgba8(220, 235, 255);
    custom.card.tokens.text = Color(0, 0, 0, 0.88F);
    custom.card.tokens.header_text = Color(0, 0, 0, 0.88F);
    custom.card.tokens.body_padding = dp(16);
    Theme(ThemeProps{}.config(custom), ThemeContent{[state, props] {
              ++state->telemetry.theme_content_runs;
              gallery_label("gallery.copy.sample.425");
              Card(props().title(gallery_text("gallery.complete.copy.410")),
                   CardContent{[] { gallery_label("gallery.copy.sample.426"); }});
          }});
    Button(ButtonProps{}.onClick([state] {
        state->card_loading.set(!state->card_loading.get());
        ++state->telemetry.activations;
    }),
           [] { gallery_label("gallery.copy.sample.427"); });
    gallery_target("gallery.card.toggle-loading");
    state->telemetry.live_samples += 8;
}

void add_anchor_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.428");
    Text(TextProps{}.content(bind([state] { return utf8("" + std::string{state->anchor_feedback.get().bytes()}); })));
    remember_target(state, "gallery.anchor.feedback", true);
    const std::vector<AnchorItem> items{
        AnchorItem{u8"overview", ryn::String{}, 0}
            .title(gallery_text("gallery.complete.copy.056"))
            .children({AnchorItem{u8"details", String{}, 240}.title(gallery_text("gallery.complete.copy.443"))}),
        AnchorItem{u8"api", ryn::String{}, 480}
            .title(gallery_text("gallery.complete.copy.057"))
            .richTitle(AnchorTitle{[] {
                Flex(FlexProps{}.gap(dp(4)), FlexContent{[] {
                         Icon(IconProps{}.name(IconName::SearchOutlined));
                         gallery_label("gallery.copy.sample.429");
                     }});
            }}),
        AnchorItem{u8"examples", String{}, 720}.title(gallery_text("gallery.complete.copy.444")),
        AnchorItem{u8"footer", ryn::String{}, 1080}.title(gallery_text("gallery.complete.copy.058")).targetOffset(24)};
    Anchor(AnchorProps{}
               .affix(state->anchor_affix)
               .showInkInFixed(true)
               .direction(bind([horizontal = state->anchor_horizontal] {
                   return horizontal.get() ? AnchorDirection::Horizontal : AnchorDirection::Vertical;
               }))
               .scroll(state->anchor_scroll)
               .items(bind([items, extra = state->anchor_extra] {
                   auto result = items;
                   if (!extra.get()) {
                       result.erase(result.begin() + 2);
                   }
                   return result;
               }))
               .onScrollTo([state](float offset) {
                   auto scroll = state->anchor_scroll.get();
                   scroll.offset = offset;
                   state->anchor_scroll.set(scroll);
               })
               .onClick([state](AnchorClickEvent& event) {
                   ++state->telemetry.activations;
                   gallery_value_changed<String>("gallery.anchor.activate", [state](String value) {
                       state->anchor_feedback.set(value);
                   })(event.key);
               })
               .onChange([state](String key) {
                   state->anchor_feedback.set(
                       GalleryMessage{"gallery.event.022", {{u8"v0", utf8(std::string{key.bytes()})}}});
               })
               .layout(LayoutStyle{}.width(state->cell_width)));
    remember_target(state, "gallery.anchor.component");
    Slider(SliderProps{}
               .limits(SliderLimits{0, 1020, 1})
               .value(bind([scroll = state->anchor_scroll] { return double(scroll.get().offset); }))
               .onChange([state](double value) {
                   auto scroll = state->anchor_scroll.get();
                   scroll.offset = float(value);
                   state->anchor_scroll.set(scroll);
               })
               .layout(LayoutStyle{}.width(state->cell_width)));
    remember_target(state, "gallery.anchor.scroll");
    ReferenceScrollViewport(
        bind([scroll = state->anchor_scroll] { return scroll.get().offset; }), state->cell_width, 180,
        ReferenceSurfaceContent{[] {
            Flex(FlexProps{}.vertical(true).gap(dp(0)), FlexContent{[] {
                     for (const auto* title :
                          {"gallery.complete.copy.498", "gallery.complete.copy.499", "gallery.complete.copy.500",
                           "gallery.complete.copy.501", "gallery.complete.copy.502"}) {
                         Text(TextProps{}.content(gallery_text(title)).layout(LayoutStyle{}.height(dp(240))));
                     }
                 }});
        }});
    remember_target(state, "gallery.anchor.viewport");
    Button(ButtonProps{}.onClick([state] { state->anchor_horizontal.set(!state->anchor_horizontal.get()); }),
           [] { gallery_label("gallery.copy.sample.430"); });
    remember_target(state, "gallery.anchor.direction");
    Button(ButtonProps{}.onClick([state] { state->anchor_affix.set(!state->anchor_affix.get()); }),
           [] { gallery_label("gallery.copy.sample.431"); });
    remember_target(state, "gallery.anchor.affix");
    Button(ButtonProps{}.onClick([state] { state->anchor_extra.set(!state->anchor_extra.get()); }),
           [] { gallery_label("gallery.copy.sample.432"); });
    remember_target(state, "gallery.anchor.topology");
    ++state->telemetry.live_samples;
}

void add_breadcrumb_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(gallery_text("gallery.copy.property.086"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.breadcrumb.trail", true);
             Text(TextProps{}.content(
                 bind([state] { return utf8("" + std::string{state->breadcrumb_feedback.get().bytes()}); })));
             remember_target(state, "gallery.breadcrumb.feedback", true);
             Breadcrumb(
                 BreadcrumbProps{}
                     .onActivate([state](BreadcrumbActivation event) {
                         state->breadcrumb_feedback.set(GalleryMessage{"gallery.event.023",
                                                                       {{u8"v0", utf8(std::string{event.key.bytes()})},
                                                                        {u8"v1", utf8(std::to_string(event.index))}}});
                     })
                     .separator(BreadcrumbSeparator{[] { Text(u8"›"); }})
                     .layout(LayoutStyle{}
                                 .width(bind([state] { return dp(state->document_width.get().value() - 16); }))
                                 .margin_left(dp(8))
                                 .margin_top(dp(4))
                                 .margin_bottom(dp(4))),
                 {BreadcrumbItem{u8"home", ryn::String{}}
                      .title(gallery_text("gallery.complete.copy.059"))
                      .disabled(state->breadcrumb_disabled)
                      .icon(BreadcrumbIcon{[] { Icon(IconProps{}.name(IconName::HomeOutlined)); }}),
                  BreadcrumbItem{u8"projects", ryn::String{}}
                      .title(gallery_text("gallery.complete.copy.060"))
                      .title(state->breadcrumb_title)
                      .visible(state->breadcrumb_visible)
                      .disabled(state->breadcrumb_disabled),
                  BreadcrumbItem{u8"draft", ryn::String{}}
                      .title(gallery_text("gallery.complete.copy.061"))
                      .visible(state->breadcrumb_draft_visible)
                      .disabled(state->breadcrumb_disabled)
                      .richTitle(BreadcrumbTitle{[state] {
                          Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(4)), FlexContent{[state] {
                                   Input(InputProps{}
                                             .defaultValue(
                                                 gallery_initial_text("gallery.breadcrumb.draft", u8"Breadcrumb draft"))
                                             .placeholder(gallery_text("gallery.copy.property.087"))
                                             .ref(state->breadcrumb_ref)
                                             .onChange(gallery_value_changed<ryn::String>(
                                                 "gallery.breadcrumb.draft",
                                                 [state](String value) { state->breadcrumb_draft.set(value); }))
                                             .layout(LayoutStyle{}.width(dp(150))));
                                   remember_target(state, "gallery.breadcrumb.draft");
                                   Button(ButtonProps{}.onClick([state] {
                                       state->breadcrumb_feedback.set(GalleryMessage{
                                           "gallery.event.024",
                                           {{u8"v0", utf8(std::string{state->breadcrumb_draft.get().bytes()})}}});
                                   }),
                                          [] { gallery_label("gallery.copy.sample.433"); });
                                   remember_target(state, "gallery.breadcrumb.apply");
                               }});
                      }})});
             remember_target(state, "gallery.breadcrumb.component");
             gallery_label("gallery.copy.sample.434");
             Breadcrumb(
                 BreadcrumbProps{}.layout(LayoutStyle{}.margin_left(dp(8))),
                 {BreadcrumbItem{u8"preview", ryn::String{}}
                      .title(gallery_text("gallery.complete.copy.062"))
                      .richTitle(BreadcrumbTitle{[state] {
                          Input(InputProps{}
                                    .defaultValue(
                                        gallery_initial_text("gallery.breadcrumb.display-draft", u8"Display draft"))
                                    .onChange(gallery_value_changed<ryn::String>("gallery.breadcrumb.display-draft"))
                                    .layout(LayoutStyle{}.width(dp(150))));
                          remember_target(state, "gallery.breadcrumb.display-draft");
                      }}),
                  BreadcrumbItem{u8"read", ryn::String{}}.title(gallery_text("gallery.complete.copy.063"))});
             remember_target(state, "gallery.breadcrumb.display-component");
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}.onClick([state] {
                           state->breadcrumb_title.set(state->breadcrumb_title.is("gallery.complete.copy.130")
                                                           ? GalleryMessage{"gallery.complete.copy.139", {}}
                                                           : GalleryMessage{"gallery.complete.copy.130", {}});
                       }),
                              [] { gallery_label("gallery.copy.sample.435"); });
                       remember_target(state, "gallery.breadcrumb.rename");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->breadcrumb_visible.set(!state->breadcrumb_visible.get()); }),
                              [] { gallery_label("gallery.copy.sample.436"); });
                       remember_target(state, "gallery.breadcrumb.visible");
                       Button(ButtonProps{}.onClick([state] {
                           state->breadcrumb_draft_visible.set(!state->breadcrumb_draft_visible.get());
                       }),
                              [] { gallery_label("gallery.copy.sample.437"); });
                       remember_target(state, "gallery.breadcrumb.draft-visible");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->breadcrumb_disabled.set(!state->breadcrumb_disabled.get()); }),
                              [] { gallery_label("gallery.copy.sample.438"); });
                       remember_target(state, "gallery.breadcrumb.disabled");
                       Button(ButtonProps{}.onClick([state] {
                           state->breadcrumb_title.set(GalleryMessage{"gallery.complete.copy.130", {}});
                           state->breadcrumb_visible.set(true);
                           state->breadcrumb_draft_visible.set(true);
                           state->breadcrumb_disabled.set(false);
                       }),
                              [] { gallery_label("gallery.copy.sample.439"); });
                       remember_target(state, "gallery.breadcrumb.restore");
                       Button(ButtonProps{}.onClick([state] { static_cast<void>(state->breadcrumb_ref.focus()); }),
                              [] { gallery_label("gallery.copy.sample.440"); });
                       remember_target(state, "gallery.breadcrumb.focus");
                   }});
         }});
    ++state->telemetry.live_samples;
}

void add_breadcrumb_dynamic_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.441");
    const auto home = BreadcrumbItem{u8"home", String{}}
                          .title(gallery_text("gallery.complete.copy.522"))
                          .icon(BreadcrumbIcon{[] { Icon(IconProps{}.name(IconName::HomeOutlined)); }})
                          .menu(MenuProps{}
                                    .items({{u8"overview", gallery_text("gallery.complete.copy.525")},
                                            {u8"settings", gallery_text("gallery.complete.copy.526")}})
                                    .onClick([state](MenuClickInfo info) {
                                        ++state->telemetry.activations;
                                        state->breadcrumb_feedback.set(GalleryMessage{
                                            "gallery.event.025", {{u8"v0", utf8(std::string{info.key.bytes()})}}});
                                    }));
    const auto draft = BreadcrumbItem{u8"draft", String{}}
                           .title(gallery_text("gallery.complete.copy.523"))
                           .richTitle(BreadcrumbTitle{[state] {
                               Input(InputProps{}
                                         .value(state->breadcrumb_draft)
                                         .placeholder(gallery_text("gallery.complete.copy.411"))
                                         .onChange([state](String value) { state->breadcrumb_draft.set(value); })
                                         .layout(LayoutStyle{}.width(dp(140))));
                           }});
    const auto extra = BreadcrumbItem{u8"extra", String{}}
                           .title(gallery_text("gallery.complete.copy.484"))
                           .separator(BreadcrumbSeparator{[] { Text(u8"›"); }});
    const auto end = BreadcrumbItem{u8"end", String{}}.title(gallery_text("gallery.complete.copy.524"));
    Breadcrumb(BreadcrumbProps{}
                   .items(bind([state, home, draft, extra, end] {
                       std::vector<BreadcrumbItem> result{home, draft};
                       if (state->breadcrumb_extra.get()) {
                           result.push_back(extra);
                       }
                       result.push_back(end);
                       if (state->breadcrumb_reversed.get()) {
                           std::reverse(result.begin(), result.end());
                       }
                       return result;
                   }))
                   .direction(bind([rtl = state->breadcrumb_rtl] {
                       return rtl.get() ? TextDirection::RightToLeft : TextDirection::LeftToRight;
                   }))
                   .onActivate([state](BreadcrumbActivation event) {
                       state->breadcrumb_feedback.set(GalleryMessage{"gallery.event.026",
                                                                     {{u8"v0", utf8(std::string{event.key.bytes()})},
                                                                      {u8"v1", utf8(std::to_string(event.index))}}});
                   })
                   .layout(LayoutStyle{}.width(state->cell_width)));
    remember_target(state, "gallery.breadcrumb.dynamic-component");
    Button(ButtonProps{}.onClick([state] { state->breadcrumb_reversed.set(!state->breadcrumb_reversed.get()); }),
           [] { gallery_label("gallery.copy.sample.442"); });
    remember_target(state, "gallery.breadcrumb.reorder");
    Button(ButtonProps{}.onClick([state] { state->breadcrumb_extra.set(!state->breadcrumb_extra.get()); }),
           [] { gallery_label("gallery.copy.sample.443"); });
    remember_target(state, "gallery.breadcrumb.topology");
    Button(ButtonProps{}.onClick([state] { state->breadcrumb_rtl.set(!state->breadcrumb_rtl.get()); }),
           [] { gallery_label("gallery.copy.sample.444"); });
    remember_target(state, "gallery.breadcrumb.direction");
    ++state->telemetry.live_samples;
}

void add_steps_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(gallery_text("gallery.copy.property.088"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.steps.workflow", true);
             Text(TextProps{}.content(bind([state] {
                 return state->i18n.translate(
                     gallery_message("gallery.snapshot.004"),
                     MessageArgs{{u8"v0", utf8(std::to_string(state->steps_current.get()))},
                                 {u8"v1", utf8(std::string{state->steps_feedback.get().bytes()})}});
             })));
             remember_target(state, "gallery.steps.feedback", true);
             Steps(StepsProps{}
                       .current(state->steps_current)
                       .vertical(state->steps_vertical)
                       .size(state->steps_size)
                       .status(state->steps_status)
                       .onChange([state](std::size_t index) {
                           state->steps_current.set(index);
                           state->steps_feedback.set(
                               GalleryMessage{"gallery.event.027", {{u8"v0", utf8(std::to_string(index))}}});
                       })
                       .layout(LayoutStyle{}
                                   .width(bind([state] { return dp(state->document_width.get().value() - 16); }))
                                   .margin_left(dp(8))
                                   .margin_top(dp(4))
                                   .margin_bottom(dp(4))),
                   {StepsItem{u8"details", ryn::String{}}
                        .title(gallery_text("gallery.complete.copy.064"))
                        .subtitle(gallery_text("gallery.copy.property.089"))
                        .disabled(state->steps_disabled)
                        .richDescription(StepsDescription{[state] {
                            Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(4)), FlexContent{[state] {
                                     Input(
                                         InputProps{}
                                             .defaultValue(gallery_initial_text("gallery.steps.draft", u8"Steps draft"))
                                             .placeholder(gallery_text("gallery.copy.property.090"))
                                             .ref(state->steps_ref)
                                             .onChange(gallery_value_changed<ryn::String>(
                                                 "gallery.steps.draft",
                                                 [state](String value) { state->steps_draft.set(value); }))
                                             .layout(LayoutStyle{}.width(dp(120))));
                                     remember_target(state, "gallery.steps.draft");
                                     Button(ButtonProps{}.onClick([state] {
                                         state->steps_feedback.set(GalleryMessage{
                                             "gallery.event.028",
                                             {{u8"v0", utf8(std::string{state->steps_draft.get().bytes()})}}});
                                     }),
                                            [] { gallery_label("gallery.copy.sample.445"); });
                                     remember_target(state, "gallery.steps.save");
                                 }});
                        }}),
                    StepsItem{u8"review", ryn::String{}}
                        .title(gallery_text("gallery.complete.copy.065"))
                        .description(gallery_text("gallery.copy.property.091"))
                        .visible(state->steps_visible)
                        .disabled(state->steps_disabled)
                        .icon(StepsIcon{[] { Icon(IconProps{}.name(IconName::SearchOutlined)); }}),
                    StepsItem{u8"publish", ryn::String{}}
                        .title(gallery_text("gallery.complete.copy.066"))
                        .description(gallery_text("gallery.copy.property.092"))});
             remember_target(state, "gallery.steps.component");
             Space(
                 SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                 SpaceContent{[state] {
                     Button(ButtonProps{}.onClick([state] {
                         state->steps_current.set(state->steps_current.get() > 0 ? state->steps_current.get() - 1 : 0);
                         state->steps_feedback.set(GalleryMessage{"gallery.feedback.114", {}});
                     }),
                            [] { gallery_label("gallery.copy.sample.446"); });
                     remember_target(state, "gallery.steps.previous");
                     Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                         state->steps_current.set(std::min<std::size_t>(3, state->steps_current.get() + 1));
                         state->steps_feedback.set(state->steps_current.get() == 3
                                                       ? GalleryMessage{"gallery.feedback.115", {}}
                                                       : GalleryMessage{"gallery.feedback.116", {}});
                     }),
                            [] { gallery_label("gallery.copy.sample.447"); });
                     remember_target(state, "gallery.steps.next");
                     Button(ButtonProps{}.onClick([state] {
                         state->steps_current.set(0);
                         state->steps_visible.set(true);
                         state->steps_disabled.set(false);
                         state->steps_status.set(StepsStatus::Process);
                         state->steps_feedback.set(GalleryMessage{"gallery.feedback.117", {}});
                     }),
                            [] { gallery_label("gallery.copy.sample.448"); });
                     remember_target(state, "gallery.steps.reset");
                     Button(ButtonProps{}.onClick([state] { state->steps_vertical.set(!state->steps_vertical.get()); }),
                            [] { gallery_label("gallery.copy.sample.449"); });
                     remember_target(state, "gallery.steps.vertical");
                     Button(ButtonProps{}.onClick([state] {
                         state->steps_size.set(state->steps_size.get() == StepsSize::Default ? StepsSize::Small
                                                                                             : StepsSize::Default);
                     }),
                            [] { gallery_label("gallery.copy.sample.450"); });
                     remember_target(state, "gallery.steps.size");
                     Button(ButtonProps{}.onClick([state] {
                         state->steps_status.set(state->steps_status.get() == StepsStatus::Process
                                                     ? StepsStatus::Error
                                                     : StepsStatus::Process);
                     }),
                            [] { gallery_label("gallery.copy.sample.451"); });
                     remember_target(state, "gallery.steps.status");
                     Button(ButtonProps{}.onClick([state] { state->steps_visible.set(!state->steps_visible.get()); }),
                            [] { gallery_label("gallery.copy.sample.452"); });
                     remember_target(state, "gallery.steps.visible");
                     Button(ButtonProps{}.onClick([state] { state->steps_disabled.set(!state->steps_disabled.get()); }),
                            [] { gallery_label("gallery.copy.sample.453"); });
                     remember_target(state, "gallery.steps.disabled");
                     Button(ButtonProps{}.onClick([state] { static_cast<void>(state->steps_ref.focus()); }),
                            [] { gallery_label("gallery.copy.sample.454"); });
                     remember_target(state, "gallery.steps.focus");
                 }});
         }});
    ++state->telemetry.live_samples;
    auto dynamic = Signal<std::vector<StepsItem>>{std::vector<StepsItem>{
        StepsItem{u8"one", ryn::String{}}
            .title(gallery_text("gallery.complete.copy.067"))
            .richDescription(StepsDescription{[] { Input(InputProps{}.defaultValue(u8"保留步骤草稿")); }}),
        StepsItem{u8"two", ryn::String{}}.title(gallery_text("gallery.complete.copy.068")),
        StepsItem{u8"three", ryn::String{}}.title(gallery_text("gallery.complete.copy.069"))}};
    gallery_label("gallery.copy.sample.455");
    Steps(StepsProps{}.defaultCurrent(1).percent(65).onChange([state](std::size_t index) {
        state->steps_feedback.set(GalleryMessage{"gallery.event.029", {{u8"v0", utf8(std::to_string(index))}}});
    }),
          dynamic);
    remember_target(state, "gallery.steps.dynamic");
    Button(ButtonProps{}.onClick([dynamic] {
        auto values = dynamic.get();
        std::rotate(values.begin(), values.begin() + 1, values.end());
        dynamic.set(std::move(values));
    }),
           [] { gallery_label("gallery.copy.sample.456"); });
    remember_target(state, "gallery.steps.reorder");
    for (auto variant : {StepsType::Dot, StepsType::Inline, StepsType::Navigation, StepsType::Panel}) {
        Text(String::from_utf8(std::u8string_view{variant == StepsType::Dot          ? u8"Dot"
                                                  : variant == StepsType::Inline     ? u8"Inline"
                                                  : variant == StepsType::Navigation ? u8"Navigation"
                                                                                     : u8"Panel"})
                 .value());
        Steps(StepsProps{}
                  .type(variant)
                  .rtl()
                  .defaultCurrent(1)
                  .maxCount(variant == StepsType::Dot ? 3 : 4)
                  .onChange([](std::size_t) {}),
              {StepsItem{u8"one", ryn::String{}}
                   .title(gallery_text("gallery.complete.copy.070"))
                   .richDescription(StepsDescription{[] { Input(InputProps{}.defaultValue(u8"步骤说明草稿")); }}),
               StepsItem{u8"two", String{}}.title(gallery_text("gallery.complete.copy.440")),
               StepsItem{u8"three", String{}}.title(gallery_text("gallery.complete.copy.441")),
               StepsItem{u8"four", String{}}.title(gallery_text("gallery.complete.copy.442"))});
        remember_target(state, variant == StepsType::Dot          ? "gallery.steps.dot"
                               : variant == StepsType::Inline     ? "gallery.steps.inline"
                               : variant == StepsType::Navigation ? "gallery.steps.navigation"
                                                                  : "gallery.steps.panel");
    }
    state->telemetry.live_samples += 5;
}

void add_time_picker_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    TimePickerRules rules;
    rules.minute_step = 15;
    rules.min = TimeOfDay{8};
    rules.max = TimeOfDay{18};
    rules.disabled = [](TimeOfDay value) {
        return value.hour() == 12;
    };
    Flex(
        FlexProps{}
            .vertical(true)
            .align(FlexAlign::Start)
            .gap(dp(8))
            .layout(LayoutStyle{}.width(state->document_width)),
        FlexContent{[state, rules] {
            Text(TextProps{}
                     .content(gallery_text("gallery.copy.property.093"))
                     .layout(LayoutStyle{}.margin_left(dp(1))));
            remember_target(state, "gallery.time-picker.controlled", true);
            Text(
                TextProps{}
                    .content(bind([state] {
                        const auto value = state->time_value.get();
                        return state->i18n.translate(
                            gallery_message("gallery.snapshot.005"),
                            MessageArgs{
                                {u8"v0",
                                 utf8(
                                     (value
                                          ? std::string{format_time_of_day(*value, TimePickerPrecision::Minute).bytes()}
                                          : std::string{state->i18n.translate(gallery_message("gallery.state.no-time"))
                                                            .bytes()}))},
                                {u8"v1",
                                 utf8((
                                     state->time_confirm.get()
                                         ? std::string{state->i18n.translate(gallery_message("gallery.state.confirm"))
                                                           .bytes()}
                                         : std::string{state->i18n.translate(gallery_message("gallery.state.immediate"))
                                                           .bytes()}))},
                                {u8"v2",
                                 utf8((state->time_accept.get()
                                           ? std::string{state->i18n
                                                             .translate(
                                                                 gallery_message("gallery.state.accept-selection"))
                                                             .bytes()}
                                           : std::string{state->i18n.translate(gallery_message("gallery.state.locked"))
                                                             .bytes()}))},
                                {u8"v3",
                                 utf8((state->time_input_read_only.get()
                                           ? std::string{state->i18n
                                                             .translate(gallery_message("gallery.state.input-readonly"))
                                                             .bytes()}
                                           : std::string{state->i18n
                                                             .translate(gallery_message("gallery.state.input-editable"))
                                                             .bytes()}))},
                                {u8"v4", utf8(std::string{state->time_request.get().bytes()})}});
                    }))
                    .layout(LayoutStyle{}.margin_left(dp(1))));
            remember_target(state, "gallery.time-picker.value", true);
            gallery_label("gallery.copy.sample.457");
            TimePicker(
                TimePickerProps{}
                    .value(state->time_value)
                    .open(state->time_open)
                    .onChange([state](TimePickerValue value) {
                        state->time_request.set(
                            (value ? (state->time_accept.get()
                                          ? GalleryMessage{"gallery.event.030.yes",
                                                           {{u8"v1",
                                                             utf8(std::string{
                                                                 format_time_of_day(*value, TimePickerPrecision::Minute)
                                                                     .bytes()})}}}
                                          : GalleryMessage{"gallery.event.030.no",
                                                           {{u8"v1",
                                                             utf8(std::string{format_time_of_day(
                                                                                  *value, TimePickerPrecision::Minute)
                                                                                  .bytes()})}}})
                                   : (state->time_accept.get() ? GalleryMessage{"gallery.event.030.clear.yes", {}}
                                                               : GalleryMessage{"gallery.event.030.clear.no", {}})));
                        if (state->time_accept.get()) {
                            state->time_value.set(value);
                        }
                    })
                    .onOpenChange([state](bool value) { state->time_open.set(value); })
                    .defaultPanelValue(TimeOfDay{8})
                    .precision(TimePickerPrecision::Minute)
                    .rules(rules)
                    .needConfirm(state->time_confirm)
                    .allowClear(true)
                    .status(InputStatus::Default)
                    .disabled(false)
                    .inputReadOnly(state->time_input_read_only)
                    .ref(state->time_ref)
                    .layout(LayoutStyle{}.width(dp(280))),
                TimePickerFooter{[] {
                    Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[] {
                             gallery_label("gallery.copy.sample.458");
                             Input(
                                 InputProps{}
                                     .defaultValue(gallery_initial_text("gallery.time-picker.note", u8"保留的时间备注"))
                                     .onChange(gallery_value_changed<ryn::String>("gallery.time-picker.note"))
                                     .layout(LayoutStyle{}.width(dp(220))));
                             gallery_target("gallery.time-picker.note");
                         }});
                }});
            Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                  SpaceContent{[state] {
                      Button(ButtonProps{}.onClick([state] {
                          state->time_value.set(TimeOfDay{8});
                          state->time_open.set(false);
                          state->time_confirm.set(true);
                          state->time_accept.set(true);
                          state->time_input_read_only.set(false);
                          state->time_request.set(GalleryMessage{"gallery.feedback.118", {}});
                      }),
                             [] { gallery_label("gallery.copy.sample.459"); });
                      gallery_target("gallery.time-picker.restore");
                      Button(ButtonProps{}.onClick([state] { state->time_confirm.set(!state->time_confirm.get()); }),
                             [] { gallery_label("gallery.copy.sample.460"); });
                      gallery_target("gallery.time-picker.mode");
                      Button(ButtonProps{}.onClick([state] { state->time_accept.set(!state->time_accept.get()); }),
                             [] { gallery_label("gallery.copy.sample.461"); });
                      gallery_target("gallery.time-picker.authority");
                      Button(ButtonProps{}.onClick(
                                 [state] { state->time_input_read_only.set(!state->time_input_read_only.get()); }),
                             [] { gallery_label("gallery.copy.sample.462"); });
                      gallery_target("gallery.time-picker.input-readonly");
                  }});
        }});
    Text(TextProps{}.content(gallery_text("gallery.copy.property.094")).layout(LayoutStyle{}.margin_left(dp(1))));
    Text(
        TextProps{}
            .content(bind([state] {
                return state->i18n.translate(
                    gallery_message("gallery.snapshot.006"),
                    MessageArgs{
                        {u8"v0", utf8(std::string{state->time_feedback.get().bytes()})},
                        {u8"v1",
                         utf8((
                             state->time_form_disabled.get()
                                 ? std::string{state->i18n.translate(gallery_message("gallery.state.disabled")).bytes()}
                                 : std::string{state->i18n.translate(gallery_message("gallery.state.enabled"))
                                                   .bytes()}))},
                        {u8"v2",
                         utf8((
                             state->time_form_read_only.get()
                                 ? std::string{state->i18n.translate(gallery_message("gallery.state.readonly")).bytes()}
                                 : std::string{
                                       state->i18n.translate(gallery_message("gallery.state.editable")).bytes()}))}});
            }))
            .layout(LayoutStyle{}.margin_left(dp(1))));
    remember_target(state, "gallery.time-picker.status", true);
    Form(FormProps{}
             .ref(state->time_form_ref)
             .disabled(state->time_form_disabled)
             .readOnly(state->time_form_read_only)
             .layout(LayoutStyle{}
                         .width(bind([state] { return dp(state->document_width.get().value() - 2); }))
                         .margin_left(dp(1))
                         .margin_right(dp(1)))
             .onFinish([state](FormResult result) {
                 state->time_feedback.set(GalleryMessage{
                     "gallery.event.031",
                     {{u8"v0", utf8(std::string{std::get<String>(result.values.front().value).bytes()})}}});
             })
             .onFinishFailed(
                 [state](FormResult) { state->time_feedback.set(GalleryMessage{"gallery.feedback.119", {}}); }),
         FormContent{[state] {
             gallery_validated_item(FormItemProps{}
                                        .field(u8"time", state->time_field)
                                        .label(gallery_text("gallery.copy.property.095"))
                                        .help(state->time_help)
                                        .control(state->time_field_ref),
                                    state->time_field, state->time_issue, gallery_text("gallery.copy.property.095"),
                                    state->time_help, FormItemContent{[state] {
                                        const auto change = state->time_field.onChange();
                                        TimePickerRules form_rules;
                                        form_rules.min = TimeOfDay{8};
                                        form_rules.max = TimeOfDay{18};
                                        form_rules.disabled = [](TimeOfDay value) {
                                            return value.hour() == 12;
                                        };
                                        TimePicker(TimePickerProps{}
                                                       .value(bind([field = state->time_field] {
                                                           return parse_time_of_day(field.get().bytes());
                                                       }))
                                                       .onChange([change](TimePickerValue value) {
                                                           change(value ? format_time_of_day(*value) : String{});
                                                       })
                                                       .defaultPanelValue(TimeOfDay{8})
                                                       .precision(TimePickerPrecision::Second)
                                                       .rules(form_rules)
                                                       .needConfirm(true)
                                                       .allowClear(true)
                                                       .status(state->time_field.status())
                                                       .disabled(state->time_field.uneditable())
                                                       .inputReadOnly(state->time_field.readOnly())
                                                       .ref(state->time_field_ref)
                                                       .layout(LayoutStyle{}.width(dp(280))));
                                        remember_target(state, "gallery.time-picker.field");
                                    }});
         }});
    Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)), SpaceContent{[state] {
              Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                  if (!state->time_form_ref.submit()) {
                      state->time_feedback.set(GalleryMessage{"gallery.feedback.120", {}});
                  }
              }),
                     [] { gallery_label("gallery.copy.sample.463"); });
              gallery_target("gallery.time-picker.submit");
              Button(ButtonProps{}.onClick([state] {
                  if (state->time_form_ref.reset()) {
                      state->time_feedback.set(GalleryMessage{"gallery.feedback.121", {}});
                  }
              }),
                     [] { gallery_label("gallery.copy.sample.464"); });
              gallery_target("gallery.time-picker.reset");
              Button(
                  ButtonProps{}.onClick([state] { state->time_form_disabled.set(!state->time_form_disabled.get()); }),
                  [] { gallery_label("gallery.copy.sample.465"); });
              gallery_target("gallery.time-picker.disabled");
              Button(
                  ButtonProps{}.onClick([state] { state->time_form_read_only.set(!state->time_form_read_only.get()); }),
                  [] { gallery_label("gallery.copy.sample.466"); });
              gallery_target("gallery.time-picker.readonly");
              Button(ButtonProps{}.onClick([state] {
                  state->time_help.set(state->time_help.is("gallery.complete.copy.129")
                                           ? GalleryMessage{"gallery.complete.copy.138", {}}
                                           : GalleryMessage{"gallery.complete.copy.129", {}});
              }),
                     [] { gallery_label("gallery.copy.sample.467"); });
              gallery_target("gallery.time-picker.help");
          }});
    state->telemetry.live_samples += 2;
}

void add_descriptions_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(
        FlexProps{}
            .vertical(true)
            .align(FlexAlign::Start)
            .gap(dp(12))
            .layout(LayoutStyle{}.width(state->document_width)),
        FlexContent{[state] {
            Text(TextProps{}
                     .content(gallery_text("gallery.copy.property.096"))
                     .layout(LayoutStyle{}.margin_left(dp(1))));
            remember_target(state, "gallery.descriptions.editor", true);
            Text(TextProps{}.content(
                bind([state] { return utf8("" + std::string{state->details_feedback.get().bytes()}); })));
            remember_target(state, "gallery.descriptions.feedback", true);
            Descriptions(
                DescriptionsProps{}
                    .column(
                        bind([state] { return state->details_columns.get() && !state->narrow_layout.get() ? 2 : 1; }))
                    .bordered(state->details_bordered)
                    .labelPlacement(bind([state] {
                        return state->details_vertical.get() ? DescriptionsLabelPlacement::Vertical
                                                             : DescriptionsLabelPlacement::Horizontal;
                    }))
                    .size(state->details_size)
                    .title(DescriptionsTitle{[] { gallery_label("gallery.copy.sample.468"); }})
                    .layout(LayoutStyle{}
                                .width(bind([state] { return dp(state->document_width.get().value() - 16); }))
                                .margin_left(dp(8))
                                .margin_top(dp(4))
                                .margin_bottom(dp(4))),
                {DescriptionsItem{u8"name", DescriptionsLabel{[] { gallery_label("gallery.copy.sample.469"); }},
                                  DescriptionsContent{[state] {
                                      Input(InputProps{}
                                                .defaultValue(
                                                    gallery_initial_text("gallery.descriptions.name", u8"晨间项目"))
                                                .placeholder(gallery_text("gallery.copy.property.097"))
                                                .ref(state->details_ref)
                                                .onChange(gallery_value_changed<ryn::String>(
                                                    "gallery.descriptions.name",
                                                    [state](String value) { state->details_name.set(value); }))
                                                .layout(LayoutStyle{}
                                                            .width(dp(156))
                                                            .margin_left(dp(6))
                                                            .margin_right(dp(6))
                                                            .margin_top(dp(6))
                                                            .margin_bottom(dp(6))));
                                      gallery_target("gallery.descriptions.name");
                                  }}}
                     .span(state->details_span)
                     .visible(state->details_visible),
                 DescriptionsItem{
                     u8"role", DescriptionsLabel{[] { gallery_label("gallery.copy.sample.470"); }},
                     DescriptionsContent{[state] {
                         Select(SelectProps{}
                                    .value(state->details_role)
                                    .options(gallery_option_labels<ryn::SelectOption>(
                                        std::vector<SelectOption>{{u8"editor", u8"编辑者"}, {u8"reviewer", u8"审核者"}},
                                        {{u8"editor", "gallery.complete.copy.116"},
                                         {u8"reviewer", "gallery.complete.copy.117"}}))
                                    .onChange([state](SelectSelection value) { state->details_role.set(value); })
                                    .layout(LayoutStyle{}
                                                .width(dp(148))
                                                .margin_left(dp(6))
                                                .margin_right(dp(6))
                                                .margin_top(dp(6))
                                                .margin_bottom(dp(6))));
                     }}}
                     .visible(state->details_visible),
                 DescriptionsItem{u8"enabled", DescriptionsLabel{[] { gallery_label("gallery.copy.sample.471"); }},
                                  DescriptionsContent{[state] {
                                      Switch(SwitchProps{}
                                                 .checked(state->details_enabled)
                                                 .onChange([state](bool value) { state->details_enabled.set(value); })
                                                 .layout(LayoutStyle{}
                                                             .margin_left(dp(6))
                                                             .margin_right(dp(6))
                                                             .margin_top(dp(6))
                                                             .margin_bottom(dp(6))));
                                  }}},
                 DescriptionsItem{
                     u8"save", DescriptionsLabel{[] { gallery_label("gallery.copy.sample.472"); }},
                     DescriptionsContent{[state] {
                         Button(ButtonProps{}
                                    .type(ButtonType::Primary)
                                    .onClick([state] {
                                        state->details_feedback.set(
                                            (state->details_role.get() == SelectSelection{u8"reviewer"}
                                                 ? (state->details_enabled.get()
                                                        ? GalleryMessage{"gallery.event.032.reviewer.enabled",
                                                                         {{u8"v0",
                                                                           utf8(std::string{
                                                                               state->details_name.get().bytes()})}}}
                                                        : GalleryMessage{"gallery.event.032.reviewer.disabled",
                                                                         {{u8"v0",
                                                                           utf8(std::string{
                                                                               state->details_name.get().bytes()})}}})
                                                 : (state->details_enabled.get()
                                                        ? GalleryMessage{"gallery.event.032.editor.enabled",
                                                                         {{u8"v0",
                                                                           utf8(std::string{
                                                                               state->details_name.get().bytes()})}}}
                                                        : GalleryMessage{
                                                              "gallery.event.032.editor.disabled",
                                                              {{u8"v0", utf8(std::string{
                                                                            state->details_name.get().bytes()})}}})));
                                    })
                                    .layout(LayoutStyle{}
                                                .margin_left(dp(6))
                                                .margin_right(dp(6))
                                                .margin_top(dp(6))
                                                .margin_bottom(dp(6))),
                                [] { gallery_label("gallery.copy.sample.473"); });
                     }}}});
            Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                  SpaceContent{[state] {
                      Button(
                          ButtonProps{}.onClick([state] { state->details_columns.set(!state->details_columns.get()); }),
                          [] { gallery_label("gallery.copy.sample.474"); });
                      gallery_target("gallery.descriptions.columns");
                      Button(ButtonProps{}.onClick(
                                 [state] { state->details_span.set(state->details_span.get() == 1 ? 2 : 1); }),
                             [] { gallery_label("gallery.copy.sample.475"); });
                      gallery_target("gallery.descriptions.span");
                      Button(
                          ButtonProps{}.onClick([state] { state->details_visible.set(!state->details_visible.get()); }),
                          [] { gallery_label("gallery.copy.sample.476"); });
                      gallery_target("gallery.descriptions.visible");
                      Button(ButtonProps{}.onClick(
                                 [state] { state->details_vertical.set(!state->details_vertical.get()); }),
                             [] { gallery_label("gallery.copy.sample.477"); });
                      gallery_target("gallery.descriptions.vertical");
                      Button(ButtonProps{}.onClick(
                                 [state] { state->details_bordered.set(!state->details_bordered.get()); }),
                             [] { gallery_label("gallery.copy.sample.478"); });
                      gallery_target("gallery.descriptions.bordered");
                      Button(ButtonProps{}.onClick([state] {
                          state->details_size.set(
                              state->details_size.get() == DescriptionsSize::Default  ? DescriptionsSize::Middle
                              : state->details_size.get() == DescriptionsSize::Middle ? DescriptionsSize::Small
                                                                                      : DescriptionsSize::Default);
                      }),
                             [] { gallery_label("gallery.copy.sample.479"); });
                      gallery_target("gallery.descriptions.size");
                      Button(ButtonProps{}.onClick([state] {
                          state->details_visible.set(true);
                          static_cast<void>(state->details_ref.focus());
                      }),
                             [] { gallery_label("gallery.copy.sample.480"); });
                      gallery_target("gallery.descriptions.focus");
                  }});
        }});
    ++state->telemetry.live_samples;
    add_descriptions_contract_samples(state);
}

void add_skeleton_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<bool> active{false};
    Button(ButtonProps{}.onClick([active]() mutable { active.set(!active.get()); }),
           [] { gallery_label("gallery.copy.sample.481"); });
    gallery_target("gallery.skeleton.active");
    Flex(
        FlexProps{}
            .vertical(true)
            .align(FlexAlign::Start)
            .gap(dp(12))
            .layout(LayoutStyle{}.width(state->document_width)),
        FlexContent{[state, active] {
            Text(TextProps{}
                     .content(gallery_text("gallery.copy.property.098"))
                     .layout(LayoutStyle{}.margin_left(dp(1))));
            remember_target(state, "gallery.skeleton.preview", true);
            Text(TextProps{}.content(
                bind([state] { return utf8("" + std::string{state->skeleton_feedback.get().bytes()}); })));
            remember_target(state, "gallery.skeleton.feedback", true);
            Space(
                SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                SpaceContent{[state] {
                    Button(
                        ButtonProps{}.onClick([state] { state->skeleton_loading.set(!state->skeleton_loading.get()); }),
                        [] { gallery_label("gallery.copy.sample.482"); });
                    gallery_target("gallery.skeleton.loading");
                    Button(ButtonProps{}.onClick([state] { state->skeleton_title.set(!state->skeleton_title.get()); }),
                           [] { gallery_label("gallery.copy.sample.483"); });
                    gallery_target("gallery.skeleton.title");
                    Button(ButtonProps{}.onClick(
                               [state] { state->skeleton_paragraph.set((state->skeleton_paragraph.get() + 1) % 3); }),
                           [] { gallery_label("gallery.copy.sample.484"); });
                    gallery_target("gallery.skeleton.paragraph");
                    Button(
                        ButtonProps{}.onClick([state] { state->skeleton_avatar.set(!state->skeleton_avatar.get()); }),
                        [] { gallery_label("gallery.copy.sample.485"); });
                    gallery_target("gallery.skeleton.avatar");
                    Button(ButtonProps{}.onClick([state] { state->skeleton_round.set(!state->skeleton_round.get()); }),
                           [] { gallery_label("gallery.copy.sample.486"); });
                    gallery_target("gallery.skeleton.round");
                    Button(ButtonProps{}.onClick([state] {
                        state->skeleton_loading.set(false);
                        static_cast<void>(state->skeleton_ref.focus());
                    }),
                           [] { gallery_label("gallery.copy.sample.487"); });
                    gallery_target("gallery.skeleton.focus");
                }});
            Skeleton(
                SkeletonProps{}
                    .active(active)
                    .loading(state->skeleton_loading)
                    .title(bind([state] { return SkeletonTitle{state->skeleton_title.get()}; }))
                    .paragraph(bind([state] {
                        SkeletonParagraph value;
                        value.enabled = state->skeleton_paragraph.get() != 2;
                        value.rows = state->skeleton_paragraph.get() == 0 ? 3 : 1;
                        return value;
                    }))
                    .avatar(bind([state] { return SkeletonAvatarOptions{state->skeleton_avatar.get()}; }))
                    .round(state->skeleton_round)
                    .layout(LayoutStyle{}
                                .width(bind([state] { return dp(state->document_width.get().value() - 16); }))
                                .margin_left(dp(8))
                                .margin_top(dp(4))
                                .margin_bottom(dp(4))),
                SkeletonContent{[state] {
                    Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(12)), FlexContent{[state] {
                             gallery_label("gallery.copy.sample.488");
                             Input(
                                 InputProps{}
                                     .defaultValue(gallery_initial_text("gallery.skeleton.draft", u8"今天整理项目资料"))
                                     .placeholder(gallery_text("gallery.copy.property.099"))
                                     .ref(state->skeleton_ref)
                                     .onChange(gallery_value_changed<ryn::String>(
                                         "gallery.skeleton.draft",
                                         [state](String value) { state->skeleton_draft.set(value); }))
                                     .layout(LayoutStyle{}
                                                 .width(dp(260))
                                                 .margin_left(dp(6))
                                                 .margin_right(dp(6))
                                                 .margin_top(dp(6))
                                                 .margin_bottom(dp(6))));
                             gallery_target("gallery.skeleton.draft");
                             Select(
                                 SelectProps{}
                                     .value(state->skeleton_audience)
                                     .options(gallery_option_labels<ryn::SelectOption>(
                                         std::vector<SelectOption>{{u8"team", u8"团队可见"}, {u8"private", u8"仅自己"}},
                                         {{u8"team", "gallery.complete.copy.118"},
                                          {u8"private", "gallery.complete.copy.119"}}))
                                     .onChange([state](SelectSelection value) { state->skeleton_audience.set(value); })
                                     .layout(LayoutStyle{}
                                                 .width(dp(180))
                                                 .margin_left(dp(6))
                                                 .margin_right(dp(6))
                                                 .margin_top(dp(6))
                                                 .margin_bottom(dp(6))));
                             Button(
                                 ButtonProps{}
                                     .type(ButtonType::Primary)
                                     .onClick([state] {
                                         state->skeleton_feedback.set((
                                             state->skeleton_audience.get() == SelectSelection{u8"private"}
                                                 ? GalleryMessage{"gallery.event.033.yes",
                                                                  {{u8"v0", utf8(std::string{
                                                                                state->skeleton_draft.get().bytes()})}}}
                                                 : GalleryMessage{
                                                       "gallery.event.033.no",
                                                       {{u8"v0",
                                                         utf8(std::string{state->skeleton_draft.get().bytes()})}}}));
                                     })
                                     .layout(LayoutStyle{}
                                                 .margin_left(dp(6))
                                                 .margin_right(dp(6))
                                                 .margin_top(dp(6))
                                                 .margin_bottom(dp(6))),
                                 [] { gallery_label("gallery.copy.sample.489"); });
                         }});
                }});
            gallery_label("gallery.copy.sample.490");
            Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                  SpaceContent{[state] {
                      Button(ButtonProps{}.onClick(
                                 [state] { state->skeleton_shape.set((state->skeleton_shape.get() + 1) % 4); }),
                             [] { gallery_label("gallery.copy.sample.491"); });
                      gallery_target("gallery.skeleton.shape");
                      Button(
                          ButtonProps{}.onClick([state] { state->skeleton_block.set(!state->skeleton_block.get()); }),
                          [] { gallery_label("gallery.copy.sample.492"); });
                      gallery_target("gallery.skeleton.block");
                  }});
            Flex(FlexProps{}
                     .vertical(true)
                     .align(FlexAlign::Start)
                     .gap(dp(12))
                     .layout(LayoutStyle{}.width(state->document_width)),
                 FlexContent{[state, active] {
                     const auto size = bind([state] {
                         return state->skeleton_shape.get() % 2 == 0 ? ControlSize::Middle : ControlSize::Large;
                     });
                     SkeletonAvatar(SkeletonAvatarProps{}.active(active).size(size).shape(bind([state] {
                         return state->skeleton_shape.get() % 2 == 0 ? SkeletonAvatarShape::Circle
                                                                     : SkeletonAvatarShape::Square;
                     })));
                     SkeletonButton(SkeletonButtonProps{}
                                        .active(active)
                                        .size(size)
                                        .block(state->skeleton_block)
                                        .shape(bind([state] {
                                            return static_cast<SkeletonButtonShape>(state->skeleton_shape.get());
                                        })));
                     SkeletonInput(SkeletonInputProps{}.active(active).size(size).block(state->skeleton_block));
                     gallery_label("gallery.copy.sample.493");
                     SkeletonImage(SkeletonImageProps{}.active(active));
                     gallery_label("gallery.copy.sample.494");
                     SkeletonNode(SkeletonNodeProps{}.active(active),
                                  SkeletonNodeContent{[] { Icon(IconProps{}.name(IconName::BarChartOutlined)); }});
                 }});
        }});
    state->telemetry.live_samples += 4;
}

void add_popover_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    if (state->page_component == "ant.component.popover") {
        Signal<bool> rtl{false};
        Signal<bool> wireframe{false};
        GalleryFeedback feedback{GalleryMessage{"gallery.feedback.038", {}}, useI18n()};
        Text(TextProps{}.content(feedback));
        Theme(ThemeProps{}.config(bind([wireframe] {
            ThemeConfig config;
            config.seed.wireframe = wireframe.get();
            return config;
        })),
              ThemeContent{[rtl, feedback] {
                  Popover(PopoverProps{}
                              .title(gallery_text("gallery.copy.property.100"))
                              .animated()
                              .triggers(PopoverTriggers{true, true, true, true})
                              .direction(bind([rtl] {
                                  return rtl.get() ? TextDirection::RightToLeft : TextDirection::LeftToRight;
                              }))
                              .placement(PopoverPlacement::BottomLeft),
                          PopoverTrigger{[] {
                              Button(ButtonProps{}, [] { gallery_label("gallery.copy.sample.495"); });
                              gallery_target("gallery.popover.context-trigger");
                          }},
                          {.content = PopoverContent{[feedback] {
                               Flex(FlexProps{}
                                        .vertical(true)
                                        .align(FlexAlign::Start)
                                        .gap(dp(8))
                                        .layout(LayoutStyle{}.width(dp(240))),
                                    FlexContent{[feedback] {
                                        for (int i = 0; i < 40; ++i) {
                                            Text(TextProps{}.content(gallery_text(
                                                "gallery.complete.copy.399",
                                                MessageArgs{{u8"index", static_cast<std::uint64_t>(i + 1)}})));
                                        }
                                        Input(InputProps{}.defaultValue(u8"底部草稿保持不变"));
                                        gallery_target("gallery.popover.scroll-draft");
                                        Button(ButtonProps{}.onClick([feedback] {
                                            feedback.set(GalleryMessage{"gallery.feedback.122", {}});
                                        }),
                                               [] { gallery_label("gallery.copy.sample.496"); });
                                        gallery_target("gallery.popover.scroll-save");
                                    }});
                           }}});
              }});
        Space(SpaceProps{}.wrap(true), SpaceContent{[rtl, wireframe] {
                  Button(ButtonProps{}.onClick([rtl] { rtl.set(!rtl.get()); }),
                         [] { gallery_label("gallery.copy.sample.497"); });
                  gallery_target("gallery.popover.rtl");
                  Button(ButtonProps{}.onClick([wireframe] { wireframe.set(!wireframe.get()); }),
                         [] { gallery_label("gallery.copy.sample.498"); });
                  gallery_target("gallery.popover.wireframe");
              }});
        ++state->telemetry.live_samples;
    }
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(gallery_text("gallery.copy.property.101"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.popover.editor", true);
             Text(TextProps{}
                      .content(bind([state] { return utf8("" + std::string{state->popover_feedback.get().bytes()}); }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.popover.feedback", true);
             Space(
                 SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                 SpaceContent{[state] {
                     Popover(
                         PopoverProps{}
                             .open(state->popover_open)
                             .trigger(PopoverTriggerMode::Click)
                             .disabled(state->popover_disabled)
                             .placement(bind([state] {
                                 constexpr std::array positions{PopoverPlacement::Top, PopoverPlacement::Right,
                                                                PopoverPlacement::Bottom, PopoverPlacement::Left};
                                 return positions[static_cast<std::size_t>(state->popover_placement.get())];
                             }))
                             .arrow(state->popover_arrow)
                             .onOpenChange([state](bool value) { state->popover_open.set(value); }),
                         PopoverTrigger{[state] {
                             Button(ButtonProps{}.disabled(state->popover_disabled),
                                    [] { gallery_label("gallery.copy.sample.499"); });
                             gallery_target("gallery.popover.trigger");
                         }},
                         {.title = PopoverTitle{[] { gallery_label("gallery.copy.sample.500"); }},
                          .content = PopoverContent{[state] {
                              Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)), FlexContent{[state] {
                                       Input(InputProps{}
                                                 .defaultValue(gallery_initial_text("gallery.popover.draft",
                                                                                    u8"下次会议前补齐项目备注"))
                                                 .placeholder(gallery_text("gallery.copy.property.102"))
                                                 .ref(state->popover_ref)
                                                 .onChange(gallery_value_changed<ryn::String>(
                                                     "gallery.popover.draft",
                                                     [state](String value) { state->popover_draft.set(value); }))
                                                 .layout(LayoutStyle{}
                                                             .width(dp(180))
                                                             .margin_left(dp(6))
                                                             .margin_right(dp(6))
                                                             .margin_top(dp(6))
                                                             .margin_bottom(dp(6))));
                                       gallery_target("gallery.popover.draft");
                                       Space(SpaceProps{}.size(dp(8)), SpaceContent{[state] {
                                                 Button(
                                                     ButtonProps{}
                                                         .type(ButtonType::Primary)
                                                         .onClick([state] {
                                                             state->popover_feedback.set(GalleryMessage{
                                                                 "gallery.event.034",
                                                                 {{u8"v0", utf8(std::string{
                                                                               state->popover_draft.get().bytes()})}}});
                                                             state->popover_open.set(false);
                                                         })
                                                         .layout(LayoutStyle{}
                                                                     .margin_left(dp(6))
                                                                     .margin_right(dp(6))
                                                                     .margin_top(dp(6))
                                                                     .margin_bottom(dp(6))),
                                                     [] { gallery_label("gallery.copy.sample.501"); });
                                                 Button(ButtonProps{}
                                                            .onClick([state] { state->popover_open.set(false); })
                                                            .layout(LayoutStyle{}
                                                                        .margin_left(dp(6))
                                                                        .margin_right(dp(6))
                                                                        .margin_top(dp(6))
                                                                        .margin_bottom(dp(6))),
                                                        [] { gallery_label("gallery.copy.sample.502"); });
                                             }});
                                   }});
                          }}});
                     Button(ButtonProps{}.onClick(
                                [state] { state->popover_placement.set((state->popover_placement.get() + 1) % 4); }),
                            [] { gallery_label("gallery.copy.sample.503"); });
                     gallery_target("gallery.popover.placement");
                     Button(ButtonProps{}.onClick([state] { state->popover_arrow.set(!state->popover_arrow.get()); }),
                            [] { gallery_label("gallery.copy.sample.504"); });
                     gallery_target("gallery.popover.arrow");
                     Button(ButtonProps{}.onClick(
                                [state] { state->popover_disabled.set(!state->popover_disabled.get()); }),
                            [] { gallery_label("gallery.copy.sample.505"); });
                     gallery_target("gallery.popover.disabled");
                     Button(ButtonProps{}.onClick([state] {
                         state->popover_disabled.set(false);
                         state->popover_open.set(true);
                     }),
                            [] { gallery_label("gallery.copy.sample.506"); });
                     gallery_target("gallery.popover.restore");
                 }});
         }});
    ++state->telemetry.live_samples;
}

void add_config_provider_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;

    struct Demo final {
        Signal<ControlSize> size{ControlSize::Small};
        Signal<bool> disabled{false};
        Signal<TextDirection> direction{TextDirection::LeftToRight};
        Signal<String> draft{String{}};
        Signal<std::uint64_t> activations{0};
    };

    const auto demo = std::make_shared<Demo>();
    demo->draft.set(state->initial("gallery.config-provider.draft", String{}));
    const auto tr = useI18n();
    const auto change_size = gallery_message("gallery.config-provider.size");
    const auto change_disabled = gallery_message("gallery.config-provider.disabled");
    const auto change_direction = gallery_message("gallery.config-provider.direction");
    const auto inherited = gallery_message("gallery.config-provider.inherited");
    const auto explicit_value = gallery_message("gallery.config-provider.explicit");
    const auto nested = gallery_message("gallery.config-provider.nested");
    const auto placeholder = gallery_message("gallery.config-provider.placeholder");
    const auto activations = gallery_message("gallery.config-provider.activations");
    Flex(FlexProps{}.vertical(true).gap(dp(12)), FlexContent{[=] {
             Button(ButtonProps{}.onClick([demo] {
                 const auto size = demo->size.get();
                 demo->size.set(size == ControlSize::Small    ? ControlSize::Middle
                                : size == ControlSize::Middle ? ControlSize::Large
                                                              : ControlSize::Small);
             }),
                    [=] { Text(TextProps{}.content(tr.text(change_size))); });
             gallery_target("gallery.config-provider.size");
             Button(ButtonProps{}.onClick([demo] { demo->disabled.set(!demo->disabled.get()); }),
                    [=] { Text(TextProps{}.content(tr.text(change_disabled))); });
             gallery_target("gallery.config-provider.disabled");
             Button(ButtonProps{}.onClick([demo] {
                 demo->direction.set(demo->direction.get() == TextDirection::RightToLeft ? TextDirection::LeftToRight
                                                                                         : TextDirection::RightToLeft);
             }),
                    [=] { Text(TextProps{}.content(tr.text(change_direction))); });
             gallery_target("gallery.config-provider.direction");
             ConfigProvider(
                 ConfigProviderProps{}.size(demo->size).disabled(demo->disabled).direction(demo->direction),
                 ConfigProviderContent{[=] {
                     Button(ButtonProps{}.onClick([demo] { demo->activations.set(demo->activations.get() + 1); }),
                            [=] { Text(TextProps{}.content(tr.text(inherited))); });
                     gallery_target("gallery.config-provider.inherited");
                     GalleryDraftBuild draft_build{state};
                     Input(InputProps{}
                               .value(demo->draft)
                               .placeholder(tr.text(placeholder))
                               .onChange(
                                   gallery_value_changed<String>("gallery.config-provider.draft", [demo](String value) {
                                       demo->draft.set(std::move(value));
                                   })));
                     remember_target(state, "gallery.config-provider.draft");
                     Button(ButtonProps{}.size(ControlSize::Middle).disabled(false),
                            [=] { Text(TextProps{}.content(tr.text(explicit_value))); });
                     gallery_target("gallery.config-provider.explicit");
                     ConfigProvider(ConfigProviderProps{}.disabled(false), ConfigProviderContent{[=] {
                                        Button(ButtonProps{}, [=] { Text(TextProps{}.content(tr.text(nested))); });
                                        gallery_target("gallery.config-provider.nested");
                                    }});
                     Text(TextProps{}.content(tr.text(activations)));
                     Text(TextProps{}.content(bind([=] { return tr.formatNumber(demo->activations.get()); })));
                 }});
         }});
    ++state->telemetry.live_samples;
}

void add_app_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;

    struct Demo final {
        AppRef app;
        I18n i18n;
        InputRef editor;
        std::optional<NoticeHandle> handle;
        InputRef panel_editor;
        PopconfirmRef anchor;
        std::optional<ModalHandle> modal;
        std::optional<DrawerHandle> drawer;
        std::optional<PopconfirmHandle> popconfirm;
        Signal<LocaleId> language{locales::zh_CN};
        GalleryFeedback result{GalleryMessage{"gallery.complete.copy.240", {}}, useI18n()};
    };

    const auto demo = std::make_shared<Demo>();
    const MessageKey title{u8"gallery.app.notice", String{u8"Scoped notification"}};
    ConfigProvider(
        ConfigProviderProps{}
            .locale(demo->language)
            .messages(MessageCatalog({{locales::zh_CN, title, String{u8"作用域通知"}}})),
        ConfigProviderContent{[=] {
            App(AppProps{}.messageMaxCount(std::size_t{3}).notificationMaxCount(std::size_t{3}), AppContent{[=] {
                    demo->app = useApp();
                    demo->i18n = useI18n();
                    demo->result.context(demo->i18n);
                    Text(TextProps{}.content(demo->result));
                    Button(ButtonProps{}.onClick([=] {
                        if (demo->editor.bound()) {
                            static_cast<void>(demo->handle->update(NotificationProps{}.status(NoticeStatus::Info)));
                            return;
                        }
                        demo->handle = demo->app.notification().openAsync(
                            u8"draft", NotificationProps{}.title(demo->i18n.text(title)).duration(0),
                            {.description = NotificationDescription{
                                 [demo] { Input(InputProps{}.ref(demo->editor).defaultValue(u8"保留这份草稿")); }}});
                    }),
                           [] { gallery_label("gallery.copy.sample.507"); });
                    remember_target(state, "gallery.app.open");
                    Button(ButtonProps{}.onClick([demo] {
                        if (demo->handle) {
                            static_cast<void>(demo->handle->update(NotificationProps{}.status(NoticeStatus::Success)));
                        }
                    }),
                           [] { gallery_label("gallery.copy.sample.508"); });
                    remember_target(state, "gallery.app.update");
                    Button(ButtonProps{}.onClick([demo] {
                        demo->language.set(demo->language.get() == locales::zh_CN ? locales::en_US : locales::zh_CN);
                    }),
                           [] { gallery_label("gallery.copy.sample.509"); });
                    remember_target(state, "gallery.app.language");
                    Button(ButtonProps{}.onClick([demo] {
                        if (demo->handle) {
                            static_cast<void>(demo->handle->close());
                        }
                    }),
                           [] { gallery_label("gallery.copy.sample.510"); });
                    remember_target(state, "gallery.app.close");
                    Button(ButtonProps{}.onClick([demo] {
                        if (demo->handle &&
                            demo->handle->closed().wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
                            const auto result = demo->handle->closed().get();
                            demo->result.set(result == NoticeCloseResult::Closed
                                                 ? GalleryMessage{"gallery.complete.copy.255", {}}
                                                 : GalleryMessage{"gallery.complete.copy.256", {}});
                        } else {
                            demo->result.set(GalleryMessage{"gallery.complete.copy.249", {}});
                        }
                    }),
                           [] { gallery_label("gallery.copy.sample.511"); });
                    remember_target(state, "gallery.app.result");
                    Button(ButtonProps{}.onClick([demo, title] {
                        if (demo->modal && demo->modal->bound()) {
                            demo->modal->update({.title = demo->i18n.text(title)});
                            return;
                        }
                        demo->modal = demo->app.modal().confirm(
                            ModalProps{}.title(demo->i18n.text(title)).onOk([demo] {
                                demo->result.set(GalleryMessage{"gallery.complete.copy.250", {}});
                            }),
                            ModalContent{[demo] {
                                Input(InputProps{}.ref(demo->panel_editor).defaultValue(u8"确认框中的草稿"));
                            }});
                    }),
                           [] { gallery_label("gallery.complete.copy.021"); });
                    remember_target(state, "gallery.app.modal");
                    Button(ButtonProps{}.onClick([demo, title] {
                        if (demo->drawer && demo->drawer->bound()) {
                            demo->drawer->update({.title = demo->i18n.text(title)});
                            return;
                        }
                        demo->drawer = demo->app.drawer().open(
                            DrawerProps{}.title(demo->i18n.text(title)),
                            DrawerContent{[] { Input(InputProps{}.defaultValue(u8"抽屉中的草稿")); }});
                    }),
                           [] { gallery_label("gallery.complete.copy.022"); });
                    remember_target(state, "gallery.app.drawer");
                    Popconfirm(PopconfirmProps{}.serviceAnchor(demo->anchor), PopconfirmTrigger{[demo, state] {
                                   Button(ButtonProps{}.onClick([demo, tr = useI18n()] {
                                       if (demo->popconfirm && demo->popconfirm->bound()) {
                                           return;
                                       }
                                       demo->popconfirm = demo->app.popconfirm().confirm(
                                           demo->anchor,
                                           PopconfirmProps{}
                                               .title(tr.text(gallery_message("gallery.complete.copy.000")))
                                               .onConfirm([demo] {
                                                   demo->result.set(GalleryMessage{"gallery.complete.copy.251", {}});
                                               }));
                                   }),
                                          [] { gallery_label("gallery.complete.copy.023"); });
                                   remember_target(state, "gallery.app.popconfirm");
                               }});
                }});
        }});
    ++state->telemetry.live_samples;
}

void add_notice_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    MessageHolder(MessageHolderProps{}.ref(state->messages).maxCount(std::size_t{3}));
    NotificationHolder(NotificationHolderProps{}.ref(state->notifications).maxCount(std::size_t{3}));
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(gallery_text("gallery.complete.copy.412"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.notices.workflow", true);
             Text(TextProps{}
                      .content(bind([state] { return utf8("" + std::string{state->notice_feedback.get().bytes()}); }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.notices.feedback", true);
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}.onClick([state, tr = useI18n()] {
                           static_cast<void>(state->messages.open(
                               u8"project-save", MessageProps{}
                                                     .content(tr.text(gallery_message("gallery.complete.copy.017")))
                                                     .status(NoticeStatus::Loading)
                                                     .duration(0)
                                                     .onClose([state] {
                                                         state->notice_feedback.set(
                                                             GalleryMessage{"gallery.feedback.123", {}});
                                                     })));
                           state->notice_feedback.set(GalleryMessage{"gallery.feedback.124", {}});
                       }),
                              [] { gallery_label("gallery.copy.sample.512"); });
                       gallery_target("gallery.message.open");
                       Button(ButtonProps{}.onClick([state, tr = useI18n()] {
                           static_cast<void>(state->messages.open(
                               u8"project-save", MessageProps{}
                                                     .content(tr.text(gallery_message("gallery.complete.copy.018")))
                                                     .status(NoticeStatus::Success)
                                                     .duration(0)));
                           state->notice_feedback.set(GalleryMessage{"gallery.feedback.125", {}});
                       }),
                              [] { gallery_label("gallery.copy.sample.513"); });
                       gallery_target("gallery.message.update");
                       Button(ButtonProps{}.onClick(
                                  [state] { static_cast<void>(state->messages.close(u8"project-save")); }),
                              [] { gallery_label("gallery.copy.sample.514"); });
                       gallery_target("gallery.message.close");
                       Button(ButtonProps{}.onClick([state, tr = useI18n()] {
                           state->notice_handle = state->messages.openAsync(
                               u8"timed-save", MessageProps{}
                                                   .content(tr.text(gallery_message("gallery.complete.copy.019")))
                                                   .duration(5)
                                                   .pauseOnHover()
                                                   .showProgress()
                                                   .motion());
                       }),
                              [] { gallery_label("gallery.copy.sample.515"); });
                       gallery_target("gallery.message.progress");
                       Button(ButtonProps{}.onClick([state] {
                           if (state->notice_handle && state->notice_handle->closed().wait_for(
                                                           std::chrono::seconds{0}) == std::future_status::ready) {
                               const auto result = state->notice_handle->closed().get();
                               state->notice_feedback.set(result == NoticeCloseResult::Closed
                                                              ? GalleryMessage{"gallery.feedback.126", {}}
                                                              : GalleryMessage{"gallery.feedback.127", {}});
                           } else {
                               state->notice_feedback.set(GalleryMessage{"gallery.feedback.128", {}});
                           }
                       }),
                              [] { gallery_label("gallery.copy.sample.516"); });
                       gallery_target("gallery.message.completion");
                       Button(ButtonProps{}.disabled(state->notification_open).onClick([state, tr = useI18n()] {
                           const bool opened = state->notifications.open(
                               u8"project-review",
                               NotificationProps{}
                                   .title(tr.text(gallery_message("gallery.complete.copy.001")))
                                   .status(state->notification_status)
                                   .placement(state->notification_placement)
                                   .duration(0)
                                   .onClose([state] {
                                       state->notification_open.set(false);
                                       state->notice_feedback.set(GalleryMessage{"gallery.feedback.129", {}});
                                   }),
                               {.description = NotificationDescription{[state] {
                                    GalleryDraftBuild draft_build{state};
                                    Input(InputProps{}
                                              .defaultValue(gallery_initial_text("gallery.notification.draft",
                                                                                 state->notification_draft.get()))
                                              .placeholder(gallery_text("gallery.complete.copy.010"))
                                              .ref(state->notification_ref)
                                              .onChange(gallery_value_changed<ryn::String>(
                                                  "gallery.notification.draft",
                                                  [state](String value) { state->notification_draft.set(value); }))
                                              .layout(LayoutStyle{}
                                                          .width(dp(180))
                                                          .margin_left(dp(6))
                                                          .margin_right(dp(6))
                                                          .margin_top(dp(6))
                                                          .margin_bottom(dp(6))));
                                    remember_target(state, "gallery.notification.draft");
                                }},
                                .actions = NotificationActions{[state] {
                                    Button(ButtonProps{}
                                               .onClick([state] {
                                                   static_cast<void>(state->notifications.close(u8"project-review"));
                                                   state->notice_feedback.set(GalleryMessage{
                                                       "gallery.event.035",
                                                       {{u8"v0",
                                                         utf8(std::string{state->notification_draft.get().bytes()})}}});
                                               })
                                               .layout(LayoutStyle{}
                                                           .margin_left(dp(6))
                                                           .margin_right(dp(6))
                                                           .margin_top(dp(6))
                                                           .margin_bottom(dp(6))),
                                           [] { gallery_label("gallery.copy.sample.517"); });
                                }}});
                           state->notification_open.set(opened);
                           if (opened) {
                               state->notice_feedback.set(GalleryMessage{"gallery.feedback.130", {}});
                           }
                       }),
                              [] { gallery_label("gallery.copy.sample.518"); });
                       gallery_target("gallery.notification.open");
                       Button(ButtonProps{}
                                  .disabled(bind([state] { return !state->notification_open.get(); }))
                                  .onClick([state, tr = useI18n()] {
                                      static_cast<void>(state->notifications.open(
                                          u8"project-review", NotificationProps{}.title(tr.text(
                                                                  gallery_message("gallery.complete.copy.002")))));
                                      state->notice_feedback.set(GalleryMessage{"gallery.feedback.131", {}});
                                  }),
                              [] { gallery_label("gallery.copy.sample.519"); });
                       gallery_target("gallery.notification.update");
                       Button(ButtonProps{}.onClick(
                                  [state] { static_cast<void>(state->notifications.close(u8"project-review")); }),
                              [] { gallery_label("gallery.copy.sample.520"); });
                       gallery_target("gallery.notification.close");
                       Button(ButtonProps{}.onClick([state] {
                           state->notification_placement.set(static_cast<NotificationPlacement>(
                               (static_cast<int>(state->notification_placement.get()) + 1) % 4));
                       }),
                              [] { gallery_label("gallery.copy.sample.521"); });
                       gallery_target("gallery.notification.placement");
                       Button(ButtonProps{}.onClick([state] {
                           state->notification_status.set(
                               static_cast<NoticeStatus>((static_cast<int>(state->notification_status.get()) + 1) % 5));
                       }),
                              [] { gallery_label("gallery.copy.sample.522"); });
                       gallery_target("gallery.notification.status");
                       Button(ButtonProps{}.onClick([state] {
                           static_cast<void>(state->messages.clear());
                           static_cast<void>(state->notifications.clear());
                           state->notice_feedback.set(GalleryMessage{"gallery.feedback.132", {}});
                       }),
                              [] { gallery_label("gallery.copy.sample.523"); });
                       gallery_target("gallery.notices.clear");
                   }});
         }});
    state->telemetry.live_samples += 2;
}

void add_notification_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    NotificationHolder(
        NotificationHolderProps{}.ref(state->notifications).maxCount(std::size_t{8}).stack(NoticeStack{true, 3}));
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}.content(gallery_text("gallery.complete.copy.413")));
             remember_target(state, "gallery.notification.workflow", true);
             Text(TextProps{}.content(state->notice_feedback));
             Space(
                 SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
                     Button(ButtonProps{}.disabled(state->notification_open).onClick([state, tr = useI18n()] {
                         const bool opened = state->notifications.open(
                             u8"review",
                             NotificationProps{}
                                 .title(tr.text(gallery_message("gallery.complete.copy.003")))
                                 .placement(state->notification_placement)
                                 .status(state->notification_status)
                                 .duration(0)
                                 .motion()
                                 .onClose([state] {
                                     state->notification_open.set(false);
                                     state->notice_feedback.set(GalleryMessage{"gallery.feedback.133", {}});
                                 }),
                             {.description = NotificationDescription{[state] {
                                  GalleryDraftBuild draft_build{state};
                                  Input(InputProps{}
                                            .ref(state->notification_ref)
                                            .defaultValue(gallery_initial_text("gallery.notification.independent.draft",
                                                                               state->notification_draft.get()))
                                            .onChange(gallery_value_changed<String>(
                                                "gallery.notification.independent.draft",
                                                [state](String value) {
                                                    state->notification_draft.set(std::move(value));
                                                }))
                                            .layout(LayoutStyle{}
                                                        .width(dp(180))
                                                        .margin_left(dp(6))
                                                        .margin_right(dp(6))
                                                        .margin_top(dp(6))
                                                        .margin_bottom(dp(6))));
                                  remember_target(state, "gallery.notification.independent.draft");
                              }},
                              .actions = NotificationActions{[state] {
                                  Button(ButtonProps{}
                                             .type(ButtonType::Primary)
                                             .onClick([state] {
                                                 state->notice_feedback.set(GalleryMessage{
                                                     "gallery.event.036",
                                                     {{u8"v0",
                                                       utf8(std::string{state->notification_draft.get().bytes()})}}});
                                                 static_cast<void>(state->notifications.close(u8"review"));
                                             })
                                             .layout(LayoutStyle{}
                                                         .margin_left(dp(6))
                                                         .margin_right(dp(6))
                                                         .margin_top(dp(6))
                                                         .margin_bottom(dp(6))),
                                         [] { gallery_label("gallery.copy.sample.524"); });
                                  gallery_target("gallery.notification.independent.confirm");
                              }}});
                         state->notification_open.set(opened);
                     }),
                            [] { gallery_label("gallery.copy.sample.525"); });
                     gallery_target("gallery.notification.independent.open");
                     Button(ButtonProps{}.onClick([state, tr = useI18n()] {
                         static_cast<void>(state->notifications.open(
                             u8"review",
                             NotificationProps{}.title(tr.text(gallery_message("gallery.complete.copy.004")))));
                     }),
                            [] { gallery_label("gallery.copy.sample.526"); });
                     gallery_target("gallery.notification.independent.update");
                     Button(
                         ButtonProps{}.onClick([state] { static_cast<void>(state->notifications.close(u8"review")); }),
                         [] { gallery_label("gallery.copy.sample.527"); });
                     gallery_target("gallery.notification.independent.close");
                     Button(ButtonProps{}.onClick([state] {
                         state->notification_placement.set(static_cast<NotificationPlacement>(
                             (static_cast<unsigned>(state->notification_placement.get()) + 1) % 4));
                     }),
                            [] { gallery_label("gallery.copy.sample.528"); });
                     gallery_target("gallery.notification.independent.placement");
                     Button(ButtonProps{}.onClick([state] {
                         state->notification_status.set(static_cast<NoticeStatus>(
                             (static_cast<unsigned>(state->notification_status.get()) + 1) % 5));
                     }),
                            [] { gallery_label("gallery.copy.sample.529"); });
                     gallery_target("gallery.notification.independent.status");
                     Button(ButtonProps{}.onClick([state, tr = useI18n()] {
                         const auto key = utf8("timed-" + std::to_string(++state->notice_sequence));
                         state->notice_handle = state->notifications.openAsync(
                             key, NotificationProps{}
                                      .title(tr.text(gallery_message("gallery.complete.copy.005")))
                                      .description(tr.text(gallery_message("gallery.complete.copy.007")))
                                      .placement(state->notification_placement)
                                      .duration(5)
                                      .showProgress()
                                      .pauseOnHover()
                                      .motion());
                     }),
                            [] { gallery_label("gallery.copy.sample.530"); });
                     gallery_target("gallery.notification.independent.timed");
                     Button(ButtonProps{}.onClick([state] { static_cast<void>(state->notifications.clear()); }),
                            [] { gallery_label("gallery.copy.sample.531"); });
                     gallery_target("gallery.notification.independent.clear");
                 }});
         }});
    state->telemetry.live_samples += 1;
}

void add_timeline_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(gallery_text("gallery.copy.property.103"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.timeline.workflow", true);
             Text(TextProps{}
                      .content(bind([state] { return utf8("" + std::string{state->timeline_feedback.get().bytes()}); }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.timeline.feedback", true);
             Timeline(TimelineProps{}
                          .mode(bind([state] { return static_cast<TimelineMode>(state->timeline_mode.get()); }))
                          .reverse(state->timeline_reverse)
                          .layout(LayoutStyle{}
                                      .width(bind([state] { return dp(state->document_width.get().value() - 16); }))
                                      .margin_left(dp(8))
                                      .margin_right(dp(8))),
                      {TimelineItem{u8"created", gallery_text("gallery.complete.copy.071")}.title(u8"09:00").tone(
                           TimelineTone::Green),
                       TimelineItem{u8"review", gallery_text("gallery.complete.copy.072")}.title(u8"09:30").loading(
                           state->timeline_loading),
                       TimelineItem{u8"publish", u8""}
                           .title(u8"10:00")
                           .visible(state->timeline_visible)
                           .richContent(TimelineContent{[state] {
                               Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)), FlexContent{[state] {
                                        Input(InputProps{}
                                                  .defaultValue(gallery_initial_text("gallery.timeline.draft",
                                                                                     u8"记录发布后的跟进事项"))
                                                  .placeholder(gallery_text("gallery.copy.property.104"))
                                                  .ref(state->timeline_ref)
                                                  .onChange(gallery_value_changed<ryn::String>(
                                                      "gallery.timeline.draft",
                                                      [state](String value) { state->timeline_draft.set(value); }))
                                                  .layout(LayoutStyle{}
                                                              .width(dp(160))
                                                              .margin_left(dp(6))
                                                              .margin_right(dp(6))
                                                              .margin_top(dp(6))
                                                              .margin_bottom(dp(6))));
                                        gallery_target("gallery.timeline.draft");
                                        Button(ButtonProps{}
                                                   .onClick([state] {
                                                       state->timeline_feedback.set(GalleryMessage{
                                                           "gallery.event.037",
                                                           {{u8"v0",
                                                             utf8(std::string{state->timeline_draft.get().bytes()})}}});
                                                   })
                                                   .layout(LayoutStyle{}
                                                               .margin_left(dp(6))
                                                               .margin_right(dp(6))
                                                               .margin_top(dp(6))
                                                               .margin_bottom(dp(6))),
                                               [] { gallery_label("gallery.copy.sample.532"); });
                                    }});
                           }})});
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}.onClick(
                                  [state] { state->timeline_mode.set((state->timeline_mode.get() + 1) % 3); }),
                              [] { gallery_label("gallery.copy.sample.533"); });
                       gallery_target("gallery.timeline.mode");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->timeline_reverse.set(!state->timeline_reverse.get()); }),
                              [] { gallery_label("gallery.copy.sample.534"); });
                       gallery_target("gallery.timeline.reverse");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->timeline_loading.set(!state->timeline_loading.get()); }),
                              [] { gallery_label("gallery.copy.sample.535"); });
                       gallery_target("gallery.timeline.loading");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->timeline_visible.set(!state->timeline_visible.get()); }),
                              [] { gallery_label("gallery.copy.sample.536"); });
                       gallery_target("gallery.timeline.visible");
                       Button(ButtonProps{}.onClick([state] {
                           state->timeline_visible.set(true);
                           static_cast<void>(state->timeline_ref.focus());
                       }),
                              [] { gallery_label("gallery.copy.sample.537"); });
                       gallery_target("gallery.timeline.focus");
                   }});
         }});
    ++state->telemetry.live_samples;
    add_timeline_contract_samples(state);
}

ryn::LayoutStyle member_control_margin() {
    return ryn::LayoutStyle{}
        .margin_left(ryn::dp(6))
        .margin_right(ryn::dp(6))
        .margin_top(ryn::dp(6))
        .margin_bottom(ryn::dp(6));
}

ryn::String metric_raw_value(const ryn::StatisticValue& value) {
    return std::visit(
        [](const auto& item) -> ryn::String {
            if constexpr (std::same_as<std::decay_t<decltype(item)>, ryn::String>) {
                return item;
            } else {
                std::array<char, 64> buffer{};
                const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), item);
                if (result.ec != std::errc{}) {
                    throw std::runtime_error("Gallery business value formatting failed");
                }
                return utf8(std::string_view{buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())});
            }
        },
        value.storage());
}

void add_member_avatar_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(gallery_text("gallery.copy.property.105"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.avatar.members", true);
             Text(TextProps{}
                      .content(bind([state] { return utf8("" + std::string{state->member_feedback.get().bytes()}); }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.avatar.feedback", true);
             Space(SpaceProps{}.wrap(true).size(dp(12)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Avatar(AvatarProps{}
                                  .text(state->member_name)
                                  .size(state->member_size)
                                  .shape(state->member_shape)
                                  .visible(bind(
                                      [state] { return state->members_visible.get() && !state->member_icon.get(); })));
                       Avatar(AvatarProps{}.size(state->member_size).shape(state->member_shape).visible(bind([state] {
                           return state->members_visible.get() && state->member_icon.get();
                       })),
                              {.icon = AvatarIcon{[] { Icon(IconProps{}.name(IconName::UserOutlined)); }}});
                       std::vector<AvatarGroupItem> members;
                       for (int i = 1; i <= 128; ++i) {
                           const auto name = utf8("M" + std::to_string(i));
                           members.emplace_back(name, i == 1 ? Prop<String>{state->member_name} : Prop<String>{name});
                       }
                       ThemeConfig member_popup;
                       member_popup.popover.tokens.max_width = dp(240);
                       Theme(ThemeProps{}.config(member_popup), ThemeContent{[state, members = std::move(members)] {
                                 ++state->telemetry.theme_content_runs;
                                 AvatarGroup(AvatarGroupProps{}
                                                 .size(AvatarSize::Large)
                                                 .shape(state->member_shape)
                                                 .visible(state->members_visible)
                                                 .max(bind([state] {
                                                     return AvatarGroupMax{state->member_limit.get(),
                                                                           PopoverTriggerMode::Click,
                                                                           PopoverPlacement::Bottom};
                                                 })),
                                             members);
                             }});
                   }});
             Avatar(AvatarProps{}.size(240.0F).shape(AvatarShape::Square).visible(state->members_visible),
                    {.content = AvatarContent{[state] {
                         Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)), FlexContent{[state] {
                                  Input(InputProps{}
                                            .defaultValue(gallery_initial_text("gallery.avatar.name", u8"江"))
                                            .placeholder(gallery_text("gallery.copy.property.106"))
                                            .ref(state->member_ref)
                                            .onChange(gallery_value_changed<ryn::String>(
                                                "gallery.avatar.name",
                                                [state](String value) { state->member_name.set(value); }))
                                            .layout(member_control_margin().width(dp(160))));
                                  gallery_target("gallery.avatar.name");
                                  Button(ButtonProps{}
                                             .onClick([state] {
                                                 state->member_feedback.set(GalleryMessage{
                                                     "gallery.event.038",
                                                     {{u8"v0", utf8(std::string{state->member_name.get().bytes()})}}});
                                             })
                                             .layout(member_control_margin()),
                                         [] { gallery_label("gallery.copy.sample.538"); });
                              }});
                     }}});
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}
                                  .onClick([state] { state->member_icon.set(!state->member_icon.get()); })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.539"); });
                       gallery_target("gallery.avatar.icon");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->member_shape.set(state->member_shape.get() == AvatarShape::Circle
                                                                  ? AvatarShape::Square
                                                                  : AvatarShape::Circle);
                                  })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.540"); });
                       gallery_target("gallery.avatar.shape");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->member_size.set(state->member_size.get() ==
                                                                     AvatarSizeValue{AvatarSize::Large}
                                                                 ? AvatarSizeValue{AvatarSize::Small}
                                                                 : AvatarSizeValue{AvatarSize::Large});
                                  })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.541"); });
                       gallery_target("gallery.avatar.size");
                       Button(ButtonProps{}
                                  .onClick([state] { state->member_limit.set(state->member_limit.get() == 2 ? 0 : 2); })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.542"); });
                       gallery_target("gallery.avatar.limit");
                       Button(ButtonProps{}
                                  .onClick([state] { state->members_visible.set(!state->members_visible.get()); })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.543"); });
                       gallery_target("gallery.avatar.visible");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->members_visible.set(true);
                                      static_cast<void>(state->member_ref.focus());
                                  })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.544"); });
                       gallery_target("gallery.avatar.restore");
                   }});
         }});
    state->telemetry.live_samples += 4;
}

void add_statistic_metric_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    if (state->page_component == "ant.component.statistic") {
        Signal<std::optional<StatisticTimer>> timer{std::nullopt};
        Signal<StatisticValue> amount{StatisticValue{1234567}};
        Signal<StatisticLocale> locale{StatisticLocale{}};
        GalleryFeedback timer_feedback{GalleryMessage{"gallery.feedback.039", {}}, useI18n()};
        Statistic(StatisticProps{}
                      .title(gallery_text("gallery.copy.property.107"))
                      .timer(timer)
                      .suffix(bind([timer, tr = useI18n()] {
                          const auto current = timer.get();
                          return current && current->type == StatisticTimerType::Countdown
                                     ? tr.translate(gallery_message("gallery.complete.copy.459"))
                                     : String{};
                      }))
                      .locale(locale)
                      .onFinish([timer_feedback] { timer_feedback.set(GalleryMessage{"gallery.feedback.134", {}}); }));
        gallery_target("gallery.statistic.timer");
        Text(TextProps{}.content(timer_feedback));
        Statistic(StatisticProps{}
                      .title(gallery_text("gallery.copy.property.108"))
                      .value(amount)
                      .locale(locale)
                      .precision(2)
                      .animated());
        gallery_target("gallery.statistic.animated-value");
        Space(SpaceProps{}.wrap(true), SpaceContent{[timer, amount, locale, timer_feedback] {
                  Button(ButtonProps{}.onClick([timer, timer_feedback] {
                      timer.set(std::nullopt);
                      timer.set(StatisticTimer{StatisticTimerType::Countdown, Duration::seconds(5), String{u8"ss"}});
                      timer_feedback.set(GalleryMessage{"gallery.feedback.135", {}});
                  }),
                         [] { gallery_label("gallery.copy.sample.545"); });
                  gallery_target("gallery.statistic.countdown");
                  Button(ButtonProps{}.onClick([timer] {
                      timer.set(std::nullopt);
                      timer.set(StatisticTimer{StatisticTimerType::Countup, Duration{}, String{u8"mm:ss"}});
                  }),
                         [] { gallery_label("gallery.copy.sample.546"); });
                  gallery_target("gallery.statistic.countup");
                  Button(ButtonProps{}.onClick([timer] { timer.set(std::nullopt); }),
                         [] { gallery_label("gallery.copy.sample.547"); });
                  gallery_target("gallery.statistic.timer-stop");
                  Button(ButtonProps{}.onClick([amount] {
                      amount.set(StatisticValue{amount.get() == StatisticValue{1234567} ? 9876543 : 1234567});
                  }),
                         [] { gallery_label("gallery.copy.sample.548"); });
                  gallery_target("gallery.statistic.animate");
                  Button(ButtonProps{}.onClick([locale] {
                      auto value = locale.get();
                      value.decimal_separator =
                          value.decimal_separator == String{u8"."} ? String{u8","} : String{u8"."};
                      value.group_separator = value.decimal_separator == String{u8","} ? String{u8"."} : String{u8","};
                      value.secondary_group = value.decimal_separator == String{u8","} ? 2 : 3;
                      locale.set(std::move(value));
                  }),
                         [] { gallery_label("gallery.copy.sample.549"); });
                  gallery_target("gallery.statistic.locale");
              }});
        state->telemetry.live_samples += 2;
    }
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(gallery_text("gallery.copy.property.109"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.statistic.metrics", true);
             Text(TextProps{}
                      .content(bind([state] {
                          return state->i18n.translate(
                              gallery_message("gallery.snapshot.007"),
                              MessageArgs{
                                  {u8"v0", utf8(std::string{metric_raw_value(state->metric_value.get()).bytes()})},
                                  {u8"v1", utf8(std::string{state->metric_feedback.get().bytes()})}});
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.statistic.feedback", true);
             Statistic(StatisticProps{}
                           .title(gallery_text("gallery.copy.property.110"))
                           .value(state->metric_value)
                           .precision(state->metric_precision)
                           .groupSeparator(
                               bind([state] { return state->metric_separators.get() ? String{u8" "} : String{u8","}; }))
                           .decimalSeparator(
                               bind([state] { return state->metric_separators.get() ? String{u8","} : String{u8"."}; }))
                           .formatter(bind([state, tr = useI18n()] {
                               return state->metric_formatter.get()
                                          ? StatisticFormatter{[tr](const StatisticValue& value) {
                                                return tr.translate(gallery_message("gallery.complete.copy.460"),
                                                                    {{u8"value", metric_raw_value(value)}});
                                            }}
                                          : StatisticFormatter{};
                           }))
                           .loading(state->metric_loading)
                           .layout(member_control_margin().width(
                               bind([state] { return dp(state->document_width.get().value() - 12); }))));
             Statistic(
                 StatisticProps{}
                     .title(gallery_text("gallery.copy.property.111"))
                     .value(state->metric_value)
                     .loading(state->metric_loading)
                     .richValue(StatisticValueContent{[state] {
                         Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)), FlexContent{[state] {
                                  Input(InputProps{}
                                            .defaultValue(
                                                gallery_initial_text("gallery.statistic.note", u8"核对本期指标"))
                                            .placeholder(gallery_text("gallery.copy.property.112"))
                                            .ref(state->metric_ref)
                                            .onChange(gallery_value_changed<ryn::String>(
                                                "gallery.statistic.note",
                                                [state](String value) { state->metric_note.set(value); }))
                                            .layout(member_control_margin().width(dp(180))));
                                  gallery_target("gallery.statistic.note");
                                  Button(ButtonProps{}
                                             .onClick([state] {
                                                 state->metric_feedback.set(GalleryMessage{
                                                     "gallery.event.039",
                                                     {{u8"v0", utf8(std::string{state->metric_note.get().bytes()})}}});
                                             })
                                             .layout(member_control_margin()),
                                         [] { gallery_label("gallery.copy.sample.550"); });
                              }});
                     }})
                     .layout(member_control_margin().width(
                         bind([state] { return dp(state->document_width.get().value() - 12); }))));
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      const std::array values{StatisticValue{u8"-1234.123456789"},
                                                              StatisticValue{1.0000004},
                                                              StatisticValue{std::uint64_t{18446744073709551615ULL}},
                                                              StatisticValue{std::int64_t{-1234567}}};
                                      state->metric_step = (state->metric_step + 1) % values.size();
                                      state->metric_value.set(values[state->metric_step]);
                                  })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.551"); });
                       gallery_target("gallery.statistic.value");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      const auto p = state->metric_precision.get();
                                      state->metric_precision.set(p == 2   ? std::optional{8}
                                                                  : p == 8 ? std::nullopt
                                                                           : std::optional{2});
                                  })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.552"); });
                       gallery_target("gallery.statistic.precision");
                       Button(ButtonProps{}
                                  .onClick([state] { state->metric_separators.set(!state->metric_separators.get()); })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.553"); });
                       gallery_target("gallery.statistic.separators");
                       Button(ButtonProps{}
                                  .onClick([state] { state->metric_formatter.set(!state->metric_formatter.get()); })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.554"); });
                       gallery_target("gallery.statistic.formatter");
                       Button(ButtonProps{}
                                  .onClick([state] { state->metric_loading.set(!state->metric_loading.get()); })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.555"); });
                       gallery_target("gallery.statistic.loading");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->metric_loading.set(false);
                                      static_cast<void>(state->metric_ref.focus());
                                  })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.556"); });
                       gallery_target("gallery.statistic.restore");
                   }});
         }});
    state->telemetry.live_samples += 2;
}

void add_service_rate_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(
        FlexProps{}
            .vertical(true)
            .align(FlexAlign::Start)
            .gap(dp(12))
            .layout(LayoutStyle{}.width(state->document_width)),
        FlexContent{[state] {
            Text(TextProps{}
                     .content(gallery_text("gallery.copy.property.113"))
                     .layout(LayoutStyle{}.margin_left(dp(1))));
            remember_target(state, "gallery.rate.service", true);
            Text(
                TextProps{}
                    .content(bind([state] {
                        return state->i18n.translate(
                            gallery_message("gallery.snapshot.008"),
                            MessageArgs{
                                {u8"v0", utf8(std::string{
                                             metric_raw_value(StatisticValue{static_cast<double>(state->rating.get())})
                                                 .bytes()})},
                                {u8"v1",
                                 utf8((state->rating_accept.get()
                                           ? std::string{state->i18n
                                                             .translate(gallery_message("gallery.state.accept-change"))
                                                             .bytes()}
                                           : std::string{state->i18n
                                                             .translate(gallery_message("gallery.state.reject-change"))
                                                             .bytes()}))},
                                {u8"v2",
                                 utf8((state->rating_disabled.get()
                                           ? std::string{state->i18n
                                                             .translate(gallery_message("gallery.state.disabled-short"))
                                                             .bytes()}
                                           : ""))},
                                {u8"v3",
                                 utf8(
                                     (state->rating_read_only.get()
                                          ? std::string{state->i18n.translate(gallery_message("gallery.state.readonly"))
                                                            .bytes()}
                                          : ""))},
                                {u8"v4", utf8(std::string{state->rating_request.get().bytes()})},
                                {u8"v5", utf8(std::string{state->rating_feedback.get().bytes()})}});
                    }))
                    .layout(LayoutStyle{}.margin_left(dp(1))));
            remember_target(state, "gallery.rate.feedback", true);
            Text(TextProps{}.content(state->rating_events));
            Rate(
                RateProps{}
                    .ref(state->rating_control_ref)
                    .direction(state->rating_direction)
                    .characterFor([state](std::size_t index) -> Prop<RateCharacter> {
                        return index == 4 ? Prop<RateCharacter>{RateCharacter{u8"好"}}
                                          : Prop<RateCharacter>{bind([state] {
                                                return RateCharacter{state->rating_hearts.get() ? IconName::HeartFilled
                                                                                                : IconName::StarFilled};
                                            })};
                    })
                    .onFocus([state] { state->rating_events.set(GalleryMessage{"gallery.feedback.136", {}}); })
                    .onBlur([state] { state->rating_events.set(GalleryMessage{"gallery.feedback.137", {}}); })
                    .onKeyDown(
                        [state](RateKeyEvent) { state->rating_events.set(GalleryMessage{"gallery.feedback.138", {}}); })
                    .value(state->rating)
                    .allowHalf()
                    .allowClear()
                    .disabled(state->rating_disabled)
                    .readOnly(state->rating_read_only)
                    .visible(state->rating_visible)
                    .onChange([state](float candidate) {
                        if (state->rating_accept.get()) {
                            state->rating.set(candidate);
                        }
                        state->rating_request.set(
                            (state->rating_accept.get()
                                 ? GalleryMessage{"gallery.event.040.yes",
                                                  {{u8"v1",
                                                    utf8(std::string{
                                                        metric_raw_value(StatisticValue{static_cast<double>(candidate)})
                                                            .bytes()})}}}
                                 : GalleryMessage{
                                       "gallery.event.040.no",
                                       {{u8"v1", utf8(std::string{
                                                     metric_raw_value(StatisticValue{static_cast<double>(candidate)})
                                                         .bytes()})}}}));
                    })
                    .tooltips(
                        {RateTooltip{gallery_text("gallery.complete.copy.463")},
                         RateTooltip{gallery_text("gallery.complete.copy.464")},
                         RateTooltip{gallery_text("gallery.complete.copy.465")},
                         RateTooltip{gallery_text("gallery.complete.copy.466")}, RateTooltip{TooltipTitle{[state] {
                             Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)), FlexContent{[state] {
                                      Input(
                                          InputProps{}
                                              .defaultValue(gallery_initial_text("gallery.rate.note", u8"补充服务评价"))
                                              .placeholder(gallery_text("gallery.copy.property.114"))
                                              .ref(state->rating_ref)
                                              .onChange(gallery_value_changed<ryn::String>(
                                                  "gallery.rate.note",
                                                  [state](String value) { state->rating_note.set(value); }))
                                              .layout(member_control_margin().width(dp(180))));
                                      gallery_target("gallery.rate.note");
                                      Button(
                                          ButtonProps{}
                                              .onClick([state] {
                                                  state->rating_feedback.set(GalleryMessage{
                                                      "gallery.event.041",
                                                      {{u8"v0", utf8(std::string{state->rating_note.get().bytes()})}}});
                                              })
                                              .layout(member_control_margin()),
                                          [] { gallery_label("gallery.copy.sample.557"); });
                                  }});
                         }}}})
                    .layout(member_control_margin()));
            gallery_target("gallery.rate.control");
            Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                  SpaceContent{[state] {
                      Button(ButtonProps{}.onClick([state] {
                          state->rating_direction.set(state->rating_direction.get() == RateDirection::LeftToRight
                                                          ? RateDirection::RightToLeft
                                                          : RateDirection::LeftToRight);
                      }),
                             [] { gallery_label("gallery.copy.sample.558"); });
                      gallery_target("gallery.rate.direction");
                      Button(ButtonProps{}.onClick([state] { state->rating_hearts.set(!state->rating_hearts.get()); }),
                             [] { gallery_label("gallery.copy.sample.559"); });
                      gallery_target("gallery.rate.character");
                      Button(ButtonProps{}.onClick([state] { static_cast<void>(state->rating_control_ref.focus()); }),
                             [] { gallery_label("gallery.copy.sample.560"); });
                      gallery_target("gallery.rate.focus");
                      Button(ButtonProps{}
                                 .onClick([state] { state->rating_accept.set(!state->rating_accept.get()); })
                                 .layout(member_control_margin()),
                             [] { gallery_label("gallery.copy.sample.561"); });
                      gallery_target("gallery.rate.authority");
                      Button(ButtonProps{}
                                 .onClick([state] { state->rating_disabled.set(!state->rating_disabled.get()); })
                                 .layout(member_control_margin()),
                             [] { gallery_label("gallery.copy.sample.562"); });
                      gallery_target("gallery.rate.disabled");
                      Button(ButtonProps{}
                                 .onClick([state] { state->rating_read_only.set(!state->rating_read_only.get()); })
                                 .layout(member_control_margin()),
                             [] { gallery_label("gallery.copy.sample.563"); });
                      gallery_target("gallery.rate.readonly");
                      Button(ButtonProps{}
                                 .onClick([state] { state->rating_visible.set(!state->rating_visible.get()); })
                                 .layout(member_control_margin()),
                             [] { gallery_label("gallery.copy.sample.564"); });
                      gallery_target("gallery.rate.visible");
                      Button(ButtonProps{}
                                 .onClick([state] {
                                     state->rating_visible.set(true);
                                     state->rating_disabled.set(false);
                                     state->rating_read_only.set(false);
                                     state->rating_accept.set(true);
                                 })
                                 .layout(member_control_margin()),
                             [] { gallery_label("gallery.copy.sample.565"); });
                      gallery_target("gallery.rate.restore");
                  }});
        }});
    ++state->telemetry.live_samples;
}

void add_complete_autocomplete_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.566");
    std::vector<AutoCompleteOption> cities;
    for (int i = 0; i < 1000; ++i) {
        const auto key = utf8("city-" + std::to_string(i));
        cities.push_back({key, key, utf8("合成城市 " + std::to_string(i))});
    }
    AutoComplete(AutoCompleteProps{}
                     .groups(bind([cities = std::move(cities), tr = useI18n()] {
                         auto rows = cities;
                         for (std::size_t i = 0; i < rows.size(); ++i) {
                             rows[i].label = tr.translate(gallery_message("gallery.complete.copy.467"),
                                                          {{u8"index", static_cast<std::uint64_t>(i)}});
                         }
                         return std::vector<AutoCompleteOptionGroup>{
                             {u8"cities", tr.translate(gallery_message("gallery.complete.copy.468")), std::move(rows)},
                             {u8"people",
                              tr.translate(gallery_message("gallery.complete.copy.469")),
                              {{u8"group-ada", u8"Ada", u8"Ada"}}}};
                     }))
                     .filterOption(AutoCompleteFilter{true})
                     .listHeight(160)
                     .direction(TextDirection::RightToLeft)
                     .motion()
                     .placeholder(gallery_text("gallery.copy.property.115"))
                     .layout(member_control_margin().width(dp(320))));
    remember_target(state, "gallery.auto-complete.virtual");
    gallery_label("gallery.copy.sample.567");
    AutoComplete(AutoCompleteProps{}
                     .options(gallery_option_labels<ryn::AutoCompleteOption>(
                         std::vector<ryn::AutoCompleteOption>{{u8"custom-a", u8"第一段", u8"第一段"},
                                                              {u8"custom-b", u8"第二段", u8"第二段"}},
                         {{u8"custom-a", "gallery.complete.copy.120"}, {u8"custom-b", "gallery.complete.copy.121"}}))
                     .editor(AutoCompleteEditor{[](AutoCompleteEditorContext context) {
                         TextArea(context.text_area.rows(std::size_t{2}).resize(TextAreaResize::None));
                     }})
                     .popupRender(AutoCompletePopupRenderer{[](std::function<void()> origin) {
                         Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[origin] {
                                  origin();
                                  Input(InputProps{}.placeholder(gallery_text("gallery.copy.property.116")));
                                  gallery_target("gallery.auto-complete.popup-note");
                              }});
                     }})
                     .backfill()
                     .allowClear()
                     .placeholder(gallery_text("gallery.copy.property.117"))
                     .layout(member_control_margin().width(dp(320))));
    remember_target(state, "gallery.auto-complete.custom");
    gallery_label("gallery.copy.sample.568");
    const auto weak = std::weak_ptr{state};
    AutoCompleteSource source{[weak](AsyncOptionsRequest request, AutoCompleteSource::Complete complete) {
                                  if (auto state = weak.lock()) {
                                      state->completion_jobs.push_back([request, complete = std::move(complete)] {
                                          const auto suggestion =
                                              utf8(std::string{request.query.bytes()} + " · synthetic");
                                          complete({{{u8"synthetic", suggestion, suggestion}}, {}});
                                      });
                                  }
                                  return AsyncOptionsCancel{};
                              },
                              [weak](std::function<void()> task) {
                                  if (auto state = weak.lock()) {
                                      state->owner_posts.push_back(std::move(task));
                                  }
                              }};
    AutoComplete(AutoCompleteProps{}
                     .source(source)
                     .options(gallery_option_labels<AutoCompleteOption>(
                         std::vector<AutoCompleteOption>{{u8"synthetic-wait", u8"", String{}, true}},
                         {{u8"synthetic-wait", "gallery.complete.copy.369"}}))
                     .popupRender(AutoCompletePopupRenderer{[state](std::function<void()> origin) {
                         Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[state, origin] {
                                  origin();
                                  Button(ButtonProps{}.onClick([state] {
                                      auto jobs = std::move(state->completion_jobs);
                                      state->completion_jobs.clear();
                                      for (auto& job : jobs) {
                                          job();
                                      }
                                      auto posts = std::move(state->owner_posts);
                                      state->owner_posts.clear();
                                      for (auto& post : posts) {
                                          post();
                                      }
                                      state->async_feedback.set(GalleryMessage{"gallery.feedback.139", {}});
                                  }),
                                         ButtonContent{[] { gallery_label("gallery.copy.sample.569"); }});
                                  gallery_target("gallery.auto-complete.deliver");
                              }});
                     }})
                     .placeholder(gallery_text("gallery.copy.property.118"))
                     .onLoadError([state](String error) { state->async_feedback.set(std::move(error)); })
                     .layout(member_control_margin().width(dp(320))));

    remember_target(state, "gallery.auto-complete.async");
    Text(TextProps{}.content(state->async_feedback));
    state->telemetry.live_samples += 3;
}

void add_mentions_async_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    gallery_label("gallery.copy.sample.570");
    const auto weak = std::weak_ptr{state};
    MentionsSource source{[weak](AsyncOptionsRequest request, MentionsSource::Complete complete) {
                              if (auto state = weak.lock()) {
                                  state->completion_jobs.push_back([request, complete = std::move(complete)] {
                                      const auto value = utf8(std::string{request.query.bytes()} + "-synthetic");
                                      complete({{{u8"synthetic", value, value}}, {}});
                                  });
                              }
                              return AsyncOptionsCancel{};
                          },
                          [weak](std::function<void()> apply) {
                              if (auto state = weak.lock()) {
                                  state->owner_posts.push_back(std::move(apply));
                              }
                          }};
    Mentions(
        MentionsProps{}
            .source(source)
            .rows(std::size_t{2})
            .placeholder(gallery_text("gallery.copy.property.119"))
            .onSearch([state](String text, String prefix) {
                state->mentions_async_feedback.set(GalleryMessage{
                    "gallery.event.042",
                    {{u8"v0", utf8(std::string{prefix.bytes()})}, {u8"v1", utf8(std::string{text.bytes()})}}});
            })
            .onSelect([state](MentionsOption option, String prefix) {
                state->mentions_async_feedback.set(GalleryMessage{
                    "gallery.event.043",
                    {{u8"v0", utf8(std::string{prefix.bytes()})}, {u8"v1", utf8(std::string{option.value.bytes()})}}});
            })
            .onLoadError([state](String error) { state->mentions_async_feedback.set(std::move(error)); })
            .popupRender(MentionsPopupRenderer{[state](std::function<void()> origin) {
                Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[state, origin] {
                         origin();
                         Button(ButtonProps{}.onClick([state] {
                             auto jobs = std::move(state->completion_jobs);
                             state->completion_jobs.clear();
                             for (auto& job : jobs) {
                                 job();
                             }
                             auto posts = std::move(state->owner_posts);
                             state->owner_posts.clear();
                             for (auto& post : posts) {
                                 post();
                             }
                         }),
                                ButtonContent{[] { gallery_label("gallery.copy.sample.571"); }});
                         gallery_target("gallery.mentions.deliver");
                     }});
            }})
            .layout(LayoutStyle{}.width(state->cell_width)));
    remember_target(state, "gallery.mentions.async");
    Text(TextProps{}.content(state->mentions_async_feedback));
}

void add_carousel_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    CarouselRef ref;
    Signal<bool> autoplay{false};
    Signal<bool> vertical{false};
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    Signal<CarouselEffect> effect{CarouselEffect::ScrollX};
    GalleryFeedback feedback{GalleryMessage{"gallery.feedback.040", {}}, useI18n()};
    std::vector<CarouselSlide> slides;
    for (int i = 0; i < 3; ++i) {
        const auto key = utf8("panel-" + std::to_string(i + 1));
        slides.emplace_back(
            key, CarouselContent{[key, feedback] {
                Flex(
                    FlexProps{}
                        .vertical(true)
                        .align(FlexAlign::Start)
                        .gap(dp(8))
                        .layout(LayoutStyle{}.margin_left(dp(48)).margin_right(dp(48)).margin_top(dp(16)).margin_bottom(
                            dp(44))),
                    [key, feedback] {
                        Text(TextProps{}.content(key));
                        Input(InputProps{}
                                  .defaultValue(u8"保留面板草稿")
                                  .onChange([key, feedback](String text) {
                                      feedback.set(GalleryMessage{"gallery.event.044",
                                                                  {{u8"v0", utf8(std::string{key.bytes()})},
                                                                   {u8"v1", utf8(std::string{text.bytes()})}}});
                                  })
                                  .layout(LayoutStyle{}.width(dp(300))));
                        Popover(PopoverProps{}, PopoverTrigger{[] {
                                    Button(ButtonProps{}, [] { gallery_label("gallery.copy.sample.572"); });
                                }},
                                PopoverSlots{.content = PopoverContent{
                                                 [] { Input(InputProps{}.defaultValue(u8"子浮层备注")); }}});
                    });
            }});
    }
    ThemeConfig theme;
    theme.carousel.tokens.arrow = Color::rgba8(22, 119, 255);
    theme.carousel.tokens.dot = Color::rgba8(160, 160, 160);
    theme.carousel.tokens.dot_active = Color::rgba8(22, 119, 255);
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(8))
             .layout(LayoutStyle{}.width(state->document_width)),
         [state, ref, autoplay, vertical, direction, effect, feedback, slides, theme] {
             Text(TextProps{}.content(feedback));
             Theme(ThemeProps{}.config(theme), ThemeContent{[=] {
                       Carousel(CarouselProps{}
                                    .ref(ref)
                                    .slides(slides)
                                    .effect(effect)
                                    .arrows()
                                    .draggable()
                                    .infinite()
                                    .vertical(vertical)
                                    .direction(direction)
                                    .autoplay(autoplay)
                                    .autoplaySpeed(Duration::milliseconds(1000))
                                    .dotDuration()
                                    .beforeChange([feedback](std::size_t from, std::size_t to) {
                                        feedback.set(GalleryMessage{"gallery.event.045",
                                                                    {{u8"v0", utf8(std::to_string(from))},
                                                                     {u8"v1", utf8(std::to_string(to))}}});
                                    })
                                    .afterChange([feedback](std::size_t to) {
                                        feedback.set(
                                            GalleryMessage{"gallery.event.046", {{u8"v0", utf8(std::to_string(to))}}});
                                    })
                                    .layout(LayoutStyle{}.width(state->document_width)));
                       remember_target(state, "gallery.carousel.main");
                   }});
             Flex(FlexProps{}.wrap(true).gap(dp(8)), [=] {
                 Button(ButtonProps{}.onClick([ref] { static_cast<void>(ref.prev()); }),
                        [] { gallery_label("gallery.complete.copy.201"); });
                 Button(ButtonProps{}.onClick([ref] { static_cast<void>(ref.next()); }),
                        [] { gallery_label("gallery.complete.copy.202"); });
                 Button(ButtonProps{}.onClick([effect] {
                     effect.set(effect.get() == CarouselEffect::Fade ? CarouselEffect::ScrollX : CarouselEffect::Fade);
                 }),
                        [] { gallery_label("gallery.complete.copy.203"); });
                 remember_target(state, "gallery.carousel.effect");
                 Button(ButtonProps{}.onClick([autoplay] { autoplay.set(!autoplay.get()); }),
                        [] { gallery_label("gallery.complete.copy.204"); });
                 remember_target(state, "gallery.carousel.autoplay");
                 Button(ButtonProps{}.onClick([ref] { static_cast<void>(ref.pause()); }),
                        [] { gallery_label("gallery.complete.copy.205"); });
                 remember_target(state, "gallery.carousel.pause");
                 Button(ButtonProps{}.onClick([ref] { static_cast<void>(ref.play()); }),
                        [] { gallery_label("gallery.complete.copy.206"); });
                 remember_target(state, "gallery.carousel.play");
                 Button(ButtonProps{}.onClick([vertical] { vertical.set(!vertical.get()); }),
                        [] { gallery_label("gallery.complete.copy.207"); });
                 Button(ButtonProps{}.onClick([direction] {
                     direction.set(direction.get() == TextDirection::LeftToRight ? TextDirection::RightToLeft
                                                                                 : TextDirection::LeftToRight);
                 }),
                        [] { Text(u8"RTL"); });
             });
             gallery_label("gallery.complete.copy.024");
         });
}

void add_carousel_controlled_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    CarouselRef ref;
    Signal<std::size_t> current{0};
    Signal<bool> accept{false};
    GalleryFeedback feedback{GalleryMessage{"gallery.feedback.041", {}}, useI18n()};
    std::vector<CarouselSlide> original;
    for (int i = 0; i < 3; ++i) {
        auto key = utf8("key-" + std::to_string(i));
        original.emplace_back(
            key, CarouselContent{[key] {
                Flex(FlexProps{}.vertical(true).gap(dp(8)).layout(
                         LayoutStyle{}.margin_left(dp(48)).margin_right(dp(48)).margin_top(dp(16)).margin_bottom(
                             dp(60))),
                     [key] {
                         Text(TextProps{}.content(key));
                         TextArea(TextAreaProps{}
                                      .defaultValue(u8"动态重排保留草稿")
                                      .rows(2)
                                      .layout(LayoutStyle{}.width(dp(300))));
                     });
            }});
    }
    Signal<std::vector<CarouselSlide>> slides{original};
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(8))
             .layout(LayoutStyle{}.width(state->document_width)),
         [state, ref, current, accept, feedback, slides, original] {
             Text(TextProps{}.content(feedback));
             Carousel(
                 CarouselProps{}
                     .ref(ref)
                     .slides(slides)
                     .current(current)
                     .arrows()
                     .draggable()
                     .infinite(false)
                     .adaptiveHeight()
                     .onCurrentChange([current, accept, feedback](std::size_t target, CarouselChangeSource source) {
                         feedback.set((accept.get() ? GalleryMessage{"gallery.event.047.yes",
                                                                     {{u8"v1", utf8(std::to_string(target))},
                                                                      {u8"v2", utf8(std::to_string(int(source)))}}}
                                                    : GalleryMessage{"gallery.event.047.no",
                                                                     {{u8"v1", utf8(std::to_string(target))},
                                                                      {u8"v2", utf8(std::to_string(int(source)))}}}));
                         if (accept.get()) {
                             current.set(target);
                         }
                     })
                     .customPaging([](Binding<CarouselDotInfo> info, std::function<void()> activate) {
                         return CarouselDot{[info, activate] {
                             Flex(FlexProps{}.gap(dp(4)), [info, activate] {
                                 Button(ButtonProps{}.onClick(activate), [info] {
                                     Text(TextProps{}.content(bind([info] {
                                         auto value = info.get();
                                         return utf8(std::string{value.active ? "● " : "○ "} +
                                                     std::string{value.key.bytes()});
                                     })));
                                 });
                                 Input(InputProps{}.defaultValue(u8"dot 草稿").layout(LayoutStyle{}.width(dp(72))));
                             });
                         }};
                     })
                     .layout(LayoutStyle{}.width(state->document_width)));
             remember_target(state, "gallery.carousel.controlled");
             Flex(FlexProps{}.wrap(true).gap(dp(8)), [=] {
                 Button(ButtonProps{}.onClick([accept] { accept.set(!accept.get()); }),
                        [] { gallery_label("gallery.copy.sample.573"); });
                 Button(ButtonProps{}.onClick([ref] { static_cast<void>(ref.next()); }),
                        [] { gallery_label("gallery.complete.copy.208"); });
                 Button(ButtonProps{}.onClick([slides] {
                     auto values = slides.get();
                     std::reverse(values.begin(), values.end());
                     slides.set(std::move(values));
                 }),
                        [] { gallery_label("gallery.copy.sample.574"); });
                 Button(ButtonProps{}.onClick([slides] {
                     auto values = slides.get();
                     if (!values.empty()) {
                         values.pop_back();
                         slides.set(std::move(values));
                     }
                 }),
                        [] { gallery_label("gallery.copy.sample.575"); });
                 Button(ButtonProps{}.onClick([slides, original] { slides.set(original); }),
                        [] { gallery_label("gallery.copy.sample.576"); });
             });
         });
}

const ryn::ThemeConfig& calendar_cell_theme(bool narrow) {
    static const ryn::ThemeConfig regular;
    static const ryn::ThemeConfig compact = [] {
        ryn::ThemeConfig theme;
        theme.calendar.tokens.cell_padding = ryn::dp(2);
        theme.button.tokens.padding_inline_small = ryn::dp(2);
        theme.input.tokens.padding_inline = ryn::dp(2);
        theme.input.tokens.padding_inline_small = ryn::dp(2);
        return theme;
    }();
    return narrow ? compact : regular;
}

void add_calendar_custom_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<CivilDate> value{CivilDate{2024, 2, 29}};
    GalleryFeedback event{GalleryMessage{"gallery.complete.copy.242", {}}, useI18n()};
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(8))
             .layout(LayoutStyle{}.width(state->document_width)),
         [state, value, event] {
             Text(TextProps{}.content(event));
             Theme(ThemeProps{}.config(bind([state] { return calendar_cell_theme(state->narrow_layout.get()); })),
                   ThemeContent{[state, value, event] {
                       Calendar(
                           CalendarProps{}
                               .value(value)
                               .showWeek()
                               .validRange(CalendarRange{{2024, 1, 1}, {2025, 12, 31}})
                               .headerRender([event](CalendarHeaderContext context) {
                                   return CalendarHeader{[context, event] {
                                       Flex(FlexProps{}.wrap(true).gap(dp(8)), [context, event] {
                                           Text(TextProps{}.content(
                                               bind([context] { return format_iso_date(context.info.get().value); })));
                                           Button(ButtonProps{}.onClick([context] {
                                               static_cast<void>(context.calendar.select({2024, 12, 31}));
                                           }),
                                                  [] { Text(u8"2024-12-31"); });
                                           Button(ButtonProps{}.onClick([context] {
                                               static_cast<void>(context.calendar.changeMode(CalendarMode::Year));
                                           }),
                                                  [] { gallery_label("gallery.complete.copy.209"); });
                                           Button(ButtonProps{}.onClick([context] {
                                               static_cast<void>(context.calendar.changeMode(CalendarMode::Month));
                                           }),
                                                  [] { gallery_label("gallery.complete.copy.210"); });
                                           Input(InputProps{}
                                                     .defaultValue(u8"保留 header 草稿")
                                                     .layout(LayoutStyle{}.width(dp(180))));
                                       });
                                   }};
                               })
                               .fullCellRender(CalendarCellRenderer{[](Binding<CalendarCellInfo> info) {
                                   return CalendarCell{[info] {
                                       Flex(FlexProps{}.vertical(true).gap(dp(4)), [info] {
                                           Text(TextProps{}.content(bind([info] {
                                               const auto cell = info.get();
                                               return cell.mode == CalendarMode::Month
                                                          ? utf8(std::to_string(cell.date.day()))
                                                          : utf8(std::to_string(cell.date.month()));
                                           })));
                                           Popover(PopoverProps{}, PopoverTrigger{[] {
                                                       Button(ButtonProps{}.size(ControlSize::Small),
                                                              [] { gallery_label("gallery.copy.sample.577"); });
                                                   }},
                                                   PopoverSlots{.content = PopoverContent{[info] {
                                                                    Text(TextProps{}.content(bind([info] {
                                                                        return format_iso_date(info.get().date);
                                                                    })));
                                                                    Input(InputProps{}.defaultValue(u8"保留日期备注"));
                                                                }}});
                                       });
                                   }};
                               }})
                               .onChange([value](CivilDate date) { value.set(date); })
                               .onPanelChange([event](CivilDate date, CalendarMode mode) {
                                   event.set(GalleryMessage{"gallery.complete.copy.420",
                                                            {{u8"date", format_iso_date(date)},
                                                             {u8"value", static_cast<std::int64_t>(mode)}}});
                               })
                               .onSelect([event](CivilDate date, CalendarSelectInfo info) {
                                   event.set(GalleryMessage{"gallery.complete.copy.421",
                                                            {{u8"date", format_iso_date(date)},
                                                             {u8"value", static_cast<std::int64_t>(info.source)}}});
                               })
                               .layout(LayoutStyle{}.width(state->document_width)));
                   }});
             remember_target(state, "gallery.calendar.custom");
         });
}

void add_calendar_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<CivilDate> value{CivilDate{2024, 2, 29}};
    GalleryFeedback feedback{GalleryMessage{"gallery.feedback.042", {}}, useI18n()};
    Signal<bool> mini{false};
    Signal<PickerLocale> locale{PickerLocale{}};
    Signal<std::optional<CalendarRange>> range{std::optional<CalendarRange>{}};
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(8))
             .layout(LayoutStyle{}.width(state->document_width)),
         [state, value, feedback, mini, locale, range] {
             Text(TextProps{}.content(feedback));
             Theme(
                 ThemeProps{}.config(bind([state] { return calendar_cell_theme(state->narrow_layout.get()); })),
                 ThemeContent{[state, value, feedback, mini, locale, range] {
                     Calendar(
                         CalendarProps{}
                             .value(value)
                             .fullscreen(bind([mini] { return !mini.get(); }))
                             .locale(locale)
                             .validRange(range)
                             .showWeek()
                             .clock([] { return CivilDate{2024, 2, 29}; })
                             .showToday()
                             .onChange([value, feedback](CivilDate date) {
                                 value.set(date);
                                 feedback.set(format_iso_date(date));
                             })
                             .onPanelChange([feedback](CivilDate date, CalendarMode mode) {
                                 feedback.set(
                                     (mode == CalendarMode::Month
                                          ? GalleryMessage{"gallery.event.048.yes",
                                                           {{u8"v0", utf8(std::string{format_iso_date(date).bytes()})}}}
                                          : GalleryMessage{
                                                "gallery.event.048.no",
                                                {{u8"v0", utf8(std::string{format_iso_date(date).bytes()})}}}));
                             })
                             .onSelect([feedback](CivilDate date, CalendarSelectInfo info) {
                                 feedback.set(
                                     GalleryMessage{"gallery.event.049",
                                                    {{u8"v0", utf8(std::string{format_iso_date(date).bytes()})},
                                                     {u8"v1", utf8(std::to_string(int(info.source)))}}});
                             })
                             .cellRender(CalendarCellRenderer{[feedback](Binding<CalendarCellInfo> info) {
                                 return CalendarCell{[info, feedback] {
                                     Input(InputProps{}
                                               .defaultValue(u8"日程备注")
                                               .placeholder(bind([info, tr = useI18n()] {
                                                   return tr.translate(
                                                       gallery_message(info.get().today ? "gallery.complete.copy.461"
                                                                                        : "gallery.complete.copy.462"));
                                               }))
                                               .onChange([info, feedback](String draft) {
                                                   feedback.set(GalleryMessage{
                                                       "gallery.event.050",
                                                       {{u8"v0",
                                                         utf8(std::string{format_iso_date(info.get().date).bytes()})},
                                                        {u8"v1", utf8(std::string{draft.bytes()})}}});
                                               }));
                                 }};
                             }})
                             .layout(LayoutStyle{}.width(state->document_width)));
                 }});
             remember_target(state, "gallery.calendar.main");
             Button(ButtonProps{}.onClick([mini] { mini.set(!mini.get()); }),
                    [] { gallery_label("gallery.copy.sample.578"); });
             remember_target(state, "gallery.calendar.mini");
             Button(ButtonProps{}.onClick([locale] {
                 auto next = locale.get();
                 next.first_day = next.first_day == 0 ? 1 : 0;
                 locale.set(std::move(next));
             }),
                    [] { gallery_label("gallery.copy.sample.579"); });
             remember_target(state, "gallery.calendar.locale");
             Button(ButtonProps{}.onClick([range] {
                 range.set(range.get() ? std::optional<CalendarRange>{}
                                       : std::optional{CalendarRange{{2024, 2, 10}, {2024, 3, 15}}});
             }),
                    [] { gallery_label("gallery.copy.sample.580"); });
             remember_target(state, "gallery.calendar.range");
         });
}

void add_mentions_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(10))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             gallery_label("gallery.copy.sample.581");
             Text(TextProps{}.content(bind([state] {
                 return state->i18n.translate(
                     gallery_message("gallery.snapshot.009"),
                     MessageArgs{{u8"v0", utf8(std::string{state->mentions_field.get().bytes()})},
                                 {u8"v1", utf8(std::string{state->mentions_feedback.get().bytes()})}});
             })));
             const MentionsRichLabel rich{MentionsOptionLabel{[state] {
                 Flex(FlexProps{}.align(FlexAlign::Center).gap(dp(6)), FlexContent{[state] {
                          Text(u8"Ada");
                          Input(InputProps{}
                                    .value(state->mentions_note)
                                    .size(ControlSize::Small)
                                    .layout(LayoutStyle{}.width(dp(150)))
                                    .onChange([state](String value) { state->mentions_note.set(std::move(value)); }));
                          remember_target(state, "gallery.mentions.note");
                      }});
             }}};
             Form(FormProps{}.ref(state->mentions_form), FormContent{[state, rich] {
                      FormItem(FormItemProps{}.field(u8"text", state->mentions_field).control(state->mentions_ref),
                               FormItemContent{[state, rich] {
                                   const auto change = state->mentions_field.onChange();
                                   Mentions(MentionsProps{}
                                                .ref(state->mentions_ref)
                                                .value(state->mentions_field.value())
                                                .onChange([state, change](String value) {
                                                    if (state->mentions_accept.get()) {
                                                        change(value);
                                                    } else {
                                                        state->mentions_feedback.set(
                                                            GalleryMessage{"gallery.feedback.140", {}});
                                                    }
                                                })
                                                .prefix({u8"@", u8"@@", u8"#", u8"提及"})
                                                .onSearch([state](String text, String prefix) {
                                                    state->mentions_prefix.set(prefix);
                                                    state->mentions_feedback.set(
                                                        GalleryMessage{"gallery.event.051",
                                                                       {{u8"v0", utf8(std::string{prefix.bytes()})},
                                                                        {u8"v1", utf8(std::string{text.bytes()})}}});
                                                })
                                                .onSelect([state](MentionsOption option, String prefix) {
                                                    state->mentions_feedback.set(GalleryMessage{
                                                        "gallery.event.052",
                                                        {{u8"v0", utf8(std::string{prefix.bytes()})},
                                                         {u8"v1", utf8(std::string{option.value.bytes()})}}});
                                                })
                                                .options(gallery_option_labels<MentionsOption>(
                                                    bind([state, rich] {
                                                        if (state->mentions_prefix.get() == String{u8"#"}) {
                                                            return std::vector<MentionsOption>{
                                                                {u8"native", u8"native", u8"原生组件"},
                                                                {u8"unicode", u8"Unicode", u8"Unicode 编辑"}};
                                                        }
                                                        return std::vector<MentionsOption>{
                                                            {u8"ada", u8"Ada", u8"Ada", false, rich},
                                                            {u8"disabled", u8"Alan", u8"禁用联系人", true},
                                                            {u8"grace", u8"Grace", u8"Grace"},
                                                            {u8"ming", u8"阿明", u8"阿明"}};
                                                    }),
                                                    {{u8"native", "gallery.complete.copy.364"},
                                                     {u8"unicode", "gallery.complete.copy.365"},
                                                     {u8"disabled", "gallery.complete.copy.366"}}))
                                                .disabled(state->mentions_disabled)
                                                .readOnly(state->mentions_read_only)
                                                .visible(state->mentions_visible)
                                                .direction(bind([state] {
                                                    return state->mentions_rtl.get() ? TextDirection::RightToLeft
                                                                                     : TextDirection::Auto;
                                                }))
                                                .placement(bind([state] {
                                                    return state->mentions_top.get() ? MentionsPlacement::Top
                                                                                     : MentionsPlacement::Bottom;
                                                }))
                                                .autoSize(TextAreaAutoSize{true, 2, 5})
                                                .maxLength(std::size_t{500})
                                                .allowClear()
                                                .motion()
                                                .placeholder(gallery_text("gallery.copy.property.120"))
                                                .popupWidth(320.0F)
                                                .layout(LayoutStyle{}.width(state->cell_width)));
                                   remember_target(state, "gallery.mentions.editor");
                               }});
                  }});
             Flex(FlexProps{}.gap(dp(6)).wrap(true), FlexContent{[state] {
                      Button(
                          ButtonProps{}.onClick([state] { state->mentions_accept.set(!state->mentions_accept.get()); }),
                          ButtonContent{[] { gallery_label("gallery.copy.sample.582"); }});
                      remember_target(state, "gallery.mentions.accept");
                      Button(ButtonProps{}.onClick(
                                 [state] { state->mentions_disabled.set(!state->mentions_disabled.get()); }),
                             ButtonContent{[] { gallery_label("gallery.copy.sample.583"); }});
                      remember_target(state, "gallery.mentions.disabled");
                      Button(ButtonProps{}.onClick(
                                 [state] { state->mentions_read_only.set(!state->mentions_read_only.get()); }),
                             ButtonContent{[] { gallery_label("gallery.copy.sample.584"); }});
                      remember_target(state, "gallery.mentions.readonly");
                      Button(ButtonProps{}.onClick(
                                 [state] { state->mentions_visible.set(!state->mentions_visible.get()); }),
                             ButtonContent{[] { gallery_label("gallery.copy.sample.585"); }});
                      remember_target(state, "gallery.mentions.visible");
                      Button(ButtonProps{}.onClick([state] { state->mentions_rtl.set(!state->mentions_rtl.get()); }),
                             ButtonContent{[] { gallery_label("gallery.copy.sample.586"); }});
                      remember_target(state, "gallery.mentions.rtl");
                      Button(ButtonProps{}.onClick([state] { state->mentions_top.set(!state->mentions_top.get()); }),
                             ButtonContent{[] { gallery_label("gallery.copy.sample.587"); }});
                      remember_target(state, "gallery.mentions.placement");
                      Button(ButtonProps{}.onClick([state] {
                          (void)state->mentions_form.reset();
                          state->mentions_feedback.set(GalleryMessage{"gallery.feedback.141", {}});
                      }),
                             ButtonContent{[] { gallery_label("gallery.copy.sample.588"); }});
                      remember_target(state, "gallery.mentions.reset");
                  }});
             gallery_label("gallery.copy.sample.589");
             std::vector<MentionsOption> many;
             many.reserve(10000);
             for (int index = 0; index < 10000; ++index) {
                 const auto key = utf8("person-" + std::to_string(index));
                 many.push_back({key, key, key});
             }
             Mentions(MentionsProps{}
                          .options(std::move(many))
                          .filterOption(false)
                          .listHeight(128.0F)
                          .placeholder(gallery_text("gallery.copy.property.121"))
                          .onSelect([state](MentionsOption option, String prefix) {
                              state->mentions_feedback.set(
                                  GalleryMessage{"gallery.event.053",
                                                 {{u8"v0", utf8(std::string{prefix.bytes()})},
                                                  {u8"v1", utf8(std::string{option.value.bytes()})}}});
                          })
                          .layout(LayoutStyle{}.width(state->cell_width)));
             remember_target(state, "gallery.mentions.long");
             add_mentions_async_samples(state);
             state->telemetry.live_samples += 3;
         }});
}

void add_contact_autocomplete_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(gallery_text("gallery.copy.property.122"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.auto-complete.contacts", true);
             Text(TextProps{}
                      .content(bind([state] {
                          return state->i18n.translate(
                              gallery_message("gallery.snapshot.010"),
                              MessageArgs{{u8"v0", utf8(std::string{state->contact_value.get().bytes()})},
                                          {u8"v1", utf8(std::string{state->contact_request.get().bytes()})},
                                          {u8"v2", utf8(std::string{state->contact_saved.get().bytes()})}});
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.auto-complete.feedback", true);
             const AutoCompleteRichLabel rich{AutoCompleteOptionLabel{[state] {
                 Flex(FlexProps{}.align(FlexAlign::Center).gap(dp(4)), FlexContent{[state] {
                          Input(InputProps{}
                                    .value(state->contact_note)
                                    .size(ControlSize::Small)
                                    .ref(state->contact_note_ref)
                                    .onChange([state](String value) { state->contact_note.set(value); })
                                    .placeholder(gallery_text("gallery.copy.property.123"))
                                    .layout(
                                        member_control_margin().margin_top(dp(0)).margin_bottom(dp(0)).width(dp(132))));
                          Button(ButtonProps{}
                                     .size(ControlSize::Small)
                                     .onClick([state] {
                                         state->contact_saved.set(GalleryMessage{
                                             "gallery.complete.copy.142", {{u8"note", state->contact_note.get()}}});
                                     })
                                     .layout(member_control_margin().margin_top(dp(0)).margin_bottom(dp(0))),
                                 [] { gallery_label("gallery.copy.sample.590"); });
                      }});
             }}};
             AutoComplete(
                 AutoCompleteProps{}
                     .value(state->contact_value)
                     .ref(state->contact_ref)
                     .placeholder(gallery_text("gallery.copy.property.124"))
                     .filterOption(AutoCompleteFilter{true})
                     .backfill()
                     .allowClear()
                     .listHeight(224)
                     .disabled(state->contact_disabled)
                     .readOnly(state->contact_read_only)
                     .options(gallery_option_labels<AutoCompleteOption>(
                         bind([state, rich] {
                             std::vector<AutoCompleteOption> options{
                                 {u8"alice", u8"alice@example.com", u8"Alice"},
                                 {u8"blocked", u8"blocked@example.com", u8"停用联系人", true},
                                 {u8"chen", u8"chen@example.com", u8"Chen"},
                                 {u8"notes", u8"notes@example.com", u8"备注联系人", false, rich}};
                             if (state->contact_extra.get()) {
                                 options.push_back({u8"dora", u8"dora@example.com", u8"Dora"});
                             }
                             return options;
                         }),
                         {{u8"blocked", "gallery.complete.copy.367"}, {u8"notes", "gallery.complete.copy.368"}}))
                     .onChange([state](String value) {
                         state->contact_request.set((
                             state->contact_accept.get()
                                 ? GalleryMessage{"gallery.event.054.yes", {{u8"v1", utf8(std::string{value.bytes()})}}}
                                 : GalleryMessage{"gallery.event.054.no",
                                                  {{u8"v1", utf8(std::string{value.bytes()})}}}));
                         if (state->contact_accept.get()) {
                             state->contact_value.set(value);
                         }
                     })
                     .onSelect([state](String value, AutoCompleteOption) {
                         state->contact_request.set(
                             GalleryMessage{"gallery.event.055", {{u8"v0", utf8(std::string{value.bytes()})}}});
                     })
                     .layout(member_control_margin().width(dp(320))));
             remember_target(state, "gallery.auto-complete.input");
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}
                                  .onClick([state] { state->contact_accept.set(!state->contact_accept.get()); })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.591"); });
                       gallery_target("gallery.auto-complete.authority");
                       Button(ButtonProps{}
                                  .onClick([state] { state->contact_extra.set(!state->contact_extra.get()); })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.592"); });
                       gallery_target("gallery.auto-complete.options");
                       Button(ButtonProps{}
                                  .onClick([state] { state->contact_disabled.set(!state->contact_disabled.get()); })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.593"); });
                       gallery_target("gallery.auto-complete.disabled");
                       Button(ButtonProps{}
                                  .onClick([state] { state->contact_read_only.set(!state->contact_read_only.get()); })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.594"); });
                       gallery_target("gallery.auto-complete.readonly");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->contact_accept.set(true);
                                      state->contact_disabled.set(false);
                                      state->contact_read_only.set(false);
                                      static_cast<void>(state->contact_ref.focus());
                                  })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.595"); });
                       gallery_target("gallery.auto-complete.restore");
                   }});
             Text(TextProps{}
                      .content(
                          bind([state] { return utf8("" + std::string{state->contact_form_feedback.get().bytes()}); }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.auto-complete.form-feedback", true);
             Form(FormProps{}
                      .ref(state->contact_form)
                      .disabled(state->contact_disabled)
                      .readOnly(state->contact_read_only)
                      .onFinish([state](FormResult) {
                          state->contact_form_feedback.set(GalleryMessage{
                              "gallery.event.056", {{u8"v0", utf8(std::string{state->contact_field.get().bytes()})}}});
                      })
                      .layout(LayoutStyle{}.width(state->document_width)),
                  FormContent{[state] {
                      FormItem(FormItemProps{}
                                   .field(u8"contact", state->contact_field)
                                   .label(gallery_text("gallery.copy.property.125"))
                                   .control(state->contact_form_ref),
                               FormItemContent{[state] {
                                   AutoComplete(
                                       AutoCompleteProps{}
                                           .value(state->contact_field.value())
                                           .onChange(state->contact_field.onChange())
                                           .ref(state->contact_form_ref)
                                           .placeholder(gallery_text("gallery.copy.property.126"))
                                           .options({{u8"ada", u8"Ada", u8"Ada"}, {u8"grace", u8"Grace", u8"Grace"}})
                                           .defaultActiveFirstOption(false)
                                           .backfill()
                                           .allowClear()
                                           .disabled(state->contact_field.disabled())
                                           .readOnly(state->contact_field.readOnly())
                                           .status(state->contact_field.status())
                                           .layout(member_control_margin().width(dp(260))));
                                   remember_target(state, "gallery.auto-complete.form");
                               }});
                      Space(SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
                                Button(ButtonProps{}
                                           .onClick([state] { static_cast<void>(state->contact_form.submit()); })
                                           .layout(member_control_margin()),
                                       [] { gallery_label("gallery.copy.sample.596"); });
                                gallery_target("gallery.auto-complete.submit");
                                Button(ButtonProps{}
                                           .onClick([state] {
                                               static_cast<void>(state->contact_form.reset());
                                               state->contact_form_feedback.set(
                                                   GalleryMessage{"gallery.feedback.142", {}});
                                           })
                                           .layout(member_control_margin()),
                                       [] { gallery_label("gallery.copy.sample.597"); });
                                gallery_target("gallery.auto-complete.reset");
                            }});
                  }});
         }});
    state->telemetry.live_samples += 2;
    add_complete_autocomplete_samples(state);
}

ryn::String gallery_project_keys(const ryn::TreeKeys& keys) {
    std::string result;
    for (const auto& key : keys) {
        if (!result.empty()) {
            result += ',';
        }
        result += key.bytes();
    }
    return utf8(result);
}

std::pair<ryn::TreeKeys, ryn::TreeKeys> gallery_project_checks(const ryn::TreeKeys& leaves, bool archive_disabled) {
    ryn::TreeKeys checked = leaves;
    ryn::TreeKeys half;
    const auto has = [&](std::string_view key) {
        return std::ranges::any_of(leaves, [&](const auto& value) { return value.bytes() == key; });
    };
    const std::size_t source = has("controls") + has("theme");
    const std::size_t docs = has("guide") + (!archive_disabled && has("locked"));
    const auto assets = static_cast<std::size_t>(
        std::ranges::count_if(leaves, [](const auto& key) { return key.bytes().starts_with("asset-"); }));
    const auto group = [&](ryn::String key, std::size_t selected, std::size_t available) {
        if (selected == available) {
            checked.push_back(std::move(key));
        } else if (selected > 0) {
            half.push_back(std::move(key));
        }
    };
    group(ryn::String{u8"src"}, source, 2);
    group(ryn::String{u8"docs"}, docs, archive_disabled ? 1 : 2);
    group(ryn::String{u8"assets"}, assets, 16);
    group(ryn::String{u8"project"}, source + docs + assets, archive_disabled ? 19 : 20);
    return {std::move(checked), std::move(half)};
}

void add_tree_completion_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<std::vector<TreeItem>> nodes{
        std::vector<TreeItem>{TreeItem{u8"native-a", gallery_text("gallery.complete.copy.479")},
                              TreeItem{u8"native-b", gallery_text("gallery.complete.copy.480")},
                              TreeItem{u8"native-c", gallery_text("gallery.complete.copy.527")}}};
    Signal<TreeKeys> selected{TreeKeys{}};
    GalleryFeedback feedback{GalleryMessage{"gallery.feedback.043", {}}, useI18n()};
    Signal<String> search{String{}};
    Input(InputProps{}
              .placeholder(gallery_text("gallery.copy.property.127"))
              .value(search)
              .onChange([search](String value) mutable { search.set(value); })
              .layout(member_control_margin().width(dp(280))));
    Text(TextProps{}.content(feedback));
    Tree(TreeProps{}
             .items(nodes)
             .multiple()
             .selectedKeys(selected)
             .checkable()
             .showLine()
             .blockNode()
             .searchValue(search)
             .draggable()
             .onSelect([selected, feedback](TreeSelectInfo value) mutable {
                 selected.set(value.selected_keys);
                 feedback.set(GalleryMessage{"gallery.event.057",
                                             {{u8"v0", static_cast<std::uint64_t>(value.selected_keys.size())}}});
             })
             .onCheck([feedback](TreeCheckInfo value) mutable {
                 feedback.set(GalleryMessage{"gallery.event.058",
                                             {{u8"v0", static_cast<std::uint64_t>(value.checked_keys.size())},
                                              {u8"v1", static_cast<std::uint64_t>(value.half_checked_keys.size())}}});
             })
             .onDrop([nodes, feedback](TreeDragInfo drop) mutable {
                 auto next = nodes.get();
                 auto source = std::ranges::find(next, drop.key, &TreeItem::key);
                 auto target = std::ranges::find(next, drop.target_key, &TreeItem::key);
                 if (source == next.end() || target == next.end() || drop.position == TreeDropPosition::Inside) {
                     feedback.set(GalleryMessage{"gallery.feedback.143", {}});
                     return;
                 }
                 auto moved = *source;
                 next.erase(source);
                 target = std::ranges::find(next, drop.target_key, &TreeItem::key);
                 if (drop.position == TreeDropPosition::After) {
                     ++target;
                 }
                 next.insert(target, moved);
                 nodes.set(next);
                 feedback.set(GalleryMessage{"gallery.feedback.144", {}});
             })
             .allowDrop([](const TreeDragInfo& drop) { return drop.position != TreeDropPosition::Inside; })
             .layout(member_control_margin().width(dp(320)).height(dp(112))));
    remember_target(state, "gallery.tree.complete");
    Button(ButtonProps{}.onClick([nodes]() mutable {
        auto next = nodes.get();
        std::reverse(next.begin(), next.end());
        nodes.set(next);
    }),
           [] { gallery_label("gallery.copy.sample.598"); });
    remember_target(state, "gallery.tree.reorder");
    DirectoryTree(TreeProps{}.defaultExpandAll().rtl().layout(member_control_margin().width(dp(320)).height(dp(96))),
                  {TreeItem{u8"folder",
                            gallery_text("gallery.complete.copy.470"),
                            {TreeItem{u8"file", gallery_text("gallery.complete.copy.471")}.isLeaf()}}});
    std::vector<TreeItem> large;
    for (int i = 0; i < 10000; ++i) {
        large.emplace_back(
            utf8("virtual-" + std::to_string(i)),
            gallery_text("gallery.complete.copy.491", MessageArgs{{u8"index", static_cast<std::uint64_t>(i)}}));
    }
    TreeRef reference;
    Tree(TreeProps{}.virtualized().ref(reference).layout(member_control_margin().width(dp(320)).height(dp(120))),
         std::move(large));
    remember_target(state, "gallery.tree.virtual");
    Button(ButtonProps{}.onClick([reference] { static_cast<void>(reference.scrollTo({u8"virtual-9999"})); }),
           [] { gallery_label("gallery.copy.sample.599"); });
    remember_target(state, "gallery.tree.scroll-end");
    auto deliveries = std::make_shared<std::vector<std::function<void()>>>();
    TreeLoader loader{[deliveries, tr = useI18n()](AsyncOptionsRequest request, TreeLoader::Complete done) {
                          deliveries->push_back([request, done, tr] {
                              if (!request.cancelled->load()) {
                                  done({{TreeItem{utf8(std::string{request.query.bytes()} + "-child"),
                                                  tr.text(gallery_message("gallery.complete.copy.489"))}
                                             .isLeaf()},
                                        {}});
                              }
                          });
                          return AsyncOptionsCancel{};
                      },
                      [](auto apply) { apply(); }};
    gallery_label("gallery.copy.sample.600");
    Tree(TreeProps{}.loadData(loader).layout(member_control_margin().width(dp(320)).height(dp(96))),
         {TreeItem{u8"async-a", gallery_text("gallery.complete.copy.472")},
          TreeItem{u8"async-b", gallery_text("gallery.complete.copy.473")}});
    remember_target(state, "gallery.tree.async");
    Button(ButtonProps{}.onClick([deliveries] {
        auto next = std::move(*deliveries);
        deliveries->clear();
        for (auto it = next.rbegin(); it != next.rend(); ++it) {
            (*it)();
        }
    }),
           [] { gallery_label("gallery.copy.sample.601"); });
    remember_target(state, "gallery.tree.deliver");
    static_cast<void>(state);
}

void add_tree_select_completion_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<TreeKeys> values{TreeKeys{}};
    GalleryFeedback feedback{GalleryMessage{"gallery.feedback.044", {}}, useI18n()};
    Text(TextProps{}.content(feedback));
    TreeSelect(TreeSelectProps{}
                   .multiple()
                   .value(values)
                   .showSearch()
                   .allowClear()
                   .maxTagCount(TreeSelectMaxTagCount{TreeSelectTagCount::Responsive})
                   .maxTagTextLength(12)
                   .treeDefaultExpandedKeys({u8"targets"})
                   .onValuesChange([values, feedback](TreeKeys next) mutable {
                       values.set(next);
                       feedback.set(
                           GalleryMessage{"gallery.event.059", {{u8"v0", static_cast<std::uint64_t>(next.size())}}});
                   })
                   .tagRender([](TreeSelectTagInfo info) {
                       return TreeSelectTag{[info] {
                           Tag(TagProps{}.closable().visible(true).disabled(info.disabled).onClose(info.onClose),
                               TagContent{[label = info.label] { Text(TextProps{}.content(label)); }});
                       }};
                   })
                   .prefix(TreeSelectAffix{[feedback, state] {
                       Button(ButtonProps{}.wave(false).onClick(
                                  [feedback]() mutable { feedback.set(GalleryMessage{"gallery.feedback.145", {}}); }),
                              [] { gallery_label("gallery.complete.copy.414"); });
                       remember_target(state, "gallery.tree-select.affix");
                   }})
                   .popupRender([feedback, state](std::function<void()> original) {
                       return TreeSelectPopup{[original, feedback, state] {
                           Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[original, feedback, state] {
                                    gallery_label("gallery.copy.sample.602");
                                    original();
                                    Button(ButtonProps{}.onClick([feedback]() mutable {
                                        feedback.set(GalleryMessage{"gallery.feedback.146", {}});
                                    }),
                                           [] { gallery_label("gallery.copy.sample.603"); });
                                    remember_target(state, "gallery.tree-select.footer");
                                }});
                       }};
                   })
                   .layout(member_control_margin().width(dp(320))),
               {TreeSelectItem{u8"targets",
                               gallery_text("gallery.complete.copy.474"),
                               {TreeSelectItem{u8"target-a", gallery_text("gallery.complete.copy.475")},
                                TreeSelectItem{u8"target-b", gallery_text("gallery.complete.copy.476")},
                                TreeSelectItem{u8"target-c", gallery_text("gallery.complete.copy.477")}}}});
    remember_target(state, "gallery.tree-select.multiple");
    TreeSelect(
        TreeSelectProps{}
            .treeCheckable()
            .treeCheckStrictly()
            .showSearch()
            .rtl()
            .allowClear()
            .treeDefaultExpandedKeys({u8"strict"})
            .onLabelChange([feedback](TreeSelectLabelValues values) mutable {
                feedback.set(
                    GalleryMessage{"gallery.event.060", {{u8"v0", static_cast<std::uint64_t>(values.size())}}});
            })
            .layout(member_control_margin().width(dp(320))),
        {TreeSelectItem{u8"strict",
                        gallery_text("gallery.complete.copy.478"),
                        {TreeSelectItem{u8"strict-a", gallery_text("gallery.complete.copy.479")},
                         TreeSelectItem{u8"strict-b", gallery_text("gallery.complete.copy.480")}.disableCheckbox()}}});
    remember_target(state, "gallery.tree-select.strict");
    std::vector<TreeSelectItem> choices;
    for (int i = 0; i < 10000; ++i) {
        choices.emplace_back(
            utf8("choice-" + std::to_string(i)),
            gallery_text("gallery.complete.copy.492", MessageArgs{{u8"index", static_cast<std::uint64_t>(i)}}));
    }
    Signal<std::vector<TreeSelectItem>> topology{std::move(choices)};
    Signal<String> virtual_value{String{}};
    TreeRef virtual_ref;
    gallery_label("gallery.copy.sample.604");
    TreeSelect(TreeSelectProps{}
                   .items(topology)
                   .virtualized()
                   .treeRef(virtual_ref)
                   .showSearch()
                   .allowClear()
                   .value(virtual_value)
                   .onChange([virtual_value, feedback](String value) mutable {
                       virtual_value.set(value);
                       feedback.set(GalleryMessage{"gallery.event.061", {{u8"v0", utf8(std::string{value.bytes()})}}});
                   })
                   .popupRender([virtual_ref, topology, feedback, state](std::function<void()> original) {
                       return TreeSelectPopup{[original, virtual_ref, topology, feedback, state] {
                           Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[=] {
                                    original();
                                    Button(ButtonProps{}.onClick([virtual_ref] {
                                        static_cast<void>(virtual_ref.scrollTo({u8"choice-9999"}));
                                    }),
                                           [] { gallery_label("gallery.copy.sample.605"); });
                                    remember_target(state, "gallery.tree-select.virtual-scroll");
                                    Button(ButtonProps{}.onClick([topology, feedback]() mutable {
                                        auto next = topology.get();
                                        std::reverse(next.begin(), next.end());
                                        topology.set(next);
                                        feedback.set(GalleryMessage{"gallery.feedback.147", {}});
                                    }),
                                           [] { gallery_label("gallery.copy.sample.606"); });
                                    remember_target(state, "gallery.tree-select.virtual-reverse");
                                }});
                       }};
                   })
                   .layout(member_control_margin().width(dp(320))));
    remember_target(state, "gallery.tree-select.virtual");
    auto deliveries = std::make_shared<std::vector<std::function<void()>>>();
    TreeLoader loader{
        [deliveries, tr = useI18n()](AsyncOptionsRequest request, TreeLoader::Complete done) {
            deliveries->push_back([request, done, tr] {
                if (!request.cancelled->load()) {
                    done({{TreeItem{u8"async-choice-child", tr.text(gallery_message("gallery.complete.copy.490"))}
                               .isLeaf()},
                          {}});
                }
            });
            return AsyncOptionsCancel{};
        },
        [](auto apply) { apply(); }};
    gallery_label("gallery.copy.sample.607");
    TreeSelect(TreeSelectProps{}
                   .loadData(loader)
                   .showSearch()
                   .listHeight(120)
                   .onChange([feedback](String value) mutable {
                       feedback.set(GalleryMessage{"gallery.event.062", {{u8"v0", utf8(std::string{value.bytes()})}}});
                   })
                   .onTreeLoad([feedback](TreeLoadInfo info) mutable {
                       feedback.set(GalleryMessage{"gallery.event.063",
                                                   {{u8"v0", static_cast<std::uint64_t>(info.loaded_keys.size())}}});
                   })
                   .popupRender([deliveries, state](std::function<void()> original) {
                       return TreeSelectPopup{[original, deliveries, state] {
                           Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[=] {
                                    original();
                                    Button(ButtonProps{}.onClick([deliveries] {
                                        auto pending = std::move(*deliveries);
                                        deliveries->clear();
                                        for (auto& deliver : pending) {
                                            deliver();
                                        }
                                    }),
                                           [] { gallery_label("gallery.copy.sample.608"); });
                                    remember_target(state, "gallery.tree-select.async-deliver");
                                }});
                       }};
                   })
                   .layout(member_control_margin().width(dp(320))),
               {TreeSelectItem{u8"async-choice", gallery_text("gallery.complete.copy.481")}});
    remember_target(state, "gallery.tree-select.async");
    static_cast<void>(state);
}

void add_project_tree_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(gallery_text("gallery.copy.property.128"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.tree.project", true);
             Text(TextProps{}
                      .content(bind([state] {
                          const auto [checked, half] =
                              gallery_project_checks(state->project_checked.get(), state->project_locked.get());
                          return state->i18n.translate(
                              gallery_message("gallery.snapshot.011"),
                              MessageArgs{{u8"v0", utf8(std::string{
                                                       gallery_project_keys(state->project_selected.get()).bytes()})},
                                          {u8"v1", utf8(std::string{gallery_project_keys(checked).bytes()})},
                                          {u8"v2", utf8(std::string{gallery_project_keys(half).bytes()})},
                                          {u8"v3", utf8(std::string{state->project_saved.get().bytes()})}});
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.tree.feedback", true);
             std::vector<TreeItem> assets;
             for (int i = 1; i <= 16; ++i) {
                 assets.emplace_back(utf8("asset-" + std::to_string(i)),
                                     gallery_text("gallery.complete.copy.395",
                                                  MessageArgs{{u8"index", static_cast<std::uint64_t>(i)}}));
             }
             TreeItem notes{u8"notes", gallery_text("gallery.complete.copy.445")};
             notes.richTitle(TreeTitle{[state] {
                 Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(4)), FlexContent{[state] {
                          Input(InputProps{}
                                    .value(state->project_note)
                                    .ref(state->project_note_ref)
                                    .onChange([state](String value) { state->project_note.set(value); })
                                    .placeholder(gallery_text("gallery.copy.property.129"))
                                    .layout(member_control_margin().width(dp(180))));
                          Button(ButtonProps{}
                                     .onClick([state] {
                                         state->project_saved.set(GalleryMessage{
                                             "gallery.complete.copy.143", {{u8"note", state->project_note.get()}}});
                                     })
                                     .layout(member_control_margin()),
                                 [] { gallery_label("gallery.copy.sample.609"); });
                      }});
             }});
             Tree(TreeProps{}
                      .checkable()
                      .expandedKeys(state->project_expanded)
                      .selectedKeys(state->project_selected)
                      .checkedKeys(state->project_checked)
                      .disabled(state->project_disabled)
                      .visible(state->project_visible)
                      .onExpand([state](TreeExpandInfo info) { state->project_expanded.set(info.expanded_keys); })
                      .onSelect([state](TreeSelectInfo info) { state->project_selected.set(info.selected_keys); })
                      .onCheck([state](TreeCheckInfo info) {
                          std::erase_if(info.checked_keys, [](const auto& key) {
                              return key.bytes() == "project" || key.bytes() == "src" || key.bytes() == "docs" ||
                                     key.bytes() == "assets";
                          });
                          state->project_checked.set(info.checked_keys);
                      })
                      .layout(member_control_margin()
                                  .width(bind([state] { return dp(state->document_width.get().value() - 12); }))
                                  .height(dp(240))),
                  {TreeItem{u8"project",
                            gallery_text("gallery.complete.copy.073"),
                            {TreeItem{u8"src",
                                      gallery_text("gallery.complete.copy.074"),
                                      {TreeItem{u8"controls", gallery_text("gallery.complete.copy.075")},
                                       TreeItem{u8"theme", gallery_text("gallery.complete.copy.076")}}},
                             TreeItem{u8"docs",
                                      gallery_text("gallery.complete.copy.077"),
                                      {TreeItem{u8"guide", gallery_text("gallery.complete.copy.078")},
                                       TreeItem{u8"locked", gallery_text("gallery.complete.copy.079")}.disabled(
                                           state->project_locked)}},
                             TreeItem{u8"assets", gallery_text("gallery.complete.copy.080"), std::move(assets)}}},
                   std::move(notes)});
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}
                                  .onClick([state] { state->project_disabled.set(!state->project_disabled.get()); })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.610"); });
                       gallery_target("gallery.tree.disabled");
                       Button(ButtonProps{}
                                  .onClick([state] { state->project_visible.set(!state->project_visible.get()); })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.611"); });
                       gallery_target("gallery.tree.visible");
                       Button(ButtonProps{}
                                  .onClick([state] { state->project_locked.set(!state->project_locked.get()); })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.612"); });
                       gallery_target("gallery.tree.locked");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->project_visible.set(true);
                                      state->project_disabled.set(false);
                                      state->project_expanded.set(TreeKeys{});
                                      static_cast<void>(state->project_note_ref.focus());
                                  })
                                  .layout(member_control_margin()),
                              [] { gallery_label("gallery.copy.sample.613"); });
                       gallery_target("gallery.tree.restore");
                   }});
         }});
    add_tree_completion_samples(state);
    ++state->telemetry.live_samples;
}

void add_tree_select_destination_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(
        FlexProps{}
            .vertical(true)
            .align(FlexAlign::Start)
            .gap(dp(12))
            .layout(LayoutStyle{}.width(state->document_width)),
        FlexContent{[state] {
            const auto items = [state] {
                std::vector<TreeSelectItem> assets;
                for (int i = 1; i <= 14; ++i) {
                    assets.emplace_back(utf8("deliverable-" + std::to_string(i)),
                                        gallery_text("gallery.complete.copy.396",
                                                     MessageArgs{{u8"index", static_cast<std::uint64_t>(i)}}));
                }
                return std::vector<TreeSelectItem>{TreeSelectItem{
                    u8"project",
                    gallery_text("gallery.complete.copy.081"),
                    {TreeSelectItem{u8"docs",
                                    gallery_text("gallery.complete.copy.082"),
                                    {TreeSelectItem{u8"guide", bind([state] {
                                                        return state->i18n.translate(
                                                            gallery_message(state->destination_renamed.get()
                                                                                ? "gallery.complete.copy.393"
                                                                                : "gallery.complete.copy.394"));
                                                    })},
                                     TreeSelectItem{u8"archive", gallery_text("gallery.complete.copy.083")}.disabled(
                                         state->destination_archive_disabled)}},
                     TreeSelectItem{u8"assets", gallery_text("gallery.complete.copy.084"), std::move(assets)}}}};
            };
            Text(TextProps{}
                     .content(gallery_text("gallery.copy.property.130"))
                     .layout(LayoutStyle{}.margin_left(dp(1))));
            remember_target(state, "gallery.tree-select.destination", true);
            Text(TextProps{}
                     .content(bind([state] {
                         return state->i18n.translate(
                             gallery_message("gallery.snapshot.012"),
                             MessageArgs{{u8"v0", utf8(std::string{state->destination_value.get().bytes()})},
                                         {u8"v1", utf8(std::to_string(state->destination_closes.get()))},
                                         {u8"v2", utf8(std::string{state->destination_request.get().bytes()})},
                                         {u8"v3", utf8(std::string{state->destination_saved.get().bytes()})}});
                     }))
                     .layout(LayoutStyle{}.margin_left(dp(1))));
            remember_target(state, "gallery.tree-select.feedback");
            Space(
                SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                SpaceContent{[state] {
                    Button(ButtonProps{}
                               .onClick([state] {
                                   state->destination_accept_value.set(!state->destination_accept_value.get());
                               })
                               .layout(member_control_margin()),
                           [] { gallery_label("gallery.copy.sample.614"); });
                    remember_target(state, "gallery.tree-select.value-authority");
                    Button(ButtonProps{}
                               .onClick([state] {
                                   state->destination_accept_close.set(!state->destination_accept_close.get());
                               })
                               .layout(member_control_margin()),
                           [] { gallery_label("gallery.copy.sample.615"); });
                    remember_target(state, "gallery.tree-select.close-authority");
                    Button(ButtonProps{}
                               .onClick([state] { state->destination_renamed.set(!state->destination_renamed.get()); })
                               .layout(member_control_margin()),
                           [] { gallery_label("gallery.copy.sample.616"); });
                    remember_target(state, "gallery.tree-select.rename");
                    Button(ButtonProps{}
                               .onClick([state] {
                                   state->destination_archive_disabled.set(!state->destination_archive_disabled.get());
                               })
                               .layout(member_control_margin()),
                           [] { gallery_label("gallery.copy.sample.617"); });
                    remember_target(state, "gallery.tree-select.node-disabled");
                    Button(
                        ButtonProps{}
                            .onClick([state] { state->destination_disabled.set(!state->destination_disabled.get()); })
                            .layout(member_control_margin()),
                        [] { gallery_label("gallery.copy.sample.618"); });
                    remember_target(state, "gallery.tree-select.disabled");
                    Button(
                        ButtonProps{}
                            .onClick([state] { state->destination_read_only.set(!state->destination_read_only.get()); })
                            .layout(member_control_margin()),
                        [] { gallery_label("gallery.copy.sample.619"); });
                    remember_target(state, "gallery.tree-select.readonly");
                    Button(ButtonProps{}
                               .onClick([state] { state->destination_visible.set(!state->destination_visible.get()); })
                               .layout(member_control_margin()),
                           [] { gallery_label("gallery.copy.sample.620"); });
                    remember_target(state, "gallery.tree-select.visible");
                    Button(ButtonProps{}
                               .onClick([state] {
                                   state->destination_accept_value.set(true);
                                   state->destination_accept_close.set(true);
                                   state->destination_disabled.set(false);
                                   state->destination_read_only.set(false);
                                   state->destination_visible.set(true);
                                   state->destination_open.set(false);
                                   static_cast<void>(state->destination_ref.focus());
                               })
                               .layout(member_control_margin()),
                           [] { gallery_label("gallery.copy.sample.621"); });
                    remember_target(state, "gallery.tree-select.restore");
                }});
            TreeSelect(
                TreeSelectProps{}
                    .value(state->destination_value)
                    .open(state->destination_open)
                    .treeExpandedKeys(state->destination_expanded)
                    .ref(state->destination_ref)
                    .allowClear()
                    .placeholder(gallery_text("gallery.copy.property.131"))
                    .listHeight(160)
                    .disabled(state->destination_disabled)
                    .readOnly(state->destination_read_only)
                    .visible(state->destination_visible)
                    .onChange([state](String value) {
                        state->destination_request.set(
                            (state->destination_accept_value.get()
                                 ? GalleryMessage{"gallery.event.064.yes", {{u8"v1", utf8(std::string{value.bytes()})}}}
                                 : GalleryMessage{"gallery.event.064.no",
                                                  {{u8"v1", utf8(std::string{value.bytes()})}}}));
                        if (state->destination_accept_value.get()) {
                            state->destination_value.set(value);
                        }
                    })
                    .onOpenChange([state](bool open) {
                        if (open) {
                            state->destination_open.set(true);
                        } else {
                            state->destination_closes.set(state->destination_closes.get() + 1);
                            if (state->destination_accept_close.get()) {
                                state->destination_open.set(false);
                            }
                        }
                    })
                    .onTreeExpand([state](TreeExpandInfo info) { state->destination_expanded.set(info.expanded_keys); })
                    .layout(member_control_margin().width(dp(300))),
                items());
            remember_target(state, "gallery.tree-select.selector");
            Input(InputProps{}
                      .value(state->destination_note)
                      .ref(state->destination_note_ref)
                      .onChange([state](String value) { state->destination_note.set(value); })
                      .placeholder(gallery_text("gallery.copy.property.132"))
                      .layout(member_control_margin().width(dp(300))));
            remember_target(state, "gallery.tree-select.note");
            Button(ButtonProps{}
                       .onClick([state] {
                           state->destination_saved.set(
                               GalleryMessage{"gallery.complete.copy.144",
                                              {{u8"destination", state->destination_value.get()},
                                               {u8"note", state->destination_note.get()}}});
                       })
                       .layout(member_control_margin()),
                   [] { gallery_label("gallery.copy.sample.622"); });
            remember_target(state, "gallery.tree-select.save");
            Text(TextProps{}
                     .content(gallery_text("gallery.copy.property.133"))
                     .layout(LayoutStyle{}.margin_left(dp(1))));
            remember_target(state, "gallery.tree-select.form");
            Text(TextProps{}
                     .content(bind(
                         [state] { return utf8("" + std::string{state->destination_form_feedback.get().bytes()}); }))
                     .layout(LayoutStyle{}.margin_left(dp(1))));
            remember_target(state, "gallery.tree-select.form-feedback");
            Form(FormProps{}
                     .ref(state->destination_form)
                     .onFinish([state](FormResult) {
                         state->destination_form_feedback.set(
                             GalleryMessage{"gallery.event.065",
                                            {{u8"v0", utf8(std::string{state->destination_field.get().bytes()})}}});
                     })
                     .onFinishFailed([state](FormResult) {
                         state->destination_form_feedback.set(GalleryMessage{"gallery.feedback.148", {}});
                     })
                     .layout(LayoutStyle{}.width(state->document_width)),
                 FormContent{[state, items] {
                     FormItem(FormItemProps{}
                                  .field(u8"destination", state->destination_field)
                                  .label(gallery_text("gallery.copy.property.134"))
                                  .control(state->destination_form_ref),
                              FormItemContent{[state, items] {
                                  TreeSelect(TreeSelectProps{}
                                                 .value(state->destination_field.value())
                                                 .onChange(state->destination_field.onChange())
                                                 .ref(state->destination_form_ref)
                                                 .treeDefaultExpandedKeys({u8"project", u8"docs"})
                                                 .allowClear()
                                                 .listHeight(160)
                                                 .disabled(state->destination_field.disabled())
                                                 .readOnly(state->destination_field.readOnly())
                                                 .status(state->destination_field.status())
                                                 .placeholder(gallery_text("gallery.copy.property.135"))
                                                 .layout(member_control_margin().width(dp(280))),
                                             items());
                                  remember_target(state, "gallery.tree-select.form-selector");
                              }});
                     Space(SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
                               Button(ButtonProps{}
                                          .onClick([state] { static_cast<void>(state->destination_form.submit()); })
                                          .layout(member_control_margin()),
                                      [] { gallery_label("gallery.copy.sample.623"); });
                               remember_target(state, "gallery.tree-select.submit");
                               Button(ButtonProps{}
                                          .onClick([state] {
                                              static_cast<void>(state->destination_form_ref.focus());
                                              static_cast<void>(state->destination_form.reset());
                                              state->destination_form_feedback.set(
                                                  GalleryMessage{"gallery.feedback.149", {}});
                                          })
                                          .layout(member_control_margin()),
                                      [] { gallery_label("gallery.copy.sample.624"); });
                               remember_target(state, "gallery.tree-select.reset");
                           }});
                 }});
        }});
    add_tree_select_completion_samples(state);
    state->telemetry.live_samples += 2;
}
} // namespace

TokenGalleryViewport token_gallery_logical_viewport(int pixel_width, int pixel_height, float render_scale) {
    if (pixel_width <= 0 || pixel_height <= 0 || !std::isfinite(render_scale) || render_scale <= 0.0F) {
        throw std::invalid_argument("Token Gallery pixel extent and render scale must be positive");
    }
    return {
        static_cast<float>(pixel_width) / render_scale,
        static_cast<float>(pixel_height) / render_scale,
    };
}

float token_gallery_pointer_to_render_logical(float host_logical_coordinate, float host_display_scale,
                                              float render_scale) {
    if (!std::isfinite(host_logical_coordinate) || !std::isfinite(host_display_scale) || host_display_scale <= 0.0F ||
        !std::isfinite(render_scale) || render_scale <= 0.0F) {
        throw std::invalid_argument("Token Gallery pointer coordinate and display scales must be finite and positive");
    }
    return host_logical_coordinate * host_display_scale / render_scale;
}

namespace {

void add_page_bidi_input(const std::shared_ptr<GalleryState>& state) {
    add_bidi_input(state);
    add_bidi_direction_control(state);
}

void add_page_bidi_text_area(const std::shared_ptr<GalleryState>& state) {
    add_bidi_text_area(state);
    add_bidi_direction_control(state);
}

void add_page_bidi_password(const std::shared_ptr<GalleryState>& state) {
    add_bidi_password(state);
    add_bidi_direction_control(state);
}

void add_page_bidi_typography(const std::shared_ptr<GalleryState>& state) {
    add_bidi_typography(state);
    add_bidi_direction_control(state);
}

void add_table_workflow_samples(const std::shared_ptr<GalleryState>& state) {
    add_data_and_selection_samples(state);
}

void add_switch_page_samples(const std::shared_ptr<GalleryState>& state) {
    add_switch_base_samples(state);
    add_switch_samples(state);
}

void add_checkbox_page_samples(const std::shared_ptr<GalleryState>& state) {
    add_checkbox_base_samples(state);
    add_checkbox_samples(state);
}

void add_radio_page_samples(const std::shared_ptr<GalleryState>& state) {
    add_radio_base_samples(state);
    add_radio_samples(state);
}

struct GallerySampleFactory final {
    GallerySampleDescriptor descriptor;
    void (*mount)(const std::shared_ptr<GalleryState>&);
};

void add_date_modes_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<DatePickerMode> mode{DatePickerMode::Date};
    Signal<PickerLocale> locale{PickerLocale{}};
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    Signal<DatePickerValue> value{CivilDate{2026, 11, 5}};
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(8))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[=] {
             gallery_label("gallery.copy.sample.625");
             DatePicker(
                 DatePickerProps{}
                     .value(value)
                     .picker(mode)
                     .locale(locale)
                     .direction(direction)
                     .clock([] { return CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{9, 30}}; })
                     .showNow()
                     .presets({{String{u8"release"}, String{}, [] { return DatePickerValue{CivilDate{2026, 12, 1}}; },
                                gallery_message("gallery.complete.copy.377")}})
                     .onChange([value](DatePickerValue v) mutable { value.set(v); })
                     .layout(LayoutStyle{}.width(dp(280))),
                 DatePickerFooter{[state] {
                     Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(4)), FlexContent{[state] {
                              Input(InputProps{}.defaultValue(String{u8"保留的面板备注"}));
                              remember_target(state, "gallery.date-picker.mode-note");
                              Popover(PopoverProps{}.trigger(PopoverTriggerMode::Click), PopoverTrigger{[state] {
                                          Button(ButtonProps{},
                                                 ButtonContent{[] { gallery_label("gallery.copy.sample.626"); }});
                                          remember_target(state, "gallery.date-picker.child-trigger");
                                      }},
                                      PopoverSlots{{}, PopoverContent{[state] {
                                                       Input(InputProps{}.defaultValue(String{u8"子浮层草稿"}));
                                                       remember_target(state, "gallery.date-picker.child-editor");
                                                   }}});
                          }});
                 }});
             remember_target(state, "gallery.date-picker.modes");
             Button(ButtonProps{}.onClick([mode]() mutable {
                 mode.set(static_cast<DatePickerMode>((static_cast<int>(mode.get()) + 1) % 5));
             }),
                    ButtonContent{[] { gallery_label("gallery.copy.sample.627"); }});
             gallery_target("gallery.date-picker.picker-mode");
             Button(ButtonProps{}.onClick([locale]() mutable {
                 auto v = locale.get();
                 v.first_day = v.first_day ? 0 : 1;
                 v.first_week_contains = v.first_day ? 4 : 1;
                 locale.set(v);
             }),
                    ButtonContent{[] { gallery_label("gallery.copy.sample.628"); }});
             gallery_target("gallery.date-picker.locale");
             Button(ButtonProps{}.onClick([direction]() mutable {
                 direction.set(direction.get() == TextDirection::RightToLeft ? TextDirection::LeftToRight
                                                                             : TextDirection::RightToLeft);
             }),
                    ButtonContent{[] { gallery_label("gallery.copy.sample.629"); }});
             gallery_target("gallery.date-picker.rtl");
         }});
    state->telemetry.live_samples += 1;
}

void add_date_range_multiple_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<DatePickerMultipleValue> multiple{DatePickerMultipleValue{CivilDate{2026, 11, 5}}};
    Signal<DatePickerRangeValue> range{DatePickerRangeValue{CivilDate{2026, 11, 5}, CivilDate{2026, 11, 12}}};
    Signal<bool> accept{true};
    PickerRangePolicy policy;
    policy.allow_empty = {true, true};
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(8))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[=] {
             gallery_label("gallery.copy.sample.630");
             DatePicker(
                 DatePickerProps{}
                     .multiple()
                     .multipleValue(multiple)
                     .multiplePresets(
                         {{String{u8"work-days"}, String{},
                           [] { return DatePickerMultipleValue{CivilDate{2026, 11, 5}, CivilDate{2026, 11, 6}}; },
                           gallery_message("gallery.complete.copy.378")}})
                     .defaultPanelDate(CivilDate{2026, 11, 1})
                     .onMultipleChange([multiple, accept](DatePickerMultipleValue v) mutable {
                         if (accept.get()) {
                             multiple.set(std::move(v));
                         }
                     })
                     .layout(LayoutStyle{}.width(dp(300))));
             remember_target(state, "gallery.date-picker.multiple");
             DateRangePicker(DateRangePickerProps{}
                                 .value(range)
                                 .policy(policy)
                                 .picker(DatePickerProps{}
                                             .defaultPanelDate(CivilDate{2026, 11, 1})
                                             .format(u8"DD/MM/YYYY")
                                             .layout(LayoutStyle{}.width(dp(220))))
                                 .onChange([range, accept](DatePickerRangeValue v) mutable {
                                     if (accept.get()) {
                                         range.set(v);
                                     }
                                 }));
             remember_target(state, "gallery.date-picker.range");
             Button(ButtonProps{}.onClick([accept]() mutable { accept.set(!accept.get()); }),
                    ButtonContent{[] { gallery_label("gallery.copy.sample.631"); }});
             gallery_target("gallery.date-picker.range-authority");
         }});
    state->telemetry.live_samples += 1;
}

void add_date_time_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<DatePickerDateTimeValue> value{CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{9, 30}}};
    Signal<DateTimePickerRangeValue> range{DateTimePickerRangeValue{
        CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{9}}, CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{18}}}};
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(8))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[=] {
             gallery_label("gallery.copy.sample.632");
             DatePicker(DatePickerProps{}
                            .showTime(TimePickerProps{}.precision(TimePickerPrecision::Millisecond))
                            .format(u8"DD/MM/YYYY HH:mm:ss.SSS")
                            .dateTimeValue(value)
                            .dateTimePresets({{String{u8"precise-noon"}, String{},
                                               [] {
                                                   return DatePickerDateTimeValue{
                                                       CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{12, 0, 0, 125}}};
                                               },
                                               gallery_message("gallery.complete.copy.379")}})
                            .onDateTimeChange([value](DatePickerDateTimeValue v) mutable { value.set(v); })
                            .layout(LayoutStyle{}.width(dp(320))));
             remember_target(state, "gallery.date-picker.datetime");
             DateRangePicker(DateTimeRangePickerProps{}
                                 .value(range)
                                 .picker(DatePickerProps{}
                                             .showTime(TimePickerProps{}.precision(TimePickerPrecision::Minute))
                                             .defaultPanelDate(CivilDate{2026, 11, 1})
                                             .layout(LayoutStyle{}.width(dp(250))))
                                 .onChange([range](DateTimePickerRangeValue v) mutable { range.set(v); }));
             remember_target(state, "gallery.date-picker.datetime-range");
         }});
    state->telemetry.live_samples += 1;
}

void add_time_precision_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<TimePickerValue> value{TimeOfDay{12, 30, 0, 125}};
    Signal<bool> twelve{true};
    Signal<bool> scroll{false};
    TimePickerRules rules;
    rules.millisecond_step = 125;
    rules.disabled_milliseconds = [](int, int, int, int ms) {
        return ms == 500;
    };
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(8))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[=] {
             gallery_label("gallery.copy.sample.633");
             TimePicker(TimePickerProps{}
                            .value(value)
                            .precision(TimePickerPrecision::Millisecond)
                            .use12Hours(twelve)
                            .rules(rules)
                            .changeOnScroll(scroll)
                            .clock([] { return TimeOfDay{9, 30, 0, 125}; })
                            .showNow()
                            .presets({{String{u8"noon"}, String{}, [] { return TimePickerValue{TimeOfDay{12}}; },
                                       gallery_message("gallery.complete.copy.380")}})
                            .onChange([value](TimePickerValue v) mutable { value.set(v); })
                            .layout(LayoutStyle{}.width(dp(300))),
                        TimePickerFooter{[state] {
                            Input(InputProps{}.defaultValue(String{u8"保留的精确时间备注"}));
                            remember_target(state, "gallery.time-picker.precise-note");
                        }});
             remember_target(state, "gallery.time-picker.precise");
             Button(ButtonProps{}.onClick([twelve]() mutable { twelve.set(!twelve.get()); }),
                    ButtonContent{[] { gallery_label("gallery.copy.sample.634"); }});
             gallery_target("gallery.time-picker.twelve");
             Button(ButtonProps{}.onClick([scroll]() mutable { scroll.set(!scroll.get()); }),
                    ButtonContent{[] { gallery_label("gallery.copy.sample.635"); }});
             gallery_target("gallery.time-picker.scroll-select");
         }});
    state->telemetry.live_samples += 1;
}

void add_time_range_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<TimePickerRangeValue> value{TimePickerRangeValue{TimeOfDay{9}, TimeOfDay{18}}};
    Signal<bool> accept{true};
    PickerRangePolicy policy;
    policy.allow_empty = {true, true};
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(8))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[=] {
             gallery_label("gallery.copy.sample.636");
             TimeRangePicker(
                 TimeRangePickerProps{}
                     .value(value)
                     .policy(policy)
                     .picker(TimePickerProps{}
                                 .use12Hours()
                                 .precision(TimePickerPrecision::Minute)
                                 .layout(LayoutStyle{}.width(dp(240))))
                     .disabledTime([](TimeOfDay time, PickerEndpoint endpoint, const TimePickerRangeValue& range) {
                         return endpoint == PickerEndpoint::End && range.start && time.hour() == range.start->hour() &&
                                time.minute() < range.start->minute();
                     })
                     .onChange([value, accept](TimePickerRangeValue v) mutable {
                         if (accept.get()) {
                             value.set(v);
                         }
                     }));
             remember_target(state, "gallery.time-picker.range");
             Button(ButtonProps{}.onClick([accept]() mutable { accept.set(!accept.get()); }),
                    ButtonContent{[] { gallery_label("gallery.copy.sample.637"); }});
             gallery_target("gallery.time-picker.range-authority");
         }});
    state->telemetry.live_samples += 1;
}

void add_qrcode_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<String> content{String{u8"RynUI native QRCode 107"}};
    Signal<QRCodeStatus> status{QRCodeStatus::Active};
    Signal<QRCodeErrorLevel> level{QRCodeErrorLevel::M};
    Signal<bool> accept{true};
    Signal<bool> invalid{false};
    Signal<bool> logo{false};
    GalleryFeedback feedback{GalleryMessage{"gallery.feedback.045", {}}, useI18n()};
    const auto artifact = std::make_shared<std::vector<std::byte>>();
    const auto svg = std::make_shared<std::string>();
    QRCodeRef reference;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(8))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[=] {
             gallery_label("gallery.copy.sample.638");
             Input(InputProps{}
                       .value(content)
                       .onChange([=](String v) { content.set(v); })
                       .layout(LayoutStyle{}.width(dp(300))));
             remember_target(state, "gallery.qrcode.content");
             QRCode(QRCodeProps{}
                        .value(bind([=] { return invalid.get() ? QRCodeValue{} : QRCodeValue{content.get()}; }))
                        .status(status)
                        .errorLevel(level)
                        .boostLevel(false)
                        .size(224)
                        .ref(reference)
                        .color(Color{0, 0, 0, 1})
                        .bgColor(Color{1, 1, 1, 1})
                        .icon(bind([=] { return logo.get() ? ImageSource{media_fixture_source()} : ImageSource{}; }))
                        .iconSize(QRCodeIconSize{20, 20})
                        .onRefresh([=] {
                            feedback.set(GalleryMessage{"gallery.feedback.150", {}});
                            if (accept.get()) {
                                status.set(QRCodeStatus::Active);
                            }
                        })
                        .onError([=](const QRCodeError& e) {
                            feedback.set(GalleryMessage{"gallery.complete.copy.417",
                                                        {{u8"error", String::from_utf8(e.message).value()}}});
                        }));
             remember_target(state, "gallery.qrcode.main");
             Flex(FlexProps{}.wrap(true).gap(dp(8)), FlexContent{[=] {
                      Button(ButtonProps{}.onClick([=] { status.set(QRCodeStatus::Expired); }),
                             ButtonContent{[] { gallery_label("gallery.copy.sample.639"); }});
                      remember_target(state, "gallery.qrcode.expire");
                      Button(ButtonProps{}.onClick([=] {
                          status.set(static_cast<QRCodeStatus>((static_cast<int>(status.get()) + 1) % 4));
                      }),
                             ButtonContent{[] { gallery_label("gallery.copy.sample.640"); }});
                      remember_target(state, "gallery.qrcode.status");
                      Button(ButtonProps{}.onClick([=] {
                          level.set(static_cast<QRCodeErrorLevel>((static_cast<int>(level.get()) + 1) % 4));
                          feedback.set(GalleryMessage{"gallery.feedback.151", {}});
                      }),
                             ButtonContent{[] { Text(u8"L / M / Q / H"); }});
                      remember_target(state, "gallery.qrcode.level");
                      Button(ButtonProps{}.onClick([=] {
                          accept.set(!accept.get());
                          feedback.set(accept.get() ? GalleryMessage{"gallery.feedback.152", {}}
                                                    : GalleryMessage{"gallery.feedback.153", {}});
                      }),
                             ButtonContent{[] { gallery_label("gallery.copy.sample.641"); }});
                      remember_target(state, "gallery.qrcode.refuse");
                      Button(ButtonProps{}.onClick([=] { invalid.set(!invalid.get()); }),
                             ButtonContent{[] { gallery_label("gallery.copy.sample.642"); }});
                      remember_target(state, "gallery.qrcode.invalid");
                      Button(ButtonProps{}.onClick([=] { logo.set(!logo.get()); }),
                             ButtonContent{[] { gallery_label("gallery.copy.sample.643"); }});
                      remember_target(state, "gallery.qrcode.logo");
                      Button(ButtonProps{}.onClick([=] {
                          try {
                              *artifact = reference.exportPng(464);
                              *svg = reference.exportSvg();
                              feedback.set(GalleryMessage{"gallery.complete.copy.265",
                                                          {{u8"png", static_cast<std::uint64_t>(artifact->size())},
                                                           {u8"svg", static_cast<std::uint64_t>(svg->size())}}});
                          } catch (const std::exception& e) {
                              feedback.set(GalleryMessage{"gallery.complete.copy.418",
                                                          {{u8"error", String::from_utf8(e.what()).value()}}});
                          }
                      }),
                             ButtonContent{[] { gallery_label("gallery.copy.sample.644"); }});
                      remember_target(state, "gallery.qrcode.export");
                  }});
             Text(TextProps{}.content(feedback).layout(LayoutStyle{}.width(state->document_width)));
         }});
    state->telemetry.live_samples += 1;
}

void add_watermark_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;

    struct Pending final {
        ImageCompletion done;
        GalleryFeedback message{GalleryMessage{"gallery.complete.copy.239", {}}, useI18n()};
    };

    auto pending = std::make_shared<Pending>();
    auto loader = std::make_shared<ImageLoader>();
    loader->start = [pending](ImageCompletion done) {
        pending->done = std::move(done);
        return [pending] {
            pending->done = {};
            pending->message.set(GalleryMessage{"gallery.complete.copy.246", {}});
        };
    };
    Signal<ImageSource> source{ImageSource{}};
    Signal<float> rotate{-22};
    Signal<WatermarkGap> gap{WatermarkGap{50, 40}};
    Signal<WatermarkOffset> offset{WatermarkOffset{20, 20}};
    Signal<LogicalLength> height{dp(260)};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Flex(FlexProps{}.gap(dp(8)).wrap(FlexWrap::Wrap), FlexContent{[=] {
             Button(ButtonProps{}.onClick([=] { source.set(ImageSource{}); }),
                    [] { gallery_label("gallery.copy.sample.645"); });
             remember_target(state, "gallery.watermark.text");
             Button(ButtonProps{}.onClick([=] { source.set(ImageSource{media_fixture_source()}); }),
                    [] { gallery_label("gallery.copy.sample.646"); });
             remember_target(state, "gallery.watermark.image");
             Button(ButtonProps{}.onClick([=] { source.set(ImageSource{std::make_shared<ImageLoader>(*loader)}); }),
                    [] { gallery_label("gallery.copy.sample.647"); });
             remember_target(state, "gallery.watermark.async");
             Button(ButtonProps{}.onClick([=] {
                 if (pending->done) {
                     auto complete = std::exchange(pending->done, {});
                     complete(decode_image(*media_fixture_source()));
                 }
             }),
                    [] { gallery_label("gallery.copy.sample.648"); });
             remember_target(state, "gallery.watermark.complete");
             Button(ButtonProps{}.onClick([=] {
                 if (pending->done) {
                     auto complete = std::exchange(pending->done, {});
                     complete({{}, {ImageErrorCode::loader_failure, "Application source unavailable"}});
                 }
             }),
                    [] { gallery_label("gallery.copy.sample.649"); });
             remember_target(state, "gallery.watermark.failure");
             Button(ButtonProps{}.onClick([=] { rotate.set(rotate.get() == -22 ? 32.F : -22.F); }),
                    [] { gallery_label("gallery.copy.sample.650"); });
             remember_target(state, "gallery.watermark.rotate");
             Button(ButtonProps{}.onClick([=] {
                 gap.set(gap.get().x == 50 ? WatermarkGap{100, 80} : WatermarkGap{50, 40});
                 offset.set(offset.get().x == 20 ? WatermarkOffset{60, 35} : WatermarkOffset{20, 20});
             }),
                    [] { gallery_label("gallery.copy.sample.651"); });
             remember_target(state, "gallery.watermark.spacing");
             Button(ButtonProps{}.onClick([=] { height.set(height.get() == dp(260) ? dp(380) : dp(260)); }),
                    [] { gallery_label("gallery.copy.sample.652"); });
             remember_target(state, "gallery.watermark.resize");
             Button(ButtonProps{}.onClick([=] {
                 auto next = theme.get();
                 next.watermark.tokens.color =
                     next.watermark.tokens.color ? std::optional<Color>{} : Color::rgba8(22, 119, 255, 75);
                 next.watermark.tokens.image_opacity = .28F;
                 theme.set(next);
             }),
                    [] { gallery_label("gallery.copy.sample.653"); });
             remember_target(state, "gallery.watermark.theme");
         }});
    Theme(ThemeProps{}.config(theme), ThemeContent{[=] {
              Watermark(
                  WatermarkProps{}
                      .content(u8"RynUI 原生水印\nConfidential")
                      .image(source)
                      .rotate(rotate)
                      .gap(gap)
                      .offset(offset)
                      .onLoad([pending] { pending->message.set(GalleryMessage{"gallery.complete.copy.247", {}}); })
                      .onError([pending](const ImageError&) {
                          pending->message.set(GalleryMessage{"gallery.complete.copy.248", {}});
                      })
                      .layout(LayoutStyle{}.width(state->document_width).height(height)),
                  WatermarkContent{[state] {
                      Card(CardProps{}.title(gallery_text("gallery.copy.property.137")), CardContent{[state] {
                               Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state] {
                                        Input(InputProps{}.defaultValue(u8"旋转、切换水印或尺寸变化后草稿仍保留"));
                                        remember_target(state, "gallery.watermark.note");
                                        gallery_label("gallery.copy.sample.654");
                                        remember_target(state, "gallery.watermark.detail");
                                    }});
                           }});
                  }});
              remember_target(state, "gallery.watermark.main");
          }});
    Text(TextProps{}.content(pending->message));
    state->telemetry.live_samples += 1;
}

void add_affix_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    // The Gallery's real document scroll translates the placeholder subtree.
    // This explicit window-logical viewport bounds both retained window layers.
    Signal<AffixTarget> target{AffixTarget{1, 0, 140, 10000, 360}};
    Signal<float> top{12};
    Signal<float> bottom{12};
    Signal<bool> disabled{false};
    GalleryFeedback feedback{GalleryMessage{"gallery.feedback.046", {}}, useI18n()};
    Flex(FlexProps{}.gap(dp(8)).wrap(FlexWrap::Wrap), FlexContent{[=] {
             Button(ButtonProps{}.onClick([=] {
                 auto next = target.get();
                 next.attached = !next.attached;
                 target.set(next);
             }),
                    [] { gallery_label("gallery.copy.sample.655"); });
             remember_target(state, "gallery.affix.attach");
             Button(ButtonProps{}.onClick([=] {
                 auto next = target.get();
                 ++next.identity;
                 next.y = next.y == 140 ? 180.F : 140.F;
                 target.set(next);
             }),
                    [] { gallery_label("gallery.copy.sample.656"); });
             remember_target(state, "gallery.affix.target");
             Button(ButtonProps{}.onClick([=] {
                 top.set(top.get() == 12 ? 36.F : 12.F);
                 bottom.set(bottom.get() == 12 ? 36.F : 12.F);
             }),
                    [] { gallery_label("gallery.copy.sample.657"); });
             remember_target(state, "gallery.affix.offset");
             Button(ButtonProps{}.onClick([=] { disabled.set(!disabled.get()); }),
                    [] { gallery_label("gallery.copy.sample.658"); });
             remember_target(state, "gallery.affix.disabled");
         }});
    Affix(AffixProps{}
              .target(target)
              .offsetTop(top)
              .disabled(disabled)
              .onChange([=](bool fixed) {
                  feedback.set(fixed ? GalleryMessage{"gallery.feedback.154", {}}
                                     : GalleryMessage{"gallery.feedback.155", {}});
              })
              .layout(LayoutStyle{}.width(state->document_width)),
          AffixContent{[] {
              Card(CardProps{}.title(gallery_text("gallery.copy.property.138")),
                   CardContent{[] { Input(InputProps{}.defaultValue(u8"滚动与切换目标不会重建这份草稿")); }});
          }});
    remember_target(state, "gallery.affix.top");
    gallery_label("gallery.copy.sample.659");
    Card(CardProps{}.title(gallery_text("gallery.copy.property.139")).layout(LayoutStyle{}.height(dp(400))),
         CardContent{[] { gallery_label("gallery.copy.sample.660"); }});
    Affix(AffixProps{}.target(target).offsetBottom(bottom).disabled(disabled).layout(
              LayoutStyle{}.width(state->document_width)),
          AffixContent{[=] {
              Button(ButtonProps{}.onClick([=] { feedback.set(GalleryMessage{"gallery.feedback.156", {}}); }),
                     [] { gallery_label("gallery.copy.sample.661"); });
          }});
    remember_target(state, "gallery.affix.bottom");
    Text(TextProps{}.content(feedback));
    state->telemetry.live_samples += 1;
}

void add_border_beam_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Signal<Duration> duration{Duration::seconds(6)};
    Signal<BorderBeamDirection> direction{BorderBeamDirection::Clockwise};
    Signal<bool> visible{true};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Flex(FlexProps{}.gap(dp(8)).wrap(FlexWrap::Wrap), FlexContent{[=] {
             Button(ButtonProps{}.onClick([=] {
                 direction.set(direction.get() == BorderBeamDirection::Clockwise ? BorderBeamDirection::Counterclockwise
                                                                                 : BorderBeamDirection::Clockwise);
             }),
                    [] { gallery_label("gallery.copy.sample.662"); });
             remember_target(state, "gallery.border-beam.direction");
             Button(ButtonProps{}.onClick([=] {
                 duration.set(duration.get() == Duration::seconds(6) ? Duration::seconds(2) : Duration::seconds(6));
             }),
                    [] { gallery_label("gallery.copy.sample.663"); });
             remember_target(state, "gallery.border-beam.duration");
             Button(ButtonProps{}.onClick([=] {
                 auto next = theme.get();
                 next.border_beam.tokens.color =
                     next.border_beam.tokens.color ? std::optional<Color>{} : Color::rgba8(235, 47, 150);
                 next.border_beam.tokens.width = 3;
                 next.border_beam.tokens.radius = 16;
                 next.border_beam.tokens.length = 110;
                 theme.set(next);
             }),
                    [] { gallery_label("gallery.copy.sample.664"); });
             remember_target(state, "gallery.border-beam.theme");
             Button(ButtonProps{}.onClick([=] {
                 auto next = theme.get();
                 next.seed.motion = !next.seed.motion.value_or(true);
                 theme.set(next);
             }),
                    [] { gallery_label("gallery.copy.sample.665"); });
             remember_target(state, "gallery.border-beam.motion");
             Button(ButtonProps{}.onClick([=] { visible.set(!visible.get()); }),
                    [] { gallery_label("gallery.copy.sample.666"); });
             remember_target(state, "gallery.border-beam.visible");
         }});
    Theme(ThemeProps{}.config(theme), ThemeContent{[=] {
              BorderBeam(BorderBeamProps{}.duration(duration).direction(direction).visible(visible).layout(
                             LayoutStyle{}.width(state->document_width).height(dp(200))),
                         BorderBeamContent{[state] {
                             Card(CardProps{}.title(gallery_text("gallery.copy.property.140")), CardContent{[state] {
                                      Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state] {
                                               Input(InputProps{}.defaultValue(u8"改变颜色与运动参数保留此输入"));
                                               remember_target(state, "gallery.border-beam.note");
                                               gallery_label("gallery.copy.sample.667");
                                               remember_target(state, "gallery.border-beam.detail");
                                           }});
                                  }});
                         }});
              remember_target(state, "gallery.border-beam.main");
          }});
    state->telemetry.live_samples += 1;
}

void add_table_completion_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    std::vector<GalleryRecord> initial;
    for (int i = 0; i < 1000; ++i) {
        initial.push_back({utf8("virtual-" + std::to_string(i)), utf8("保留编辑行 " + std::to_string(i)),
                           static_cast<double>(i), i % 3 == 0});
    }
    Signal<std::vector<GalleryRecord>> records{std::move(initial)};
    Signal<std::vector<String>> expanded{std::vector<String>{}};
    Signal<std::vector<String>> filtered{std::vector<String>{}};
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    Signal<bool> accept{true};
    Flex(FlexProps{}.wrap(true).gap(dp(8)), FlexContent{[=] {
             Button(ButtonProps{}.onClick([=] {
                 direction.set(direction.get() == TextDirection::LeftToRight ? TextDirection::RightToLeft
                                                                             : TextDirection::LeftToRight);
             }),
                    ButtonContent{[] { gallery_label("gallery.complete.copy.025"); }});
             remember_target(state, "gallery.table.rtl");
             Button(ButtonProps{}.onClick([=] { accept.set(!accept.get()); }),
                    ButtonContent{[] { gallery_label("gallery.complete.copy.026"); }});
             remember_target(state, "gallery.table.refuse");
             Button(ButtonProps{}.onClick([=] {
                 expanded.set(expanded.get().empty() ? std::vector<String>{u8"virtual-0"} : std::vector<String>{});
             }),
                    ButtonContent{[] { gallery_label("gallery.complete.copy.027"); }});
             remember_target(state, "gallery.table.expand");
         }});
    TableExpandable<GalleryRecord> expansion;
    expansion.expandedRowKeys = expanded;
    expansion.onExpandedRowsChange = [=](auto value) {
        if (accept.get()) {
            expanded.set(std::move(value));
        }
    };
    expansion.children = [](const GalleryRecord& row) {
        return row.key == String{u8"virtual-0"} ? std::vector<GalleryRecord>{{u8"virtual-child", u8"树子行", 1, false}}
                                                : std::vector<GalleryRecord>{};
    };
    expansion.expandedRowRender = [](TableCell<GalleryRecord> cell) {
        Input(InputProps{}.defaultValue(cell.value().name).layout(LayoutStyle{}.width(dp(280))));
    };
    Table(
        TableProps<GalleryRecord>{}
            .dataSource(records)
            .rowKey([](const GalleryRecord& row) { return row.key; })
            .columns(
                {TableColumn<GalleryRecord>{u8"key", ryn::String{},
                                            [](TableCell<GalleryRecord> cell) {
                                                Text(TextProps{}.content(bind([cell] { return cell.key(); })));
                                            }}
                     .title(gallery_text("gallery.complete.copy.085"))
                     .width(bind([width = state->document_width] { return std::min(200.0F, width.get().value() / 4); }))
                     .fixed(TableFixed::Start),
                 TableColumn<GalleryRecord>{
                     u8"editor", ryn::String{},
                     [](TableCell<GalleryRecord> cell) {
                         Input(InputProps{}.defaultValue(cell.value().name).layout(LayoutStyle{}.width(dp(450))));
                     }}
                     .title(gallery_text("gallery.complete.copy.086"))
                     .width(700),
                 TableColumn<GalleryRecord>{u8"state", ryn::String{},
                                            [](TableCell<GalleryRecord> cell) {
                                                Text(TextProps{}.content(bind([cell, tr = useI18n()] {
                                                    return tr.translate(gallery_message(
                                                        cell.value().locked ? "gallery.complete.copy.384"
                                                                            : "gallery.complete.copy.385"));
                                                })));
                                            }}
                     .title(gallery_text("gallery.complete.copy.087"))
                     .width(bind([width = state->document_width] { return std::min(180.0F, width.get().value() / 4); }))
                     .fixed(TableFixed::End)
                     .filters({{u8"locked", String{}, gallery_message("gallery.complete.copy.375")},
                               {u8"editable", String{}, gallery_message("gallery.complete.copy.376")}})
                     .filteredValue(filtered)
                     .onFilter([](const String& value, const GalleryRecord& row) {
                         return (value == String{u8"locked"}) == row.locked;
                     })
                     .onFilterChange([=](auto value) {
                         if (accept.get()) {
                             filtered.set(std::move(value));
                         }
                     })})
            .expandable(std::move(expansion))
            .virtualRows({64, 120, 2})
            .direction(direction)
            .scrollY(220)
            .bordered()
            .layout(LayoutStyle{}.width(state->document_width)));
    remember_target(state, "gallery.table.virtual");
    ++state->telemetry.live_samples;
}

void add_tour_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    TourTargetRef editor_target;
    TourTargetRef action_target;
    TourTargetRef missing;
    Signal<bool> open{false};
    Signal<bool> accept{true};
    Signal<bool> child_modal{false};
    Signal<std::size_t> current{0};
    Signal<std::vector<TourStep>> steps{std::vector<TourStep>{}};
    TourTarget(editor_target, TourTargetContent{[=] {
                   Input(InputProps{}.defaultValue(u8"原生目标中的编辑草稿").layout(LayoutStyle{}.width(dp(300))));
                   remember_target(state, "gallery.tour.target-editor");
               }});
    TourTarget(action_target, TourTargetContent{[=] {
                   Button(ButtonProps{}.onClick(
                              [state] { state->data_feedback.set(GalleryMessage{"gallery.complete.copy.252", {}}); }),
                          ButtonContent{[] { gallery_label("gallery.complete.copy.028"); }});
                   remember_target(state, "gallery.tour.target-action");
               }});
    Button(ButtonProps{}.onClick([=] {
        current.set(0);
        open.set(true);
    }),
           ButtonContent{[] { gallery_label("gallery.complete.copy.029"); }});
    remember_target(state, "gallery.tour.open");
    steps.set({TourStep{u8"editor", ryn::String{}}
                   .title(gallery_text("gallery.complete.copy.088"))
                   .target(editor_target)
                   .richDescription(TourDescription{[=] {
                       Input(
                           InputProps{}.defaultValue(u8"切步后仍保留的步骤草稿").layout(LayoutStyle{}.width(dp(280))));
                       remember_target(state, "gallery.tour.draft");
                       Button(ButtonProps{}.onClick([=] { accept.set(!accept.get()); }),
                              ButtonContent{[] { gallery_label("gallery.complete.copy.030"); }});
                       remember_target(state, "gallery.tour.refuse");
                       Button(ButtonProps{}.onClick([=] { child_modal.set(true); }),
                              ButtonContent{[] { gallery_label("gallery.complete.copy.031"); }});
                       remember_target(state, "gallery.tour.modal");
                       Modal(ModalProps{}
                                 .open(child_modal)
                                 .title(gallery_text("gallery.complete.copy.006"))
                                 .onCancel([=](auto) { child_modal.set(false); }),
                             ModalContent{[] { Input(InputProps{}.defaultValue(u8"最内层输入边界")); }});
                   }}),
               TourStep{u8"action", ryn::String{}}
                   .title(gallery_text("gallery.complete.copy.089"))
                   .target(action_target)
                   .description(gallery_text("gallery.complete.copy.008")),
               TourStep{u8"missing", ryn::String{}}
                   .title(gallery_text("gallery.complete.copy.090"))
                   .target(missing)
                   .description(gallery_text("gallery.complete.copy.009"))});
    Tour(TourProps{}
             .steps(steps)
             .open(open)
             .current(current)
             .onChange([=](auto next) {
                 if (accept.get()) {
                     current.set(next);
                 }
             })
             .onOpenChange([=](bool next) {
                 if (accept.get()) {
                     open.set(next);
                 }
             }));
    remember_target(state, "gallery.tour.main");
    Text(TextProps{}.content(state->data_feedback));
    ++state->telemetry.live_samples;
}

constexpr auto sample_factories = std::to_array<GallerySampleFactory>({
    {{"gallery.sample.config-provider.context",
      "ant.component.config-provider",
      {},
      "作用域默认值与保留编辑",
      "add_config_provider_samples",
      "include/ryn/config_provider.hpp"},
     add_config_provider_samples},
    {{"gallery.sample.watermark.native",
      "ant.component.watermark",
      {},
      "文字与共享图像、旋转、重复和异步取消",
      "add_watermark_samples",
      "include/ryn/watermark.hpp"},
     add_watermark_samples},
    {{"gallery.sample.affix.native",
      "ant.component.affix",
      {},
      "原生滚动目标、顶部 / 底部固定与释放恢复",
      "add_affix_samples",
      "include/ryn/affix.hpp"},
     add_affix_samples},
    {{"gallery.sample.border-beam.native",
      "ant.component.border-beam",
      {},
      "圆角周长运动、方向、主题与减少动态效果",
      "add_border_beam_samples",
      "include/ryn/border_beam.hpp"},
     add_border_beam_samples},
    {{"gallery.sample.table.completion",
      "ant.component.table",
      {},
      "虚拟行、树展开、筛选、固定列与拖拽滚动条",
      "add_table_completion_samples",
      "include/ryn/table.hpp"},
     add_table_completion_samples},
    {{"gallery.sample.tour.native",
      "ant.component.tour",
      {},
      "真实目标、保留富步骤、受控导航与嵌套浮层",
      "add_tour_samples",
      "include/ryn/tour.hpp"},
     add_tour_samples},
    {{"gallery.sample.qr-code.native",
      "ant.component.qr-code",
      {},
      "标准编码、状态请求、共享图标与 PNG/SVG 数据导出",
      "add_qrcode_samples",
      "include/ryn/qrcode.hpp"},
     add_qrcode_samples},
    {{"gallery.sample.carousel.panels",
      "ant.component.carousel",
      {},
      "保留面板、拖动、效果与自动播放",
      "add_carousel_samples",
      "include/ryn/carousel.hpp"},
     add_carousel_samples},
    {{"gallery.sample.carousel.controlled",
      "ant.component.carousel",
      {},
      "受控拒绝、自定义指示器与动态 key",
      "add_carousel_controlled_samples",
      "include/ryn/carousel.hpp"},
     add_carousel_controlled_samples},
    {{"gallery.sample.calendar.custom",
      "ant.component.calendar",
      {},
      "自定义 header、fullCell 与保留浮层",
      "add_calendar_custom_samples",
      "include/ryn/calendar.hpp"},
     add_calendar_custom_samples},
    {{"gallery.sample.calendar.schedule",
      "ant.component.calendar",
      {},
      "公历、年月选择与保留日程",
      "add_calendar_samples",
      "include/ryn/calendar.hpp"},
     add_calendar_samples},
    {{"gallery.sample.float-button.actions",
      "ant.component.float-button",
      {},
      "浮动操作、分组与返回顶部",
      "add_float_button_samples",
      "include/ryn/float_button.hpp"},
     add_float_button_samples},
    {{"gallery.sample.button.basics",
      "ant.component.button",
      {},
      "类型与交互状态",
      "add_button_base_samples",
      "include/ryn/button.hpp"},
     add_button_base_samples},
    {{"gallery.sample.button.variants",
      "ant.component.button",
      {},
      "颜色、变体与图标",
      "add_button_variant_samples",
      "include/ryn/button.hpp"},
     add_button_variant_samples},
    {{"gallery.sample.input.basics", "ant.component.input", GalleryInputFamily::input, "受控与非受控输入",
      "add_input_base_samples", "include/ryn/input.hpp"},
     add_input_base_samples},
    {{"gallery.sample.input.features", "ant.component.input", GalleryInputFamily::input, "变体、统计与焦点",
      "add_input_only_features", "include/ryn/input.hpp"},
     add_input_only_features},
    {{"gallery.sample.input.bidi", "ant.component.input", GalleryInputFamily::input, "双向文字编辑",
      "add_page_bidi_input", "include/ryn/input.hpp"},
     add_page_bidi_input},
    {{"gallery.sample.search.basics", "ant.component.input", GalleryInputFamily::search, "搜索与提交来源",
      "add_search_base_samples", "include/ryn/search.hpp"},
     add_search_base_samples},
    {{"gallery.sample.search.features", "ant.component.input", GalleryInputFamily::search, "连接外观与图标",
      "add_search_features", "include/ryn/search.hpp"},
     add_search_features},
    {{"gallery.sample.password.basics", "ant.component.input", GalleryInputFamily::password, "密码输入",
      "add_password_base_samples", "include/ryn/password.hpp"},
     add_password_base_samples},
    {{"gallery.sample.password.features", "ant.component.input", GalleryInputFamily::password, "显隐动作与受控状态",
      "add_password_features", "include/ryn/password.hpp"},
     add_password_features},
    {{"gallery.sample.password.bidi", "ant.component.input", GalleryInputFamily::password, "双向密码输入",
      "add_page_bidi_password", "include/ryn/password.hpp"},
     add_page_bidi_password},
    {{"gallery.sample.text-area.features", "ant.component.input", GalleryInputFamily::text_area, "多行、自动尺寸与滚动",
      "add_text_area_samples", "include/ryn/text_area.hpp"},
     add_text_area_samples},
    {{"gallery.sample.text-area.bidi", "ant.component.input", GalleryInputFamily::text_area, "双向多行编辑",
      "add_page_bidi_text_area", "include/ryn/text_area.hpp"},
     add_page_bidi_text_area},
    {{"gallery.sample.otp.features", "ant.component.input", GalleryInputFamily::otp, "分格输入与粘贴",
      "add_otp_samples", "include/ryn/otp.hpp"},
     add_otp_samples},
    {{"gallery.sample.tree-select.destination",
      "ant.component.tree-select",
      {},
      "交付目标、备注与表单",
      "add_tree_select_destination_samples",
      "include/ryn/tree_select.hpp"},
     add_tree_select_destination_samples},
    {{"gallery.sample.typography.levels",
      "ant.component.typography",
      {},
      "标题与行内样式",
      "add_typography_catalog_samples",
      "include/ryn/typography.hpp"},
     add_typography_catalog_samples},
    {{"gallery.sample.typography.features",
      "ant.component.typography",
      {},
      "省略、复制与原地编辑",
      "add_typography_features",
      "include/ryn/typography.hpp"},
     add_typography_features},
    {{"gallery.sample.typography.bidi",
      "ant.component.typography",
      {},
      "双向文字排版",
      "add_page_bidi_typography",
      "include/ryn/typography.hpp"},
     add_page_bidi_typography},
    {{"gallery.sample.divider.basics",
      "ant.component.divider",
      {},
      "水平、标签与虚线",
      "add_divider_catalog_samples",
      "include/ryn/divider.hpp"},
     add_divider_catalog_samples},
    {{"gallery.sample.divider.orientation",
      "ant.component.divider",
      {},
      "朝向与垂直分隔",
      "add_divider_base_samples",
      "include/ryn/divider.hpp"},
     add_divider_base_samples},
    {{"gallery.sample.divider.variants",
      "ant.component.divider",
      {},
      "尺寸、线型与 RTL",
      "add_divider_variant_samples",
      "include/ryn/divider.hpp"},
     add_divider_variant_samples},
    {{"gallery.sample.flex.layouts",
      "ant.component.flex",
      {},
      "基线、换行与拉伸",
      "add_flex_samples",
      "include/ryn/flex.hpp"},
     add_flex_samples},
    {{"gallery.sample.grid.layouts",
      "ant.component.grid",
      {},
      "二十四列、响应断点与弹性",
      "add_grid_samples",
      "include/ryn/grid.hpp"},
     add_grid_samples},
    {{"gallery.sample.masonry.layouts",
      "ant.component.masonry",
      {},
      "真实高度、响应列数与 keyed 内容",
      "add_masonry_samples",
      "include/ryn/masonry.hpp"},
     add_masonry_samples},
    {{"gallery.sample.splitter.multiple",
      "ant.component.splitter",
      {},
      "多面板、约束与连续调整",
      "add_splitter_multiple_samples",
      "include/ryn/splitter.hpp"},
     add_splitter_multiple_samples},
    {{"gallery.sample.splitter.controlled",
      "ant.component.splitter",
      {},
      "受控百分比、自定义拖动图标与双击",
      "add_splitter_controlled_samples",
      "include/ryn/splitter.hpp"},
     add_splitter_controlled_samples},
    {{"gallery.sample.splitter.lazy-collapse",
      "ant.component.splitter",
      {},
      "Lazy、双侧折叠、动画与隐藏生命周期",
      "add_splitter_lazy_collapse_samples",
      "include/ryn/splitter.hpp"},
     add_splitter_lazy_collapse_samples},
    {{"gallery.sample.splitter.nested",
      "ant.component.splitter",
      {},
      "嵌套水平/垂直面板",
      "add_splitter_nested_samples",
      "include/ryn/splitter.hpp"},
     add_splitter_nested_samples},
    {{"gallery.sample.layout.structure",
      "ant.component.layout",
      {},
      "页头、双侧栏、正文与页尾",
      "add_shell_structure_samples",
      "include/ryn/layout.hpp"},
     add_shell_structure_samples},
    {{"gallery.sample.layout.responsive",
      "ant.component.layout",
      {},
      "响应断点与零宽侧栏",
      "add_shell_responsive_samples",
      "include/ryn/layout.hpp"},
     add_shell_responsive_samples},
    {{"gallery.sample.layout.controlled",
      "ant.component.layout",
      {},
      "受控、自定义触发器与主题",
      "add_shell_controlled_samples",
      "include/ryn/layout.hpp"},
     add_shell_controlled_samples},
    {{"gallery.sample.space.composition",
      "ant.component.space",
      {},
      "间距、分隔与紧凑组合",
      "add_space_samples",
      "include/ryn/space.hpp"},
     add_space_samples},
    {{"gallery.sample.icon.catalog",
      "ant.component.icon",
      {},
      "离线图标、旋转与向量",
      "add_icon_samples",
      "include/ryn/icon.hpp"},
     add_icon_samples},
    {{"gallery.sample.anchor.navigation",
      "ant.component.anchor",
      {},
      "层级、方向、滚动与动态锚点",
      "add_anchor_samples",
      "include/ryn/anchor.hpp"},
     add_anchor_samples},
    {{"gallery.sample.breadcrumb.dynamic",
      "ant.component.breadcrumb",
      {},
      "动态键控、菜单与 RTL",
      "add_breadcrumb_dynamic_samples",
      "include/ryn/breadcrumb.hpp"},
     add_breadcrumb_dynamic_samples},
    {{"gallery.sample.breadcrumb.trail",
      "ant.component.breadcrumb",
      {},
      "层级导航与编辑",
      "add_breadcrumb_samples",
      "include/ryn/breadcrumb.hpp"},
     add_breadcrumb_samples},
    {{"gallery.sample.steps.workflow",
      "ant.component.steps",
      {},
      "发布流程与备注",
      "add_steps_samples",
      "include/ryn/steps.hpp"},
     add_steps_samples},
    {{"gallery.sample.image.native",
      "ant.component.image",
      {},
      "真实像素、异步源与原生预览",
      "add_image_samples",
      "include/ryn/image.hpp"},
     add_image_samples},
    {{"gallery.sample.upload.native",
      "ant.component.upload",
      {},
      "原生文件选择与合成应用传输",
      "add_upload_samples",
      "include/ryn/upload.hpp"},
     add_upload_samples},
    {{"gallery.sample.transfer.lists",
      "ant.component.transfer",
      {},
      "双列表、搜索、分页与保留草稿",
      "add_transfer_samples",
      "include/ryn/transfer.hpp"},
     add_transfer_samples},
    {{"gallery.sample.cascader.paths",
      "ant.component.cascader",
      {},
      "层级路径、多选与应用加载器",
      "add_cascader_samples",
      "include/ryn/cascader.hpp"},
     add_cascader_samples},
    {{"gallery.sample.menu.inline",
      "ant.component.menu",
      {},
      "选择、子菜单与响应文字",
      "add_menu_samples",
      "include/ryn/menu.hpp"},
     add_menu_samples},
    {{"gallery.sample.dropdown.actions",
      "ant.component.dropdown",
      {},
      "点击、右键与菜单命令",
      "add_dropdown_samples",
      "include/ryn/dropdown.hpp"},
     add_dropdown_samples},
    {{"gallery.sample.pagination.controlled",
      "ant.component.pagination",
      {},
      "受控分页与页大小",
      "add_pagination_samples",
      "include/ryn/pagination.hpp"},
     add_pagination_samples},
    {{"gallery.sample.tabs.panels",
      "ant.component.tabs",
      {},
      "标签、保留内容与溢出",
      "add_tabs_samples",
      "include/ryn/tabs.hpp"},
     add_tabs_samples},
    {{"gallery.sample.switch.states",
      "ant.component.switch",
      {},
      "尺寸、状态与引用",
      "add_switch_page_samples",
      "include/ryn/switch.hpp"},
     add_switch_page_samples},
    {{"gallery.sample.checkbox.selection",
      "ant.component.checkbox",
      {},
      "单项与多项选择",
      "add_checkbox_page_samples",
      "include/ryn/checkbox.hpp"},
     add_checkbox_page_samples},
    {{"gallery.sample.radio.selection",
      "ant.component.radio",
      {},
      "互斥选择与动态选项",
      "add_radio_page_samples",
      "include/ryn/radio.hpp"},
     add_radio_page_samples},
    {{"gallery.sample.slider.values",
      "ant.component.slider",
      {},
      "刻度、范围与拖动",
      "add_slider_samples",
      "include/ryn/slider.hpp"},
     add_slider_samples},
    {{"gallery.sample.tooltip.triggers",
      "ant.component.tooltip",
      {},
      "悬停、焦点与组合触发",
      "add_tooltip_samples",
      "include/ryn/tooltip.hpp"},
     add_tooltip_samples},
    {{"gallery.sample.modal.dialogs",
      "ant.component.modal",
      {},
      "确认、取消与保留编辑",
      "add_modal_samples",
      "include/ryn/modal.hpp"},
     add_modal_samples},
    {{"gallery.sample.avatar.images",
      "ant.component.avatar",
      {},
      "图片请求、响应式与动态分组",
      "add_avatar_image_samples",
      "include/ryn/avatar.hpp"},
     add_avatar_image_samples},
    {{"gallery.sample.badge.native",
      "ant.component.badge",
      {},
      "数字动画、富计数与 Ribbon",
      "add_badge_native_samples",
      "include/ryn/badge.hpp"},
     add_badge_native_samples},
    {{"gallery.sample.tag.native",
      "ant.component.tag",
      {},
      "取消、受控选择与保留草稿拖排",
      "add_tag_native_samples",
      "include/ryn/tag.hpp"},
     add_tag_native_samples},
    {{"gallery.sample.tag.states",
      "ant.component.tag",
      {},
      "标签、关闭与选择",
      "add_tag_samples",
      "include/ryn/tag.hpp"},
     add_tag_samples},
    {{"gallery.sample.badge.counts",
      "ant.component.badge",
      {},
      "计数、圆点与状态",
      "add_badge_samples",
      "include/ryn/badge.hpp"},
     add_badge_samples},
    {{"gallery.sample.drawer.account",
      "ant.component.drawer",
      {},
      "四边抽屉与账户草稿",
      "add_drawer_samples",
      "include/ryn/drawer.hpp"},
     add_drawer_samples},
    {{"gallery.sample.popconfirm.confirm",
      "ant.component.popconfirm",
      {},
      "确认、取消与加载",
      "add_popconfirm_samples",
      "include/ryn/popconfirm.hpp"},
     add_popconfirm_samples},
    {{"gallery.sample.alert.feedback",
      "ant.component.alert",
      {},
      "状态提示与恢复",
      "add_alert_samples",
      "include/ryn/alert.hpp"},
     add_alert_samples},
    {{"gallery.sample.spin.loading",
      "ant.component.spin",
      {},
      "加载与保留内容",
      "add_spin_samples",
      "include/ryn/spin.hpp"},
     add_spin_samples},
    {{"gallery.sample.progress.values",
      "ant.component.progress",
      {},
      "线形、环形与状态",
      "add_progress_samples",
      "include/ryn/progress.hpp"},
     add_progress_samples},
    {{"gallery.sample.table.workflow",
      "ant.component.table",
      {},
      "编辑、选择、分页与命令",
      "add_table_workflow_samples",
      "include/ryn/table.hpp"},
     add_table_workflow_samples},
    {{"gallery.sample.color-picker.controls",
      "ant.component.color-picker",
      {},
      "颜色、透明度、通道与渐变色标",
      "add_color_picker_samples",
      "include/ryn/color_picker.hpp"},
     add_color_picker_samples},
    {{"gallery.sample.input-number.values",
      "ant.component.input-number",
      {},
      "小数、范围与状态",
      "add_input_number_samples",
      "include/ryn/input_number.hpp"},
     add_input_number_samples},
    {{"gallery.sample.collapse.panels",
      "ant.component.collapse",
      {},
      "展开、操作与保留内容",
      "add_collapse_samples",
      "include/ryn/collapse.hpp"},
     add_collapse_samples},
    {{"gallery.sample.segmented.views",
      "ant.component.segmented",
      {},
      "视图选择与富标签",
      "add_segmented_samples",
      "include/ryn/segmented.hpp"},
     add_segmented_samples},
    {{"gallery.sample.empty.states",
      "ant.component.empty",
      {},
      "空状态与创建操作",
      "add_empty_samples",
      "include/ryn/empty.hpp"},
     add_empty_samples},
    {{"gallery.sample.result.states",
      "ant.component.result",
      {},
      "处理结果与详情",
      "add_result_samples",
      "include/ryn/result.hpp"},
     add_result_samples},
    {{"gallery.sample.form.fields",
      "ant.component.form",
      {},
      "字段、校验与提交",
      "add_form_samples",
      "include/ryn/form.hpp"},
     add_form_samples},
    {{"gallery.sample.form.layouts",
      "ant.component.form",
      {},
      "布局、自动绑定与滚动",
      "add_form_layout_samples",
      "include/ryn/form.hpp"},
     add_form_layout_samples},
    {{"gallery.sample.form.lists",
      "ant.component.form",
      {},
      "深路径与稳定嵌套列表",
      "add_form_list_samples",
      "include/ryn/form.hpp"},
     add_form_list_samples},
    {{"gallery.sample.form.async",
      "ant.component.form",
      {},
      "异步、依赖与取消",
      "add_form_async_samples",
      "include/ryn/form.hpp"},
     add_form_async_samples},
    {{"gallery.sample.date-picker.dates",
      "ant.component.date-picker",
      {},
      "日期、规则与表单",
      "add_date_picker_samples",
      "include/ryn/date_picker.hpp"},
     add_date_picker_samples},
    {{"gallery.sample.select.options",
      "ant.component.select",
      {},
      "选择、搜索与动态选项",
      "add_select_samples",
      "include/ryn/select.hpp"},
     add_select_samples},
    {{"gallery.sample.card.completion",
      "ant.component.card",
      {},
      "Grid、Meta、保留 Tabs 与图片封面",
      "add_card_completion_samples",
      "include/ryn/card.hpp"},
     add_card_completion_samples},
    {{"gallery.sample.empty.dynamic",
      "ant.component.empty",
      {},
      "图片服务与动态富内容",
      "add_empty_completion_samples",
      "include/ryn/empty.hpp"},
     add_empty_completion_samples},
    {{"gallery.sample.listy.retained",
      "ant.component.listy",
      {},
      "稳定 key、网格、分页与滚动",
      "add_listy_samples",
      "include/ryn/listy.hpp"},
     add_listy_samples},
    {{"gallery.sample.card.content",
      "ant.component.card",
      {},
      "内容、操作与加载",
      "add_card_samples",
      "include/ryn/card.hpp"},
     add_card_samples},
    {{"gallery.sample.time-picker.times",
      "ant.component.time-picker",
      {},
      "时间、规则与表单",
      "add_time_picker_samples",
      "include/ryn/time_picker.hpp"},
     add_time_picker_samples},
    {{"gallery.sample.descriptions.fields",
      "ant.component.descriptions",
      {},
      "资料、列数与编辑",
      "add_descriptions_samples",
      "include/ryn/descriptions.hpp"},
     add_descriptions_samples},
    {{"gallery.sample.skeleton.loading",
      "ant.component.skeleton",
      {},
      "骨架与保留编辑",
      "add_skeleton_samples",
      "include/ryn/skeleton.hpp"},
     add_skeleton_samples},
    {{"gallery.sample.popover.notes",
      "ant.component.popover",
      {},
      "气泡内容与受控交互",
      "add_popover_samples",
      "include/ryn/popover.hpp"},
     add_popover_samples},
    {{"gallery.sample.app.services",
      "ant.component.app",
      {},
      "App 通知、保留编辑与语言切换",
      "add_app_samples",
      "include/ryn/app.hpp"},
     add_app_samples},
    {{"gallery.sample.message.workflow",
      "ant.component.message",
      {},
      "Message 与 Notification 处理流程",
      "add_notice_samples",
      "include/ryn/message.hpp"},
     add_notice_samples},
    {{"gallery.sample.notification.center",
      "ant.component.notification",
      {},
      "独立通知中心与限时进度",
      "add_notification_samples",
      "include/ryn/notification.hpp"},
     add_notification_samples},
    {{"gallery.sample.timeline.events",
      "ant.component.timeline",
      {},
      "事件流与处理备注",
      "add_timeline_samples",
      "include/ryn/timeline.hpp"},
     add_timeline_samples},
    {{"gallery.sample.avatar.members",
      "ant.component.avatar",
      {},
      "成员、分组与编辑",
      "add_member_avatar_samples",
      "include/ryn/avatar.hpp"},
     add_member_avatar_samples},
    {{"gallery.sample.statistic.metrics",
      "ant.component.statistic",
      {},
      "精度、格式与指标备注",
      "add_statistic_metric_samples",
      "include/ryn/statistic.hpp"},
     add_statistic_metric_samples},
    {{"gallery.sample.rate.service",
      "ant.component.rate",
      {},
      "服务评价与备注",
      "add_service_rate_samples",
      "include/ryn/rate.hpp"},
     add_service_rate_samples},
    {{"gallery.sample.mentions.editing",
      "ant.component.mentions",
      {},
      "多行提及、长建议和可取消异步",
      "add_mentions_samples",
      "include/ryn/mentions.hpp"},
     add_mentions_samples},
    {{"gallery.sample.auto-complete.contacts",
      "ant.component.auto-complete",
      {},
      "联系人、备注与表单",
      "add_contact_autocomplete_samples",
      "include/ryn/auto_complete.hpp"},
     add_contact_autocomplete_samples},
    {{"gallery.sample.tree.projects",
      "ant.component.tree",
      {},
      "项目树与处理备注",
      "add_project_tree_samples",
      "include/ryn/tree.hpp"},
     add_project_tree_samples},
    {{"gallery.sample.tokens.theme",
      "",
      {},
      "主题算法与品牌",
      "add_theme_button_samples",
      "include/ryn/design_token.hpp",
      GalleryPageKind::tokens},
     add_theme_button_samples},
    {{"gallery.sample.tokens.palette",
      "",
      {},
      "语义色与色板",
      "add_palette_cells",
      "include/ryn/design_token.hpp",
      GalleryPageKind::tokens},
     add_palette_cells},
    {{"gallery.sample.tokens.scales",
      "",
      {},
      "字号、尺寸与圆角",
      "add_scale_cells",
      "include/ryn/design_token.hpp",
      GalleryPageKind::tokens},
     add_scale_cells},
    {{"gallery.sample.tokens.shadows",
      "",
      {},
      "阴影与效果",
      "add_shadow_cells",
      "include/ryn/design_token.hpp",
      GalleryPageKind::tokens},
     add_shadow_cells},
    {{"gallery.sample.date-picker.modes",
      "ant.component.date-picker",
      {},
      "日期模式、locale 与 RTL",
      "add_date_modes_samples",
      "include/ryn/date_picker.hpp"},
     add_date_modes_samples},
    {{"gallery.sample.date-picker.range-multiple",
      "ant.component.date-picker",
      {},
      "范围与多选确认",
      "add_date_range_multiple_samples",
      "include/ryn/date_picker.hpp"},
     add_date_range_multiple_samples},
    {{"gallery.sample.date-picker.date-time",
      "ant.component.date-picker",
      {},
      "日期时间组合值",
      "add_date_time_samples",
      "include/ryn/date_picker.hpp"},
     add_date_time_samples},
    {{"gallery.sample.time-picker.precision",
      "ant.component.time-picker",
      {},
      "12 小时与精确毫秒",
      "add_time_precision_samples",
      "include/ryn/time_picker.hpp"},
     add_time_precision_samples},
    {{"gallery.sample.time-picker.range",
      "ant.component.time-picker",
      {},
      "时间范围与受控拒绝",
      "add_time_range_samples",
      "include/ryn/time_picker.hpp"},
     add_time_range_samples},
});

constexpr auto sample_descriptors = [] {
    std::array<GallerySampleDescriptor, sample_factories.size()> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = sample_factories[index].descriptor;
    }
    return result;
}();

} // namespace

std::span<const GallerySampleDescriptor> gallery_samples() noexcept {
    return sample_descriptors;
}

namespace {
constexpr auto related_samples = std::to_array<GalleryRelatedSample>({
    {"ant.component.notification", "Message / Notification 处理流程", GalleryPageKind::component,
     "ant.component.message", "gallery.sample.message.workflow"},
    {"ant.component.form", "TreeSelect 交付目标表单", GalleryPageKind::component, "ant.component.tree-select",
     "gallery.sample.tree-select.destination"},
    {"ant.component.form", "AutoComplete 联系人表单", GalleryPageKind::component, "ant.component.auto-complete",
     "gallery.sample.auto-complete.contacts"},
    {"ant.component.form", "日期表单", GalleryPageKind::component, "ant.component.date-picker",
     "gallery.sample.date-picker.dates"},
    {"ant.component.form", "时间表单", GalleryPageKind::component, "ant.component.time-picker",
     "gallery.sample.time-picker.times"},
    {"ant.component.pagination", "Table 分页与命令流程", GalleryPageKind::component, "ant.component.table",
     "gallery.sample.table.workflow"},
    {"ant.component.menu", "表格导航与命令流程", GalleryPageKind::component, "ant.component.table",
     "gallery.sample.table.workflow"},
    {"ant.component.dropdown", "表格命令流程", GalleryPageKind::component, "ant.component.table",
     "gallery.sample.table.workflow"},
    {"ant.component.input-number", "表格数量编辑流程", GalleryPageKind::component, "ant.component.table",
     "gallery.sample.table.workflow"},
    {"ant.component.config-provider", "原生 Theme 与 Design Token", GalleryPageKind::tokens, "", ""},
});
} // namespace

GalleryPageRoute GalleryRelatedSample::route() const {
    GalleryPageRoute result{target_kind, std::string{target_component}, {}, std::string{target_sample}};
    result.section = target_kind == GalleryPageKind::tokens ? GalleryPageSection::tokens : GalleryPageSection::examples;
    return result;
}

std::span<const GalleryRelatedSample> gallery_related_samples() noexcept {
    return related_samples;
}

GallerySamplePage make_gallery_sample_page(const GalleryPageRoute& route, const GalleryPageDraft& draft,
                                           GallerySamplePageOptions options) {
    const GalleryPageModel model;
    if (!model.valid(route)) {
        throw std::invalid_argument("Gallery sample page route is not registered");
    }
    std::vector<GallerySampleFactory> selected_factories;
    std::set<std::string_view> registered;
    const auto add_factory = [&](const GallerySampleFactory& factory) {
        const auto& descriptor = factory.descriptor;
        if (!registered.insert(descriptor.identity).second) {
            throw std::invalid_argument("Gallery registry view duplicates a sample identity");
        }
        if (route.kind == descriptor.kind && descriptor.component_identity == route.component_identity &&
            descriptor.family == route.family) {
            selected_factories.push_back(factory);
        }
    };
    if (options.registry) {
        for (const auto& entry : *options.registry) {
            const auto found = std::ranges::find_if(sample_factories, [&entry](const auto& factory) {
                return factory.descriptor.identity == entry.identity;
            });
            if (found == sample_factories.end()) {
                throw std::invalid_argument("Gallery registry view contains an unknown factory");
            }
            add_factory(*found);
        }
    } else {
        for (const auto& factory : sample_factories) {
            add_factory(factory);
        }
    }
    auto state = std::make_shared<GalleryState>();
    state->page_family = route.family;
    state->page_component = route.component_identity;
    state->register_targets = true;
    state->readable_labels = options.readable_labels;
    const bool sensitive = route.component_identity == "ant.component.input" &&
                           (route.family == GalleryInputFamily::password || route.family == GalleryInputFamily::otp);
    if (!sensitive) {
        state->draft = draft;
    }
    visit_gallery_draft_fields(*state, [&](const std::string& key, auto& field) {
        if constexpr (std::same_as<std::decay_t<decltype(field)>, GalleryFeedback>) {
            gallery_restore_feedback(state->draft, key, field);
        } else if constexpr (requires { field.get(); }) {
            auto initial = field.get();
            if constexpr (std::same_as<std::remove_cvref_t<decltype(initial)>, ryn::String>) {
                initial = std::move(initial);
            }
            field.set(gallery_draft_read(state->draft, key, initial));
        } else {
            field = gallery_draft_read(state->draft, key, field);
        }
    });
    if (state->page_component == "ant.component.icon") {
        const auto step = state->icon_source_step % 3;
        state->icon_source.set(step == 0   ? ryn::IconSource{ryn::IconName::HeartTwoTone}
                               : step == 1 ? ryn::IconSource{ryn::IconName::WalletTwoTone}
                                           : icon_vector_sample());
    }
    const ryn::Signal<ryn::LogicalLength> card_width{ryn::dp(784)};
    const ryn::Signal<ryn::LogicalLength> card_padding{ryn::dp(12)};
    const auto set_width = [state, card_width, card_padding, document_spacing = options.document_spacing](float width) {
        if (!std::isfinite(width) || width <= 0) {
            throw std::invalid_argument("Gallery sample width must be positive and finite");
        }
        const auto& algorithms = state->theme.get().algorithms;
        const bool compact = std::ranges::find(algorithms, ryn::ThemeAlgorithm::Compact) != algorithms.end();
        const float padding = document_spacing ? (compact || width < 560 ? 16.0F : 24.0F) : 12.0F;
        const auto inner = std::max(1.0F, width - padding * 2);
        card_width.set(ryn::dp(width));
        card_padding.set(ryn::dp(padding));
        state->document_width.set(ryn::dp(inner));
        state->cell_width.set(ryn::dp(std::min(260.0F, inner)));
        state->narrow_layout.set(width < 560);
    };
    set_width(card_width.get().value());
    auto runs = std::make_shared<std::map<std::string, std::uint64_t>>();
    return {
        ryn::Content{[state, route, runs, card_width, card_padding, selected_factories = std::move(selected_factories),
                      footer = std::move(options.footer)] {
            state->bind_messages(ryn::useI18n());
            GalleryDraftBuild draft_build{state};
            ++state->telemetry.content_runs;
            bool has_sample{};
            for (const auto& factory : selected_factories) {
                const auto& descriptor = factory.descriptor;
                has_sample = true;
                ++(*runs)[std::string{descriptor.identity}];
                ReferenceSurface(ReferenceSurfaceProps{}
                                     .identity(descriptor.identity)
                                     .role(ReferenceSurfaceRole::example)
                                     .content_padding(card_padding)
                                     .layout(ryn::LayoutStyle{}.width(card_width)),
                                 [state, factory, footer] {
                                     ryn::Flex(
                                         ryn::FlexProps{}.vertical(true).align(ryn::FlexAlign::Start).gap(ryn::dp(12)),
                                         [state, factory, footer] {
                                             ryn::Text(ryn::TextProps{}.content(
                                                 gallery_field(factory.descriptor.identity, "title")));
                                             if (state->page_family == GalleryInputFamily::password ||
                                                 state->page_family == GalleryInputFamily::otp) {
                                                 gallery_label("gallery.copy.sample.668");
                                             }
                                             factory.mount(state);
                                             if (footer) {
                                                 footer(factory.descriptor, state->document_width);
                                             }
                                         });
                                 });
            }
            if (!has_sample && route.kind == GalleryPageKind::component) {
                const auto* entry = find_ant_design_reference_entry(route.component_identity);
                constexpr std::array empty_keys{"gallery.empty.native",     "gallery.empty.native",
                                                "gallery.empty.planned",    "gallery.empty.web",
                                                "gallery.empty.deprecated", "gallery.empty.outside"};
                ryn::Text(ryn::TextProps{}
                              .content(gallery_text(empty_keys.at(static_cast<std::size_t>(entry->support_status))))
                              .tone(ryn::TextTone::Secondary)
                              .layout(ryn::LayoutStyle{}.width(state->document_width)));
            }
        }},
        [state, sensitive](GalleryPageDraft& target) {
            if (sensitive) {
                return;
            }
            target.values = state->draft.values;
            visit_gallery_draft_fields(*state, [&](const std::string& key, const auto& field) {
                if constexpr (std::same_as<std::decay_t<decltype(field)>, GalleryFeedback>) {
                    gallery_feedback_draft(target, key, field);
                } else if constexpr (requires { field.get(); }) {
                    gallery_draft_write(target, key, field.get());
                } else {
                    gallery_draft_write(target, key, field);
                }
            });
        },
        set_width,
        [state] { return state->telemetry; },
        [runs](std::string_view identity) {
            const auto found = runs->find(std::string{identity});
            return found == runs->end() ? 0 : found->second;
        },
        [state](std::string_view identity) {
            const auto found = state->targets.find(std::string{identity});
            return found == state->targets.end() ? ryn::runtime::ComponentId{} : found->second;
        },
        [state, set_width, card_width](const ryn::ThemeConfig& config) {
            state->theme.set(config);
            set_width(card_width.get().value());
        },
        [state]() -> std::optional<ryn::ThemeConfig> {
            auto result = std::move(state->theme_request);
            state->theme_request.reset();
            return result;
        },
        [state] {
            state->disabled.set(false);
            state->loading.set(false);
            state->telemetry.state_updates += 2;
        },
    };
}

TokenGalleryDefinition make_token_gallery_definition() {
    return make_gallery_frame_definition({});
}

TokenGalleryDefinition make_gallery_frame_definition(std::span<const GalleryPageRoute> routes) {
    auto state = std::make_shared<GalleryState>();
    for (const auto& route : routes) {
        state->frame_pages.push_back(make_gallery_sample_page(route, {}));
    }
    auto set_theme = [state](const ryn::ThemeConfig& config, bool brand) {
        state->set_theme(config, brand);
        for (auto& page : state->frame_pages) {
            page.set_theme(state->theme.get());
        }
    };

    TokenGalleryDefinition definition{
        ryn::Content{[state] {
            state->bind_messages(ryn::useI18n());
            ++state->telemetry.content_runs;
            ryn::Theme(
                ryn::ThemeProps{}.config(state->theme), ryn::ThemeContent{[state] {
                    ++state->telemetry.theme_content_runs;
                    ryn::Flex(
                        ryn::FlexProps{}
                            .align(ryn::FlexAlign::Start)
                            .vertical(true)
                            .gap(ryn::dp(0.0F))
                            .layout(ryn::LayoutStyle{}.width(state->gallery_width)),
                        [state] {
                            ryn::Flex(ryn::FlexProps{}
                                          .align(ryn::FlexAlign::Start)
                                          .vertical(state->narrow_layout)
                                          .gap(ryn::dp(GalleryLayoutMetrics::column_gap))
                                          .layout(ryn::LayoutStyle{}
                                                      .width(state->gallery_width)
                                                      .margin_top(ryn::dp(GalleryLayoutMetrics::body_gap))
                                                      .order(1)),
                                      [state] {
                                          ryn::Flex(ryn::FlexProps{}
                                                        .align(ryn::FlexAlign::Start)
                                                        .vertical(true)
                                                        .gap(ryn::dp(0.0F))
                                                        .layout(ryn::LayoutStyle{}.width(state->navigation_width)),
                                                    [state] {
                                                        ryn::Theme(ryn::ThemeProps{}.config(navigation_theme_config()),
                                                                   ryn::ThemeContent{[state] {
                                                                       ++state->telemetry.theme_content_runs;
                                                                       navigation_controls(state);
                                                                   }});
                                                    });
                                          ryn::Flex(ryn::FlexProps{}
                                                        .align(ryn::FlexAlign::Start)
                                                        .vertical(true)
                                                        .gap(ryn::dp(16.0F))
                                                        .layout(ryn::LayoutStyle{}.width(state->document_width)),
                                                    [state] {
                                                        source_section(state);
                                                        section_surface(state, gallery_document_sections()[1]);
                                                        design_values(state);
                                                        foundation_tokens(state);
                                                        component_overview(state);
                                                        if (!state->frame_pages.empty()) {
                                                            section_surface(state, gallery_document_sections()[5]);
                                                            for (auto& page : state->frame_pages) {
                                                                ryn::detail::SlotContentAccess::function(
                                                                    page.content)();
                                                            }
                                                        } else {
                                                            add_live_samples(state);
                                                            add_switch_samples(state);
                                                            add_checkbox_samples(state);
                                                            add_radio_samples(state);
                                                            add_flex_samples(state);
                                                            add_space_samples(state);
                                                            add_icon_samples(state);
                                                            add_input_feature_samples(state);
                                                            add_text_area_samples(state);
                                                            add_otp_samples(state);
                                                            add_bidi_samples(state);
                                                            add_select_samples(state);
                                                            add_card_samples(state);
                                                            add_tabs_samples(state);
                                                            add_modal_samples(state);
                                                            add_common_component_samples(state);
                                                            add_data_and_selection_samples(state);
                                                            add_panel_and_selection_samples(state);
                                                            add_empty_and_result_samples(state);
                                                            add_form_samples(state);
                                                            add_date_picker_samples(state);
                                                            add_anchor_samples(state);
                                                            add_breadcrumb_samples(state);
                                                            add_steps_samples(state);
                                                            add_time_picker_samples(state);
                                                            add_descriptions_samples(state);
                                                            add_skeleton_samples(state);
                                                            add_popover_samples(state);
                                                            add_notice_samples(state);
                                                            add_timeline_samples(state);
                                                            add_member_avatar_samples(state);
                                                            add_statistic_metric_samples(state);
                                                            add_service_rate_samples(state);
                                                            add_contact_autocomplete_samples(state);
                                                            add_project_tree_samples(state);
                                                            add_tree_select_destination_samples(state);
                                                        }
                                                    });
                                      });
                            scrollbar_surface("gallery.shell.navigation.track", ReferenceSurfaceRole::scrollbar_track,
                                              state->navigation_track_height, state->navigation_bar_visible);
                            scrollbar_surface("gallery.shell.navigation.thumb", ReferenceSurfaceRole::scrollbar_thumb,
                                              state->navigation_thumb_height, state->navigation_bar_visible);
                            scrollbar_surface("gallery.shell.document.track", ReferenceSurfaceRole::scrollbar_track,
                                              state->document_track_height, state->document_bar_visible);
                            scrollbar_surface("gallery.shell.document.thumb", ReferenceSurfaceRole::scrollbar_thumb,
                                              state->document_thumb_height, state->document_bar_visible);
                            ReferenceSurface(
                                ReferenceSurfaceProps{}
                                    .identity("gallery.shell.header")
                                    .role(ReferenceSurfaceRole::site_header)
                                    .layout(ryn::LayoutStyle{}
                                                .width(state->gallery_width)
                                                .height(ryn::dp(GalleryLayoutMetrics::header_height))
                                                .order(-1)),
                                [state] {
                                    ryn::Flex(
                                        ryn::FlexProps{}.align(ryn::FlexAlign::Start).gap(ryn::dp(24.0F)), [state] {
                                            document_text(utf8("RynUI"), 20.0F, 28.0F, 600);
                                            document_text(gallery_text("gallery.shell.components"), 14.0F, 28.0F, 500);
                                            document_text(gallery_text("gallery.complete.copy.528"), 14.0F, 28.0F, 400,
                                                          ryn::TextTone::Secondary);
                                            document_text(utf8("Ant Design 6.6.5"), 12.0F, 28.0F, 400,
                                                          ryn::TextTone::Secondary);
                                            ryn::Button(
                                                ryn::ButtonProps{}.size(ryn::ControlSize::Small).onClick([state] {
                                                    state->navigation_request =
                                                        GalleryNavigationTarget::to_navigation();
                                                    ++state->telemetry.navigation_requests;
                                                }),
                                                [] { gallery_label("gallery.copy.sample.669"); });
                                            ryn::Button(
                                                ryn::ButtonProps{}
                                                    .type(ryn::ButtonType::Text)
                                                    .size(ryn::ControlSize::Small)
                                                    .onClick([state] {
                                                        const auto config = state->theme.get();
                                                        const bool dark = std::find(config.algorithms.begin(),
                                                                                    config.algorithms.end(),
                                                                                    ryn::ThemeAlgorithm::Dark) !=
                                                                          config.algorithms.end();
                                                        state->set_theme(
                                                            algorithm_config(dark ? ryn::ThemeAlgorithm::Default
                                                                                  : ryn::ThemeAlgorithm::Dark));
                                                    }),
                                                [state] {
                                                    ryn::Icon(ryn::IconProps{}.name(ryn::bind([theme = state->theme] {
                                                        const auto config = theme.get();
                                                        return std::find(
                                                                   config.algorithms.begin(), config.algorithms.end(),
                                                                   ryn::ThemeAlgorithm::Dark) != config.algorithms.end()
                                                                   ? ryn::IconName::SunOutlined
                                                                   : ryn::IconName::MoonOutlined;
                                                    })));
                                                    gallery_label("gallery.copy.sample.670");
                                                });
                                        });
                                });
                        });
                }});
        }},
        [state, set_theme](std::size_t step) {
            switch (step) {
            case 0:
                set_theme(algorithm_config(ryn::ThemeAlgorithm::Dark), false);
                break;
            case 1:
                set_theme(algorithm_config(ryn::ThemeAlgorithm::Compact), false);
                break;
            case 2: {
                auto brand = algorithm_config(ryn::ThemeAlgorithm::Default);
                brand.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
                set_theme(std::move(brand), true);
                break;
            }
            case 3:
                state->disabled.set(false);
                state->loading.set(false);
                for (auto& page : state->frame_pages) {
                    page.finish_state_demo();
                }
                state->telemetry.state_updates += 2;
                break;
            default:
                set_theme(ryn::ThemeConfig{}, false);
                break;
            }
        },
        [state](float viewport_width) {
            const auto metrics = gallery_layout_metrics({viewport_width, 900.0F});
            const auto content_width = metrics.gallery_width;
            const auto narrow = metrics.narrow;
            const auto navigation_width = metrics.navigation_width;
            const auto document_width = metrics.document_width;
            const auto next_cell_width = metrics.cell_width;
            for (auto& page : state->frame_pages) {
                page.set_width(document_width);
            }
            if (state->gallery_width.get() != ryn::dp(content_width)) {
                state->gallery_width.set(ryn::dp(content_width));
                ++state->telemetry.viewport_updates;
            }
            if (state->navigation_width.get() != ryn::dp(navigation_width)) {
                state->navigation_width.set(ryn::dp(navigation_width));
                ++state->telemetry.viewport_updates;
            }
            if (state->document_width.get() != ryn::dp(document_width)) {
                state->document_width.set(ryn::dp(document_width));
                ++state->telemetry.viewport_updates;
            }
            if (state->cell_width.get() != ryn::dp(next_cell_width)) {
                state->cell_width.set(ryn::dp(next_cell_width));
                ++state->telemetry.viewport_updates;
            }
            if (state->narrow_layout.set(narrow)) {
                ++state->telemetry.viewport_updates;
            }
        },
        [state](const GalleryScrollbarGeometry& navigation, const GalleryScrollbarGeometry& document, bool narrow) {
            const auto assign = [](const ryn::Signal<ryn::LogicalLength>& target, float value) {
                return target.set(ryn::dp(value));
            };
            bool changed = false;
            changed = assign(state->navigation_track_height, navigation.track.height) || changed;
            changed = assign(state->navigation_thumb_height, navigation.thumb.height) || changed;
            changed = assign(state->document_track_height, document.track.height) || changed;
            changed = assign(state->document_thumb_height, document.thumb.height) || changed;
            changed = state->navigation_bar_visible.set(!narrow) || changed;
            return changed;
        },
        [state] { return state->narrow_layout.get(); },
        [state](GalleryDocumentSectionKind section) {
            if (section == GalleryDocumentSectionKind::component_overview &&
                (state->active_navigation.get().kind == GalleryNavigationTargetKind::category ||
                 state->active_navigation.get().kind == GalleryNavigationTargetKind::component)) {
                return false;
            }
            return state->active_navigation.set(GalleryNavigationTarget::to_section(section));
        },
        [state, set_theme](bool enabled) {
            auto config = state->theme.get();
            if (config.seed.motion.value_or(true) == enabled) {
                return;
            }
            config.seed.motion = enabled;
            set_theme(std::move(config), false);
            ++state->telemetry.motion_updates;
        },
        [state](bool disabled) { state->clear_disabled.set(disabled); },
        [state]() -> std::optional<GalleryNavigationTarget> {
            auto request = state->navigation_request;
            state->navigation_request.reset();
            return request;
        },
        [state] {
            auto result = state->telemetry;
            for (const auto& page : state->frame_pages) {
                const auto child = page.telemetry();
                result.theme_content_runs += child.theme_content_runs;
                result.activations += child.activations;
                result.input_changes += child.input_changes;
                result.input_submits += child.input_submits;
                result.search_submits += child.search_submits;
                result.live_samples += child.live_samples;
            }
            const auto snapshot = ryn::resolve_theme(state->theme.get());
            result.snapshot_identity = snapshot.identity();
            result.snapshot_diagnostic = snapshot.diagnostic_json();
            return result;
        },
        {gallery_legacy_test_ids().begin(), gallery_legacy_test_ids().end()},
        navigation_control_count,
        [state] { return state->background_color; },
        [state](std::string_view identity) {
            const auto found = state->targets.find(std::string{identity});
            if (found != state->targets.end()) {
                return found->second;
            }
            for (const auto& page : state->frame_pages) {
                const auto target = page.target(identity);
                if (target.valid()) {
                    return target;
                }
            }
            return ryn::runtime::ComponentId{};
        },
    };
    return definition;
}

} // namespace rynui::example
