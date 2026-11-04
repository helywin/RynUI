#include "token_gallery_definition.hpp"

#include "ant_design_reference_catalog.hpp"
#include "gallery_document_model.hpp"
#include "gallery_layout.hpp"
#include "gallery_sample_registry.hpp"
#include "gallery_test_inventory.hpp"
#include "gallery_draft_codec.hpp"
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

struct GalleryState final {
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
    ryn::Signal<bool> checkbox_checked{false};
    ryn::CheckboxRef checkbox_ref;
    ryn::Signal<ryn::CheckboxValues> checkbox_values{ryn::CheckboxValues{ryn::String{u8"desktop"}}};
    ryn::Signal<std::vector<ryn::CheckboxOption>> checkbox_options{std::vector<ryn::CheckboxOption>{
        {ryn::String{u8"a"}, ryn::String{u8"A"}}, {ryn::String{u8"b"}, ryn::String{u8"B"}}}};
    bool checkbox_options_expanded{};
    ryn::Signal<std::optional<ryn::String>> radio_selected{std::optional<ryn::String>{ryn::String{u8"a"}}};
    ryn::Signal<std::vector<ryn::RadioOption>> radio_options{std::vector<ryn::RadioOption>{
        {ryn::String{u8"a"}, ryn::String{u8"A"}}, {ryn::String{u8"b"}, ryn::String{u8"B"}}}};
    bool radio_options_expanded{};
    ryn::RadioRef radio_ref;
    ryn::Signal<ryn::String> input_value{ryn::String{u8""}};
    ryn::Signal<ryn::String> input_feedback{ryn::String{u8"Enter 提交；支持选择、剪贴板、撤销/重做"}};
    ryn::Signal<ryn::String> search_value{ryn::String{}};
    ryn::Signal<ryn::String> search_feedback{ryn::String{u8"Enter 或按钮提交搜索"}};
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
        std::vector<ryn::SelectOption>{{u8"desktop", u8"桌面 / Desktop"}, {u8"service", u8"服务 / Service"}}};
    ryn::Signal<bool> select_open{false};
    ryn::SelectRef select_ref;
    bool select_options_expanded{};
    ryn::Signal<bool> float_open{false};
    ryn::Signal<ryn::FloatButtonScrollState> float_scroll{ryn::FloatButtonScrollState{600, 1000, 160}};
    ryn::Signal<bool> card_loading{false};
    ryn::Signal<ryn::String> tabs_active{ryn::String{u8"draft"}};
    ryn::Signal<ryn::String> tabs_label{ryn::String{u8"设置 / Settings"}};
    ryn::Signal<bool> tabs_disabled{false};
    ryn::Signal<int> modal_active{-1};
    ryn::Signal<bool> modal_nested{false};
    ryn::Signal<bool> modal_loading{true};
    ryn::Signal<ryn::String> modal_feedback{ryn::String{u8"打开示例后查看确认与取消来源"}};
    ryn::Signal<ryn::String> modal_lifecycle{ryn::String{u8"当前没有打开的对话框"}};
    ryn::Signal<bool> drawer_open{false};
    ryn::Signal<ryn::DrawerPlacement> drawer_placement{ryn::DrawerPlacement::Right};
    ryn::Signal<bool> tag_visible{true};
    ryn::Signal<bool> tag_checked{false};
    ryn::Signal<int> badge_count{3};
    ryn::Signal<bool> popconfirm_open{false};
    ryn::Signal<bool> popconfirm_loading{false};
    ryn::Signal<bool> alert_visible{true};
    ryn::Signal<bool> feedback_loading{false};
    ryn::Signal<float> feedback_percent{35};
    ryn::Signal<ryn::ProgressStatus> feedback_status{ryn::ProgressStatus::Normal};
    ryn::Signal<ryn::String> common_feedback{ryn::String{u8"操作下面的组件查看状态变化"}};
    ryn::Signal<std::vector<GalleryRecord>> records{gallery_records()};
    ryn::Signal<std::size_t> table_page{1};
    ryn::Signal<ryn::TableSort> table_sort{ryn::TableSort{}};
    ryn::Signal<std::vector<ryn::String>> table_selected{std::vector<ryn::String>{}};
    ryn::Signal<bool> table_loading{false};
    ryn::Signal<ryn::MenuKeys> menu_selected{ryn::MenuKeys{ryn::String{u8"overview"}}};
    ryn::Signal<ryn::MenuKeys> menu_open{ryn::MenuKeys{ryn::String{u8"tools"}}};
    ryn::Signal<ryn::String> menu_label{ryn::String{u8"任务"}};
    ryn::Signal<bool> menu_disabled{false};
    ryn::Signal<bool> dropdown_open{false};
    ryn::Signal<ryn::InputNumberValue> number_value{ryn::InputNumberValue{1.2}};
    ryn::Signal<bool> number_disabled{true};
    ryn::InputRef number_ref;
    ryn::Signal<ryn::String> data_feedback{ryn::String{u8"编辑数量、选择记录或执行菜单命令"}};
    ryn::Signal<ryn::CollapseKeys> collapse_keys{ryn::CollapseKeys{ryn::String{u8"general"}}};
    ryn::Signal<bool> collapse_accordion{false};
    ryn::Signal<bool> collapse_disabled{true};
    ryn::Signal<bool> collapse_select_open{false};
    ryn::Signal<ryn::String> segment_value{ryn::String{u8"day"}};
    ryn::Signal<ryn::String> segment_label{ryn::String{u8"日视图"}};
    ryn::Signal<bool> segment_disabled{false};
    ryn::Signal<ryn::String> panel_feedback{ryn::String{u8"展开面板或选择视图"}};
    ryn::Signal<ryn::String> empty_description{ryn::String{u8"暂无任务，创建第一条记录开始使用"}};
    ryn::Signal<ryn::EmptyImage> empty_image{ryn::EmptyImage::Default};
    ryn::Signal<ryn::ResultStatus> result_status{ryn::ResultStatus{ryn::Status::Success}};
    ryn::Signal<ryn::String> result_title{ryn::String{u8"任务已保存"}};
    ryn::Signal<ryn::String> result_feedback{ryn::String{u8"状态更新保留详情中的编辑草稿"}};
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
    ryn::Signal<ryn::String> form_help{ryn::String{u8"填写用于保存的名称"}};
    ryn::Signal<ryn::String> form_feedback{ryn::String{u8"尚未提交"}};
    ryn::Signal<ryn::DatePickerValue> date_value{ryn::CivilDate{2026, 10, 12}};
    ryn::Signal<bool> date_open{false};
    ryn::DatePickerRef date_ref;
    ryn::FormRef date_form_ref;
    ryn::FormField<ryn::String> date_field;
    ryn::DatePickerRef date_field_ref;
    ryn::Signal<bool> date_form_disabled{false};
    ryn::Signal<bool> date_form_read_only{false};
    ryn::Signal<ryn::String> date_help{ryn::String{u8"允许 2026-10-05 至 2026-11-25，每月 13 日不可选"}};
    ryn::Signal<ryn::String> date_feedback{ryn::String{u8"尚未保存日期"}};
    ryn::Signal<ryn::String> breadcrumb_title{ryn::String{u8"项目"}};
    ryn::Signal<bool> breadcrumb_visible{true};
    ryn::Signal<bool> breadcrumb_draft_visible{true};
    ryn::Signal<bool> breadcrumb_disabled{false};
    ryn::Signal<ryn::String> breadcrumb_draft{ryn::String{u8"Breadcrumb draft"}};
    ryn::Signal<ryn::String> breadcrumb_feedback{ryn::String{u8"选择祖先或编辑当前草稿"}};
    ryn::InputRef breadcrumb_ref;
    ryn::Signal<std::size_t> steps_current{0};
    ryn::Signal<bool> steps_vertical{false};
    ryn::Signal<ryn::StepsSize> steps_size{ryn::StepsSize::Default};
    ryn::Signal<ryn::StepsStatus> steps_status{ryn::StepsStatus::Process};
    ryn::Signal<bool> steps_visible{true};
    ryn::Signal<bool> steps_disabled{false};
    ryn::Signal<ryn::String> steps_draft{ryn::String{u8"Steps draft"}};
    ryn::Signal<ryn::String> steps_feedback{ryn::String{u8"填写资料后继续"}};
    ryn::InputRef steps_ref;
    ryn::Signal<ryn::TimePickerValue> time_value{ryn::TimeOfDay{8}};
    ryn::Signal<bool> time_open{false};
    ryn::Signal<bool> time_confirm{true};
    ryn::Signal<bool> time_accept{true};
    ryn::Signal<bool> time_input_read_only{false};
    ryn::Signal<ryn::String> time_request{ryn::String{u8"尚未选择"}};
    ryn::TimePickerRef time_ref;
    ryn::FormRef time_form_ref;
    ryn::FormField<ryn::String> time_field{ryn::String{u8"08:00:00"}};
    ryn::TimePickerRef time_field_ref;
    ryn::Signal<bool> time_form_disabled{false};
    ryn::Signal<bool> time_form_read_only{false};
    ryn::Signal<ryn::String> time_help{ryn::String{u8"HH:mm:ss，08:00:00 至 18:00:00，12 时不可选"}};
    ryn::Signal<ryn::String> time_feedback{ryn::String{u8"尚未保存时间"}};
    ryn::Signal<bool> details_columns{true};
    ryn::Signal<int> details_span{2};
    ryn::Signal<bool> details_visible{true};
    ryn::Signal<bool> details_vertical{false};
    ryn::Signal<bool> details_bordered{true};
    ryn::Signal<ryn::DescriptionsSize> details_size{ryn::DescriptionsSize::Default};
    ryn::Signal<ryn::String> details_name{ryn::String{u8"晨间项目"}};
    ryn::Signal<ryn::SelectSelection> details_role{ryn::SelectSelection{u8"editor"}};
    ryn::Signal<bool> details_enabled{true};
    ryn::Signal<ryn::String> details_feedback{ryn::String{u8"编辑资料后保存"}};
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
    ryn::Signal<ryn::String> skeleton_feedback{ryn::String{u8"编辑正文并选择分享范围"}};
    ryn::InputRef skeleton_ref;
    ryn::Signal<bool> popover_open{false};
    ryn::Signal<bool> popover_disabled{false};
    ryn::Signal<int> popover_placement{0};
    ryn::Signal<bool> popover_arrow{true};
    ryn::Signal<ryn::String> popover_draft{ryn::String{u8"下次会议前补齐项目备注"}};
    ryn::Signal<ryn::String> popover_feedback{ryn::String{u8"打开气泡编辑备注"}};
    ryn::InputRef popover_ref;
    ryn::MessageRef messages;
    ryn::NotificationRef notifications;
    ryn::Signal<bool> notification_open{false};
    ryn::Signal<ryn::NotificationPlacement> notification_placement{ryn::NotificationPlacement::TopRight};
    ryn::Signal<ryn::NoticeStatus> notification_status{ryn::NoticeStatus::Info};
    ryn::Signal<ryn::String> notification_draft{ryn::String{u8"请核对项目资料"}};
    ryn::Signal<ryn::String> notice_feedback{ryn::String{u8"提示会保留，可用按钮关闭或清空"}};
    ryn::InputRef notification_ref;
    ryn::Signal<int> timeline_mode{0};
    ryn::Signal<bool> timeline_reverse{false};
    ryn::Signal<bool> timeline_loading{true};
    ryn::Signal<bool> timeline_visible{true};
    ryn::Signal<ryn::String> timeline_draft{ryn::String{u8"记录发布后的跟进事项"}};
    ryn::Signal<ryn::String> timeline_feedback{ryn::String{u8"编辑并保存处理备注"}};
    ryn::InputRef timeline_ref;
    ryn::Signal<ryn::String> member_name{ryn::String{u8"江"}};
    ryn::Signal<bool> member_icon{false};
    ryn::Signal<bool> members_visible{true};
    ryn::Signal<ryn::AvatarShape> member_shape{ryn::AvatarShape::Circle};
    ryn::Signal<ryn::AvatarSizeValue> member_size{ryn::AvatarSizeValue{ryn::AvatarSize::Large}};
    ryn::Signal<std::uint32_t> member_limit{2};
    ryn::InputRef member_ref;
    ryn::Signal<ryn::String> member_feedback{ryn::String{u8"编辑成员名，打开 +N 浏览全部成员"}};
    ryn::Signal<ryn::StatisticValue> metric_value{ryn::StatisticValue{u8"-1234.123456789"}};
    std::size_t metric_step{};
    ryn::Signal<std::optional<int>> metric_precision{std::optional{2}};
    ryn::Signal<bool> metric_separators{false};
    ryn::Signal<bool> metric_formatter{false};
    ryn::Signal<bool> metric_loading{false};
    ryn::Signal<ryn::String> metric_note{ryn::String{u8"核对本期指标"}};
    ryn::Signal<ryn::String> metric_feedback{ryn::String{u8"尚未保存备注"}};
    ryn::InputRef metric_ref;
    ryn::Signal<float> rating{2.5F};
    ryn::Signal<bool> rating_accept{true};
    ryn::Signal<bool> rating_disabled{false};
    ryn::Signal<bool> rating_read_only{false};
    ryn::Signal<bool> rating_visible{true};
    ryn::Signal<ryn::String> rating_request{ryn::String{u8"尚未提交"}};
    ryn::Signal<ryn::String> rating_note{ryn::String{u8"补充服务评价"}};
    ryn::Signal<ryn::String> rating_feedback{ryn::String{u8"尚未保存评价"}};
    ryn::InputRef rating_ref;
    ryn::Signal<ryn::String> contact_value{ryn::String{}};
    ryn::Signal<ryn::String> contact_request{ryn::String{u8"尚未选择联系人"}};
    ryn::Signal<bool> contact_accept{true};
    ryn::Signal<bool> contact_extra{false};
    ryn::Signal<bool> contact_disabled{false};
    ryn::Signal<bool> contact_read_only{false};
    ryn::Signal<ryn::String> contact_note{ryn::String{u8"联系人备注"}};
    ryn::Signal<ryn::String> contact_saved{ryn::String{u8"尚未保存"}};
    ryn::AutoCompleteRef contact_ref;
    ryn::InputRef contact_note_ref;
    ryn::FormRef contact_form;
    ryn::FormField<ryn::String> contact_field{ryn::String{}};
    ryn::AutoCompleteRef contact_form_ref;
    ryn::Signal<ryn::String> contact_form_feedback{ryn::String{u8"表单尚未保存"}};
    ryn::Signal<ryn::TreeKeys> project_expanded{ryn::TreeKeys{u8"project", u8"src", u8"docs"}};
    ryn::Signal<ryn::TreeKeys> project_selected{ryn::TreeKeys{}};
    ryn::Signal<ryn::TreeKeys> project_checked{ryn::TreeKeys{}};
    ryn::Signal<bool> project_disabled{false};
    ryn::Signal<bool> project_visible{true};
    ryn::Signal<bool> project_locked{true};
    ryn::Signal<ryn::String> project_note{ryn::String{u8"项目处理备注"}};
    ryn::Signal<ryn::String> project_saved{ryn::String{u8"尚未保存"}};
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
    ryn::Signal<ryn::String> destination_request{ryn::String{u8"尚未修改"}};
    ryn::Signal<ryn::String> destination_note{ryn::String{u8"安排项目交付"}};
    ryn::Signal<ryn::String> destination_saved{ryn::String{u8"尚未保存"}};
    ryn::TreeSelectRef destination_ref;
    ryn::InputRef destination_note_ref;
    ryn::FormRef destination_form;
    ryn::FormField<ryn::String> destination_field{ryn::String{}};
    ryn::TreeSelectRef destination_form_ref;
    ryn::Signal<ryn::String> destination_form_feedback{ryn::String{u8"表单尚未保存"}};
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

    GalleryState() {
        destination_field.required(ryn::String{u8"请选择交付目标"});
        form_name.required(ryn::String{u8"请输入名称"});
        form_role.required(ryn::String{u8"请选择角色"});
        form_count.required(ryn::String{u8"请输入数量"}).validator([](ryn::InputNumberValue value) {
            return value && *value < 1 ? ryn::String{u8"数量至少为 1"} : ryn::String{};
        });
        form_consent.required(ryn::String{u8"请确认保存"});
        date_field.required(ryn::String{u8"请选择日期"}).validator([](ryn::String value) {
            if (value.empty()) {
                return ryn::String{};
            }
            const auto date = ryn::parse_iso_date(value.bytes());
            if (!date) {
                return ryn::String{u8"日期须为有效的 YYYY-MM-DD"};
            }
            return *date < ryn::CivilDate{2026, 10, 5} || *date > ryn::CivilDate{2026, 11, 25} || date->day() == 13
                       ? ryn::String{u8"日期不在允许范围"}
                       : ryn::String{};
        });
        time_field.required(ryn::String{u8"请选择时间"}).validator([](ryn::String value) {
            if (value.empty()) {
                return ryn::String{};
            }
            const auto time = ryn::parse_time_of_day(value.bytes());
            if (!time) {
                return ryn::String{u8"时间须为有效的 HH:mm:ss"};
            }
            return *time < ryn::TimeOfDay{8} || *time > ryn::TimeOfDay{18} || time->hour() == 12
                       ? ryn::String{u8"时间不在允许范围"}
                       : ryn::String{};
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
    if (state.page_component == "ant.component.breadcrumb") {
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
        visit("state/rating_accept", state.rating_accept);
        visit("state/rating_disabled", state.rating_disabled);
        visit("state/rating_read_only", state.rating_read_only);
        visit("state/rating_visible", state.rating_visible);
        visit("state/rating_note", state.rating_note);
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

ryn::String sample_text(const std::shared_ptr<GalleryState>& state, ryn::String text) {
    if (!state || !state->readable_labels || !text.bytes().starts_with("gallery.")) {
        return text;
    }
    auto value = text.bytes();
    const auto dot = value.find(" · ");
    if (dot != std::string_view::npos) {
        return ryn::String::from_utf8(value.substr(dot + std::string_view{" · "}.size())).value();
    }
    const auto slash = value.find(" / ");
    if (slash != std::string_view::npos) {
        return ryn::String::from_utf8(value.substr(slash + 3)).value();
    }
    value = value.substr(value.rfind('.') + 1);
    constexpr auto commands =
        std::to_array<std::pair<std::string_view, std::string_view>>({{"focus", "聚焦"},
                                                                      {"focus-all", "全选并聚焦"},
                                                                      {"toggle-open", "切换展开"},
                                                                      {"dynamic-options", "更新选项"},
                                                                      {"toggle-clear-disabled", "切换清空状态"},
                                                                      {"toggle-action", "切换显隐按钮"},
                                                                      {"restore", "恢复示例"},
                                                                      {"save", "保存"},
                                                                      {"submit", "提交"},
                                                                      {"reset", "重置"},
                                                                      {"loading", "加载"},
                                                                      {"mode", "切换模式"}});
    for (const auto& [key, label] : commands) {
        if (value == key) {
            return ryn::String::from_utf8(label).value();
        }
    }
    std::string label{value};
    std::ranges::replace(label, '-', ' ');
    return ryn::String::from_utf8(label).value();
}

ryn::String sample_text(ryn::String text) {
    return sample_text(active_gallery_draft.lock(), std::move(text));
}

template <typename T> T gallery_initial_value(std::string_view identity, const T& fallback) {
    const auto state = active_gallery_draft.lock();
    if (!state || !state->register_targets) {
        return fallback;
    }
    return gallery_draft_read(state->draft, "editor/" + std::string{identity}, fallback);
}

ryn::String gallery_initial_text(std::string_view identity, ryn::String fallback) {
    return gallery_initial_value(identity, sample_text(active_gallery_draft.lock(), std::move(fallback)));
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

std::string joined_scope(std::string_view prefix, std::string_view value) {
    std::string result(prefix);
    if (value.empty()) {
        result += "无";
        return result;
    }
    result += value;
    return result;
}

ryn::String reference_source_note(const AntDesignReferenceEntry& entry) {
    std::size_t records = entry.evidence_identifiers.empty() ? 0 : 1;
    std::size_t position = 0;
    while ((position = entry.evidence_identifiers.find(" · ", position)) != std::string_view::npos) {
        ++records;
        position += std::string_view{" · "}.size();
    }
    return utf8("来源：" + std::string(entry.source_path) + " · " + std::to_string(records) + " 项参考记录");
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

void document_text(ryn::String content, float size, float line_height, std::uint32_t weight = 400,
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
    const auto title = page_title ? utf8("RynUI Design System") : utf8(section.title);
    const auto summary = utf8(section.summary);
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

void navigation_button(const std::shared_ptr<GalleryState>& state, std::string_view caption,
                       GalleryNavigationTarget target, bool child = false) {
    const auto text = utf8(caption);
    const auto nav_width = state->navigation_width;
    const auto active = state->active_navigation;
    const auto config = ryn::bind([active, target, parent_theme = state->theme] {
        auto theme = navigation_theme_config();
        if (active.get() == target) {
            const auto tokens = ryn::resolve_theme(parent_theme.get());
            const auto primary = tokens.map().color_primary;
            theme.button.tokens.text_background = ryn::Color(primary.red(), primary.green(), primary.blue(), 0.08F);
            theme.button.tokens.text_color = tokens.map().color_primary;
        }
        return theme;
    });
    ryn::Theme(
        ryn::ThemeProps{}.config(config), ryn::ThemeContent{[state, target, text, nav_width, child] {
            ryn::Button(
                ryn::ButtonProps{}
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
                            const auto* entry = find_ant_design_reference_entry(target.component_identity);
                            if (entry &&
                                !gallery_support_filter_matches(state->support_filter.get(), entry->support_status)) {
                                state->support_filter.set(GallerySupportFilter::all);
                                ++state->telemetry.filter_updates;
                            }
                        }
                        state->active_navigation.set(target);
                        state->navigation_request = target;
                        ++state->telemetry.navigation_requests;
                    }),
                [text, nav_width, child] {
                    ryn::Text(ryn::TextProps{}.content(text).layout(ryn::LayoutStyle{}.width(ryn::bind(
                        [nav_width, child] { return ryn::dp(nav_width.get().value() - (child ? 48.0F : 36.0F)); }))));
                });
        }});
    const auto key = target.kind == GalleryNavigationTargetKind::component
                         ? "gallery.nav." + std::string{target.component_identity}
                     : target.kind == GalleryNavigationTargetKind::category
                         ? "gallery.nav.category." + std::to_string(static_cast<int>(target.category))
                         : "gallery.nav.section." + std::to_string(static_cast<int>(target.section));
    remember_target(state, key, true);
}

void filter_button(const std::shared_ptr<GalleryState>& state, std::string_view caption, GallerySupportFilter filter) {
    const auto text = utf8(caption);
    ryn::Button(ryn::ButtonProps{}.size(ryn::ControlSize::Small).onClick([state, filter] {
        if (state->support_filter.set(filter)) {
            ++state->telemetry.filter_updates;
        }
    }),
                [text] { ryn::Text(text); });
    remember_target(state, "gallery.filter." + std::to_string(static_cast<int>(filter)), true);
}

void navigation_controls(const std::shared_ptr<GalleryState>& state) {
    document_text(utf8("文档"), 12.0F, 24.0F, 500, ryn::TextTone::Secondary);
    ryn::Text(ryn::TextProps{}
                  .content(utf8("RynUI · Ant Design 6"))
                  .tone(ryn::TextTone::Secondary)
                  .layout(ryn::LayoutStyle{}.margin_bottom(ryn::dp(8.0F))));
    constexpr std::array<std::string_view, 6> section_labels{"概览",     "设计介绍", "设计价值", "基础样式与 Token",
                                                             "组件总览", "交互示例"};
    std::size_t section_index = 0;
    for (const auto& section : gallery_document_sections()) {
        navigation_button(state, section_labels[section_index++], GalleryNavigationTarget::to_section(section.kind));
    }
    ryn::Text(ryn::TextProps{}
                  .content(utf8("组件"))
                  .tone(ryn::TextTone::Secondary)
                  .layout(ryn::LayoutStyle{}.margin_top(ryn::dp(24.0F)).margin_bottom(ryn::dp(8.0F))));
    for (const auto& category : ant_design_reference_categories()) {
        navigation_button(state, gallery_category_title(category.category),
                          GalleryNavigationTarget::to_category(category.category));
        for (const auto& entry : ant_design_reference_entries()) {
            if (entry.category != category.category) {
                continue;
            }
            navigation_button(state, entry.english_name, GalleryNavigationTarget::to_component(entry.identity), true);
        }
    }
    ryn::Text(ryn::TextProps{}
                  .content(utf8("支持状态筛选"))
                  .tone(ryn::TextTone::Secondary)
                  .layout(ryn::LayoutStyle{}.margin_top(ryn::dp(24.0F))));
    filter_button(state, "All / 全部", GallerySupportFilter::all);
    filter_button(state, "Implemented / 已实现", GallerySupportFilter::implemented);
    filter_button(state, "Partial / 部分支持", GallerySupportFilter::partial);
    filter_button(state, "Planned / 规划中", GallerySupportFilter::planned);
    filter_button(state, "Web only / 仅 Web", GallerySupportFilter::web_only);
    filter_button(state, "Deprecated / 已弃用", GallerySupportFilter::deprecated);
    filter_button(state, "Out of scope / 不在范围", GallerySupportFilter::out_of_scope);
}

void scrollbar_surface(std::string_view identity, ReferenceSurfaceRole role,
                       const ryn::Signal<ryn::LogicalLength>& height, const ryn::Signal<bool>& visible) {
    ReferenceSurface(ReferenceSurfaceProps{}.identity(identity).role(role).visible(visible).layout(
                         ryn::LayoutStyle{}.width(ryn::dp(8.0F)).height(height).order(2)),
                     [] {});
}

void design_values(const std::shared_ptr<GalleryState>& state) {
    section_surface(state, gallery_document_sections()[2]);
    ryn::Flex(
        ryn::FlexProps{}
            .align(ryn::FlexAlign::Start)
            .wrap(true)
            .gap(ryn::dp(GalleryLayoutMetrics::card_gap), ryn::dp(GalleryLayoutMetrics::card_gap))
            .layout(ryn::LayoutStyle{}.width(state->document_width)),
        [state] {
            for (const auto& value : gallery_design_values()) {
                const auto title = utf8(std::string(value.english_name) + " / " + std::string(value.chinese_name));
                const auto summary = utf8(value.summary);
                ReferenceSurface(ReferenceSurfaceProps{}
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
    const auto title = label(test_id, caption);
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
                                        ryn::Text(title);
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
    reference_cell(state, "ant.map.colorPrimary", "Primary / 主色", ryn::Color::rgba8(22, 119, 255));
    reference_cell(state, "ant.map.colorSuccess", "Success / 成功", ryn::Color::rgba8(82, 196, 26));
    reference_cell(state, "ant.map.colorWarning", "Warning / 警告", ryn::Color::rgba8(250, 173, 20));
    reference_cell(state, "ant.map.colorError", "Error / 错误", ryn::Color::rgba8(255, 77, 79));
    reference_cell(state, "ant.map.colorInfo", "Info / 信息", ryn::Color::rgba8(22, 119, 255));
    reference_cell(state, "ant.map.blue6", "Blue 6", ryn::Color::rgba8(22, 119, 255));
    reference_cell(state, "ant.map.green6", "Green 6", ryn::Color::rgba8(82, 196, 26));
    reference_cell(state, "ant.map.red5", "Red 5", ryn::Color::rgba8(255, 77, 79));
    reference_cell(state, "ant.map.gold6", "Gold 6", ryn::Color::rgba8(250, 173, 20));
    reference_cell(state, "ant.map.purple6", "Purple 6", ryn::Color::rgba8(114, 46, 209));
}

void add_scale_cells(const std::shared_ptr<GalleryState>& state) {
    const auto text_cell = [&](std::string_view id, std::string_view caption, float size) {
        ryn::ThemeConfig config;
        config.text.tokens.font_size = ryn::dp(size);
        config.text.tokens.line_height = ryn::dp(size + 8.0F);
        reference_cell(state, id, caption, std::move(config));
    };
    text_cell("ant.map.fontSizeSM", "Font 12 / 字号", 12.0F);
    text_cell("ant.map.fontSize", "Font 14 / 字号", 14.0F);
    text_cell("ant.map.fontSizeLG", "Font 16 / 字号", 16.0F);
    reference_cell(state, "ant.map.sizeXS", "Gap 8 / 间距");
    reference_cell(state, "ant.map.size", "Gap 16 / 间距");
    reference_cell(state, "ant.map.sizeLG", "Gap 24 / 间距");
    reference_cell(state, "ant.map.controlHeightSM", "Control 24");
    reference_cell(state, "ant.seed.controlHeight", "Control 32");
    reference_cell(state, "ant.map.controlHeightLG", "Control 40");
    for (const auto [id, caption, radius] : std::array{std::tuple{"ant.map.borderRadiusSM", "Radius 4", 4.0F},
                                                       std::tuple{"ant.seed.borderRadius", "Radius 6", 6.0F},
                                                       std::tuple{"ant.map.borderRadiusLG", "Radius 8", 8.0F}}) {
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
    shadow_cell(state, "ant.alias.boxShadowTertiary", "Elevation 1", shadows.box_shadow_tertiary);
    shadow_cell(state, "ant.alias.boxShadowSecondary", "Elevation 2", shadows.box_shadow_secondary);
    shadow_cell(state, "ant.alias.boxShadow", "Elevation 3", shadows.box_shadow);
    shadow_cell(state, "ant.component.Button.defaultShadow", "Button Default", shadows.button_default);
    shadow_cell(state, "ant.component.Button.primaryShadow", "Button Primary", shadows.button_primary);
    shadow_cell(state, "ant.component.Button.dangerShadow", "Button Danger", shadows.button_danger);
    shadow_cell(state, "ant.alias.boxShadowDrawerLeft", "Drawer Left", shadows.drawer_left);
    shadow_cell(state, "ant.alias.boxShadowDrawerRight", "Drawer Right", shadows.drawer_right);
    shadow_cell(state, "ant.alias.boxShadowDrawerUp", "Drawer Up", shadows.drawer_up);
    shadow_cell(state, "ant.alias.boxShadowDrawerDown", "Drawer Down", shadows.drawer_down);
    shadow_cell(state, "ant.alias.boxShadowPopoverArrow", "Popover Arrow", shadows.popover_arrow);
    shadow_cell(state, "ant.alias.dropShadowPopover", "Popover", shadows.popover_drop);
    shadow_cell(state, "ant.alias.boxShadowCard", "Card", shadows.card);
    shadow_cell(state, "ant.alias.boxShadowTabsOverflowLeft", "Tabs Left inset", shadows.tabs_overflow_left);
    shadow_cell(state, "ant.alias.boxShadowTabsOverflowRight", "Tabs Right inset", shadows.tabs_overflow_right);
    shadow_cell(state, "ant.alias.boxShadowTabsOverflowTop", "Tabs Top inset", shadows.tabs_overflow_top);
    shadow_cell(state, "ant.alias.boxShadowTabsOverflowBottom", "Tabs Bottom inset", shadows.tabs_overflow_bottom);
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
        ryn::Title(ryn::TitleProps{}.level(level).content(u8"RynUI 标题层级"));
        remember_target(state, std::string{"gallery.typography.level-"} + std::to_string(static_cast<int>(level)));
    }
    ryn::Text(ryn::TypographyProps{}
                  .content(u8"Strong / Italic · 强调与斜体")
                  .strong(true)
                  .italic(true)
                  .type(ryn::TypographyType::Danger));
    remember_target(state, "gallery.typography.emphasis");
    ryn::Text(ryn::TypographyProps{}.content(u8"code: a += b; 中文回退").code(true).type(ryn::TypographyType::Success));
    remember_target(state, "gallery.typography.code");
    ryn::Text(ryn::TypographyProps{}.content(u8"Ctrl + C / 键帽").keyboard(true).type(ryn::TypographyType::Warning));
    remember_target(state, "gallery.typography.keyboard");
    ryn::Text(ryn::TypographyProps{}
                  .content(u8"高亮、下划线、删除线")
                  .mark(true)
                  .underline(true)
                  .strikethrough(true)
                  .type(ryn::TypographyType::Secondary));
    remember_target(state, "gallery.typography.decoration");
    ryn::Link(ryn::LinkProps{}.content(u8"Link · 指针与键盘激活").onClick([state] { ++state->telemetry.activations; }));
    remember_target(state, "gallery.typography.activation");
    state->telemetry.live_samples += 10;
}

void add_divider_catalog_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Divider();
    remember_target(state, "gallery.divider.basic");
    ryn::Divider(ryn::DividerProps{}.content(u8"Divider 标签"));
    remember_target(state, "gallery.divider.labeled");
    ryn::Divider(ryn::DividerProps{}.dashed(true));
    remember_target(state, "gallery.divider.dashed");
    state->telemetry.live_samples += 3;
}

void component_entry(const std::shared_ptr<GalleryState>& state, const AntDesignReferenceEntry& entry) {
    const auto name = utf8(std::string(entry.english_name) + " / " + std::string(entry.chinese_name));
    const auto summary = utf8(entry.summary);
    const auto supported = utf8(joined_scope("支持：", entry.supported_scope));
    const auto missing = utf8(joined_scope("缺失：", entry.missing_scope));
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
        document_text(utf8(gallery_category_title(category.category)), 18.0F, 28.0F, 500);
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
    const auto text = label(test_id, caption);
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
                   ryn::Button(std::move(props), [text] { ryn::Text(text); });
                   ++state->telemetry.live_samples;
               }});
    remember_target(state, test_id);
}

void add_theme_button_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Space(
        ryn::SpaceProps{}
            .wrap(true)
            .align(ryn::SpaceAlign::Center)
            .size(ryn::dp(8))
            .layout(ryn::LayoutStyle{}.width(state->document_width)),
        [state] {
            themed_button(state, "gallery.theme.default", "Default", algorithm_config(ryn::ThemeAlgorithm::Default),
                          ryn::ButtonType::Default, ryn::ControlSize::Middle, false, false, [state] {
                              state->set_theme(algorithm_config(ryn::ThemeAlgorithm::Default));
                              ++state->telemetry.activations;
                          });
            themed_button(state, "gallery.theme.dark", "Dark", algorithm_config(ryn::ThemeAlgorithm::Dark),
                          ryn::ButtonType::Default, ryn::ControlSize::Middle, false, false, [state] {
                              state->set_theme(algorithm_config(ryn::ThemeAlgorithm::Dark));
                              ++state->telemetry.activations;
                          });
            themed_button(state, "gallery.theme.compact", "Compact", algorithm_config(ryn::ThemeAlgorithm::Compact),
                          ryn::ButtonType::Default, ryn::ControlSize::Middle, false, false, [state] {
                              state->set_theme(algorithm_config(ryn::ThemeAlgorithm::Compact));
                              ++state->telemetry.activations;
                          });
            auto nested = algorithm_config(ryn::ThemeAlgorithm::Dark);
            nested.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
            themed_button(state, "gallery.theme.nested-brand", "Dark + Purple Seed", nested, ryn::ButtonType::Primary);
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
                   themed_button(state, "gallery.state.default", "Default", defaults);
                   themed_button(state, "gallery.state.primary", "Primary", defaults, ryn::ButtonType::Primary);
                   themed_button(state, "gallery.state.danger", "Danger", defaults, ryn::ButtonType::Danger);
                   themed_button(state, "gallery.state.hover", "Hover me / 悬停", defaults);
                   themed_button(state, "gallery.state.active", "Press me / 按下", defaults);
                   themed_button(state, "gallery.state.focus-visible", "Tab focus / 键盘焦点", defaults);
                   themed_button(state, "gallery.state.disabled", "Disabled / 禁用", defaults, ryn::ButtonType::Default,
                                 ryn::ControlSize::Middle, state->disabled);
                   themed_button(state, "gallery.state.loading", "Loading / 加载", defaults, ryn::ButtonType::Primary,
                                 ryn::ControlSize::Middle, false, state->loading);
               });
}

void add_input_base_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Input / 输入家族 · 原生桌面功能");
    ryn::Text(u8"支持：四变体、统计/上限、ref/系统提示、清空、Unicode 编辑与 IME");
    ryn::Text(u8"Input / Password / Search / TextArea / OTP：双向排版、视觉导航与逻辑编辑");
    ryn::Space(ryn::SpaceProps{}
                   .align(ryn::SpaceAlign::Start)
                   .wrap(true)
                   .size(ryn::dp(8.0F))
                   .layout(ryn::LayoutStyle{}.width(state->document_width)),
               [state] {
                   ryn::Theme(ryn::ThemeProps{}, ryn::ThemeContent{[state] {
                                  ++state->telemetry.theme_content_runs;
                                  ryn::Text(sample_text(u8"gallery.input.controlled · 受控输入"));
                                  ryn::Input(ryn::InputProps{}
                                                 .value(state->input_value)
                                                 .placeholder(u8"受控：输入中文 / Latin / emoji")
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
                                             ryn::InputPrefix{[] { ryn::Text(u8"前"); }},
                                             ryn::InputSuffix{[] { ryn::Text(u8"32"); }});
                                  remember_target(state, "gallery.input.controlled");
                                  ++state->telemetry.live_samples;
                              }});
                   ryn::Theme(ryn::ThemeProps{}, ryn::ThemeContent{[state] {
                                  ++state->telemetry.theme_content_runs;
                                  ryn::Text(sample_text(u8"gallery.input.uncontrolled · 非受控输入"));
                                  ryn::Input(ryn::InputProps{}
                                                 .defaultValue(state->initial("gallery.input.uncontrolled",
                                                                              ryn::String{u8"非受控 / Input"}))
                                                 .placeholder(u8"清空后显示 placeholder")
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
                              }});
               });
    ryn::Text(ryn::TextProps{}.content(state->input_feedback));
}

void add_search_base_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Search / 搜索 · 原生单行功能：复用 Input、Button 与 Compact");
    ryn::Text(u8"支持：四变体、全部共用属性、自定义图标、清空来源与连接外观");
    ryn::Space(
        ryn::SpaceProps{}
            .align(ryn::SpaceAlign::Start)
            .wrap(true)
            .size(ryn::dp(8.0F))
            .layout(ryn::LayoutStyle{}.width(state->document_width)),
        [state] {
            ryn::Search(ryn::SearchProps{}
                            .value(state->search_value)
                            .placeholder(u8"输入搜索词 / Search")
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
                        ryn::SearchButtonContent{[] { ryn::Text(u8"查询"); }});
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
    ryn::Text(u8"Password / 密码 · 复用 Input 编辑与窗口输入会话");
    ryn::Space(ryn::SpaceProps{}
                   .align(ryn::SpaceAlign::Start)
                   .wrap(true)
                   .size(ryn::dp(8.0F))
                   .layout(ryn::LayoutStyle{}.width(state->document_width)),
               [state] {
                   ryn::Password(ryn::PasswordProps{}
                                     .defaultValue(u8"RynUI 密码")
                                     .placeholder(u8"输入密码 / Password")
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
    ryn::Text(u8"Switch / 开关 · Middle、Small、disabled、loading");
    ryn::Space(ryn::SpaceProps{}
                   .align(ryn::SpaceAlign::Start)
                   .wrap(true)
                   .align(ryn::SpaceAlign::Center)
                   .size(ryn::dp(12.0F))
                   .layout(ryn::LayoutStyle{}.width(state->document_width)),
               [state] {
                   ryn::Text(u8"受控");
                   ryn::Switch(ryn::SwitchProps{}.checked(state->switch_checked).onChange([state](bool value) {
                       state->switch_checked.set(value);
                   }));
                   ++state->telemetry.live_samples;
                   ryn::Text(u8"Small / 已选");
                   ryn::Switch(ryn::SwitchProps{}
                                   .size(ryn::SwitchSize::Small)
                                   .defaultChecked(gallery_initial_value<bool>("gallery.switch.basic-small", true))
                                   .onChange(gallery_value_changed<bool>("gallery.switch.basic-small")));
                   gallery_target("gallery.switch.basic-small");
                   ++state->telemetry.live_samples;
                   ryn::Text(u8"禁用");
                   ryn::Switch(ryn::SwitchProps{}
                                   .disabled(true)
                                   .defaultChecked(gallery_initial_value<bool>("gallery.switch.basic-disabled", false))
                                   .onChange(gallery_value_changed<bool>("gallery.switch.basic-disabled")));
                   gallery_target("gallery.switch.basic-disabled");
                   ++state->telemetry.live_samples;
                   ryn::Text(u8"加载");
                   ryn::Switch(ryn::SwitchProps{}
                                   .loading(state->loading)
                                   .defaultChecked(gallery_initial_value<bool>("gallery.switch.basic-loading", false))
                                   .onChange(gallery_value_changed<bool>("gallery.switch.basic-loading")));
                   gallery_target("gallery.switch.basic-loading");
                   ++state->telemetry.live_samples;
               });
}

void add_checkbox_base_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Checkbox / 多选框 · checked、indeterminate、disabled");
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
                          ryn::CheckboxLabel{[] { ryn::Text(u8"受控 / Control"); }});
            ++state->telemetry.live_samples;
            ryn::Checkbox(ryn::CheckboxProps{}
                              .defaultChecked(gallery_initial_value<bool>("gallery.checkbox.basic-checked", true))
                              .onChange(gallery_value_changed<bool>("gallery.checkbox.basic-checked")),
                          ryn::CheckboxLabel{[] { ryn::Text(u8"已选 / Checked"); }});
            gallery_target("gallery.checkbox.basic-checked");
            ++state->telemetry.live_samples;
            ryn::Checkbox(ryn::CheckboxProps{}
                              .indeterminate(true)
                              .defaultChecked(gallery_initial_value<bool>("gallery.checkbox.basic-mixed", false))
                              .onChange(gallery_value_changed<bool>("gallery.checkbox.basic-mixed")),
                          ryn::CheckboxLabel{[] { ryn::Text(u8"半选 / Mixed"); }});
            gallery_target("gallery.checkbox.basic-mixed");
            ++state->telemetry.live_samples;
            ryn::Checkbox(ryn::CheckboxProps{}
                              .disabled(true)
                              .defaultChecked(gallery_initial_value<bool>("gallery.checkbox.basic-disabled", false))
                              .onChange(gallery_value_changed<bool>("gallery.checkbox.basic-disabled")),
                          ryn::CheckboxLabel{[] { ryn::Text(u8"禁用 / Disabled"); }});
            gallery_target("gallery.checkbox.basic-disabled");
            ++state->telemetry.live_samples;
        });
}

void add_radio_base_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Radio / 单选框 · 单独使用、互斥分组、disabled");
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
                              ryn::RadioLabel{[] { ryn::Text(u8"单独已选 / Standalone"); }});
                   gallery_target("gallery.radio.basic-standalone");
                   ++state->telemetry.live_samples;
                   ryn::RadioGroup(ryn::RadioGroupProps{}
                                       .options({
                                           {ryn::String{u8"a"}, ryn::String{u8"甲 / Alpha"}},
                                           {ryn::String{u8"b"}, ryn::String{u8"乙 / Beta"}},
                                           {ryn::String{u8"c"}, ryn::String{u8"禁用 / Disabled"}, true},
                                       })
                                       .value(state->radio_selected)
                                       .onChange([state](const ryn::String& value) {
                                           state->radio_selected.set(std::optional<ryn::String>{value});
                                       }));
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
    ryn::Text(u8"Typography · 省略、复制与原地编辑");
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
    ryn::Link(ryn::LinkProps{}.content(u8"Link · 激活计数").onClick([state] { ++state->telemetry.activations; }));
    remember_target(state, "gallery.typography.link");
    ryn::Link(ryn::LinkProps{}.content(u8"禁用 Link").disabled(true));
    remember_target(state, "gallery.typography.disabled-link");
    state->telemetry.live_samples += 7;
}

void add_divider_base_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Divider · 水平、虚线、plain、None 朝向间距与垂直线");
    ryn::Divider(ryn::DividerProps{}.orientation(ryn::DividerOrientation::Left).content(u8"Left"));
    remember_target(state, "gallery.divider.left");
    ryn::Divider(ryn::DividerProps{}.orientation(ryn::DividerOrientation::Right).content(u8"Right").dashed(true));
    remember_target(state, "gallery.divider.right");
    ryn::Divider(ryn::DividerProps{}.content(u8"Plain").plain(true));
    remember_target(state, "gallery.divider.plain");
    ryn::Divider(ryn::DividerProps{}
                     .content(u8"None / 显式无朝向间距")
                     .orientation(ryn::DividerOrientation::Left)
                     .orientationMargin(ryn::DividerOrientationMargin::none()));
    remember_target(state, "gallery.divider.no-margin");
    ryn::Space(ryn::SpaceProps{}.align(ryn::SpaceAlign::Center), [state] {
        ryn::Text(u8"文字");
        ryn::Divider(ryn::DividerProps{}.type(ryn::DividerType::Vertical));
        remember_target(state, "gallery.divider.vertical");
        ryn::Text(u8"文字");
    });
    state->telemetry.live_samples += 5;
}

void add_tooltip_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Tooltip · 悬停 / 焦点 / 点击 / 右键 / 富标题 / 居中箭头");
    ryn::Space(ryn::SpaceProps{}.align(ryn::SpaceAlign::Start).wrap(true), [] {
        ryn::Tooltip(ryn::TooltipProps{}.title(ryn::String{u8"悬停或 Tab 焦点显示，Escape 关闭"}),
                     ryn::TooltipTrigger{
                         [] { ryn::Button(ryn::ButtonProps{}, ryn::ButtonContent{[] { ryn::Text(u8"提示"); }}); }});
        ryn::Tooltip(
            ryn::TooltipProps{}.title(ryn::String{u8"禁用按钮保留说明"}).placement(ryn::TooltipPlacement::Right),
            ryn::TooltipTrigger{[] {
                ryn::Button(ryn::ButtonProps{}.disabled(true),
                            ryn::ButtonContent{[] { ryn::Text(u8"禁用 / Disabled"); }});
            }});
    });
    state->telemetry.live_samples += 2;
    ryn::Space(ryn::SpaceProps{}.align(ryn::SpaceAlign::Start).wrap(true), [] {
        ryn::Tooltip(ryn::TooltipProps{}.trigger(ryn::TooltipTriggerMode::Click), ryn::TooltipTrigger{[] {
                         ryn::Button(ryn::ButtonProps{}, ryn::ButtonContent{[] { ryn::Text(u8"点击 / 富标题"); }});
                     }},
                     ryn::TooltipTitle{[] {
                         ryn::Flex(ryn::FlexProps{}.align(ryn::FlexAlign::Start).vertical(true), ryn::FlexContent{[] {
                                       ryn::Text(u8"保留的原生内容 / Retained title");
                                       ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CheckOutlined));
                                   }});
                     }});
        ryn::Tooltip(ryn::TooltipProps{}
                         .title(ryn::String{u8"指针位置 / Pointer anchor"})
                         .trigger(ryn::TooltipTriggerMode::ContextMenu),
                     ryn::TooltipTrigger{[] {
                         ryn::Button(ryn::ButtonProps{}, ryn::ButtonContent{[] { ryn::Text(u8"右键 / ContextMenu"); }});
                     }});
        ryn::Tooltip(ryn::TooltipProps{}
                         .title(ryn::String{u8"居中角箭头 / Centered corner arrow"})
                         .triggers(ryn::TooltipTriggers{true, true, true, false})
                         .placement(ryn::TooltipPlacement::BottomLeft)
                         .pointAtCenter(true),
                     ryn::TooltipTrigger{[] {
                         ryn::Button(ryn::ButtonProps{}, ryn::ButtonContent{[] { ryn::Text(u8"组合触发 / 居中"); }});
                     }});
    });
    state->telemetry.live_samples += 3;
}

void add_slider_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Slider · marks / dots / 离散范围 / 值提示 / 反向 / 纵向 / 整段拖动 / 编辑 / 逐端点禁用");
    ryn::Slider(ryn::SliderProps{}
                    .value(state->slider_value)
                    .marks(ryn::SliderMarks{
                        {0, ryn::String{u8"低 Low"}}, {50, ryn::String{u8"中"}}, {100, ryn::String{u8"高 High"}}})
                    .onChange([state](double value) { state->slider_value.set(value); })
                    .layout(ryn::LayoutStyle{}.width(state->cell_width)));
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
    ryn::Text(u8"Button · 原生颜色 / 六变体 / ghost / 图标 / shape / block / ref / loading delay / wave");
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
    ryn::Space(
        ryn::SpaceProps{}
            .align(ryn::SpaceAlign::Start)
            .wrap(true)
            .layout(ryn::LayoutStyle{}.width(state->document_width)),
        [state] {
            ryn::Button(ryn::ButtonProps{}
                            .ref(state->button_ref)
                            .loading(state->button_loading)
                            .loadingDelay(ryn::Duration::milliseconds(150))
                            .iconPlacement(ryn::ButtonIconPlacement::End),
                        ryn::ButtonContent{[] { ryn::Text(sample_text(u8"gallery.button.icon-end · End / Loading")); }},
                        ryn::ButtonIcon{[] { ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CheckOutlined)); }},
                        ryn::ButtonLoadingIcon{[] { ryn::Text(u8"…"); }});
            remember_target(state, "gallery.button.icon-end");
            ryn::Button(ryn::ButtonProps{}.shape(ryn::ButtonShape::Circle),
                        ryn::ButtonSlots{.icon = ryn::ButtonIcon{
                                             [] { ryn::Icon(ryn::IconProps{}.name(ryn::IconName::SearchOutlined)); }}});
            remember_target(state, "gallery.button.circle-icon");
            ryn::Button(ryn::ButtonProps{}.shape(ryn::ButtonShape::Square),
                        [] { ryn::Text(sample_text(u8"gallery.button.square · Square")); });
            remember_target(state, "gallery.button.square");
            ryn::Button(
                ryn::ButtonProps{}.color(ryn::ButtonColor::Blue).variant(ryn::ButtonVariant::Dashed).ghost(true),
                [] { ryn::Text(sample_text(u8"gallery.button.ghost · Ghost dashed")); });
            remember_target(state, "gallery.button.ghost");
            ryn::Button(ryn::ButtonProps{}.onClick([state] {
                state->button_loading.set(!state->button_loading.get());
                static_cast<void>(state->button_ref.focus());
                ++state->telemetry.activations;
            }),
                        [] { ryn::Text(sample_text(u8"gallery.button.loading-focus · 切换加载并聚焦")); });
            remember_target(state, "gallery.button.loading-focus");
            state->telemetry.live_samples += 5;
        });
    ryn::Button(ryn::ButtonProps{}.block(true).shape(ryn::ButtonShape::Round),
                [] { ryn::Text(sample_text(u8"gallery.button.round-block · Round block / resize")); });
    remember_target(state, "gallery.button.round-block");
    ++state->telemetry.live_samples;
}

void add_divider_variant_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Divider(ryn::DividerProps{}
                     .variant(ryn::DividerVariant::Dotted)
                     .size(ryn::ControlSize::Small)
                     .orientation(ryn::DividerOrientation::Start)
                     .direction(ryn::DividerDirection::RightToLeft)
                     .content(sample_text(u8"gallery.divider.small-dotted-rtl · Small / Start / RTL")));
    remember_target(state, "gallery.divider.small-dotted-rtl");
    ryn::Divider(ryn::DividerProps{}
                     .variant(ryn::DividerVariant::Dashed)
                     .size(ryn::ControlSize::Middle)
                     .orientation(ryn::DividerOrientation::End)
                     .orientationMargin(ryn::DividerOrientationMargin::length(ryn::dp(20)))
                     .content(sample_text(u8"gallery.divider.middle-length · Middle / End / 20 dp")));
    remember_target(state, "gallery.divider.middle-length");
    ryn::Space(ryn::SpaceProps{}.align(ryn::SpaceAlign::Center), [state] {
        ryn::Text(sample_text(u8"gallery.divider.vertical-dotted · Vertical"));
        ryn::Divider(ryn::DividerProps{}.type(ryn::DividerType::Vertical).variant(ryn::DividerVariant::Dotted));
        remember_target(state, "gallery.divider.vertical-dotted");
        ryn::Text(u8"Dotted");
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
    ryn::Text(u8"Switch · retained 内容、图标、RTL、ref 与组件主题");
    ryn::Space(
        ryn::SpaceProps{}.align(ryn::SpaceAlign::Start).wrap(true).align(ryn::SpaceAlign::Center).size(ryn::dp(12)),
        [state] {
            ryn::Text(sample_text(u8"gallery.switch.content-ref"));
            ryn::Switch(
                ryn::SwitchProps{}.checked(state->switch_checked).ref(state->switch_ref).onChange([state](bool value) {
                    state->switch_checked.set(value);
                }),
                ryn::SwitchSlots{ryn::SwitchCheckedContent{[] { ryn::Text(u8"开启"); }},
                                 ryn::SwitchUncheckedContent{[] { ryn::Text(u8"关闭"); }}});
            ryn::Text(sample_text(u8"gallery.switch.small-icon-rtl"));
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
                           ryn::Text(sample_text(u8"gallery.switch.component-theme"));
                           ryn::Switch(
                               ryn::SwitchProps{}
                                   .defaultChecked(gallery_initial_value<bool>("gallery.switch.component-theme", true))
                                   .onChange(gallery_value_changed<bool>("gallery.switch.component-theme")),
                               ryn::SwitchSlots{ryn::SwitchCheckedContent{[] { ryn::Text(u8"紫色"); }},
                                                ryn::SwitchUncheckedContent{[] { ryn::Text(u8"关"); }}});
                           gallery_target("gallery.switch.component-theme");
                       }});
            ryn::Button(ryn::ButtonProps{}.onClick([state] {
                static_cast<void>(state->switch_ref.focus());
                ++state->telemetry.activations;
            }),
                        [] { ryn::Text(sample_text(u8"gallery.switch.focus · 聚焦开关")); });
            gallery_target("gallery.switch.focus");
            state->telemetry.live_samples += 4;
        });
    ryn::Flex(ryn::FlexProps{}.align(ryn::FlexAlign::Start).layout(ryn::LayoutStyle{}.height(ryn::dp(48.0F))),
              ryn::FlexContent{[] {}});
}

void add_checkbox_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Checkbox · 多选、动态保留选项、RTL、ref 与独立主题");
    ryn::Text(sample_text(u8"gallery.checkbox.group-controlled · 三类值"));
    ryn::CheckboxGroup(
        ryn::CheckboxGroupProps{}
            .options(std::vector<ryn::CheckboxOption>{{ryn::String{u8"desktop"}, ryn::String{u8"桌面"}},
                                                      {1.0, ryn::String{u8"数字"}},
                                                      {true, ryn::String{u8"禁用布尔"}, true}})
            .value(state->checkbox_values)
            .onChange([state](const ryn::CheckboxValues& values) { state->checkbox_values.set(values); }));
    ryn::Text(sample_text(u8"gallery.checkbox.group-dynamic · 按 value 保留 identity"));
    ryn::CheckboxGroup(ryn::CheckboxGroupProps{}
                           .options(state->checkbox_options)
                           .defaultValue(gallery_initial_value<ryn::CheckboxValues>(
                               "gallery.checkbox.group-dynamic", ryn::CheckboxValues{ryn::String{u8"a"}}))
                           .onChange(gallery_value_changed<ryn::CheckboxValues>("gallery.checkbox.group-dynamic")));
    ryn::Button(ryn::ButtonProps{}.onClick([state] {
        state->checkbox_options_expanded = !state->checkbox_options_expanded;
        if (state->checkbox_options_expanded) {
            state->checkbox_options.set({{ryn::String{u8"b"}, ryn::String{u8"B · retained"}},
                                         {ryn::String{u8"a"}, ryn::String{u8"A"}},
                                         {2.0, ryn::String{u8"新增数字"}}});
        } else {
            state->checkbox_options.set(
                {{ryn::String{u8"a"}, ryn::String{u8"A"}}, {ryn::String{u8"b"}, ryn::String{u8"B"}}});
        }
        ++state->telemetry.activations;
    }),
                [] { ryn::Text(sample_text(u8"gallery.checkbox.options-update · 重排 / 增删")); });
    gallery_target("gallery.checkbox.options-update");
    ryn::Checkbox(ryn::CheckboxProps{}
                      .ref(state->checkbox_ref)
                      .indeterminate(true)
                      .direction(ryn::CheckboxDirection::RightToLeft)
                      .defaultChecked(gallery_initial_value<bool>("gallery.checkbox.rich-rtl-ref", false))
                      .onChange(gallery_value_changed<bool>("gallery.checkbox.rich-rtl-ref")),
                  ryn::CheckboxLabel{[] {
                      ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CheckOutlined));
                      ryn::Text(sample_text(u8"gallery.checkbox.rich-rtl-ref · 半选与富标签"));
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
                       ryn::CheckboxLabel{
                           [] { ryn::Text(sample_text(u8"gallery.checkbox.component-theme · 紫色 / 20 dp")); }});
                   gallery_target("gallery.checkbox.component-theme");
               }});
    ryn::Button(ryn::ButtonProps{}.onClick([state] {
        static_cast<void>(state->checkbox_ref.focus());
        ++state->telemetry.activations;
    }),
                [] { ryn::Text(sample_text(u8"gallery.checkbox.focus · 聚焦多选框")); });
    gallery_target("gallery.checkbox.focus");
    state->telemetry.live_samples += 6;
}

void add_radio_themed_sample(const std::shared_ptr<GalleryState>& state) {
    ryn::ThemeConfig custom;
    custom.radio.algorithm = true;
    custom.radio.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
    ryn::Theme(ryn::ThemeProps{}.config(custom), ryn::ThemeContent{[state] {
                   ++state->telemetry.theme_content_runs;
                   ryn::Text(sample_text(u8"gallery.radio.component-theme / gallery.radio.ref"));
                   ryn::RadioButton(ryn::RadioProps{}
                                        .defaultChecked(gallery_initial_value<bool>("gallery.radio.ref", true))
                                        .onChange(gallery_value_changed<bool>("gallery.radio.ref"))
                                        .ref(state->radio_ref),
                                    ryn::RadioLabel{[] { ryn::Text(u8"独立紫色 / 富标签 / ref"); }});
                   gallery_target("gallery.radio.ref");
               }});
}

void add_radio_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Radio · 动态选项、键盘组导航、按钮连接边、三尺寸、block、RTL、ref 与独立主题");
    ryn::Text(sample_text(u8"gallery.radio.dynamic-outline · 按 value 保留 identity"));
    ryn::RadioGroup(ryn::RadioGroupProps{}
                        .options(state->radio_options)
                        .defaultValue(gallery_initial_value<ryn::RadioValue>("gallery.radio.dynamic-outline",
                                                                             ryn::RadioValue{ryn::String{u8"a"}}))
                        .onChange(gallery_value_changed<ryn::RadioValue>("gallery.radio.dynamic-outline"))
                        .optionType(ryn::RadioOptionType::Button));
    gallery_target("gallery.radio.dynamic-outline");
    ryn::Button(ryn::ButtonProps{}.onClick([state] {
        state->radio_options_expanded = !state->radio_options_expanded;
        if (state->radio_options_expanded) {
            state->radio_options.set({{ryn::String{u8"b"}, ryn::String{u8"B · retained"}},
                                      {ryn::String{u8"a"}, ryn::String{u8"A"}},
                                      {2.0, ryn::String{u8"新增数字"}}});
        } else {
            state->radio_options.set(
                {{ryn::String{u8"a"}, ryn::String{u8"A"}}, {ryn::String{u8"b"}, ryn::String{u8"B"}}});
        }
        ++state->telemetry.activations;
    }),
                [] { ryn::Text(sample_text(u8"gallery.radio.options-update · 重排 / 增删")); });
    gallery_target("gallery.radio.options-update");
    ryn::Text(sample_text(u8"gallery.radio.solid-large-block · 三类值 / 禁用"));
    ryn::RadioGroup(ryn::RadioGroupProps{}
                        .options({{ryn::String{u8"desktop"}, ryn::String{u8"桌面"}},
                                  {1.0, ryn::String{u8"数字"}},
                                  {true, ryn::String{u8"禁用布尔"}, true}})
                        .defaultValue(gallery_initial_value<ryn::RadioValue>("gallery.radio.solid-large-block",
                                                                             ryn::RadioValue{1.0}))
                        .onChange(gallery_value_changed<ryn::RadioValue>("gallery.radio.solid-large-block"))
                        .optionType(ryn::RadioOptionType::Button)
                        .buttonStyle(ryn::RadioButtonStyle::Solid)
                        .size(ryn::RadioSize::Large)
                        .block(true)
                        .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    gallery_target("gallery.radio.solid-large-block");
    ryn::Text(sample_text(u8"gallery.radio.vertical-small-rtl"));
    ryn::RadioGroup(
        ryn::RadioGroupProps{}
            .options({{false, ryn::String{u8"false"}}, {true, ryn::String{u8"true"}}, {3.0, ryn::String{u8"数字"}}})
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
                [] { ryn::Text(sample_text(u8"gallery.radio.focus · 聚焦单选按钮")); });
    gallery_target("gallery.radio.focus");
    state->telemetry.live_samples += 6;
}

void add_flex_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Flex · 真实文字基线、反向换行、RTL 与默认拉伸");
    ryn::Text(sample_text(u8"gallery.flex.baseline · 14 / 28 dp 与控件标签"));
    ryn::Flex(ryn::FlexProps{}.align(ryn::FlexAlign::Baseline).gap(ryn::dp(8)), ryn::FlexContent{[state] {
                  ryn::Text(u8"Ag 基线");
                  ryn::ThemeConfig large;
                  large.text.tokens.font_size = ryn::dp(28);
                  large.text.tokens.line_height = ryn::dp(40);
                  ryn::Theme(ryn::ThemeProps{}.config(large), ryn::ThemeContent{[state] {
                                 ++state->telemetry.theme_content_runs;
                                 ryn::Text(u8"Ag 大字");
                             }});
                  ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"Ag 按钮"); });
              }});
    remember_target(state, "gallery.flex.baseline");
    ryn::Text(sample_text(u8"gallery.flex.wrap-reverse · 从下边堆叠"));
    ryn::Flex(ryn::FlexProps{}
                  .align(ryn::FlexAlign::Start)
                  .wrap(ryn::FlexWrap::WrapReverse)
                  .gap(ryn::dp(8))
                  .layout(ryn::LayoutStyle{}.width(ryn::dp(180)).height(ryn::dp(130))),
              ryn::FlexContent{[] {
                  for (const auto label : {u8"第一行 A", u8"第二行 B", u8"第三行 C"}) {
                      ryn::Button(ryn::ButtonProps{}.layout(ryn::LayoutStyle{}.width(ryn::dp(110))), [label] {
                          ryn::Text(ryn::String::from_utf8(reinterpret_cast<const char*>(label)).value());
                      });
                  }
              }});
    remember_target(state, "gallery.flex.wrap-reverse");
    ryn::Text(sample_text(u8"gallery.flex.vertical-rtl · 纵向换列"));
    ryn::Flex(ryn::FlexProps{}
                  .align(ryn::FlexAlign::Start)
                  .vertical(true)
                  .wrap(true)
                  .direction(ryn::FlexDirection::RightToLeft)
                  .gap(ryn::dp(8))
                  .layout(ryn::LayoutStyle{}.width(ryn::dp(220)).height(ryn::dp(80))),
              ryn::FlexContent{[] {
                  for (const auto label : {u8"列 A", u8"列 B", u8"列 C"}) {
                      ryn::Button(ryn::ButtonProps{}, [label] {
                          ryn::Text(ryn::String::from_utf8(reinterpret_cast<const char*>(label)).value());
                      });
                  }
              }});
    remember_target(state, "gallery.flex.vertical-rtl");
    ryn::Text(sample_text(u8"gallery.flex.default-stretch · 自动高度 64 dp"));
    ryn::Flex(ryn::FlexProps{}.gap(ryn::dp(8)).layout(ryn::LayoutStyle{}.height(ryn::dp(64))), ryn::FlexContent{[] {
                  ryn::Text(u8"默认 Stretch");
                  ryn::Text(ryn::TextProps{}.content(u8"固定 22 dp").layout(ryn::LayoutStyle{}.height(ryn::dp(22))));
              }});
    remember_target(state, "gallery.flex.default-stretch");
    state->telemetry.live_samples += 4;
}

void add_space_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"Space · separator / baseline / Compact / mixed / Addon");
    Text(sample_text(u8"gallery.space.separator · 富分隔随内容交错布局、绘制和 Tab"));
    Space(SpaceProps{}
              .wrap(true)
              .separator(SpaceSeparator{[] {
                  Text(u8"/");
                  Button(ButtonProps{}.type(ButtonType::Text).size(ControlSize::Small), [] { Text(u8"分隔操作"); });
              }})
              .layout(LayoutStyle{}.width(state->document_width)),
          SpaceContent{[] {
              for (const auto label : {u8"第一项", u8"第二项", u8"第三项"}) {
                  Button(ButtonProps{},
                         [label] { Text(String::from_utf8(reinterpret_cast<const char*>(label)).value()); });
              }
          }});
    remember_target(state, "gallery.space.separator");
    Text(sample_text(u8"gallery.space.baseline · 大尺寸按钮、输入与文字对齐"));
    Space(SpaceProps{}.align(SpaceAlign::Baseline), SpaceContent{[] {
              Text(u8"Ag 基线");
              Button(ButtonProps{}.size(ControlSize::Large), [] { Text(u8"Ag 大按钮"); });
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
        SpaceCompact(SpaceCompactProps{}.size(size), SpaceCompactContent{[] {
                         Button(ButtonProps{}.variant(ButtonVariant::Dashed), [] { Text(u8"虚线"); });
                         Button(ButtonProps{}.type(ButtonType::Primary), [] { Text(u8"连接操作"); });
                     }});
        remember_target(state, size == ControlSize::Small   ? "gallery.space.compact.small"
                               : size == ControlSize::Large ? "gallery.space.compact.large"
                                                            : "gallery.space.compact.middle");
    }
    Text(sample_text(u8"gallery.space.compact.vertical-rtl · block / 纵向 / RTL"));
    SpaceCompact(SpaceCompactProps{}
                     .orientation(SpaceOrientation::Vertical)
                     .direction(FlexDirection::RightToLeft)
                     .block(true)
                     .layout(LayoutStyle{}.width(state->cell_width)),
                 SpaceCompactContent{[] {
                     Button(ButtonProps{}, [] { Text(u8"上方"); });
                     Button(ButtonProps{}.danger(true), [] { Text(u8"下方危险操作"); });
                 }});
    remember_target(state, "gallery.space.compact.vertical-rtl");
    Text(sample_text(u8"gallery.space.compact.mixed · Input / Password / Search / Radio / nested"));
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
                     RadioButton(RadioProps{}, RadioLabel{[] { Text(u8"选择"); }});
                     SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[] {
                                      Button(ButtonProps{}, [] { Text(u8"嵌套 A"); });
                                      Button(ButtonProps{}.disabled(true), [] { Text(u8"嵌套 B"); });
                                  }});
                 }});
    remember_target(state, "gallery.space.compact.mixed");
    Text(sample_text(u8"gallery.space.addon.variants · 四变体 / 错误与警告 / 禁用"));
    Space(SpaceProps{}.wrap(true).layout(LayoutStyle{}.width(state->document_width)), SpaceContent{[] {
              for (const auto variant :
                   {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
                  SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[variant] {
                                   SpaceAddon(SpaceAddonProps{}
                                                  .variant(variant)
                                                  .status(variant == InputVariant::Filled ? InputStatus::Error
                                                                                          : InputStatus::Warning)
                                                  .disabled(variant == InputVariant::Outlined),
                                              SpaceAddonContent{[] { Text(u8"附加"); }});
                                   Button(ButtonProps{}, [] { Text(u8"操作"); });
                               }});
              }
          }});
    remember_target(state, "gallery.space.addon.variants");
    state->telemetry.live_samples += 11;
}

void add_icon_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"Icon：848 个离线图标 / 三类 / 双色 / 旋转 / 自定义向量");
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
                     ButtonContent{[] { Text(u8"开始 / 停止 spin"); }});
              remember_target(state, "gallery.icon.toggle-spin");
              Button(ButtonProps{}.onClick(
                         [state] { state->icon_angle.set(std::fmod(state->icon_angle.get() + 45, 360.0F)); }),
                     ButtonContent{[] { Text(u8"旋转 45°"); }});
              remember_target(state, "gallery.icon.rotate");
              Button(ButtonProps{}.onClick([state] {
                  const auto step = ++state->icon_source_step % 3;
                  state->icon_source.set(step == 0   ? IconSource{IconName::HeartTwoTone}
                                         : step == 1 ? IconSource{IconName::WalletTwoTone}
                                                     : icon_vector_sample());
              }),
                     ButtonContent{[] { Text(u8"切换内置 / 自定义"); }});
              remember_target(state, "gallery.icon.change-source");
          }});
    state->telemetry.live_samples += 8;
}

void add_tabs_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"Tabs · 键盘导航 / 保留草稿 / 溢出滚动");
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
        Text(sample_text(label));
        Tabs(props.layout(LayoutStyle{}.width(state->cell_width)), std::move(items));
        const auto identity = String::from_utf8(std::u8string_view{caption}).value();
        remember_target(state, identity.bytes().substr(0, identity.bytes().find(" ·")));
        ++state->telemetry.live_samples;
    };
    const auto basic = [] {
        return std::vector<TabItem>{TabItem{u8"draft", u8"草稿", TabContent{[] { Text(u8"内容 / Content"); }}},
                                    TabItem{u8"settings", u8"设置", TabContent{[] { Text(u8"Settings content"); }}}};
    };
    sample(u8"gallery.tabs.controlled-retained · 受控 / 切换保留编辑",
           TabsProps{}.activeKey(state->tabs_active).onChange([state](String key) {
               state->tabs_active.set(std::move(key));
               ++state->telemetry.activations;
           }),
           {TabItem{
                u8"draft", u8"草稿", TabContent{[] {
                    Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[] {
                             Input(InputProps{}
                                       .defaultValue(gallery_initial_text("gallery.tabs.draft", u8"切换后保留 / Draft"))
                                       .onChange(gallery_value_changed<ryn::String>("gallery.tabs.draft"))
                                       .layout(LayoutStyle{}.width(dp(140))));
                             gallery_target("gallery.tabs.draft");
                             Select(
                                 gallery_select_default("gallery.tabs.draft-role",
                                                        SelectProps{}
                                                            .options({{1, u8"桌面"}, {2, u8"服务"}})

                                                            .layout(LayoutStyle{}.width(dp(140))),
                                                        ryn::SelectValue{1})
                                     .onChange(gallery_value_changed<ryn::SelectSelection>("gallery.tabs.draft-role")));
                             gallery_target("gallery.tabs.draft-role");
                         }});
                }}},
            TabItem{u8"settings", u8"设置", TabContent{[] {
                        Input(InputProps{}
                                  .defaultValue(gallery_initial_text("gallery.tabs.settings-draft", u8"另一份草稿"))
                                  .onChange(gallery_value_changed<ryn::String>("gallery.tabs.settings-draft"))
                                  .layout(LayoutStyle{}.width(dp(140))));
                        gallery_target("gallery.tabs.settings-draft");
                    }}},
            TabItem{u8"locked", u8"禁用", TabContent{[] { Text(u8"Locked"); }}}.disabled()});
    auto disabled = basic();
    disabled.push_back(TabItem{u8"locked", u8"禁用", TabContent{[] { Text(u8"Locked"); }}}.disabled());
    sample(u8"gallery.tabs.disabled", TabsProps{}, std::move(disabled));
    auto dynamic = basic();
    dynamic[1].label(state->tabs_label).disabled(state->tabs_disabled);
    sample(u8"gallery.tabs.dynamic-label · 响应标签 / 禁用", TabsProps{}, std::move(dynamic));
    sample(u8"gallery.tabs.card", TabsProps{}.type(TabsType::Card), basic());
    sample(u8"gallery.tabs.small", TabsProps{}.size(ControlSize::Small), basic());
    sample(u8"gallery.tabs.large", TabsProps{}.size(ControlSize::Large), basic());
    sample(u8"gallery.tabs.bottom", TabsProps{}.placement(TabsPlacement::Bottom), basic());
    sample(u8"gallery.tabs.bottom-card", TabsProps{}.type(TabsType::Card).placement(TabsPlacement::Bottom), basic());
    sample(u8"gallery.tabs.centered", TabsProps{}.centered(), basic());
    std::vector<TabItem> many;
    for (int i = 1; i <= 8; ++i) {
        auto key = String::from_utf8(std::to_string(i)).value();
        many.emplace_back(key, String::from_utf8("项目 / Tab " + std::to_string(i)).value(),
                          TabContent{[key] { Text(key); }});
    }
    sample(u8"gallery.tabs.overflow · 箭头 / 滚轮", TabsProps{}, std::move(many));
    sample(u8"gallery.tabs.empty", TabsProps{}, {});
    sample(u8"gallery.tabs.unknown-key · 未知 key 不显示面板", TabsProps{}.activeKey(u8"unknown"), basic());
    ThemeConfig custom;
    custom.tabs.algorithm = true;
    custom.tabs.seed.color_primary = Color::rgba8(114, 46, 209);
    custom.tabs.tokens.horizontal_gutter = dp(16);
    Theme(ThemeProps{}.config(custom), ThemeContent{[state, basic] {
              ++state->telemetry.theme_content_runs;
              Text(sample_text(u8"gallery.tabs.component-theme · 局部颜色 / 16 dp 间距"));
              Tabs(TabsProps{}.layout(LayoutStyle{}.width(state->cell_width)), basic());
              remember_target(state, "gallery.tabs.component-theme");
              ++state->telemetry.live_samples;
          }});
    Button(ButtonProps{}.onClick([state] {
        const bool disabled = !state->tabs_disabled.get();
        state->tabs_disabled.set(disabled);
        state->tabs_label.set(disabled ? String{u8"暂不可用"} : String{u8"设置 / Settings"});
        ++state->telemetry.activations;
    }),
           [] { Text(sample_text(u8"gallery.tabs.toggle-label · 切换标签 / 禁用")); });
    remember_target(state, "gallery.tabs.toggle-label");
    ++state->telemetry.live_samples;
}

void add_modal_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"Modal · 确认取消 / 保留编辑 / 正文滚动 / 嵌套");
    Text(TextProps{}.content(state->modal_feedback));
    Text(TextProps{}.content(state->modal_lifecycle));
    const auto sample = [state](int index, std::string_view identity, String title, ModalProps props,
                                ModalContent content, ModalSlots slots = {}) {
        Button(ButtonProps{}.onClick([state, index] {
            state->modal_active.set(index);
            ++state->telemetry.activations;
        }),
               ButtonContent{
                   [identity, title] { Text(utf8(std::string{identity} + " · " + std::string{title.bytes()})); }});
        remember_target(state, identity);
        props.open(Binding<bool>{[state, index] { return state->modal_active.get() == index; }})
            .title(title)
            .onOk([state, index] {
                ++state->telemetry.activations;
                state->modal_feedback.set(index == 2 ? String{u8"已收到确认请求，本例继续保持打开"}
                                                     : String{u8"已确认"});
                if (index != 2) {
                    state->modal_active.set(-1);
                }
            })
            .onCancel([state, index](ModalCancelReason reason) {
                ++state->telemetry.activations;
                const auto message = reason == ModalCancelReason::CancelButton  ? String{u8"取消来源：页脚按钮"}
                                     : reason == ModalCancelReason::CloseButton ? String{u8"取消来源：右上角关闭"}
                                     : reason == ModalCancelReason::Mask        ? String{u8"取消来源：遮罩"}
                                                                                : String{u8"取消来源：Escape"};
                state->modal_feedback.set(message);
                if (index != 2) {
                    state->modal_nested.set(false);
                    state->modal_active.set(-1);
                }
            })
            .afterOpenChange([state](bool shown) {
                state->modal_lifecycle.set(shown ? String{u8"对话框已显示"} : String{u8"对话框已隐藏"});
            })
            .afterClose([state] { state->modal_lifecycle.set(String{u8"对话框已关闭，输入和浮层已清理"}); });
        Modal(std::move(props), std::move(content), std::move(slots));
        remember_target(state, std::string{identity} + ".dialog");
        state->telemetry.live_samples += 2; // Dialog and its page trigger.
    };
    sample(0, "gallery.modal.basic", String{u8"基础确认"}, ModalProps{}, ModalContent{[] {
               Text(u8"确认、取消、右上角关闭、Escape或点击遮罩。");
               Text(u8"关闭后会返回原来的有效入口。");
           }});
    sample(1, "gallery.modal.retained-edit", String{u8"保留编辑草稿"}, ModalProps{}, ModalContent{[] {
               Text(u8"修改内容后关闭再打开，草稿和选项继续保留。");
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
               Select(gallery_select_default("gallery.modal.account-role",
                                             SelectProps{}
                                                 .options({{u8"desktop", u8"桌面"}, {u8"service", u8"服务"}})

                                                 .showSearch()
                                                 .allowClear(),
                                             ryn::SelectValue{u8"desktop"})
                          .onChange(gallery_value_changed<ryn::SelectSelection>("gallery.modal.account-role")));
               gallery_target("gallery.modal.account-role");
               Tooltip(TooltipProps{}
                           .title(String{u8"先按Escape关闭提示或菜单，再按一次取消对话框"})
                           .trigger(TooltipTriggerMode::Click),
                       TooltipTrigger{[] { Button(ButtonProps{}, [] { Text(u8"输入帮助"); }); }});
           }});
    sample(2, "gallery.modal.controlled-refusal", String{u8"父级保留打开状态"}, ModalProps{}, ModalContent{[state] {
               Text(u8"确认与取消只发送请求。本例不会回写关闭状态。");
               Text(TextProps{}.content(state->modal_feedback));
               Button(ButtonProps{}.onClick([state] { state->modal_active.set(-1); }), [] { Text(u8"关闭本例"); });
           }});
    sample(3, "gallery.modal.confirm-loading", String{u8"确认加载中"},
           ModalProps{}.confirmLoading(state->modal_loading), ModalContent{[state] {
               Flex(
                   FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state] {
                       Text(u8"确认在加载时禁用，取消仍可使用。");
                       Button(
                           ButtonProps{}
                               .layout(
                                   LayoutStyle{}.margin_left(dp(4)).margin_top(dp(4)).margin_right(dp(4)).margin_bottom(
                                       dp(4)))
                               .onClick([state] { state->modal_loading.set(!state->modal_loading.get()); }),
                           [] { Text(u8"切换确认加载状态"); });
                   }});
           }});
    sample(4, "gallery.modal.custom-footer", String{u8"自定义标题与页脚"}, ModalProps{}.footer(ModalFooterMode::None),
           ModalContent{[] { Text(u8"标题和页脚使用独立内容插槽。"); }},
           ModalSlots{.title = ModalTitle{[] {
                          Flex(FlexProps{}.gap(dp(8)).align(FlexAlign::Center), FlexContent{[] {
                                   Icon(IconProps{}.name(IconName::EditOutlined));
                                   Text(u8"自定义标题");
                               }});
                      }},
                      .footer = ModalFooter{[state] {
                          Flex(FlexProps{}.gap(dp(8)).justify(FlexJustify::End), FlexContent{[state] {
                                   Button(ButtonProps{}.onClick([state] { state->modal_active.set(-1); }),
                                          [] { Text(u8"返回"); });
                                   Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                                       state->modal_feedback.set(String{u8"自定义页脚：已保存"});
                                       state->modal_active.set(-1);
                                   }),
                                          [] { Text(u8"保存草稿"); });
                               }});
                      }}});
    sample(5, "gallery.modal.no-footer", String{u8"无页脚"}, ModalProps{}.footer(ModalFooterMode::None),
           ModalContent{[] { Text(u8"使用右上角关闭、Escape或点击遮罩取消。"); }});
    sample(6, "gallery.modal.maskless", String{u8"无遮罩颜色"}, ModalProps{}.mask(ModalMask{false, true}),
           ModalContent{[] { Text(u8"背景仍不可操作。点击对话框外部可以关闭。"); }});
    sample(7, "gallery.modal.centered", String{u8"垂直居中"}, ModalProps{}.centered(),
           ModalContent{[] { Text(u8"调整窗口大小，观察对话框在有限视口内居中。"); }});
    sample(8, "gallery.modal.long-body", String{u8"长正文与焦点显露"}, ModalProps{}, ModalContent{[state] {
               Button(ButtonProps{}.onClick([state] { state->modal_active.set(-1); }), [] { Text(u8"完成阅读"); });
               Text(u8"使用滚轮浏览，或按Tab移动到末尾编辑框。");
               for (int row = 1; row <= 28; ++row) {
                   Text(utf8("第 " + std::to_string(row) + " 段 · 标题与页脚固定，正文保留并独立滚动。"));
               }
               Input(InputProps{}
                         .defaultValue(gallery_initial_text("gallery.modal.end-draft", u8"末尾保留的输入"))
                         .onChange(gallery_value_changed<ryn::String>("gallery.modal.end-draft")));
               gallery_target("gallery.modal.end-draft");
               Select(SelectProps{}.options({{1, u8"末尾选项一"}, {2, u8"末尾选项二"}}));
               Tabs(TabsProps{}, {TabItem{u8"first", u8"保留面板一", TabContent{[] { Text(u8"面板一"); }}},
                                  TabItem{u8"second", u8"保留面板二", TabContent{[] { Text(u8"面板二"); }}}});
           }});
    sample(9, "gallery.modal.nested", String{u8"嵌套对话框"}, ModalProps{}, ModalContent{[state] {
               Input(InputProps{}
                         .defaultValue(gallery_initial_text("gallery.modal.parent-draft", u8"父层保留的草稿"))
                         .onChange(gallery_value_changed<ryn::String>("gallery.modal.parent-draft")));
               gallery_target("gallery.modal.parent-draft");
               Button(ButtonProps{}.onClick([state] { state->modal_nested.set(true); }), [] { Text(u8"打开下一层"); });
               Modal(ModalProps{}
                         .open(state->modal_nested)
                         .title(u8"二级确认")
                         .onOk([state] { state->modal_nested.set(false); })
                         .onCancel([state](ModalCancelReason) { state->modal_nested.set(false); }),
                     ModalContent{[] {
                         Text(sample_text(u8"gallery.modal.nested-child · 关闭后返回父层有效入口"));
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
              sample(10, "gallery.modal.component-theme", String{u8"局部紫色与圆角"}, ModalProps{},
                     ModalContent{[] { Text(u8"局部主题使用紫色焦点和12 dp圆角，跟随外部明暗主题。"); }});
          }});
}

void add_tag_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"Tag / Badge · 状态、选择与计数");
    Text(TextProps{}.content(state->common_feedback));
    Space(SpaceProps{}.wrap(true).size(dp(12)), SpaceContent{[state] {
              Tag(TagProps{}, TagContent{[] { Text(u8"默认标签"); }});
              Tag(TagProps{}.color(Status::Success), TagContent{[] { Text(u8"运行正常"); }});
              Tag(TagProps{}.color(PresetColor::Blue).closable().visible(state->tag_visible).onClose([state] {
                  state->tag_visible.set(false);
              }),
                  TagContent{[] { Text(sample_text(u8"gallery.tag.closable · 关闭标签")); }});
              CheckableTag(CheckableTagProps{}.checked(state->tag_checked).onChange([state](bool checked) {
                  state->tag_checked.set(checked);
              }),
                           TagContent{[] { Text(sample_text(u8"gallery.tag.checkable · 选择标签")); }});
              Button(ButtonProps{}.onClick([state] { state->tag_visible.set(true); }),
                     [] { Text(sample_text(u8"gallery.tag.restore · 恢复标签")); });
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
                               [] { Text(sample_text(u8"gallery.badge.clear · 已读")); });
                        gallery_target("gallery.badge.clear");
                    }});
              Badge(BadgeProps{}.count(120));
              Badge(BadgeProps{}.count(0).showZero());
              Badge(BadgeProps{}.dot(), BadgeContent{[] { Button(ButtonProps{}, [] { Text(u8"圆点通知"); }); }});
              Badge(BadgeProps{}.status(Status::Success).text(u8"服务正常"));
              Button(ButtonProps{}.onClick([state] { state->badge_count.set(state->badge_count.get() + 1); }),
                     [] { Text(sample_text(u8"gallery.badge.increment · 添加通知")); });
              gallery_target("gallery.badge.increment");
          }});
    state->telemetry.live_samples += 5;
}

void add_drawer_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    if (state->register_targets) {
        Text(TextProps{}.content(state->common_feedback));
    }
    Text(u8"Drawer · 四边停靠 / 保留草稿 / 正文滚动");
    Space(SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
              for (auto placement :
                   {DrawerPlacement::Right, DrawerPlacement::Left, DrawerPlacement::Top, DrawerPlacement::Bottom}) {
                  const auto label = placement == DrawerPlacement::Right  ? u8"gallery.drawer.right · 从右打开"
                                     : placement == DrawerPlacement::Left ? u8"gallery.drawer.left · 从左打开"
                                     : placement == DrawerPlacement::Top  ? u8"gallery.drawer.top · 从上打开"
                                                                          : u8"gallery.drawer.bottom · 从下打开";
                  Button(ButtonProps{}.onClick([state, placement] {
                      state->drawer_placement.set(placement);
                      state->drawer_open.set(true);
                  }),
                         [label] { Text(String::from_utf8(std::u8string_view{label}).value()); });
              }
          }});
    Drawer(DrawerProps{}
               .open(state->drawer_open)
               .placement(state->drawer_placement)
               .width(420)
               .height(340)
               .title(u8"编辑账户 / Edit account")
               .onCancel([state](DrawerCancelReason) { state->drawer_open.set(false); }),
           DrawerContent{[state] {
               Input(InputProps{}
                         .defaultValue(
                             gallery_initial_text("gallery.drawer.draft", u8"gallery.drawer.draft · 保留的抽屉草稿"))
                         .onChange(gallery_value_changed<ryn::String>("gallery.drawer.draft")));
               gallery_target("gallery.drawer.draft");
               TextArea(TextAreaProps{}
                            .defaultValue(gallery_initial_text("gallery.drawer.notes",
                                                               u8"抽屉中的多行备注\n关闭后重新打开仍然保留"))
                            .onChange(gallery_value_changed<ryn::String>("gallery.drawer.notes")));
               gallery_target("gallery.drawer.notes");
               Select(gallery_select_default("gallery.drawer.role",
                                             SelectProps{}.options({{1, u8"桌面"}, {2, u8"服务"}}), ryn::SelectValue{1})
                          .onChange(gallery_value_changed<ryn::SelectSelection>("gallery.drawer.role")));
               gallery_target("gallery.drawer.role");
               Popconfirm(PopconfirmProps{}.title(u8"保存当前草稿？").onConfirm([state] {
                   state->common_feedback.set(String{u8"抽屉草稿已确认保存"});
               }),
                          PopconfirmTrigger{[] {
                              Button(ButtonProps{}, [] { Text(sample_text(u8"gallery.drawer.confirm · 确认保存")); });
                              gallery_target("gallery.drawer.confirm");
                          }});
               for (int row = 1; row <= 20; ++row) {
                   Text(utf8("抽屉正文 " + std::to_string(row) + " · 标题和页脚固定，正文独立滚动。"));
               }
               Input(InputProps{}
                         .defaultValue(gallery_initial_text("gallery.drawer.end", u8"gallery.drawer.end · 末尾编辑"))
                         .onChange(gallery_value_changed<ryn::String>("gallery.drawer.end")));
               gallery_target("gallery.drawer.end");
           }},
           {.footer = DrawerFooter{[state] {
                Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] { state->drawer_open.set(false); }),
                       [] { Text(sample_text(u8"gallery.drawer.done · 完成并关闭")); });
                gallery_target("gallery.drawer.done");
            }}});
    state->telemetry.live_samples += 1;
}

void add_popconfirm_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    if (state->register_targets) {
        Text(TextProps{}.content(state->common_feedback));
    }
    Text(u8"Popconfirm · 确认 / 加载 / 取消");
    Popconfirm(PopconfirmProps{}
                   .open(state->popconfirm_open)
                   .confirmLoading(state->popconfirm_loading)
                   .title(u8"执行这项操作？")
                   .onOpenChange([state](bool open) {
                       state->popconfirm_open.set(open);
                       if (!open) {
                           state->popconfirm_loading.set(false);
                       }
                   })
                   .onConfirm([state] { state->popconfirm_loading.set(true); })
                   .onCancel([state] { state->common_feedback.set(String{u8"已取消操作"}); }),
               PopconfirmTrigger{[] {
                   Button(ButtonProps{}, [] { Text(sample_text(u8"gallery.popconfirm.controlled · 执行操作")); });
                   gallery_target("gallery.popconfirm.controlled");
               }},
               {.description = PopconfirmDescription{[state] {
                    Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state] {
                             Text(u8"点击确定进入加载状态，再点击下方按钮完成。");
                             Button(ButtonProps{}.onClick([state] {
                                 state->popconfirm_loading.set(false);
                                 state->popconfirm_open.set(false);
                                 state->common_feedback.set(String{u8"操作已完成"});
                             }),
                                    [] { Text(sample_text(u8"gallery.popconfirm.finish · 完成操作")); });
                             gallery_target("gallery.popconfirm.finish");
                         }});
                }}});
    Popconfirm(PopconfirmProps{}.title(u8"只显示确认按钮").showCancel(false).onConfirm([state] {
        state->badge_count.set(state->badge_count.get() + 1);
    }),
               PopconfirmTrigger{[] {
                   Button(ButtonProps{}, [] { Text(sample_text(u8"gallery.popconfirm.single · 快速确认")); });
                   gallery_target("gallery.popconfirm.single");
               }});
    state->telemetry.live_samples += 2;
}

void add_alert_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    if (state->register_targets) {
        Text(TextProps{}.content(state->common_feedback));
    }
    Text(u8"Alert / Spin / Progress · 反馈与进度");
    Alert(AlertProps{}
              .status(Status::Warning)
              .message(u8"连接不稳定")
              .description(u8"请检查连接后重试，也可以关闭此提示。")
              .showIcon()
              .visible(state->alert_visible)
              .closable()
              .onClose([state] { state->alert_visible.set(false); }),
          {.action = AlertAction{[state] {
               Button(ButtonProps{}.onClick([state] { state->common_feedback.set(String{u8"已请求重新连接"}); }),
                      [] { Text(sample_text(u8"gallery.alert.retry · 重试")); });
               gallery_target("gallery.alert.retry");
           }}});
    Alert(AlertProps{}.status(Status::Success).message(u8"数据已保存").showIcon());
    Alert(AlertProps{}.status(Status::Error).message(u8"操作失败，请稍后重试").showIcon());
    Button(ButtonProps{}.onClick([state] { state->alert_visible.set(true); }),
           [] { Text(sample_text(u8"gallery.alert.restore · 恢复提示")); });
    gallery_target("gallery.alert.restore");
    state->telemetry.live_samples += 3;
}

void add_spin_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    if (state->register_targets) {
        Text(TextProps{}.content(state->common_feedback));
    }
    Button(ButtonProps{}.onClick([state] { state->feedback_loading.set(!state->feedback_loading.get()); }),
           [] { Text(sample_text(u8"gallery.spin.toggle · 切换加载")); });
    gallery_target("gallery.spin.toggle");
    Spin(SpinProps{}.spinning(state->feedback_loading).description(u8"正在读取"), SpinContent{[state] {
             Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state] {
                      Input(InputProps{}
                                .defaultValue(gallery_initial_text("gallery.spin.draft",
                                                                   u8"gallery.spin.draft · 加载前后保留内容"))
                                .onChange(gallery_value_changed<ryn::String>("gallery.spin.draft")));
                      gallery_target("gallery.spin.draft");
                      Button(ButtonProps{}.onClick(
                                 [state] { state->common_feedback.set(String{u8"加载区域中的操作已执行"}); }),
                             [] { Text(sample_text(u8"gallery.spin.action · 内容操作")); });
                      gallery_target("gallery.spin.action");
                  }});
         }});
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
           [] { Text(sample_text(u8"gallery.progress.advance · 推进进度")); });
    gallery_target("gallery.progress.advance");
    Button(ButtonProps{}.onClick([state] {
        state->feedback_status.set(state->feedback_status.get() == ProgressStatus::Exception
                                       ? ProgressStatus::Normal
                                       : ProgressStatus::Exception);
    }),
           [] { Text(sample_text(u8"gallery.progress.status · 切换异常状态")); });
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
    Text(sample_text(u8"gallery.table.editable · Table / Pagination · 编辑、排序、选择与分页"));
    Text(TextProps{}.content(state->data_feedback));
    Text(TextProps{}.content(bind([state] {
        return utf8("当前第 " + std::to_string(state->table_page.get()) + " 页，已选 " +
                    std::to_string(state->table_selected.get().size()) + " 项");
    })));
    Space(SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
              Button(ButtonProps{}.onClick([state] {
                  auto rows = state->records.get();
                  if (!rows.empty()) {
                      rows.front().name = String{u8"已更新任务"};
                      state->records.set(std::move(rows));
                  }
              }),
                     [] { Text(sample_text(u8"gallery.table.update · 更新首行")); });
              gallery_target("gallery.table.update");
              Button(ButtonProps{}.onClick([state] {
                  auto rows = state->records.get();
                  std::ranges::reverse(rows);
                  state->records.set(std::move(rows));
              }),
                     [] { Text(sample_text(u8"gallery.table.reverse · 反转数据顺序")); });
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
                     [] { Text(sample_text(u8"gallery.table.remove · 删除末行")); });
              gallery_target("gallery.table.remove");
              Button(ButtonProps{}.onClick([state] {
                  state->records.set(gallery_records());
                  state->table_page.set(1);
                  state->table_sort.set(TableSort{});
                  state->table_selected.set({});
              }),
                     [] { Text(sample_text(u8"gallery.table.restore · 恢复数据")); });
              gallery_target("gallery.table.restore");
              Button(ButtonProps{}.onClick([state] { state->table_loading.set(!state->table_loading.get()); }),
                     [] { Text(sample_text(u8"gallery.table.loading · 切换加载")); });
              gallery_target("gallery.table.loading");
          }});
    Table(TableProps<GalleryRecord>{}
              .dataSource(state->records)
              .rowKey([](const GalleryRecord& row) { return row.key; })
              .columns(
                  {TableColumn<GalleryRecord>{u8"name", u8"任务",
                                              [](TableCell<GalleryRecord> cell) {
                                                  Text(TextProps{}.content(bind([cell] { return cell.value().name; })));
                                              }}
                       .width(220),
                   TableColumn<GalleryRecord>{u8"quantity", u8"数量",
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
                       .width(160)
                       .sorter([](const GalleryRecord& a, const GalleryRecord& b) {
                           return a.quantity.value_or(0) < b.quantity.value_or(0);
                       }),
                   TableColumn<GalleryRecord>{u8"status", u8"状态",
                                              [](TableCell<GalleryRecord> cell) {
                                                  Tag(TagProps{}.color(bind([cell] {
                                                      return TagColor{cell.value().locked ? Status::Warning
                                                                                          : Status::Success};
                                                  })),
                                                      TagContent{[cell] {
                                                          Text(TextProps{}.content(bind([cell] {
                                                              return cell.value().locked ? String{u8"已锁定"}
                                                                                         : String{u8"可编辑"};
                                                          })));
                                                      }});
                                              }}})
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
          TableEmpty{[] { Text(u8"暂无记录，点击恢复数据继续体验。"); }});
    remember_target(state, "gallery.table.editable");
    ++state->telemetry.live_samples;
}

void add_pagination_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    const auto identity =
        state->page_component == "ant.component.table" ? "gallery.table.pagination" : "gallery.pagination.controlled";
    Text(state->register_targets ? String{u8"每页四项"} : String{u8"gallery.pagination.controlled · 每页四项"});
    Pagination(PaginationProps{}
                   .total(bind([state] { return state->records.get().size(); }))
                   .current(state->table_page)
                   .pageSize(std::size_t{4})
                   .hideOnSinglePage()
                   .onChange([state](std::size_t page, std::size_t) { state->table_page.set(page); }));
    remember_target(state, identity);
    ++state->telemetry.live_samples;
}

void add_menu_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(sample_text(u8"gallery.menu.inline · Menu / Dropdown · 导航与命令"));
    Text(TextProps{}.content(state->data_feedback));
    Menu(MenuProps{}
             .mode(MenuMode::Inline)
             .items({{u8"overview", u8"概览"},
                     {u8"tasks", state->menu_label},
                     {u8"tools",
                      u8"工具",
                      false,
                      false,
                      MenuItemKind::SubMenu,
                      {{u8"logs", u8"日志"}, {u8"settings", u8"设置"}, {u8"unavailable", u8"不可用", true}}},
                     {u8"divider", u8"", false, false, MenuItemKind::Divider},
                     {u8"reset", u8"重置", false, true}})
             .selectedKeys(state->menu_selected)
             .openKeys(state->menu_open)
             .disabled(state->menu_disabled)
             .onSelect([state](MenuSelectInfo info) { state->menu_selected.set(std::move(info.selected_keys)); })
             .onOpenChange([state](MenuKeys keys) { state->menu_open.set(std::move(keys)); })
             .onClick([state](MenuClickInfo info) {
                 state->data_feedback.set(utf8("已选择菜单：" + std::string{info.key.bytes()}));
             })
             .layout(LayoutStyle{}.width(dp(260)).height(dp(280))));
    remember_target(state, "gallery.menu.inline");
    Space(SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
              Button(ButtonProps{}.onClick([state] {
                  state->menu_label.set(state->menu_label.get() == String{u8"任务"} ? String{u8"任务 / 已更新"}
                                                                                    : String{u8"任务"});
              }),
                     [] { Text(sample_text(u8"gallery.menu.label · 更新菜单文字")); });
              remember_target(state, "gallery.menu.label");
              Button(ButtonProps{}.onClick([state] { state->menu_disabled.set(!state->menu_disabled.get()); }),
                     [] { Text(sample_text(u8"gallery.menu.disabled · 切换菜单禁用")); });
              remember_target(state, "gallery.menu.disabled");
          }});
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
                           .items({{u8"refresh", u8"刷新数据"},
                                   {u8"clear-selection", u8"清空选择"},
                                   {u8"disabled", u8"不可用命令", true}})
                           .onClick([state](MenuClickInfo info) {
                               if (info.key == String{u8"refresh"}) {
                                   state->records.set(gallery_records());
                                   state->table_page.set(1);
                               } else if (info.key == String{u8"clear-selection"}) {
                                   state->table_selected.set({});
                               }
                               state->data_feedback.set(utf8("已执行命令：" + std::string{info.key.bytes()}));
                           })),
             DropdownTrigger{[] {
                 Button(ButtonProps{}, [] { Text(sample_text(u8"gallery.dropdown.actions · 点击或右键打开命令")); });
             }});
    remember_target(state, "gallery.dropdown.actions");
    ++state->telemetry.live_samples;
}

void add_input_number_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"InputNumber · 小数、范围、禁用与只读");
    Text(TextProps{}.content(state->data_feedback));
    Space(
        SpaceProps{}.wrap(true).size(dp(16)), SpaceContent{[state] {
            Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[state] {
                     Text(sample_text(u8"gallery.number.controlled · 受控小数 0..10"));
                     InputNumber(InputNumberProps{}
                                     .value(state->number_value)
                                     .min(0)
                                     .max(10)
                                     .step(0.1)
                                     .precision(2)
                                     .ref(state->number_ref)
                                     .onChange([state](InputNumberValue value) { state->number_value.set(value); })
                                     .onSubmit([state](InputNumberValue value) {
                                         state->data_feedback.set(value ? utf8("已提交数值：" + std::to_string(*value))
                                                                        : String{u8"已提交空值"});
                                     }));
                 }});
            Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[state] {
                     Text(sample_text(u8"gallery.number.disabled · 禁用切换"));
                     InputNumber(InputNumberProps{}
                                     .defaultValue(gallery_initial_value<ryn::InputNumberValue>(
                                         "gallery.number.disabled", ryn::InputNumberValue{5}))
                                     .onChange(gallery_value_changed<ryn::InputNumberValue>("gallery.number.disabled"))
                                     .disabled(state->number_disabled)
                                     .size(ControlSize::Small));
                 }});
            Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[] {
                     Text(sample_text(u8"gallery.number.readonly · 只读"));
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
                     [] { Text(sample_text(u8"gallery.number.toggle-disabled · 切换数值禁用")); });
              gallery_target("gallery.number.toggle-disabled");
              Button(ButtonProps{}.onClick([state] { static_cast<void>(state->number_ref.focus()); }),
                     [] { Text(sample_text(u8"gallery.number.focus · 聚焦小数编辑")); });
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

void add_collapse_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(sample_text(u8"gallery.collapse.retained · Collapse · 保留编辑与独立操作"));
    Text(TextProps{}.content(state->panel_feedback));
    Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)), SpaceContent{[state] {
              Button(
                  ButtonProps{}.onClick([state] { state->collapse_accordion.set(!state->collapse_accordion.get()); }),
                  [] { Text(sample_text(u8"gallery.collapse.accordion · 切换手风琴模式")); });
              gallery_target("gallery.collapse.accordion");
              Button(ButtonProps{}.onClick([state] { state->collapse_disabled.set(!state->collapse_disabled.get()); }),
                     [] { Text(sample_text(u8"gallery.collapse.disabled · 切换高级面板禁用")); });
              gallery_target("gallery.collapse.disabled");
              Button(ButtonProps{}.onClick([state] { state->collapse_keys.set({}); }),
                     [] { Text(sample_text(u8"gallery.collapse.close · 收起全部")); });
              gallery_target("gallery.collapse.close");
              Button(ButtonProps{}.onClick([state] {
                  state->collapse_keys.set(CollapseKeys{String{u8"general"}});
                  state->collapse_select_open.set(true);
              }),
                     [] { Text(sample_text(u8"gallery.collapse.options · 展示面板内选项")); });
              gallery_target("gallery.collapse.options");
          }});
    Collapse(
        CollapseProps{}
            .activeKeys(state->collapse_keys)
            .accordion(state->collapse_accordion)
            .onChange([state](CollapseKeys keys) { state->collapse_keys.set(std::move(keys)); }),
        {CollapsePanel{
             u8"general", CollapseTitle{[] { Text(u8"基本设置"); }}, CollapseBody{[state] {
                 Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state] {
                          Input(InputProps{}
                                    .defaultValue(gallery_initial_text("gallery.collapse.draft",
                                                                       u8"gallery.collapse.draft · 保留的设置草稿"))
                                    .onChange(gallery_value_changed<ryn::String>("gallery.collapse.draft")));
                          gallery_target("gallery.collapse.draft");
                          Select(
                              gallery_select_default(
                                  "gallery.collapse.selection",
                                  SelectProps{}
                                      .options({{u8"draft", u8"草稿"}, {u8"service", u8"服务"}})

                                      .open(state->collapse_select_open)
                                      .onOpenChange([state](bool open) { state->collapse_select_open.set(open); }),
                                  ryn::SelectValue{u8"draft"})
                                  .onChange(gallery_value_changed<ryn::SelectSelection>("gallery.collapse.selection")));
                      }});
             }}}
             .extra(CollapseExtra{[state] {
                 Button(ButtonProps{}.onClick([state] { state->panel_feedback.set(String{u8"已执行独立帮助操作"}); }),
                        [] { Text(sample_text(u8"gallery.collapse.help · 帮助")); });
                 gallery_target("gallery.collapse.help");
             }}),
         CollapsePanel{u8"advanced", CollapseTitle{[] { Text(u8"高级设置"); }}, CollapseBody{[] {
                           Input(InputProps{}
                                     .defaultValue(gallery_initial_text("gallery.collapse.advanced",
                                                                        u8"gallery.collapse.advanced · 高级草稿"))
                                     .onChange(gallery_value_changed<ryn::String>("gallery.collapse.advanced")));
                           gallery_target("gallery.collapse.advanced");
                       }}}
             .disabled(state->collapse_disabled)});
    state->telemetry.live_samples += 1;
}

void add_segmented_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(sample_text(u8"gallery.segmented.controlled · Segmented · 横向等宽视图"));
    Text(TextProps{}.content(
        bind([state] { return utf8("当前视图：" + std::string{state->segment_value.get().bytes()}); })));
    Segmented(SegmentedProps{}
                  .value(state->segment_value)
                  .block()
                  .layout(LayoutStyle{}.width(state->document_width))
                  .onChange([state](String value) { state->segment_value.set(std::move(value)); }),
              {SegmentedOption{u8"day", u8"日视图"}.label(state->segment_label).icon(SegmentedIcon{[] {
                   Icon(IconProps{}.name(IconName::SearchOutlined));
               }}),
               SegmentedOption{u8"week", u8"周视图"}.disabled(state->segment_disabled),
               {u8"month", u8"月视图"}});
    Text(sample_text(u8"gallery.segmented.vertical · 纵向小尺寸，共享受控值"));
    Segmented(SegmentedProps{}
                  .value(state->segment_value)
                  .vertical()
                  .size(ControlSize::Small)
                  .layout(LayoutStyle{}.width(dp(240)))
                  .onChange([state](String value) { state->segment_value.set(std::move(value)); }),
              {{u8"day", u8"日视图"},
               SegmentedOption{u8"week", u8"周视图"}.disabled(state->segment_disabled),
               {u8"month", u8"月视图"}});
    Text(sample_text(u8"gallery.segmented.rich · 大尺寸富标签"));
    Segmented(SegmentedProps{}.size(ControlSize::Large),
              {SegmentedOption{u8"list", u8"列表"}.content(SegmentedLabel{[] {
                   Flex(FlexProps{}.gap(dp(6)).align(FlexAlign::Center), FlexContent{[] {
                            Icon(IconProps{}.name(IconName::SearchOutlined));
                            Text(u8"列表");
                        }});
               }}),
               {u8"detail", u8"详情"}});
    Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)), SpaceContent{[state] {
              Button(ButtonProps{}.onClick([state] {
                  state->segment_label.set(state->segment_label.get() == String{u8"日视图"}
                                               ? String{u8"日视图 / 已更新"}
                                               : String{u8"日视图"});
              }),
                     [] { Text(sample_text(u8"gallery.segmented.label · 更新分段标签")); });
              gallery_target("gallery.segmented.label");
              Button(ButtonProps{}.onClick([state] { state->segment_disabled.set(!state->segment_disabled.get()); }),
                     [] { Text(sample_text(u8"gallery.segmented.disabled · 切换周视图禁用")); });
              gallery_target("gallery.segmented.disabled");
          }});
    state->telemetry.live_samples += 4;
    state->telemetry.live_samples += 3;
}

void add_panel_and_selection_samples(const std::shared_ptr<GalleryState>& state) {
    const auto before = state->telemetry.live_samples;
    add_collapse_samples(state);
    add_segmented_samples(state);
    state->telemetry.live_samples = before + 5; // Historical combined fixture counter.
}

void add_empty_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(sample_text(u8"gallery.empty.default · Empty · 空状态与创建操作"));
    Empty(EmptyProps{}.description(state->empty_description).image(state->empty_image),
          {.extra = EmptyExtra{[state] {
               Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                   state->empty_description.set(String{u8"已创建示例任务，可继续编辑详情"});
               }),
                      [] { Text(sample_text(u8"gallery.empty.create · 创建示例任务")); });
               gallery_target("gallery.empty.create");
           }}});
    Button(ButtonProps{}.onClick([state] {
        state->empty_image.set(state->empty_image.get() == EmptyImage::None ? EmptyImage::Default : EmptyImage::None);
    }),
           [] { Text(sample_text(u8"gallery.empty.image · 切换空状态图示")); });
    gallery_target("gallery.empty.image");
    Text(sample_text(u8"gallery.empty.compact · 紧凑空状态"));
    Empty(EmptyProps{}.image(EmptyImage::Compact).description(u8"暂无筛选结果，请调整查询条件"));
    Text(sample_text(u8"gallery.empty.custom · 自定义图示与外边距"));
    Empty(EmptyProps{}.description(u8"自定义图示使用原生 Icon"),
          {.image = EmptyImageContent{[] {
               Icon(IconProps{}
                        .name(IconName::SearchOutlined)
                        .layout(LayoutStyle{}.width(dp(100)).margin_left(dp(50)).margin_right(dp(50))));
           }}});
    state->telemetry.live_samples += 3;
}

void add_result_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(sample_text(u8"gallery.result.controlled · Result · 状态、详情与保留草稿"));
    Text(TextProps{}.content(state->result_feedback));
    Result(ResultProps{}
               .status(state->result_status)
               .title(state->result_title)
               .subtitle(u8"切换状态后继续编辑同一份草稿"),
           {.content = ResultContent{[] {
                Text(u8"任务详情：编辑草稿后可切换上方结果状态。");
                Input(InputProps{}
                          .defaultValue(gallery_initial_text("gallery.result.draft",
                                                             u8"gallery.result.draft · 保留的结果详情草稿"))
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
                    constexpr std::array titles{u8"任务已保存",        u8"任务需要确认",       u8"任务保存失败",
                                                u8"任务处理中",        u8"403 · 暂无访问权限", u8"404 · 未找到任务",
                                                u8"500 · 服务暂不可用"};
                    state->result_status_step = (state->result_status_step + 1) % statuses.size();
                    state->result_status.set(statuses[state->result_status_step]);
                    state->result_title.set(String::from_utf8(titles[state->result_status_step]).value());
                }),
                       [] { Text(sample_text(u8"gallery.result.status · 切换结果状态")); });
                gallery_target("gallery.result.status");
                Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                    state->result_feedback.set(String{u8"已执行结果页继续操作"});
                }),
                       [] { Text(sample_text(u8"gallery.result.action · 继续操作")); });
                gallery_target("gallery.result.action");
            }}});
    Text(sample_text(u8"gallery.result.exception · 404 异常结果"));
    Result(ResultProps{}
               .status(ResultException::NotFound)
               .title(u8"404 · 页面未找到")
               .subtitle(u8"请检查任务地址或返回列表"));
    Text(sample_text(u8"gallery.result.custom · 自定义结果图标"));
    Result(ResultProps{}.title(u8"自定义结果展示").subtitle(u8"图示可通过 typed slot 组合现有控件"),
           {.icon = ResultIcon{[] {
                Flex(FlexProps{}.gap(dp(8)).align(FlexAlign::Center), FlexContent{[] {
                         Icon(IconProps{}.name(IconName::CheckCircleFilled));
                         Text(u8"READY");
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

void add_form_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    const auto form_width = bind([state] { return dp(state->document_width.get().value() - 2); });
    Text(TextProps{}
             .content(sample_text(String{u8"gallery.form.workflow · Form · 填写、校验与保存"}))
             .layout(LayoutStyle{}.margin_left(dp(1))));
    Text(TextProps{}
             .content(bind([state] {
                 return sample_text(state,
                                    utf8("gallery.form.status · " + std::string{state->form_feedback.get().bytes()} +
                                         (state->form_disabled.get() ? " · 已禁用" : " · 已启用") +
                                         (state->form_read_only.get() ? " · 只读" : " · 可编辑")));
             }))
             .layout(LayoutStyle{}.margin_left(dp(1))));
    Form(FormProps{}
             .ref(state->form_ref)
             .disabled(state->form_disabled)
             .readOnly(state->form_read_only)
             .layout(LayoutStyle{}.width(form_width).margin_left(dp(1)).margin_right(dp(1)))
             .onFinish([state](FormResult result) {
                 std::string values = "保存成功";
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
                                     values += "empty";
                                 }
                             } else if constexpr (std::same_as<T, bool>) {
                                 values += value ? "true" : "false";
                             } else if (value) {
                                 values += std::get<String>(value->storage()).bytes();
                             }
                         },
                         field.value);
                 }
                 state->form_feedback.set(utf8(values));
             })
             .onFinishFailed([state](FormResult result) {
                 state->form_feedback.set(utf8("校验失败 · " + std::to_string(result.errors.size()) + " 项错误"));
             }),
         FormContent{[state, form_width] {
             FormItem(FormItemProps{}
                          .field(u8"name", state->form_name)
                          .label(sample_text(String{u8"gallery.form.name · 名称"}))
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
                      }});
             FormItem(FormItemProps{}
                          .field(u8"role", state->form_role)
                          .label(sample_text(String{u8"gallery.form.role · 角色"}))
                          .control(state->form_role_ref),
                      FormItemContent{[state, form_width] {
                          Select(SelectProps{}
                                     .options({{u8"operator", u8"操作员"}, {u8"reviewer", u8"审核员"}})
                                     .value(state->form_role.value())
                                     .onChange(state->form_role.onChange())
                                     .status(state->form_role.status())
                                     .disabled(state->form_role.uneditable())
                                     .ref(state->form_role_ref)
                                     .layout(LayoutStyle{}.width(form_width)));
                      }});
             FormItem(FormItemProps{}
                          .field(u8"count", state->form_count)
                          .label(sample_text(String{u8"gallery.form.count · 数量"}))
                          .help(String{u8"必填，且至少为 1；重置会清除未完成数字草稿"})
                          .control(state->form_count_ref),
                      FormItemContent{[state, form_width] {
                          InputNumber(InputNumberProps{}
                                          .value(state->form_count.value())
                                          .onChange(state->form_count.onChange())
                                          .status(state->form_count.status())
                                          .disabled(state->form_count.disabled())
                                          .readOnly(state->form_count.readOnly())
                                          .ref(state->form_count_ref)
                                          .layout(LayoutStyle{}.width(form_width)));
                      }});
             FormItem(FormItemProps{}
                          .field(u8"consent", state->form_consent)
                          .label(sample_text(String{u8"gallery.form.consent · 确认"}))
                          .control(state->form_consent_ref),
                      FormItemContent{[state] {
                          Checkbox(CheckboxProps{}
                                       .checked(state->form_consent.value())
                                       .onChange(state->form_consent.onChange())
                                       .disabled(state->form_consent.uneditable())
                                       .ref(state->form_consent_ref),
                                   CheckboxLabel{[] { Text(u8"同意保存本次填写内容"); }});
                      }});
         }});
    // These actions stay reachable when Form disables its registered controls.
    Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)), SpaceContent{[state] {
              Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                  if (!state->form_ref.submit()) {
                      state->form_feedback.set(String{u8"当前禁用，提交未执行"});
                  }
              }),
                     [] { Text(sample_text(u8"gallery.form.submit · 保存")); });
              gallery_target("gallery.form.submit");
              Button(ButtonProps{}.onClick([state] {
                  if (const auto result = state->form_ref.validate()) {
                      state->form_feedback.set(result->valid() ? String{u8"校验通过，尚未保存"}
                                                               : String{u8"校验失败，请查看字段错误"});
                  }
              }),
                     [] { Text(sample_text(u8"gallery.form.validate · 校验")); });
              gallery_target("gallery.form.validate");
              Button(ButtonProps{}.onClick([state] {
                  if (state->form_ref.reset()) {
                      state->form_feedback.set(String{u8"已重置"});
                  }
              }),
                     [] { Text(sample_text(u8"gallery.form.reset · 重置")); });
              gallery_target("gallery.form.reset");
              Button(ButtonProps{}.onClick([state] { state->form_disabled.set(!state->form_disabled.get()); }),
                     [] { Text(sample_text(u8"gallery.form.disabled · 切换禁用")); });
              gallery_target("gallery.form.disabled");
              Button(ButtonProps{}.onClick([state] { state->form_read_only.set(!state->form_read_only.get()); }),
                     [] { Text(sample_text(u8"gallery.form.readonly · 切换只读")); });
              gallery_target("gallery.form.readonly");
              Button(ButtonProps{}.onClick([state] {
                  state->form_help.set(state->form_help.get() == String{u8"填写用于保存的名称"}
                                           ? String{u8"名称与其他字段一起保存"}
                                           : String{u8"填写用于保存的名称"});
              }),
                     [] { Text(sample_text(u8"gallery.form.help · 更新帮助")); });
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
                      .content(sample_text(String{u8"gallery.date-picker.controlled · DatePicker · 日期选择"}))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          const auto value = state->date_value.get();
                          return sample_text(
                              state, utf8("gallery.date-picker.value · " +
                                          (value ? std::string{format_iso_date(*value).bytes()} : "未选择日期")));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
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
                                     Text(sample_text(u8"gallery.date-picker.footer · 选择备注"));
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
                    [] { Text(sample_text(u8"gallery.date-picker.restore · 恢复示例日期")); });
             gallery_target("gallery.date-picker.restore");
         }});
    Text(TextProps{}
             .content(sample_text(String{u8"gallery.date-picker.form · Form String 日期适配"}))
             .layout(LayoutStyle{}.margin_left(dp(1))));
    Text(TextProps{}
             .content(bind([state] {
                 return sample_text(state, utf8("gallery.date-picker.status · " +
                                                std::string{state->date_feedback.get().bytes()} +
                                                (state->date_form_disabled.get() ? " · 已禁用" : " · 已启用") +
                                                (state->date_form_read_only.get() ? " · 只读" : " · 可编辑")));
             }))
             .layout(LayoutStyle{}.margin_left(dp(1))));
    Form(FormProps{}
             .ref(state->date_form_ref)
             .disabled(state->date_form_disabled)
             .readOnly(state->date_form_read_only)
             .layout(LayoutStyle{}
                         .width(bind([state] { return dp(state->document_width.get().value() - 2); }))
                         .margin_left(dp(1))
                         .margin_right(dp(1)))
             .onFinish([state](FormResult result) {
                 state->date_feedback.set(
                     utf8("日期保存成功 · " + std::string{std::get<String>(result.values.front().value).bytes()}));
             })
             .onFinishFailed([state](FormResult) { state->date_feedback.set(String{u8"日期校验失败"}); }),
         FormContent{[state] {
             FormItem(FormItemProps{}
                          .field(u8"date", state->date_field)
                          .label(sample_text(String{u8"gallery.date-picker.field · 日期"}))
                          .help(state->date_help)
                          .control(state->date_field_ref),
                      FormItemContent{[state] {
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
                      }});
         }});
    Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)), SpaceContent{[state] {
              Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                  if (!state->date_form_ref.submit()) {
                      state->date_feedback.set(String{u8"当前禁用，日期提交未执行"});
                  }
              }),
                     [] { Text(sample_text(u8"gallery.date-picker.submit · 保存日期")); });
              gallery_target("gallery.date-picker.submit");
              Button(ButtonProps{}.onClick([state] {
                  if (state->date_form_ref.reset()) {
                      state->date_feedback.set(String{u8"日期已重置"});
                  }
              }),
                     [] { Text(sample_text(u8"gallery.date-picker.reset · 重置日期")); });
              gallery_target("gallery.date-picker.reset");
              Button(
                  ButtonProps{}.onClick([state] { state->date_form_disabled.set(!state->date_form_disabled.get()); }),
                  [] { Text(sample_text(u8"gallery.date-picker.disabled · 切换禁用")); });
              gallery_target("gallery.date-picker.disabled");
              Button(
                  ButtonProps{}.onClick([state] { state->date_form_read_only.set(!state->date_form_read_only.get()); }),
                  [] { Text(sample_text(u8"gallery.date-picker.readonly · 切换只读")); });
              gallery_target("gallery.date-picker.readonly");
              Button(ButtonProps{}.onClick([state] {
                  state->date_help.set(state->date_help.get() ==
                                               String{u8"允许 2026-10-05 至 2026-11-25，每月 13 日不可选"}
                                           ? String{u8"日期以 YYYY-MM-DD 字符串保存"}
                                           : String{u8"允许 2026-10-05 至 2026-11-25，每月 13 日不可选"});
              }),
                     [] { Text(sample_text(u8"gallery.date-picker.help · 更新日期帮助")); });
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
                         Text(sample_text(id));
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
            Text(sample_text(u8"gallery.input.grapheme-count · 原文超限只提示"));
            Input(InputProps{}
                      .defaultValue(state->initial("gallery.input.grapheme-count", ryn::String{u8"é🙂中文"}))
                      .showCount()
                      .count(InputCountOptions{3, InputCountUnit::Grapheme})
                      .onChange([state](ryn::String value) { state->remember("gallery.input.grapheme-count", value); })
                      .layout(LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.input.grapheme-count");
            Text(sample_text(u8"gallery.input.exceed-formatter · 字节统计 / 去除空格"));
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
            Text(sample_text(u8"gallery.input.focus-hints · Email / 原生提示"));
            Input(InputProps{}
                      .ref(state->input_ref)
                      .defaultValue(state->initial("gallery.input.focus-hints", ryn::String{u8"desktop@example.com"}))
                      .purpose(InputPurpose::Email)
                      .capitalization(InputCapitalization::None)
                      .autocorrect(false)
                      .onFocus([state] { state->input_feedback.set(String{u8"引用输入已聚焦"}); })
                      .onChange([state](ryn::String value) { state->remember("gallery.input.focus-hints", value); })
                      .layout(LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.input.focus-hints");
            Text(sample_text(u8"gallery.input.clear-vector · 禁用动作保留图标"));
            Input(InputProps{}
                      .defaultValue(state->initial("gallery.input.clear-vector", ryn::String{u8"自定义清空图标"}))
                      .allowClear(true)
                      .clearDisabled(state->input_clear_disabled)
                      .clearIcon(icon_vector_sample())
                      .onClear([state] { state->input_feedback.set(String{u8"onClear 已通知"}); })
                      .onChange([state](ryn::String value) { state->remember("gallery.input.clear-vector", value); })
                      .layout(LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.input.clear-vector");
            Button(
                ButtonProps{}.onClick([state] { static_cast<void>(state->input_ref.focus({InputFocusCursor::All})); }),
                ButtonContent{[] { Text(sample_text(u8"gallery.input.focus-all")); }});
            remember_target(state, "gallery.input.focus-all");
            Button(
                ButtonProps{}.onClick([state] { state->input_clear_disabled.set(!state->input_clear_disabled.get()); }),
                ButtonContent{[] { Text(sample_text(u8"gallery.input.toggle-clear-disabled")); }});
            remember_target(state, "gallery.input.toggle-clear-disabled");
        }});
    state->telemetry.live_samples += 10;
}

void add_password_features(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Space(SpaceProps{}.wrap(true).align(SpaceAlign::Start).layout(LayoutStyle{}.width(state->document_width)),
          SpaceContent{[state] {
              Text(sample_text(u8"gallery.password.hover-vector · 悬停 / prefix / count"));
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
              Text(sample_text(u8"gallery.password.controlled-toggle · 受控 / 显隐动作开关"));
              Password(PasswordProps{}
                           .defaultValue(u8"controlled")
                           .visible(state->password_visible)
                           .visibilityToggle(state->password_toggle)
                           .onVisibleChange([state](bool value) { state->password_visible.set(value); })
                           .layout(LayoutStyle{}.width(state->cell_width)));
              remember_target(state, "gallery.password.controlled-toggle");
              Button(ButtonProps{}.onClick([state] { state->password_toggle.set(!state->password_toggle.get()); }),
                     ButtonContent{[] { Text(sample_text(u8"gallery.password.toggle-action")); }});
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
                        Text(sample_text(u8"gallery.search.filled-vector · Dark Compact / Clear 来源"));
                        Search(SearchProps{}
                                   .variant(InputVariant::Filled)
                                   .size(ControlSize::Small)
                                   .defaultValue(state->initial("gallery.search.filled-vector", ryn::String{u8"RynUI"}))
                                   .searchIcon(icon_vector_sample())
                                   .allowClear(true)
                                   .showCount()
                                   .onSearch([state](String value, SearchSource source) {
                                       ++state->telemetry.search_submits;
                                       state->search_feedback.set(source == SearchSource::Clear ? String{u8"Clear"}
                                                                                                : std::move(value));
                                   })

                                   .onChange([state](ryn::String value) {
                                       state->remember("gallery.search.filled-vector", value);
                                   })
                                   .layout(LayoutStyle{}.width(state->cell_width)));
                        remember_target(state, "gallery.search.filled-vector");
                    }});
              Text(sample_text(u8"gallery.search.underlined-small · 连接高度 / typed content"));
              Search(SearchProps{}
                         .variant(InputVariant::Underlined)
                         .size(ControlSize::Small)
                         .defaultValue(state->initial("gallery.search.underlined-small", ryn::String{u8"Search"}))

                         .onChange(
                             [state](ryn::String value) { state->remember("gallery.search.underlined-small", value); })
                         .layout(LayoutStyle{}.width(state->cell_width)),
                     SearchButtonContent{[] { Text(u8"查询"); }});
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
    Text(u8"TextArea / 多行输入：换行、选择、统计、autoSize、滚动与 resize");
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
                         Text(sample_text(id));
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
            Text(sample_text(u8"gallery.text-area.autosize · minRows=2 / maxRows=5"));
            TextArea(TextAreaProps{}
                         .defaultValue(state->initial("gallery.text-area.autosize",
                                                      ryn::String{u8"autoSize 随编辑与宽度换行扩展\n保留空行\n\n"}))
                         .autoSize(TextAreaAutoSize{true, 2, 5})
                         .allowClear(true)
                         .showCount()

                         .onChange([state](ryn::String value) { state->remember("gallery.text-area.autosize", value); })
                         .layout(LayoutStyle{}.width(state->cell_width)));
            remember_target(state, "gallery.text-area.autosize");
            Text(sample_text(u8"gallery.text-area.resize-nowrap · 双向拖动 / wheel"));
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
                      Text(sample_text(u8"gallery.text-area.readonly · Dark Compact / 可选择复制"));
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
            Text(sample_text(u8"gallery.text-area.disabled · Warning / 禁用"));
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
    Text(u8"OTP / 分格输入：grapheme、粘贴分发、formatter、mask 与动态长度");
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
                           Text(sample_text(id));
                           OTP(OTPProps{}
                                   .length(4)
                                   .variant(variant)
                                   .defaultValue(u8"12")
                                   .onInput([state](const auto& cells) {
                                       ++state->telemetry.input_changes;
                                       const auto filled = std::ranges::count_if(
                                           cells, [](const String& value) { return !value.empty(); });
                                       state->input_feedback.set(utf8("OTP partial: " + std::to_string(filled) + "/" +
                                                                      std::to_string(cells.size())));
                                   })
                                   .onChange([state](String value) { state->input_feedback.set(std::move(value)); }));
                           remember_target(state, id.bytes());
                       }});
              }
              Text(sample_text(u8"gallery.otp.controlled · 六格 / 填满通知"));
              OTP(OTPProps{}
                      .value(state->otp_value)
                      .ref(state->otp_ref)
                      .onInput([state](const auto&) { ++state->telemetry.input_changes; })
                      .onChange([state](String value) {
                          state->otp_value.set(value);
                          state->input_feedback.set(std::move(value));
                      }));
              remember_target(state, "gallery.otp.controlled");
              Text(sample_text(u8"gallery.otp.formatter-mask · 大写 / bullet / Large"));
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
              Text(sample_text(u8"gallery.otp.separator-rtl · typed separator / RightToLeft"));
              OTP(OTPProps{}.length(4).defaultValue(u8"1234").direction(OTPDirection::RightToLeft),
                  OTPSeparator{[](std::size_t index) -> std::optional<OTPSeparatorContent> {
                      if (index != 1) {
                          return {};
                      }
                      return OTPSeparatorContent{[] { Text(u8"-"); }};
                  }});
              remember_target(state, "gallery.otp.separator-rtl");
              Text(sample_text(u8"gallery.otp.dynamic-length · 四/六格 / 前缀身份保留"));
              OTP(OTPProps{}.length(state->otp_length).defaultValue(u8"12345678"));
              remember_target(state, "gallery.otp.dynamic-length");
              ThemeConfig dark;
              dark.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
              Theme(ThemeProps{}.config(dark), ThemeContent{[state] {
                        ++state->telemetry.theme_content_runs;
                        Text(sample_text(u8"gallery.otp.readonly-mask · Dark Compact / Small / 自定义 mask"));
                        OTP(OTPProps{}
                                .length(4)
                                .defaultValue(u8"只读验证码")
                                .mask(u8"*")
                                .readOnly(true)
                                .variant(InputVariant::Filled)
                                .size(ControlSize::Small));
                        remember_target(state, "gallery.otp.readonly-mask");
                    }});
              Text(sample_text(u8"gallery.otp.disabled · Warning / 禁用"));
              OTP(OTPProps{}.length(4).defaultValue(u8"1234").disabled(true).status(InputStatus::Warning));
              remember_target(state, "gallery.otp.disabled");
              Button(ButtonProps{}.onClick([state] { static_cast<void>(state->otp_ref.focus()); }),
                     ButtonContent{[] { Text(sample_text(u8"gallery.otp.focus-all")); }});
              remember_target(state, "gallery.otp.focus-all");
              Button(ButtonProps{}.onClick([state] { state->otp_length.set(state->otp_length.get() == 6 ? 4 : 6); }),
                     ButtonContent{[] { Text(sample_text(u8"gallery.otp.toggle-length")); }});
              remember_target(state, "gallery.otp.toggle-length");
          }});
    state->telemetry.live_samples += 12;
}

void add_bidi_typography(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"双向文字：Arabic / Hebrew / 数字 / 括号 · Auto / LTR / RTL");
    remember_target(state, "gallery.bidi.heading");
    Text(TextProps{}
             .content(sample_text(u8"gallery.bidi.text · مرحبا (12) אבג RynUI"))
             .direction(state->text_direction));
    remember_target(state, "gallery.bidi.text");
    Paragraph(TypographyProps{}
                  .content(sample_text(u8"gallery.bidi.paragraph · مرحبا 123 (אבג)\nאבג 12 مرحبا 中文"))
                  .direction(state->text_direction)
                  .underline(true)
                  .layout(LayoutStyle{}.width(state->cell_width)));
    remember_target(state, "gallery.bidi.paragraph");
    state->telemetry.live_samples += 2;
}

void add_bidi_input(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(sample_text(u8"gallery.bidi.input"));
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
    Text(sample_text(u8"gallery.bidi.text-area"));
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
    Text(sample_text(u8"gallery.bidi.password"));
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
               Text(label(identity, "切换段落方向"));
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

void add_select_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"Select · 单选 / Unicode 搜索 / 清空 / 动态选项 / 键盘");
    const std::vector<SelectOption> choices{
        {u8"desktop", u8"桌面 / Desktop"}, {u8"service", u8"服务 / Service"}, {u8"unavailable", u8"禁用选项", true}};
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
        Text(sample_text(String::from_utf8(std::u8string_view{caption}).value()));
        Select(props.layout(LayoutStyle{}.width(state->cell_width)));
        remember_target(state, identity);
        ++state->telemetry.live_samples;
    };
    field(SelectProps{}
              .options(state->select_options)
              .value(state->select_value)
              .showSearch()
              .allowClear()
              .ref(state->select_ref)
              .placeholder(u8"请选择 / Search")
              .notFoundContent(u8"没有匹配项")
              .onChange([state](SelectSelection value) {
                  state->select_value.set(std::move(value));
                  ++state->telemetry.activations;
              }),
          u8"gallery.select.controlled-search · 受控 / 清空 / 动态 options");
    field(SelectProps{}.options({{1, u8"优先级 1"}, {42, u8"优先级 42"}, {99, u8"禁用", true}}),
          u8"gallery.select.numeric · int64 value", SelectSelection{SelectValue{42}});
    field(SelectProps{}.options(choices).disabled(true), u8"gallery.select.disabled",
          SelectSelection{SelectValue{u8"desktop"}});
    field(SelectProps{}.options(choices).loading(state->loading).showSearch(),
          u8"gallery.select.loading · 保留搜索，阻止提交");
    field(SelectProps{}.options({}).showSearch().notFoundContent(u8"暂无选项"), u8"gallery.select.empty");
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
              Text(sample_text(u8"gallery.select.component-theme · 独立颜色 / 36 dp 行高"));
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
           ButtonContent{[] { Text(sample_text(u8"gallery.select.focus")); }});
    gallery_target("gallery.select.focus");
    Button(ButtonProps{}.onClick([state] { state->select_open.set(!state->select_open.get()); }),
           ButtonContent{[] { Text(sample_text(u8"gallery.select.toggle-open")); }});
    gallery_target("gallery.select.toggle-open");
    Button(ButtonProps{}.onClick([state] {
        state->select_options_expanded = !state->select_options_expanded;
        auto options = std::vector<SelectOption>{{u8"desktop", u8"桌面 / Desktop"}, {u8"service", u8"服务 / Service"}};
        if (state->select_options_expanded) {
            options.push_back({u8"device", u8"设备 / Device"});
        }
        state->select_options.set(std::move(options));
    }),
           ButtonContent{[] { Text(sample_text(u8"gallery.select.dynamic-options")); }});
    gallery_target("gallery.select.dynamic-options");
    state->telemetry.live_samples += 3;
}

void add_float_button_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"FloatButton · 图标 / 徽标 / 分组 / 显式原生滚动目标");
    Text(sample_text(u8"gallery.float-button.basics · 默认 / 主色 / 内容 / 禁用"));
    Space(SpaceProps{}, SpaceContent{[state] {
              FloatButton(
                  FloatButtonProps{}.floating(false).tooltip(u8"帮助").badge(BadgeProps{}.count(3)).onClick([state] {
                      state->common_feedback.set(u8"已执行浮动操作");
                      ++state->telemetry.activations;
                  }));
              FloatButton(
                  FloatButtonProps{}.floating(false).type(FloatButtonType::Primary).shape(FloatButtonShape::Square),
                  {.icon = FloatButtonIcon{[] { Icon(IconProps{}.name(IconName::CustomerServiceOutlined)); }},
                   .content = FloatButtonContent{[] { Text(u8"客服"); }}});
              FloatButton(FloatButtonProps{}.floating(false).disabled(true));
          }});
    Text(TextProps{}.content(state->common_feedback));
    Text(sample_text(u8"gallery.float-button.groups · 常驻 / 点击 / 悬停 / 四方向"));
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
    Text(sample_text(u8"gallery.float-button.controlled · 显式 open authority"));
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
    Text(sample_text(u8"gallery.float-button.back-top · 滑块滚动真实内容 / 进度环返回顶部"));
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
                         Text(TextProps{}
                                  .content(String::from_utf8("原生滚动内容 / Row " + std::to_string(index + 1)).value())
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
    Text(sample_text(u8"gallery.float-button.window · 窗口右下角真实浮动 / 可拖动"));
    FloatButton(FloatButtonProps{}
                    .type(FloatButtonType::Primary)
                    .draggable(true)
                    .tooltip(u8"窗口浮动入口，可拖动")
                    .onClick([state] {
                        ++state->telemetry.activations;
                        state->common_feedback.set(u8"窗口浮动入口已点击");
                    }));
}

void add_card_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"Card · 标题 / 区域组合 / 保留编辑状态 / 加载");
    const auto props = [state] {
        return CardProps{}.layout(LayoutStyle{}.width(state->cell_width));
    };
    Text(sample_text(u8"gallery.card.basic · 无标题内容容器"));
    Card(props(), CardContent{[] { Text(u8"Business content / 业务内容"); }});
    Text(sample_text(u8"gallery.card.account · 标题 / extra / 编辑状态"));
    Card(props().title(u8"Account / 账户").loading(state->card_loading), CardContent{[] {
             Input(InputProps{}
                       .defaultValue(gallery_initial_text("gallery.card.draft", u8"保留草稿 / Draft"))
                       .onChange(gallery_value_changed<ryn::String>("gallery.card.draft"))
                       .layout(LayoutStyle{}.width(dp(140))));
             gallery_target("gallery.card.draft");
         }},
         CardSlots{.extra = CardExtra{[state] {
                       Button(ButtonProps{}.onClick([state] { ++state->telemetry.activations; }),
                              [] { Text(u8"Edit"); });
                   }}});
    Text(sample_text(u8"gallery.card.cover-actions · 富标题 / cover / actions"));
    Card(props(), CardContent{[] { Text(u8"Retained content / 保留内容"); }},
         CardSlots{.title = CardTitle{[] { Text(TextProps{}.content(u8"Native composition / 原生组合")); }},
                   .cover = CardCover{[] { Text(u8"Cover slot / 封面内容区域"); }},
                   .actions = CardActions{[state] {
                       Space(SpaceProps{}, SpaceContent{[state] {
                                 Button(ButtonProps{}.onClick([state] { ++state->telemetry.activations; }),
                                        [] { Text(u8"Save"); });
                                 Button(ButtonProps{}.onClick([state] { ++state->telemetry.activations; }),
                                        [] { Text(u8"More"); });
                             }});
                   }}});
    Text(sample_text(u8"gallery.card.small · 小号尺寸"));
    Card(props().title(u8"Small / 小号").size(CardSize::Small), CardContent{[] { Text(u8"12 dp padding"); }});
    Text(sample_text(u8"gallery.card.borderless · 无边框 / 子控件 hover"));
    Card(props().title(u8"Hover / 悬停").variant(CardVariant::Borderless).hoverable(), CardContent{[state] {
             Button(ButtonProps{}.onClick([state] { ++state->telemetry.activations; }),
                    [] { Text(u8"Nested action"); });
         }});
    Text(sample_text(u8"gallery.card.loading · 静态占位"));
    Card(props().title(u8"Loading / 加载").loading(), CardContent{[] { Text(u8"Data remains mounted"); }});
    ThemeConfig custom;
    custom.card.tokens.background = Color::rgba8(238, 246, 255);
    custom.card.tokens.header_background = Color::rgba8(220, 235, 255);
    custom.card.tokens.text = Color(0, 0, 0, 0.88F);
    custom.card.tokens.header_text = Color(0, 0, 0, 0.88F);
    custom.card.tokens.body_padding = dp(16);
    Theme(ThemeProps{}.config(custom), ThemeContent{[state, props] {
              ++state->telemetry.theme_content_runs;
              Text(sample_text(u8"gallery.card.component-theme · 局部背景 / 16 dp padding"));
              Card(props().title(u8"Component Token"), CardContent{[] { Text(u8"Scoped presentation / 局部主题"); }});
          }});
    Button(ButtonProps{}.onClick([state] {
        state->card_loading.set(!state->card_loading.get());
        ++state->telemetry.activations;
    }),
           [] { Text(sample_text(u8"gallery.card.toggle-loading · 切换账户卡片加载")); });
    gallery_target("gallery.card.toggle-loading");
    state->telemetry.live_samples += 8;
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
                      .content(sample_text(String{u8"gallery.breadcrumb.trail · Breadcrumb · 项目层级"}))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}.content(bind([state] {
                 return sample_text(state, utf8("gallery.breadcrumb.feedback · " +
                                                std::string{state->breadcrumb_feedback.get().bytes()}));
             })));
             Breadcrumb(
                 BreadcrumbProps{}
                     .onActivate([state](BreadcrumbActivation event) {
                         state->breadcrumb_feedback.set(
                             utf8("打开 " + std::string{event.key.bytes()} + " / " + std::to_string(event.index)));
                     })
                     .separator(BreadcrumbSeparator{[] { Text(u8"›"); }})
                     .layout(LayoutStyle{}
                                 .width(bind([state] { return dp(state->document_width.get().value() - 16); }))
                                 .margin_left(dp(8))
                                 .margin_top(dp(4))
                                 .margin_bottom(dp(4))),
                 {BreadcrumbItem{u8"home", u8"首页"}.disabled(state->breadcrumb_disabled).icon(BreadcrumbIcon{[] {
                      Icon(IconProps{}.name(IconName::HomeOutlined));
                  }}),
                  BreadcrumbItem{u8"projects", u8"项目"}
                      .title(state->breadcrumb_title)
                      .visible(state->breadcrumb_visible)
                      .disabled(state->breadcrumb_disabled),
                  BreadcrumbItem{u8"draft", u8"草稿"}
                      .visible(state->breadcrumb_draft_visible)
                      .disabled(state->breadcrumb_disabled)
                      .richTitle(BreadcrumbTitle{[state] {
                          Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(4)), FlexContent{[state] {
                                   Input(InputProps{}
                                             .defaultValue(
                                                 gallery_initial_text("gallery.breadcrumb.draft", u8"Breadcrumb draft"))
                                             .placeholder(u8"gallery.breadcrumb.draft")
                                             .ref(state->breadcrumb_ref)
                                             .onChange(gallery_value_changed<ryn::String>(
                                                 "gallery.breadcrumb.draft",
                                                 [state](String value) { state->breadcrumb_draft.set(value); }))
                                             .layout(LayoutStyle{}.width(dp(150))));
                                   remember_target(state, "gallery.breadcrumb.draft");
                                   Button(ButtonProps{}.onClick([state] {
                                       state->breadcrumb_feedback.set(
                                           utf8("应用 " + std::string{state->breadcrumb_draft.get().bytes()}));
                                   }),
                                          [] { Text(u8"应用草稿"); });
                                   remember_target(state, "gallery.breadcrumb.apply");
                               }});
                      }})});
             remember_target(state, "gallery.breadcrumb.component");
             Text(sample_text(u8"gallery.breadcrumb.display · 展示层级中的独立编辑"));
             Breadcrumb(BreadcrumbProps{}.layout(LayoutStyle{}.margin_left(dp(8))),
                        {BreadcrumbItem{u8"preview", u8"预览"}.richTitle(BreadcrumbTitle{[state] {
                             Input(InputProps{}
                                       .defaultValue(
                                           gallery_initial_text("gallery.breadcrumb.display-draft", u8"Display draft"))
                                       .onChange(gallery_value_changed<ryn::String>("gallery.breadcrumb.display-draft"))
                                       .layout(LayoutStyle{}.width(dp(150))));
                             remember_target(state, "gallery.breadcrumb.display-draft");
                         }}),
                         BreadcrumbItem{u8"read", u8"只展示层级"}});
             remember_target(state, "gallery.breadcrumb.display-component");
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}.onClick([state] {
                           state->breadcrumb_title.set(
                               state->breadcrumb_title.get() == String{u8"项目"} ? String{u8"归档"} : String{u8"项目"});
                       }),
                              [] { Text(sample_text(u8"gallery.breadcrumb.rename · 改名")); });
                       remember_target(state, "gallery.breadcrumb.rename");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->breadcrumb_visible.set(!state->breadcrumb_visible.get()); }),
                              [] { Text(sample_text(u8"gallery.breadcrumb.visible · 显隐祖先")); });
                       remember_target(state, "gallery.breadcrumb.visible");
                       Button(ButtonProps{}.onClick([state] {
                           state->breadcrumb_draft_visible.set(!state->breadcrumb_draft_visible.get());
                       }),
                              [] { Text(sample_text(u8"gallery.breadcrumb.draft-visible · 显隐草稿")); });
                       remember_target(state, "gallery.breadcrumb.draft-visible");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->breadcrumb_disabled.set(!state->breadcrumb_disabled.get()); }),
                              [] { Text(sample_text(u8"gallery.breadcrumb.disabled · 锁定层级")); });
                       remember_target(state, "gallery.breadcrumb.disabled");
                       Button(ButtonProps{}.onClick([state] {
                           state->breadcrumb_title.set(String{u8"项目"});
                           state->breadcrumb_visible.set(true);
                           state->breadcrumb_draft_visible.set(true);
                           state->breadcrumb_disabled.set(false);
                       }),
                              [] { Text(sample_text(u8"gallery.breadcrumb.restore · 恢复层级")); });
                       remember_target(state, "gallery.breadcrumb.restore");
                       Button(ButtonProps{}.onClick([state] { static_cast<void>(state->breadcrumb_ref.focus()); }),
                              [] { Text(sample_text(u8"gallery.breadcrumb.focus · 编辑草稿")); });
                       remember_target(state, "gallery.breadcrumb.focus");
                   }});
         }});
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
                      .content(sample_text(String{u8"gallery.steps.workflow · Steps · 发布流程"}))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}.content(bind([state] {
                 return sample_text(
                     state, utf8("gallery.steps.feedback · current=" + std::to_string(state->steps_current.get()) +
                                 " · " + std::string{state->steps_feedback.get().bytes()}));
             })));
             Steps(StepsProps{}
                       .current(state->steps_current)
                       .vertical(state->steps_vertical)
                       .size(state->steps_size)
                       .status(state->steps_status)
                       .onChange([state](std::size_t index) {
                           state->steps_current.set(index);
                           state->steps_feedback.set(utf8("切换 / " + std::to_string(index)));
                       })
                       .layout(LayoutStyle{}
                                   .width(bind([state] { return dp(state->document_width.get().value() - 16); }))
                                   .margin_left(dp(8))
                                   .margin_top(dp(4))
                                   .margin_bottom(dp(4))),
                   {StepsItem{u8"details", u8"资料"}
                        .subtitle(u8"编辑备注")
                        .disabled(state->steps_disabled)
                        .richDescription(StepsDescription{[state] {
                            Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(4)), FlexContent{[state] {
                                     Input(
                                         InputProps{}
                                             .defaultValue(gallery_initial_text("gallery.steps.draft", u8"Steps draft"))
                                             .placeholder(u8"gallery.steps.draft")
                                             .ref(state->steps_ref)
                                             .onChange(gallery_value_changed<ryn::String>(
                                                 "gallery.steps.draft",
                                                 [state](String value) { state->steps_draft.set(value); }))
                                             .layout(LayoutStyle{}.width(dp(120))));
                                     remember_target(state, "gallery.steps.draft");
                                     Button(ButtonProps{}.onClick([state] {
                                         state->steps_feedback.set(
                                             utf8("保存 " + std::string{state->steps_draft.get().bytes()}));
                                     }),
                                            [] { Text(u8"保存备注"); });
                                     remember_target(state, "gallery.steps.save");
                                 }});
                        }}),
                    StepsItem{u8"review", u8"审核"}
                        .description(u8"核对资料")
                        .visible(state->steps_visible)
                        .disabled(state->steps_disabled)
                        .icon(StepsIcon{[] { Icon(IconProps{}.name(IconName::SearchOutlined)); }}),
                    StepsItem{u8"publish", u8"发布"}.description(u8"确认后发布")});
             remember_target(state, "gallery.steps.component");
             Space(
                 SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                 SpaceContent{[state] {
                     Button(ButtonProps{}.onClick([state] {
                         state->steps_current.set(state->steps_current.get() > 0 ? state->steps_current.get() - 1 : 0);
                         state->steps_feedback.set(String{u8"上一步"});
                     }),
                            [] { Text(sample_text(u8"gallery.steps.previous · 上一步")); });
                     remember_target(state, "gallery.steps.previous");
                     Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                         state->steps_current.set(std::min<std::size_t>(3, state->steps_current.get() + 1));
                         state->steps_feedback.set(state->steps_current.get() == 3 ? String{u8"发布完成"}
                                                                                   : String{u8"下一步"});
                     }),
                            [] { Text(sample_text(u8"gallery.steps.next · 下一步")); });
                     remember_target(state, "gallery.steps.next");
                     Button(ButtonProps{}.onClick([state] {
                         state->steps_current.set(0);
                         state->steps_visible.set(true);
                         state->steps_disabled.set(false);
                         state->steps_status.set(StepsStatus::Process);
                         state->steps_feedback.set(String{u8"流程已重置，备注保留"});
                     }),
                            [] { Text(sample_text(u8"gallery.steps.reset · 重置流程")); });
                     remember_target(state, "gallery.steps.reset");
                     Button(ButtonProps{}.onClick([state] { state->steps_vertical.set(!state->steps_vertical.get()); }),
                            [] { Text(sample_text(u8"gallery.steps.vertical · 横纵切换")); });
                     remember_target(state, "gallery.steps.vertical");
                     Button(ButtonProps{}.onClick([state] {
                         state->steps_size.set(state->steps_size.get() == StepsSize::Default ? StepsSize::Small
                                                                                             : StepsSize::Default);
                     }),
                            [] { Text(sample_text(u8"gallery.steps.size · 切换尺寸")); });
                     remember_target(state, "gallery.steps.size");
                     Button(ButtonProps{}.onClick([state] {
                         state->steps_status.set(state->steps_status.get() == StepsStatus::Process
                                                     ? StepsStatus::Error
                                                     : StepsStatus::Process);
                     }),
                            [] { Text(sample_text(u8"gallery.steps.status · 切换错误状态")); });
                     remember_target(state, "gallery.steps.status");
                     Button(ButtonProps{}.onClick([state] { state->steps_visible.set(!state->steps_visible.get()); }),
                            [] { Text(sample_text(u8"gallery.steps.visible · 显隐审核")); });
                     remember_target(state, "gallery.steps.visible");
                     Button(ButtonProps{}.onClick([state] { state->steps_disabled.set(!state->steps_disabled.get()); }),
                            [] { Text(sample_text(u8"gallery.steps.disabled · 锁定资料与审核")); });
                     remember_target(state, "gallery.steps.disabled");
                     Button(ButtonProps{}.onClick([state] { static_cast<void>(state->steps_ref.focus()); }),
                            [] { Text(sample_text(u8"gallery.steps.focus · 编辑备注")); });
                     remember_target(state, "gallery.steps.focus");
                 }});
         }});
    ++state->telemetry.live_samples;
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
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(8))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state, rules] {
             Text(TextProps{}
                      .content(sample_text(String{u8"gallery.time-picker.controlled · TimePicker · 时间选择"}))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          const auto value = state->time_value.get();
                          return sample_text(
                              state,
                              utf8("gallery.time-picker.value · " +
                                   (value ? std::string{format_time_of_day(*value, TimePickerPrecision::Minute).bytes()}
                                          : "未选择时间") +
                                   (state->time_confirm.get() ? " · 确认模式" : " · 即时模式") +
                                   (state->time_accept.get() ? " · 接受选择" : " · 锁定已保存值") +
                                   (state->time_input_read_only.get() ? " · 文本只读" : " · 可输入") + " · " +
                                   std::string{state->time_request.get().bytes()}));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(u8"08:00–18:00，每 15 分钟，12 时不可选；滚轮只滚动列");
             TimePicker(
                 TimePickerProps{}
                     .value(state->time_value)
                     .open(state->time_open)
                     .onChange([state](TimePickerValue value) {
                         state->time_request.set(
                             utf8(std::string{state->time_accept.get() ? "已接受 " : "已拒绝 "} +
                                  (value ? std::string{format_time_of_day(*value, TimePickerPrecision::Minute).bytes()}
                                         : "清空")));
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
                              Text(sample_text(u8"gallery.time-picker.footer · 选择备注"));
                              Input(InputProps{}
                                        .defaultValue(
                                            gallery_initial_text("gallery.time-picker.note", u8"保留的时间备注"))
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
                           state->time_request.set(String{u8"已恢复示例"});
                       }),
                              [] { Text(sample_text(u8"gallery.time-picker.restore · 恢复时间示例")); });
                       gallery_target("gallery.time-picker.restore");
                       Button(ButtonProps{}.onClick([state] { state->time_confirm.set(!state->time_confirm.get()); }),
                              [] { Text(sample_text(u8"gallery.time-picker.mode · 确认/即时模式")); });
                       gallery_target("gallery.time-picker.mode");
                       Button(ButtonProps{}.onClick([state] { state->time_accept.set(!state->time_accept.get()); }),
                              [] { Text(sample_text(u8"gallery.time-picker.authority · 切换锁定已保存值")); });
                       gallery_target("gallery.time-picker.authority");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->time_input_read_only.set(!state->time_input_read_only.get()); }),
                              [] { Text(sample_text(u8"gallery.time-picker.input-readonly · 切换文本只读")); });
                       gallery_target("gallery.time-picker.input-readonly");
                   }});
         }});
    Text(TextProps{}
             .content(sample_text(String{u8"gallery.time-picker.form · Form String 时间适配"}))
             .layout(LayoutStyle{}.margin_left(dp(1))));
    Text(TextProps{}
             .content(bind([state] {
                 return sample_text(state, utf8("gallery.time-picker.status · " +
                                                std::string{state->time_feedback.get().bytes()} +
                                                (state->time_form_disabled.get() ? " · 已禁用" : " · 已启用") +
                                                (state->time_form_read_only.get() ? " · 只读" : " · 可编辑")));
             }))
             .layout(LayoutStyle{}.margin_left(dp(1))));
    Form(FormProps{}
             .ref(state->time_form_ref)
             .disabled(state->time_form_disabled)
             .readOnly(state->time_form_read_only)
             .layout(LayoutStyle{}
                         .width(bind([state] { return dp(state->document_width.get().value() - 2); }))
                         .margin_left(dp(1))
                         .margin_right(dp(1)))
             .onFinish([state](FormResult result) {
                 state->time_feedback.set(
                     utf8("时间保存成功 · " + std::string{std::get<String>(result.values.front().value).bytes()}));
             })
             .onFinishFailed([state](FormResult) { state->time_feedback.set(String{u8"时间校验失败"}); }),
         FormContent{[state] {
             FormItem(FormItemProps{}
                          .field(u8"time", state->time_field)
                          .label(sample_text(String{u8"gallery.time-picker.field · 时间"}))
                          .help(state->time_help)
                          .control(state->time_field_ref),
                      FormItemContent{[state] {
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
                      }});
         }});
    Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)), SpaceContent{[state] {
              Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                  if (!state->time_form_ref.submit()) {
                      state->time_feedback.set(String{u8"当前禁用，时间提交未执行"});
                  }
              }),
                     [] { Text(sample_text(u8"gallery.time-picker.submit · 保存时间")); });
              gallery_target("gallery.time-picker.submit");
              Button(ButtonProps{}.onClick([state] {
                  if (state->time_form_ref.reset()) {
                      state->time_feedback.set(String{u8"时间已重置"});
                  }
              }),
                     [] { Text(sample_text(u8"gallery.time-picker.reset · 重置时间")); });
              gallery_target("gallery.time-picker.reset");
              Button(
                  ButtonProps{}.onClick([state] { state->time_form_disabled.set(!state->time_form_disabled.get()); }),
                  [] { Text(sample_text(u8"gallery.time-picker.disabled · 切换禁用")); });
              gallery_target("gallery.time-picker.disabled");
              Button(
                  ButtonProps{}.onClick([state] { state->time_form_read_only.set(!state->time_form_read_only.get()); }),
                  [] { Text(sample_text(u8"gallery.time-picker.readonly · 切换只读")); });
              gallery_target("gallery.time-picker.readonly");
              Button(ButtonProps{}.onClick([state] {
                  state->time_help.set(state->time_help.get() == String{u8"HH:mm:ss，08:00:00 至 18:00:00，12 时不可选"}
                                           ? String{u8"仅确认后的 HH:mm:ss 字符串进入保存结果"}
                                           : String{u8"HH:mm:ss，08:00:00 至 18:00:00，12 时不可选"});
              }),
                     [] { Text(sample_text(u8"gallery.time-picker.help · 更新时间帮助")); });
              gallery_target("gallery.time-picker.help");
          }});
    state->telemetry.live_samples += 2;
}

void add_descriptions_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(sample_text(u8"gallery.descriptions.editor · Descriptions · 项目资料"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}.content(bind([state] {
                 return sample_text(state, utf8("gallery.descriptions.feedback · " +
                                                std::string{state->details_feedback.get().bytes()}));
             })));
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
                     .title(DescriptionsTitle{[] { Text(u8"项目资料"); }})
                     .layout(LayoutStyle{}
                                 .width(bind([state] { return dp(state->document_width.get().value() - 16); }))
                                 .margin_left(dp(8))
                                 .margin_top(dp(4))
                                 .margin_bottom(dp(4))),
                 {DescriptionsItem{u8"name", DescriptionsLabel{[] { Text(u8"名称"); }}, DescriptionsContent{[state] {
                                       Input(InputProps{}
                                                 .defaultValue(
                                                     gallery_initial_text("gallery.descriptions.name", u8"晨间项目"))
                                                 .placeholder(u8"gallery.descriptions.name")
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
                  DescriptionsItem{u8"role", DescriptionsLabel{[] { Text(u8"角色"); }}, DescriptionsContent{[state] {
                                       Select(SelectProps{}
                                                  .value(state->details_role)
                                                  .options(std::vector<SelectOption>{{u8"editor", u8"编辑者"},
                                                                                     {u8"reviewer", u8"审核者"}})
                                                  .onChange([state](SelectSelection value) {
                                                      state->details_role.set(value);
                                                  })
                                                  .layout(LayoutStyle{}
                                                              .width(dp(148))
                                                              .margin_left(dp(6))
                                                              .margin_right(dp(6))
                                                              .margin_top(dp(6))
                                                              .margin_bottom(dp(6))));
                                   }}}
                      .visible(state->details_visible),
                  DescriptionsItem{u8"enabled", DescriptionsLabel{[] { Text(u8"启用"); }}, DescriptionsContent{[state] {
                                       Switch(SwitchProps{}
                                                  .checked(state->details_enabled)
                                                  .onChange([state](bool value) { state->details_enabled.set(value); })
                                                  .layout(LayoutStyle{}
                                                              .margin_left(dp(6))
                                                              .margin_right(dp(6))
                                                              .margin_top(dp(6))
                                                              .margin_bottom(dp(6))));
                                   }}},
                  DescriptionsItem{u8"save", DescriptionsLabel{[] { Text(u8"操作"); }}, DescriptionsContent{[state] {
                                       Button(ButtonProps{}
                                                  .type(ButtonType::Primary)
                                                  .onClick([state] {
                                                      state->details_feedback.set(utf8(
                                                          "已保存 " + std::string{state->details_name.get().bytes()} +
                                                          (state->details_role.get() == SelectSelection{u8"reviewer"}
                                                               ? " · 审核者"
                                                               : " · 编辑者") +
                                                          (state->details_enabled.get() ? " · 已启用" : " · 已停用")));
                                                  })
                                                  .layout(LayoutStyle{}
                                                              .margin_left(dp(6))
                                                              .margin_right(dp(6))
                                                              .margin_top(dp(6))
                                                              .margin_bottom(dp(6))),
                                              [] { Text(u8"保存资料"); });
                                   }}}});
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}.onClick(
                                  [state] { state->details_columns.set(!state->details_columns.get()); }),
                              [] { Text(sample_text(u8"gallery.descriptions.columns · 单列/多列")); });
                       gallery_target("gallery.descriptions.columns");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->details_span.set(state->details_span.get() == 1 ? 2 : 1); }),
                              [] { Text(sample_text(u8"gallery.descriptions.span · 名称跨列")); });
                       gallery_target("gallery.descriptions.span");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->details_visible.set(!state->details_visible.get()); }),
                              [] { Text(sample_text(u8"gallery.descriptions.visible · 显隐资料")); });
                       gallery_target("gallery.descriptions.visible");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->details_vertical.set(!state->details_vertical.get()); }),
                              [] { Text(sample_text(u8"gallery.descriptions.vertical · 横纵标签")); });
                       gallery_target("gallery.descriptions.vertical");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->details_bordered.set(!state->details_bordered.get()); }),
                              [] { Text(sample_text(u8"gallery.descriptions.bordered · 切换边框")); });
                       gallery_target("gallery.descriptions.bordered");
                       Button(ButtonProps{}.onClick([state] {
                           state->details_size.set(
                               state->details_size.get() == DescriptionsSize::Default  ? DescriptionsSize::Middle
                               : state->details_size.get() == DescriptionsSize::Middle ? DescriptionsSize::Small
                                                                                       : DescriptionsSize::Default);
                       }),
                              [] { Text(sample_text(u8"gallery.descriptions.size · 切换尺寸")); });
                       gallery_target("gallery.descriptions.size");
                       Button(ButtonProps{}.onClick([state] {
                           state->details_visible.set(true);
                           static_cast<void>(state->details_ref.focus());
                       }),
                              [] { Text(sample_text(u8"gallery.descriptions.focus · 编辑名称")); });
                       gallery_target("gallery.descriptions.focus");
                   }});
         }});
    ++state->telemetry.live_samples;
}

void add_skeleton_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(sample_text(u8"gallery.skeleton.preview · Skeleton · 正文加载"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}.content(bind([state] {
                 return sample_text(
                     state, utf8("gallery.skeleton.feedback · " + std::string{state->skeleton_feedback.get().bytes()}));
             })));
             Space(
                 SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                 SpaceContent{[state] {
                     Button(ButtonProps{}.onClick(
                                [state] { state->skeleton_loading.set(!state->skeleton_loading.get()); }),
                            [] { Text(sample_text(u8"gallery.skeleton.loading · 加载/显示正文")); });
                     gallery_target("gallery.skeleton.loading");
                     Button(ButtonProps{}.onClick([state] { state->skeleton_title.set(!state->skeleton_title.get()); }),
                            [] { Text(sample_text(u8"gallery.skeleton.title · 标题")); });
                     gallery_target("gallery.skeleton.title");
                     Button(ButtonProps{}.onClick(
                                [state] { state->skeleton_paragraph.set((state->skeleton_paragraph.get() + 1) % 3); }),
                            [] { Text(sample_text(u8"gallery.skeleton.paragraph · 正文行数")); });
                     gallery_target("gallery.skeleton.paragraph");
                     Button(
                         ButtonProps{}.onClick([state] { state->skeleton_avatar.set(!state->skeleton_avatar.get()); }),
                         [] { Text(sample_text(u8"gallery.skeleton.avatar · 头像")); });
                     gallery_target("gallery.skeleton.avatar");
                     Button(ButtonProps{}.onClick([state] { state->skeleton_round.set(!state->skeleton_round.get()); }),
                            [] { Text(sample_text(u8"gallery.skeleton.round · 圆角")); });
                     gallery_target("gallery.skeleton.round");
                     Button(ButtonProps{}.onClick([state] {
                         state->skeleton_loading.set(false);
                         static_cast<void>(state->skeleton_ref.focus());
                     }),
                            [] { Text(sample_text(u8"gallery.skeleton.focus · 编辑正文")); });
                     gallery_target("gallery.skeleton.focus");
                 }});
             Skeleton(SkeletonProps{}
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
                                   Text(u8"项目动态");
                                   Input(InputProps{}
                                             .defaultValue(
                                                 gallery_initial_text("gallery.skeleton.draft", u8"今天整理项目资料"))
                                             .placeholder(u8"gallery.skeleton.draft")
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
                                   Select(SelectProps{}
                                              .value(state->skeleton_audience)
                                              .options(std::vector<SelectOption>{{u8"team", u8"团队可见"},
                                                                                 {u8"private", u8"仅自己"}})
                                              .onChange([state](SelectSelection value) {
                                                  state->skeleton_audience.set(value);
                                              })
                                              .layout(LayoutStyle{}
                                                          .width(dp(180))
                                                          .margin_left(dp(6))
                                                          .margin_right(dp(6))
                                                          .margin_top(dp(6))
                                                          .margin_bottom(dp(6))));
                                   Button(ButtonProps{}
                                              .type(ButtonType::Primary)
                                              .onClick([state] {
                                                  state->skeleton_feedback.set(utf8(
                                                      "已发布 " + std::string{state->skeleton_draft.get().bytes()} +
                                                      (state->skeleton_audience.get() == SelectSelection{u8"private"}
                                                           ? " · 仅自己"
                                                           : " · 团队可见")));
                                              })
                                              .layout(LayoutStyle{}
                                                          .margin_left(dp(6))
                                                          .margin_right(dp(6))
                                                          .margin_top(dp(6))
                                                          .margin_bottom(dp(6))),
                                          [] { Text(u8"发布动态"); });
                               }});
                      }});
             Text(sample_text(u8"gallery.skeleton.standalone · 独立占位"));
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}.onClick(
                                  [state] { state->skeleton_shape.set((state->skeleton_shape.get() + 1) % 4); }),
                              [] { Text(sample_text(u8"gallery.skeleton.shape · 切换形态与尺寸")); });
                       gallery_target("gallery.skeleton.shape");
                       Button(
                           ButtonProps{}.onClick([state] { state->skeleton_block.set(!state->skeleton_block.get()); }),
                           [] { Text(sample_text(u8"gallery.skeleton.block · 填满宽度")); });
                       gallery_target("gallery.skeleton.block");
                   }});
             Flex(FlexProps{}
                      .vertical(true)
                      .align(FlexAlign::Start)
                      .gap(dp(12))
                      .layout(LayoutStyle{}.width(state->document_width)),
                  FlexContent{[state] {
                      const auto size = bind([state] {
                          return state->skeleton_shape.get() % 2 == 0 ? ControlSize::Middle : ControlSize::Large;
                      });
                      SkeletonAvatar(SkeletonAvatarProps{}.size(size).shape(bind([state] {
                          return state->skeleton_shape.get() % 2 == 0 ? SkeletonAvatarShape::Circle
                                                                      : SkeletonAvatarShape::Square;
                      })));
                      SkeletonButton(SkeletonButtonProps{}.size(size).block(state->skeleton_block).shape(bind([state] {
                          return static_cast<SkeletonButtonShape>(state->skeleton_shape.get());
                      })));
                      SkeletonInput(SkeletonInputProps{}.size(size).block(state->skeleton_block));
                  }});
         }});
    state->telemetry.live_samples += 4;
}

void add_popover_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(sample_text(u8"gallery.popover.editor · Popover · 项目备注"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return sample_text(state, utf8("gallery.popover.feedback · " +
                                                         std::string{state->popover_feedback.get().bytes()}));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
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
                                      [] { Text(sample_text(u8"gallery.popover.trigger · 编辑备注")); });
                               gallery_target("gallery.popover.trigger");
                           }},
                           {.title = PopoverTitle{[] { Text(u8"项目备注"); }},
                            .content = PopoverContent{[state] {
                                Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)),
                                     FlexContent{[state] {
                                         Input(InputProps{}
                                                   .defaultValue(gallery_initial_text("gallery.popover.draft",
                                                                                      u8"下次会议前补齐项目备注"))
                                                   .placeholder(u8"gallery.popover.draft")
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
                                                   Button(ButtonProps{}
                                                              .type(ButtonType::Primary)
                                                              .onClick([state] {
                                                                  state->popover_feedback.set(utf8(
                                                                      "已保存 " +
                                                                      std::string{state->popover_draft.get().bytes()}));
                                                                  state->popover_open.set(false);
                                                              })
                                                              .layout(LayoutStyle{}
                                                                          .margin_left(dp(6))
                                                                          .margin_right(dp(6))
                                                                          .margin_top(dp(6))
                                                                          .margin_bottom(dp(6))),
                                                          [] { Text(u8"保存备注"); });
                                                   Button(ButtonProps{}
                                                              .onClick([state] { state->popover_open.set(false); })
                                                              .layout(LayoutStyle{}
                                                                          .margin_left(dp(6))
                                                                          .margin_right(dp(6))
                                                                          .margin_top(dp(6))
                                                                          .margin_bottom(dp(6))),
                                                          [] { Text(u8"取消编辑"); });
                                               }});
                                     }});
                            }}});
                       Button(ButtonProps{}.onClick(
                                  [state] { state->popover_placement.set((state->popover_placement.get() + 1) % 4); }),
                              [] { Text(sample_text(u8"gallery.popover.placement · 切换方位")); });
                       gallery_target("gallery.popover.placement");
                       Button(ButtonProps{}.onClick([state] { state->popover_arrow.set(!state->popover_arrow.get()); }),
                              [] { Text(sample_text(u8"gallery.popover.arrow · 切换箭头")); });
                       gallery_target("gallery.popover.arrow");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->popover_disabled.set(!state->popover_disabled.get()); }),
                              [] { Text(sample_text(u8"gallery.popover.disabled · 禁用/恢复")); });
                       gallery_target("gallery.popover.disabled");
                       Button(ButtonProps{}.onClick([state] {
                           state->popover_disabled.set(false);
                           state->popover_open.set(true);
                       }),
                              [] { Text(sample_text(u8"gallery.popover.restore · 重新打开")); });
                       gallery_target("gallery.popover.restore");
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
                      .content(sample_text(u8"gallery.notices.workflow · Message / Notification · 处理提示"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return sample_text(state, utf8("gallery.notices.feedback · " +
                                                         std::string{state->notice_feedback.get().bytes()}));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Space(
                 SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                 SpaceContent{[state] {
                     Button(ButtonProps{}.onClick([state] {
                         static_cast<void>(state->messages.open(
                             u8"project-save", MessageProps{}
                                                   .content(u8"正在处理项目")
                                                   .status(NoticeStatus::Loading)
                                                   .duration(0)
                                                   .onClose([state] { state->notice_feedback.set(u8"消息已关闭"); })));
                         state->notice_feedback.set(u8"处理提示已打开");
                     }),
                            [] { Text(sample_text(u8"gallery.message.open · 处理提示")); });
                     gallery_target("gallery.message.open");
                     Button(ButtonProps{}.onClick([state] {
                         static_cast<void>(state->messages.open(
                             u8"project-save",
                             MessageProps{}.content(u8"项目处理成功").status(NoticeStatus::Success).duration(0)));
                         state->notice_feedback.set(u8"同一提示已更新为处理成功");
                     }),
                            [] { Text(sample_text(u8"gallery.message.update · 更新成功")); });
                     gallery_target("gallery.message.update");
                     Button(
                         ButtonProps{}.onClick([state] { static_cast<void>(state->messages.close(u8"project-save")); }),
                         [] { Text(sample_text(u8"gallery.message.close · 关闭提示")); });
                     gallery_target("gallery.message.close");
                     Button(ButtonProps{}.disabled(state->notification_open).onClick([state] {
                         const bool opened = state->notifications.open(
                             u8"project-review",
                             NotificationProps{}
                                 .title(u8"项目资料待核对")
                                 .status(state->notification_status)
                                 .placement(state->notification_placement)
                                 .duration(0)
                                 .onClose([state] {
                                     state->notification_open.set(false);
                                     state->notice_feedback.set(u8"通知已关闭，可重新打开");
                                 }),
                             {.description = NotificationDescription{[state] {
                                  GalleryDraftBuild draft_build{state};
                                  Input(InputProps{}
                                            .defaultValue(gallery_initial_text("gallery.notification.draft",
                                                                               state->notification_draft.get()))
                                            .placeholder(u8"编辑通知备注")
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
                                                 state->notice_feedback.set(utf8(
                                                     "已确认 " + std::string{state->notification_draft.get().bytes()}));
                                             })
                                             .layout(LayoutStyle{}
                                                         .margin_left(dp(6))
                                                         .margin_right(dp(6))
                                                         .margin_top(dp(6))
                                                         .margin_bottom(dp(6))),
                                         [] { Text(u8"确认核对"); });
                              }}});
                         state->notification_open.set(opened);
                         if (opened) {
                             state->notice_feedback.set(u8"通知已打开，可编辑核对备注");
                         }
                     }),
                            [] { Text(sample_text(u8"gallery.notification.open · 打开通知")); });
                     gallery_target("gallery.notification.open");
                     Button(ButtonProps{}
                                .disabled(bind([state] { return !state->notification_open.get(); }))
                                .onClick([state] {
                                    static_cast<void>(state->notifications.open(
                                        u8"project-review", NotificationProps{}.title(u8"项目资料已更新")));
                                    state->notice_feedback.set(u8"已更新通知标题，编辑内容保持");
                                }),
                            [] { Text(sample_text(u8"gallery.notification.update · 更新标题")); });
                     gallery_target("gallery.notification.update");
                     Button(ButtonProps{}.onClick(
                                [state] { static_cast<void>(state->notifications.close(u8"project-review")); }),
                            [] { Text(sample_text(u8"gallery.notification.close · 关闭通知")); });
                     gallery_target("gallery.notification.close");
                     Button(ButtonProps{}.onClick([state] {
                         state->notification_placement.set(static_cast<NotificationPlacement>(
                             (static_cast<int>(state->notification_placement.get()) + 1) % 4));
                     }),
                            [] { Text(sample_text(u8"gallery.notification.placement · 四角位置")); });
                     gallery_target("gallery.notification.placement");
                     Button(ButtonProps{}.onClick([state] {
                         state->notification_status.set(
                             static_cast<NoticeStatus>((static_cast<int>(state->notification_status.get()) + 1) % 5));
                     }),
                            [] { Text(sample_text(u8"gallery.notification.status · 切换状态")); });
                     gallery_target("gallery.notification.status");
                     Button(ButtonProps{}.onClick([state] {
                         static_cast<void>(state->messages.clear());
                         static_cast<void>(state->notifications.clear());
                         state->notice_feedback.set(u8"已清空消息与通知");
                     }),
                            [] { Text(sample_text(u8"gallery.notices.clear · 清空提示")); });
                     gallery_target("gallery.notices.clear");
                 }});
         }});
    state->telemetry.live_samples += 2;
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
                      .content(sample_text(u8"gallery.timeline.workflow · Timeline · 项目处理记录"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return sample_text(state, utf8("gallery.timeline.feedback · " +
                                                         std::string{state->timeline_feedback.get().bytes()}));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Timeline(TimelineProps{}
                          .mode(bind([state] { return static_cast<TimelineMode>(state->timeline_mode.get()); }))
                          .reverse(state->timeline_reverse)
                          .layout(LayoutStyle{}
                                      .width(bind([state] { return dp(state->document_width.get().value() - 16); }))
                                      .margin_left(dp(8))
                                      .margin_right(dp(8))),
                      {TimelineItem{u8"created", u8"创建项目并填写资料"}.title(u8"09:00").tone(TimelineTone::Green),
                       TimelineItem{u8"review", u8"核对项目材料"}.title(u8"09:30").loading(state->timeline_loading),
                       TimelineItem{u8"publish", u8""}
                           .title(u8"10:00")
                           .visible(state->timeline_visible)
                           .richContent(TimelineContent{[state] {
                               Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)), FlexContent{[state] {
                                        Input(InputProps{}
                                                  .defaultValue(gallery_initial_text("gallery.timeline.draft",
                                                                                     u8"记录发布后的跟进事项"))
                                                  .placeholder(u8"gallery.timeline.draft")
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
                                                       state->timeline_feedback.set(
                                                           utf8("已记录 " +
                                                                std::string{state->timeline_draft.get().bytes()}));
                                                   })
                                                   .layout(LayoutStyle{}
                                                               .margin_left(dp(6))
                                                               .margin_right(dp(6))
                                                               .margin_top(dp(6))
                                                               .margin_bottom(dp(6))),
                                               [] { Text(u8"保存处理备注"); });
                                    }});
                           }})});
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}.onClick(
                                  [state] { state->timeline_mode.set((state->timeline_mode.get() + 1) % 3); }),
                              [] { Text(sample_text(u8"gallery.timeline.mode · 切换位置")); });
                       gallery_target("gallery.timeline.mode");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->timeline_reverse.set(!state->timeline_reverse.get()); }),
                              [] { Text(sample_text(u8"gallery.timeline.reverse · 正序/反序")); });
                       gallery_target("gallery.timeline.reverse");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->timeline_loading.set(!state->timeline_loading.get()); }),
                              [] { Text(sample_text(u8"gallery.timeline.loading · 切换处理中")); });
                       gallery_target("gallery.timeline.loading");
                       Button(ButtonProps{}.onClick(
                                  [state] { state->timeline_visible.set(!state->timeline_visible.get()); }),
                              [] { Text(sample_text(u8"gallery.timeline.visible · 显隐备注")); });
                       gallery_target("gallery.timeline.visible");
                       Button(ButtonProps{}.onClick([state] {
                           state->timeline_visible.set(true);
                           static_cast<void>(state->timeline_ref.focus());
                       }),
                              [] { Text(sample_text(u8"gallery.timeline.focus · 编辑备注")); });
                       gallery_target("gallery.timeline.focus");
                   }});
         }});
    ++state->telemetry.live_samples;
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
                      .content(sample_text(u8"gallery.avatar.members · Avatar · 成员头像"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return sample_text(state, utf8("gallery.avatar.feedback · " +
                                                         std::string{state->member_feedback.get().bytes()}));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
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
                                            .placeholder(u8"gallery.avatar.name")
                                            .ref(state->member_ref)
                                            .onChange(gallery_value_changed<ryn::String>(
                                                "gallery.avatar.name",
                                                [state](String value) { state->member_name.set(value); }))
                                            .layout(member_control_margin().width(dp(160))));
                                  gallery_target("gallery.avatar.name");
                                  Button(ButtonProps{}
                                             .onClick([state] {
                                                 state->member_feedback.set(utf8(
                                                     "已保存成员 " + std::string{state->member_name.get().bytes()}));
                                             })
                                             .layout(member_control_margin()),
                                         [] { Text(u8"保存成员"); });
                              }});
                     }}});
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}
                                  .onClick([state] { state->member_icon.set(!state->member_icon.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.avatar.icon · 文字/图标")); });
                       gallery_target("gallery.avatar.icon");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->member_shape.set(state->member_shape.get() == AvatarShape::Circle
                                                                  ? AvatarShape::Square
                                                                  : AvatarShape::Circle);
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.avatar.shape · 圆形/方形")); });
                       gallery_target("gallery.avatar.shape");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->member_size.set(state->member_size.get() ==
                                                                     AvatarSizeValue{AvatarSize::Large}
                                                                 ? AvatarSizeValue{AvatarSize::Small}
                                                                 : AvatarSizeValue{AvatarSize::Large});
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.avatar.size · 大/小头像")); });
                       gallery_target("gallery.avatar.size");
                       Button(ButtonProps{}
                                  .onClick([state] { state->member_limit.set(state->member_limit.get() == 2 ? 0 : 2); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.avatar.limit · 显示2/0人")); });
                       gallery_target("gallery.avatar.limit");
                       Button(ButtonProps{}
                                  .onClick([state] { state->members_visible.set(!state->members_visible.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.avatar.visible · 显隐成员")); });
                       gallery_target("gallery.avatar.visible");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->members_visible.set(true);
                                      static_cast<void>(state->member_ref.focus());
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.avatar.restore · 恢复编辑")); });
                       gallery_target("gallery.avatar.restore");
                   }});
         }});
    state->telemetry.live_samples += 4;
}

void add_statistic_metric_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(sample_text(u8"gallery.statistic.metrics · Statistic · 统计指标"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return sample_text(state,
                                             utf8("gallery.statistic.feedback · 原值=" +
                                                  std::string{metric_raw_value(state->metric_value.get()).bytes()} +
                                                  " · " + std::string{state->metric_feedback.get().bytes()}));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Statistic(StatisticProps{}
                           .title(u8"本期数值")
                           .value(state->metric_value)
                           .precision(state->metric_precision)
                           .groupSeparator(
                               bind([state] { return state->metric_separators.get() ? String{u8" "} : String{u8","}; }))
                           .decimalSeparator(
                               bind([state] { return state->metric_separators.get() ? String{u8","} : String{u8"."}; }))
                           .formatter(bind([state] {
                               return state->metric_formatter.get()
                                          ? StatisticFormatter{[](const StatisticValue& value) {
                                                return utf8("原值:" + std::string{metric_raw_value(value).bytes()});
                                            }}
                                          : StatisticFormatter{};
                           }))
                           .loading(state->metric_loading)
                           .layout(member_control_margin().width(
                               bind([state] { return dp(state->document_width.get().value() - 12); }))));
             Statistic(StatisticProps{}
                           .title(u8"指标备注")
                           .value(state->metric_value)
                           .loading(state->metric_loading)
                           .richValue(StatisticValueContent{[state] {
                               Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)), FlexContent{[state] {
                                        Input(InputProps{}
                                                  .defaultValue(
                                                      gallery_initial_text("gallery.statistic.note", u8"核对本期指标"))
                                                  .placeholder(u8"gallery.statistic.draft")
                                                  .ref(state->metric_ref)
                                                  .onChange(gallery_value_changed<ryn::String>(
                                                      "gallery.statistic.note",
                                                      [state](String value) { state->metric_note.set(value); }))
                                                  .layout(member_control_margin().width(dp(180))));
                                        gallery_target("gallery.statistic.note");
                                        Button(ButtonProps{}
                                                   .onClick([state] {
                                                       state->metric_feedback.set(utf8(
                                                           "已保存 " + std::string{state->metric_note.get().bytes()}));
                                                   })
                                                   .layout(member_control_margin()),
                                               [] { Text(u8"保存指标备注"); });
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
                              [] { Text(sample_text(u8"gallery.statistic.value · 切换业务值")); });
                       gallery_target("gallery.statistic.value");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      const auto p = state->metric_precision.get();
                                      state->metric_precision.set(p == 2   ? std::optional{8}
                                                                  : p == 8 ? std::nullopt
                                                                           : std::optional{2});
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.statistic.precision · 2/8/原精度")); });
                       gallery_target("gallery.statistic.precision");
                       Button(ButtonProps{}
                                  .onClick([state] { state->metric_separators.set(!state->metric_separators.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.statistic.separators · 切换分隔符")); });
                       gallery_target("gallery.statistic.separators");
                       Button(ButtonProps{}
                                  .onClick([state] { state->metric_formatter.set(!state->metric_formatter.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.statistic.formatter · 切换formatter")); });
                       gallery_target("gallery.statistic.formatter");
                       Button(ButtonProps{}
                                  .onClick([state] { state->metric_loading.set(!state->metric_loading.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.statistic.loading · 加载/完成")); });
                       gallery_target("gallery.statistic.loading");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->metric_loading.set(false);
                                      static_cast<void>(state->metric_ref.focus());
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.statistic.restore · 恢复指标编辑")); });
                       gallery_target("gallery.statistic.restore");
                   }});
         }});
    state->telemetry.live_samples += 2;
}

void add_service_rate_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(sample_text(u8"gallery.rate.service · Rate · 服务评分"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return sample_text(
                              state, utf8("gallery.rate.feedback · 已接受=" +
                                          std::string{
                                              metric_raw_value(StatisticValue{static_cast<double>(state->rating.get())})
                                                  .bytes()} +
                                          (state->rating_accept.get() ? " · 接受修改" : " · 拒绝修改") +
                                          (state->rating_disabled.get() ? " · 禁用" : "") +
                                          (state->rating_read_only.get() ? " · 只读" : "") + " · " +
                                          std::string{state->rating_request.get().bytes()} + " · " +
                                          std::string{state->rating_feedback.get().bytes()}));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Rate(RateProps{}
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
                          state->rating_request.set(utf8(
                              std::string{state->rating_accept.get() ? "已接受候选=" : "已拒绝候选="} +
                              std::string{metric_raw_value(StatisticValue{static_cast<double>(candidate)}).bytes()}));
                      })
                      .tooltips(
                          {u8"需要改进", u8"一般", u8"满意", u8"很好", RateTooltip{TooltipTitle{[state] {
                               Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)), FlexContent{[state] {
                                        Input(InputProps{}
                                                  .defaultValue(
                                                      gallery_initial_text("gallery.rate.note", u8"补充服务评价"))
                                                  .placeholder(u8"gallery.rate.draft")
                                                  .ref(state->rating_ref)
                                                  .onChange(gallery_value_changed<ryn::String>(
                                                      "gallery.rate.note",
                                                      [state](String value) { state->rating_note.set(value); }))
                                                  .layout(member_control_margin().width(dp(180))));
                                        gallery_target("gallery.rate.note");
                                        Button(ButtonProps{}
                                                   .onClick([state] {
                                                       state->rating_feedback.set(utf8(
                                                           "已保存 " + std::string{state->rating_note.get().bytes()}));
                                                   })
                                                   .layout(member_control_margin()),
                                               [] { Text(u8"保存服务评价"); });
                                    }});
                           }}}})
                      .layout(member_control_margin()));
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}
                                  .onClick([state] { state->rating_accept.set(!state->rating_accept.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.rate.authority · 接受/拒绝修改")); });
                       gallery_target("gallery.rate.authority");
                       Button(ButtonProps{}
                                  .onClick([state] { state->rating_disabled.set(!state->rating_disabled.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.rate.disabled · 禁用/启用评分")); });
                       gallery_target("gallery.rate.disabled");
                       Button(ButtonProps{}
                                  .onClick([state] { state->rating_read_only.set(!state->rating_read_only.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.rate.readonly · 只读/编辑评分")); });
                       gallery_target("gallery.rate.readonly");
                       Button(ButtonProps{}
                                  .onClick([state] { state->rating_visible.set(!state->rating_visible.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.rate.visible · 显隐评分")); });
                       gallery_target("gallery.rate.visible");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->rating_visible.set(true);
                                      state->rating_disabled.set(false);
                                      state->rating_read_only.set(false);
                                      state->rating_accept.set(true);
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.rate.restore · 恢复评分")); });
                       gallery_target("gallery.rate.restore");
                   }});
         }});
    ++state->telemetry.live_samples;
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
                      .content(sample_text(u8"gallery.auto-complete.contacts · AutoComplete · 联系人建议"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return sample_text(state, utf8("gallery.auto-complete.feedback · 接受值=" +
                                                         std::string{state->contact_value.get().bytes()} + " · " +
                                                         std::string{state->contact_request.get().bytes()} + " · " +
                                                         std::string{state->contact_saved.get().bytes()}));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             const AutoCompleteRichLabel rich{AutoCompleteOptionLabel{[state] {
                 Flex(FlexProps{}.align(FlexAlign::Center).gap(dp(4)), FlexContent{[state] {
                          Input(InputProps{}
                                    .value(state->contact_note)
                                    .size(ControlSize::Small)
                                    .ref(state->contact_note_ref)
                                    .onChange([state](String value) { state->contact_note.set(value); })
                                    .placeholder(u8"gallery.auto-complete.note")
                                    .layout(
                                        member_control_margin().margin_top(dp(0)).margin_bottom(dp(0)).width(dp(132))));
                          Button(ButtonProps{}
                                     .size(ControlSize::Small)
                                     .onClick([state] {
                                         state->contact_saved.set(
                                             utf8("已保存 " + std::string{state->contact_note.get().bytes()}));
                                     })
                                     .layout(member_control_margin().margin_top(dp(0)).margin_bottom(dp(0))),
                                 [] { Text(u8"保存联系人备注"); });
                      }});
             }}};
             AutoComplete(AutoCompleteProps{}
                              .value(state->contact_value)
                              .ref(state->contact_ref)
                              .placeholder(u8"gallery.auto-complete.input · 输入或选择联系人")
                              .filterOption(AutoCompleteFilter{true})
                              .backfill()
                              .allowClear()
                              .listHeight(224)
                              .disabled(state->contact_disabled)
                              .readOnly(state->contact_read_only)
                              .options(bind([state, rich] {
                                  std::vector<AutoCompleteOption> options{
                                      {u8"alice", u8"alice@example.com", u8"Alice"},
                                      {u8"blocked", u8"blocked@example.com", u8"停用联系人", true},
                                      {u8"chen", u8"chen@example.com", u8"Chen"},
                                      {u8"notes", u8"notes@example.com", u8"备注联系人", false, rich}};
                                  if (state->contact_extra.get()) {
                                      options.push_back({u8"dora", u8"dora@example.com", u8"Dora"});
                                  }
                                  return options;
                              }))
                              .onChange([state](String value) {
                                  state->contact_request.set(
                                      utf8(std::string{state->contact_accept.get() ? "接受候选=" : "拒绝候选="} +
                                           std::string{value.bytes()}));
                                  if (state->contact_accept.get()) {
                                      state->contact_value.set(value);
                                  }
                              })
                              .onSelect([state](String value, AutoCompleteOption) {
                                  state->contact_request.set(utf8("选择候选=" + std::string{value.bytes()}));
                              })
                              .layout(member_control_margin().width(dp(320))));
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}
                                  .onClick([state] { state->contact_accept.set(!state->contact_accept.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.auto-complete.authority · 接受/拒绝修改")); });
                       gallery_target("gallery.auto-complete.authority");
                       Button(ButtonProps{}
                                  .onClick([state] { state->contact_extra.set(!state->contact_extra.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.auto-complete.options · 更新建议")); });
                       gallery_target("gallery.auto-complete.options");
                       Button(ButtonProps{}
                                  .onClick([state] { state->contact_disabled.set(!state->contact_disabled.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.auto-complete.disabled · 禁用/启用")); });
                       gallery_target("gallery.auto-complete.disabled");
                       Button(ButtonProps{}
                                  .onClick([state] { state->contact_read_only.set(!state->contact_read_only.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.auto-complete.readonly · 只读/编辑")); });
                       gallery_target("gallery.auto-complete.readonly");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->contact_accept.set(true);
                                      state->contact_disabled.set(false);
                                      state->contact_read_only.set(false);
                                      static_cast<void>(state->contact_ref.focus());
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.auto-complete.restore · 恢复联系人输入")); });
                       gallery_target("gallery.auto-complete.restore");
                   }});
             Text(TextProps{}
                      .content(bind([state] {
                          return sample_text(state, utf8("gallery.auto-complete.form-feedback · " +
                                                         std::string{state->contact_form_feedback.get().bytes()}));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Form(FormProps{}
                      .ref(state->contact_form)
                      .disabled(state->contact_disabled)
                      .readOnly(state->contact_read_only)
                      .onFinish([state](FormResult) {
                          state->contact_form_feedback.set(
                              utf8("已保存 " + std::string{state->contact_field.get().bytes()}));
                      })
                      .layout(LayoutStyle{}.width(state->document_width)),
                  FormContent{[state] {
                      FormItem(FormItemProps{}
                                   .field(u8"contact", state->contact_field)
                                   .label(String{u8"表单联系人"})
                                   .control(state->contact_form_ref),
                               FormItemContent{[state] {
                                   AutoComplete(
                                       AutoCompleteProps{}
                                           .value(state->contact_field.value())
                                           .onChange(state->contact_field.onChange())
                                           .ref(state->contact_form_ref)
                                           .placeholder(u8"gallery.auto-complete.form")
                                           .options({{u8"ada", u8"Ada", u8"Ada"}, {u8"grace", u8"Grace", u8"Grace"}})
                                           .defaultActiveFirstOption(false)
                                           .backfill()
                                           .allowClear()
                                           .disabled(state->contact_field.disabled())
                                           .readOnly(state->contact_field.readOnly())
                                           .status(state->contact_field.status())
                                           .layout(member_control_margin().width(dp(260))));
                               }});
                      Space(SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
                                Button(ButtonProps{}
                                           .onClick([state] { static_cast<void>(state->contact_form.submit()); })
                                           .layout(member_control_margin()),
                                       [] { Text(sample_text(u8"gallery.auto-complete.submit · 保存表单联系人")); });
                                gallery_target("gallery.auto-complete.submit");
                                Button(ButtonProps{}
                                           .onClick([state] {
                                               static_cast<void>(state->contact_form.reset());
                                               state->contact_form_feedback.set(String{u8"已重置联系人"});
                                           })
                                           .layout(member_control_margin()),
                                       [] { Text(sample_text(u8"gallery.auto-complete.reset · 重置联系人")); });
                                gallery_target("gallery.auto-complete.reset");
                            }});
                  }});
         }});
    state->telemetry.live_samples += 2;
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

void add_project_tree_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             Text(TextProps{}
                      .content(sample_text(u8"gallery.tree.project · Tree · 项目目录"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          const auto [checked, half] =
                              gallery_project_checks(state->project_checked.get(), state->project_locked.get());
                          return sample_text(
                              state, utf8("gallery.tree.feedback · 选择=" +
                                          std::string{gallery_project_keys(state->project_selected.get()).bytes()} +
                                          " · 勾选=" + std::string{gallery_project_keys(checked).bytes()} +
                                          " · 半选=" + std::string{gallery_project_keys(half).bytes()} + " · " +
                                          std::string{state->project_saved.get().bytes()}));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             std::vector<TreeItem> assets;
             for (int i = 1; i <= 16; ++i) {
                 assets.emplace_back(utf8("asset-" + std::to_string(i)), utf8("资源 " + std::to_string(i)));
             }
             TreeItem notes{u8"notes", u8"处理备注"};
             notes.richTitle(TreeTitle{[state] {
                 Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(4)), FlexContent{[state] {
                          Input(InputProps{}
                                    .value(state->project_note)
                                    .ref(state->project_note_ref)
                                    .onChange([state](String value) { state->project_note.set(value); })
                                    .placeholder(u8"gallery.tree.note")
                                    .layout(member_control_margin().width(dp(180))));
                          Button(ButtonProps{}
                                     .onClick([state] {
                                         state->project_saved.set(
                                             utf8("已保存 " + std::string{state->project_note.get().bytes()}));
                                     })
                                     .layout(member_control_margin()),
                                 [] { Text(u8"保存项目备注"); });
                      }});
             }});
             Tree(
                 TreeProps{}
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
                 {TreeItem{
                      u8"project",
                      u8"RynUI 项目",
                      {TreeItem{u8"src", u8"源代码", {TreeItem{u8"controls", u8"控件"}, TreeItem{u8"theme", u8"主题"}}},
                       TreeItem{u8"docs",
                                u8"文档",
                                {TreeItem{u8"guide", u8"使用指南"},
                                 TreeItem{u8"locked", u8"只读归档"}.disabled(state->project_locked)}},
                       TreeItem{u8"assets", u8"资源", std::move(assets)}}},
                  std::move(notes)});
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}
                                  .onClick([state] { state->project_disabled.set(!state->project_disabled.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.tree.disabled · 禁用/启用项目树")); });
                       gallery_target("gallery.tree.disabled");
                       Button(ButtonProps{}
                                  .onClick([state] { state->project_visible.set(!state->project_visible.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.tree.visible · 显隐项目树")); });
                       gallery_target("gallery.tree.visible");
                       Button(ButtonProps{}
                                  .onClick([state] { state->project_locked.set(!state->project_locked.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.tree.locked · 归档分支禁用/恢复")); });
                       gallery_target("gallery.tree.locked");
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->project_visible.set(true);
                                      state->project_disabled.set(false);
                                      state->project_expanded.set(TreeKeys{});
                                      static_cast<void>(state->project_note_ref.focus());
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(sample_text(u8"gallery.tree.restore · 恢复项目备注")); });
                       gallery_target("gallery.tree.restore");
                   }});
         }});
    ++state->telemetry.live_samples;
}

void add_tree_select_destination_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Flex(FlexProps{}
             .vertical(true)
             .align(FlexAlign::Start)
             .gap(dp(12))
             .layout(LayoutStyle{}.width(state->document_width)),
         FlexContent{[state] {
             const auto items = [state] {
                 std::vector<TreeSelectItem> assets;
                 for (int i = 1; i <= 14; ++i) {
                     assets.emplace_back(utf8("deliverable-" + std::to_string(i)), utf8("交付件 " + std::to_string(i)));
                 }
                 return std::vector<TreeSelectItem>{TreeSelectItem{
                     u8"project",
                     u8"RynUI 项目",
                     {TreeSelectItem{
                          u8"docs",
                          u8"文档",
                          {TreeSelectItem{u8"guide", bind([state] {
                                              return String{state->destination_renamed.get() ? u8"上手指南"
                                                                                             : u8"使用指南"};
                                          })},
                           TreeSelectItem{u8"archive", u8"归档"}.disabled(state->destination_archive_disabled)}},
                      TreeSelectItem{u8"assets", u8"交付件", std::move(assets)}}}};
             };
             Text(TextProps{}
                      .content(sample_text(u8"gallery.tree-select.destination · TreeSelect · 层级目标"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.tree-select.destination");
             Text(TextProps{}
                      .content(bind([state] {
                          return sample_text(state,
                                             utf8("gallery.tree-select.feedback · 接受值=" +
                                                  std::string{state->destination_value.get().bytes()} +
                                                  " · 关闭请求=" + std::to_string(state->destination_closes.get()) +
                                                  " · " + std::string{state->destination_request.get().bytes()} +
                                                  " · " + std::string{state->destination_saved.get().bytes()}));
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
                            [] { Text(sample_text(u8"gallery.tree-select.value-authority · 接受/拒绝选择")); });
                     remember_target(state, "gallery.tree-select.value-authority");
                     Button(ButtonProps{}
                                .onClick([state] {
                                    state->destination_accept_close.set(!state->destination_accept_close.get());
                                })
                                .layout(member_control_margin()),
                            [] { Text(sample_text(u8"gallery.tree-select.close-authority · 接受/拒绝关闭")); });
                     remember_target(state, "gallery.tree-select.close-authority");
                     Button(ButtonProps{}
                                .onClick([state] { state->destination_renamed.set(!state->destination_renamed.get()); })
                                .layout(member_control_margin()),
                            [] { Text(sample_text(u8"gallery.tree-select.rename · 切换指南标题")); });
                     remember_target(state, "gallery.tree-select.rename");
                     Button(ButtonProps{}
                                .onClick([state] {
                                    state->destination_archive_disabled.set(!state->destination_archive_disabled.get());
                                })
                                .layout(member_control_margin()),
                            [] { Text(sample_text(u8"gallery.tree-select.node-disabled · 禁用/启用归档")); });
                     remember_target(state, "gallery.tree-select.node-disabled");
                     Button(
                         ButtonProps{}
                             .onClick([state] { state->destination_disabled.set(!state->destination_disabled.get()); })
                             .layout(member_control_margin()),
                         [] { Text(sample_text(u8"gallery.tree-select.disabled · 禁用/启用选择器")); });
                     remember_target(state, "gallery.tree-select.disabled");
                     Button(ButtonProps{}
                                .onClick(
                                    [state] { state->destination_read_only.set(!state->destination_read_only.get()); })
                                .layout(member_control_margin()),
                            [] { Text(sample_text(u8"gallery.tree-select.readonly · 只读/恢复选择")); });
                     remember_target(state, "gallery.tree-select.readonly");
                     Button(ButtonProps{}
                                .onClick([state] { state->destination_visible.set(!state->destination_visible.get()); })
                                .layout(member_control_margin()),
                            [] { Text(sample_text(u8"gallery.tree-select.visible · 显隐选择器")); });
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
                            [] { Text(sample_text(u8"gallery.tree-select.restore · 恢复目标选择")); });
                     remember_target(state, "gallery.tree-select.restore");
                 }});
             TreeSelect(TreeSelectProps{}
                            .value(state->destination_value)
                            .open(state->destination_open)
                            .treeExpandedKeys(state->destination_expanded)
                            .ref(state->destination_ref)
                            .allowClear()
                            .placeholder(u8"gallery.tree-select.selector · 选择交付目标")
                            .listHeight(160)
                            .disabled(state->destination_disabled)
                            .readOnly(state->destination_read_only)
                            .visible(state->destination_visible)
                            .onChange([state](String value) {
                                state->destination_request.set(utf8(
                                    std::string{state->destination_accept_value.get() ? "接受选择=" : "拒绝选择="} +
                                    std::string{value.bytes()}));
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
                            .onTreeExpand(
                                [state](TreeExpandInfo info) { state->destination_expanded.set(info.expanded_keys); })
                            .layout(member_control_margin().width(dp(300))),
                        items());
             remember_target(state, "gallery.tree-select.selector");
             Input(InputProps{}
                       .value(state->destination_note)
                       .ref(state->destination_note_ref)
                       .onChange([state](String value) { state->destination_note.set(value); })
                       .placeholder(u8"gallery.tree-select.note · 交付备注")
                       .layout(member_control_margin().width(dp(300))));
             remember_target(state, "gallery.tree-select.note");
             Button(ButtonProps{}
                        .onClick([state] {
                            state->destination_saved.set(
                                utf8("已保存目标=" + std::string{state->destination_value.get().bytes()} + " / " +
                                     std::string{state->destination_note.get().bytes()}));
                        })
                        .layout(member_control_margin()),
                    [] { Text(sample_text(u8"gallery.tree-select.save · 保存目标备注")); });
             remember_target(state, "gallery.tree-select.save");
             Text(TextProps{}
                      .content(sample_text(u8"gallery.tree-select.form · Form · 交付目标"))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.tree-select.form");
             Text(TextProps{}
                      .content(bind([state] {
                          return sample_text(state, utf8("gallery.tree-select.form-feedback · " +
                                                         std::string{state->destination_form_feedback.get().bytes()}));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             remember_target(state, "gallery.tree-select.form-feedback");
             Form(FormProps{}
                      .ref(state->destination_form)
                      .onFinish([state](FormResult) {
                          state->destination_form_feedback.set(
                              utf8("已提交目标=" + std::string{state->destination_field.get().bytes()}));
                      })
                      .onFinishFailed(
                          [state](FormResult) { state->destination_form_feedback.set(String{u8"请选择交付目标"}); })
                      .layout(LayoutStyle{}.width(state->document_width)),
                  FormContent{[state, items] {
                      FormItem(FormItemProps{}
                                   .field(u8"destination", state->destination_field)
                                   .label(String{u8"交付目标"})
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
                                                  .placeholder(u8"请选择表单目标")
                                                  .layout(member_control_margin().width(dp(280))),
                                              items());
                                   remember_target(state, "gallery.tree-select.form-selector");
                               }});
                      Space(SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
                                Button(ButtonProps{}
                                           .onClick([state] { static_cast<void>(state->destination_form.submit()); })
                                           .layout(member_control_margin()),
                                       [] { Text(sample_text(u8"gallery.tree-select.submit · 提交交付目标")); });
                                remember_target(state, "gallery.tree-select.submit");
                                Button(ButtonProps{}
                                           .onClick([state] {
                                               static_cast<void>(state->destination_form_ref.focus());
                                               static_cast<void>(state->destination_form.reset());
                                               state->destination_form_feedback.set(String{u8"已重置目标"});
                                           })
                                           .layout(member_control_margin()),
                                       [] { Text(sample_text(u8"gallery.tree-select.reset · 聚焦后重置目标")); });
                                remember_target(state, "gallery.tree-select.reset");
                            }});
                  }});
         }});
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

constexpr auto sample_factories = std::to_array<GallerySampleFactory>({
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
    {{"gallery.sample.message.workflow",
      "ant.component.message",
      {},
      "Message 与 Notification 处理流程",
      "add_notice_samples",
      "include/ryn/message.hpp"},
     add_notice_samples},
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
        if constexpr (requires { field.get(); }) {
            auto initial = field.get();
            if constexpr (std::same_as<std::remove_cvref_t<decltype(initial)>, ryn::String>) {
                initial = sample_text(state, std::move(initial));
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
                                             ryn::Text(ryn::TextProps{}.content(utf8(factory.descriptor.title)));
                                             if (state->page_family == GalleryInputFamily::password ||
                                                 state->page_family == GalleryInputFamily::otp) {
                                                 ryn::Text(u8"离开此分组会清空输入；不保留密码或验证码草稿。");
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
                ryn::Text(ryn::TextProps{}
                              .content(utf8(gallery_page_empty_message(entry->support_status)))
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
                if constexpr (requires { field.get(); }) {
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
                                            document_text(utf8("组件"), 14.0F, 28.0F, 500);
                                            document_text(utf8("Design Tokens"), 14.0F, 28.0F, 400,
                                                          ryn::TextTone::Secondary);
                                            document_text(utf8("Ant Design 6.6.5"), 12.0F, 28.0F, 400,
                                                          ryn::TextTone::Secondary);
                                            ryn::Button(
                                                ryn::ButtonProps{}.size(ryn::ControlSize::Small).onClick([state] {
                                                    state->navigation_request =
                                                        GalleryNavigationTarget::to_navigation();
                                                    ++state->telemetry.navigation_requests;
                                                }),
                                                [] { ryn::Text(u8"目录"); });
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
                                                    ryn::Text(u8"主题");
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
