#include "token_gallery_definition.hpp"

#include "ant_design_reference_catalog.hpp"
#include "gallery_document_model.hpp"
#include "gallery_layout.hpp"
#include "reference_surface.hpp"
#include "icon_samples.hpp"

#include <ryn/rynui.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <memory>
#include <optional>
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

    GalleryState() {
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
        background_color = ryn::resolve_theme(config).alias().color_background_container;
        theme.set(config);
        ++telemetry.theme_updates;
        if (brand) {
            ++telemetry.brand_updates;
        }
    }
};

const std::size_t navigation_control_count = 20 + ant_design_reference_entries().size();

constexpr auto stable_test_ids = std::to_array<std::string_view>({
    "gallery.flex.baseline",
    "gallery.flex.wrap-reverse",
    "gallery.flex.vertical-rtl",
    "gallery.flex.default-stretch",
    "gallery.space.separator",
    "gallery.space.baseline",
    "gallery.space.compact.small",
    "gallery.space.compact.middle",
    "gallery.space.compact.large",
    "gallery.space.compact.vertical-rtl",
    "gallery.space.compact.mixed",
    "gallery.space.addon.variants",
    "gallery.theme.default",
    "gallery.theme.dark",
    "gallery.theme.compact",
    "gallery.theme.nested-brand",
    "gallery.state.default",
    "gallery.state.primary",
    "gallery.state.danger",
    "gallery.state.hover",
    "gallery.state.active",
    "gallery.state.focus-visible",
    "gallery.state.disabled",
    "gallery.state.loading",
    "gallery.button.blue",
    "gallery.button.purple",
    "gallery.button.cyan",
    "gallery.button.green",
    "gallery.button.magenta",
    "gallery.button.pink",
    "gallery.button.red",
    "gallery.button.orange",
    "gallery.button.yellow",
    "gallery.button.volcano",
    "gallery.button.geekblue",
    "gallery.button.lime",
    "gallery.button.gold",
    "gallery.button.outlined",
    "gallery.button.dashed",
    "gallery.button.solid",
    "gallery.button.filled",
    "gallery.button.text",
    "gallery.button.link",
    "gallery.button.icon-end",
    "gallery.button.circle",
    "gallery.button.square",
    "gallery.button.ghost",
    "gallery.button.round-block",
    "gallery.button.loading-focus",
    "gallery.divider.small-dotted-rtl",
    "gallery.divider.middle-length",
    "gallery.divider.vertical-dotted",
    "gallery.switch.content-ref",
    "gallery.switch.small-icon-rtl",
    "gallery.switch.component-theme",
    "gallery.switch.focus",
    "gallery.checkbox.group-controlled",
    "gallery.checkbox.group-dynamic",
    "gallery.checkbox.options-update",
    "gallery.checkbox.rich-rtl-ref",
    "gallery.checkbox.component-theme",
    "gallery.checkbox.focus",
    "gallery.radio.dynamic-outline",
    "gallery.radio.options-update",
    "gallery.radio.solid-large-block",
    "gallery.radio.vertical-small-rtl",
    "gallery.radio.component-theme",
    "gallery.radio.ref",
    "gallery.radio.focus",
    "gallery.input.controlled",
    "gallery.input.uncontrolled",
    "gallery.input.outlined",
    "gallery.input.filled",
    "gallery.input.borderless",
    "gallery.input.underlined",
    "gallery.input.grapheme-count",
    "gallery.input.exceed-formatter",
    "gallery.input.focus-hints",
    "gallery.input.clear-vector",
    "gallery.password.hover-vector",
    "gallery.password.controlled-toggle",
    "gallery.search.filled-vector",
    "gallery.search.underlined-small",
    "gallery.input.focus-all",
    "gallery.input.toggle-clear-disabled",
    "gallery.password.toggle-action",
    "ant.map.colorPrimary",
    "ant.map.colorSuccess",
    "ant.map.colorWarning",
    "ant.map.colorError",
    "ant.map.colorInfo",
    "ant.map.blue6",
    "ant.map.green6",
    "ant.map.red5",
    "ant.map.gold6",
    "ant.map.purple6",
    "ant.map.fontSizeSM",
    "ant.map.fontSize",
    "ant.map.fontSizeLG",
    "ant.map.sizeXS",
    "ant.map.size",
    "ant.map.sizeLG",
    "ant.map.controlHeightSM",
    "ant.seed.controlHeight",
    "ant.map.controlHeightLG",
    "ant.map.borderRadiusSM",
    "ant.seed.borderRadius",
    "ant.map.borderRadiusLG",
    "ant.alias.boxShadowTertiary",
    "ant.alias.boxShadowSecondary",
    "ant.alias.boxShadow",
    "ant.component.Button.defaultShadow",
    "ant.component.Button.primaryShadow",
    "ant.component.Button.dangerShadow",
    "ant.alias.boxShadowDrawerLeft",
    "ant.alias.boxShadowDrawerRight",
    "ant.alias.boxShadowDrawerUp",
    "ant.alias.boxShadowDrawerDown",
    "ant.alias.boxShadowPopoverArrow",
    "ant.alias.dropShadowPopover",
    "ant.alias.boxShadowCard",
    "ant.alias.boxShadowTabsOverflowLeft",
    "ant.alias.boxShadowTabsOverflowRight",
    "ant.alias.boxShadowTabsOverflowTop",
    "ant.alias.boxShadowTabsOverflowBottom",
    "gallery.icon.outlined",
    "gallery.icon.filled",
    "gallery.icon.two-tone",
    "gallery.icon.four-layers",
    "gallery.icon.retained-source",
    "gallery.icon.spin",
    "gallery.icon.custom-vector",
    "gallery.icon.wide-vector",
    "gallery.text-area.outlined",
    "gallery.text-area.filled",
    "gallery.text-area.borderless",
    "gallery.text-area.underlined",
    "gallery.text-area.autosize",
    "gallery.text-area.resize-nowrap",
    "gallery.text-area.readonly",
    "gallery.text-area.disabled",
    "gallery.otp.outlined",
    "gallery.otp.filled",
    "gallery.otp.borderless",
    "gallery.otp.underlined",
    "gallery.otp.controlled",
    "gallery.otp.formatter-mask",
    "gallery.otp.separator-rtl",
    "gallery.otp.dynamic-length",
    "gallery.otp.readonly-mask",
    "gallery.otp.disabled",
    "gallery.otp.focus-all",
    "gallery.otp.toggle-length",
    "gallery.bidi.text",
    "gallery.bidi.paragraph",
    "gallery.bidi.input",
    "gallery.bidi.text-area",
    "gallery.bidi.password",
    "gallery.bidi.toggle-direction",
    "gallery.select.controlled-search",
    "gallery.select.numeric",
    "gallery.select.disabled",
    "gallery.select.loading",
    "gallery.select.empty",
    "gallery.select.controlled-open",
    "gallery.select.outlined",
    "gallery.select.filled",
    "gallery.select.borderless",
    "gallery.select.underlined",
    "gallery.select.small",
    "gallery.select.middle",
    "gallery.select.large",
    "gallery.select.warning",
    "gallery.select.error",
    "gallery.select.component-theme",
    "gallery.select.focus",
    "gallery.select.toggle-open",
    "gallery.select.dynamic-options",
    "gallery.card.basic",
    "gallery.card.account",
    "gallery.card.cover-actions",
    "gallery.card.small",
    "gallery.card.borderless",
    "gallery.card.loading",
    "gallery.card.component-theme",
    "gallery.card.toggle-loading",
    "gallery.tabs.controlled-retained",
    "gallery.tabs.disabled",
    "gallery.tabs.dynamic-label",
    "gallery.tabs.card",
    "gallery.tabs.small",
    "gallery.tabs.large",
    "gallery.tabs.bottom",
    "gallery.tabs.bottom-card",
    "gallery.tabs.centered",
    "gallery.tabs.overflow",
    "gallery.tabs.empty",
    "gallery.tabs.unknown-key",
    "gallery.tabs.component-theme",
    "gallery.tabs.toggle-label",
    "gallery.modal.basic",
    "gallery.modal.retained-edit",
    "gallery.modal.controlled-refusal",
    "gallery.modal.confirm-loading",
    "gallery.modal.custom-footer",
    "gallery.modal.no-footer",
    "gallery.modal.maskless",
    "gallery.modal.centered",
    "gallery.modal.long-body",
    "gallery.modal.nested",
    "gallery.modal.nested-child",
    "gallery.modal.component-theme",
    "gallery.tag.closable",
    "gallery.tag.checkable",
    "gallery.tag.restore",
    "gallery.badge.clear",
    "gallery.badge.increment",
    "gallery.drawer.right",
    "gallery.drawer.left",
    "gallery.drawer.top",
    "gallery.drawer.bottom",
    "gallery.drawer.draft",
    "gallery.drawer.confirm",
    "gallery.drawer.end",
    "gallery.drawer.done",
    "gallery.popconfirm.controlled",
    "gallery.popconfirm.finish",
    "gallery.popconfirm.single",
    "gallery.alert.retry",
    "gallery.alert.restore",
    "gallery.spin.toggle",
    "gallery.spin.draft",
    "gallery.spin.action",
    "gallery.progress.advance",
    "gallery.progress.status",
    "gallery.table.editable",
    "gallery.table.update",
    "gallery.table.reverse",
    "gallery.table.remove",
    "gallery.table.restore",
    "gallery.table.loading",
    "gallery.pagination.controlled",
    "gallery.menu.inline",
    "gallery.menu.label",
    "gallery.menu.disabled",
    "gallery.dropdown.actions",
    "gallery.number.controlled",
    "gallery.number.disabled",
    "gallery.number.readonly",
    "gallery.number.toggle-disabled",
    "gallery.number.focus",
    "gallery.collapse.retained",
    "gallery.collapse.accordion",
    "gallery.collapse.disabled",
    "gallery.collapse.close",
    "gallery.collapse.options",
    "gallery.collapse.help",
    "gallery.segmented.controlled",
    "gallery.segmented.vertical",
    "gallery.segmented.rich",
    "gallery.segmented.label",
    "gallery.segmented.disabled",
    "gallery.empty.default",
    "gallery.empty.compact",
    "gallery.empty.custom",
    "gallery.empty.create",
    "gallery.empty.image",
    "gallery.result.controlled",
    "gallery.result.status",
    "gallery.result.exception",
    "gallery.result.custom",
    "gallery.result.action",
    "gallery.result.draft",
    "gallery.form.workflow",
    "gallery.form.name",
    "gallery.form.role",
    "gallery.form.count",
    "gallery.form.consent",
    "gallery.form.submit",
    "gallery.form.validate",
    "gallery.form.reset",
    "gallery.form.disabled",
    "gallery.form.readonly",
    "gallery.form.help",
    "gallery.form.status",
    "gallery.date-picker.controlled",
    "gallery.date-picker.value",
    "gallery.date-picker.footer",
    "gallery.date-picker.restore",
    "gallery.date-picker.form",
    "gallery.date-picker.field",
    "gallery.date-picker.submit",
    "gallery.date-picker.reset",
    "gallery.date-picker.disabled",
    "gallery.date-picker.readonly",
    "gallery.date-picker.help",
    "gallery.date-picker.status",
    "gallery.breadcrumb.trail",
    "gallery.breadcrumb.feedback",
    "gallery.breadcrumb.draft",
    "gallery.breadcrumb.display",
    "gallery.breadcrumb.rename",
    "gallery.breadcrumb.visible",
    "gallery.breadcrumb.draft-visible",
    "gallery.breadcrumb.disabled",
    "gallery.breadcrumb.restore",
    "gallery.breadcrumb.focus",
    "gallery.steps.workflow",
    "gallery.steps.feedback",
    "gallery.steps.draft",
    "gallery.steps.previous",
    "gallery.steps.next",
    "gallery.steps.reset",
    "gallery.steps.vertical",
    "gallery.steps.size",
    "gallery.steps.status",
    "gallery.steps.visible",
    "gallery.steps.disabled",
    "gallery.steps.focus",
    "gallery.time-picker.controlled",
    "gallery.time-picker.value",
    "gallery.time-picker.footer",
    "gallery.time-picker.restore",
    "gallery.time-picker.mode",
    "gallery.time-picker.authority",
    "gallery.time-picker.input-readonly",
    "gallery.time-picker.form",
    "gallery.time-picker.field",
    "gallery.time-picker.submit",
    "gallery.time-picker.reset",
    "gallery.time-picker.disabled",
    "gallery.time-picker.readonly",
    "gallery.time-picker.help",
    "gallery.time-picker.status",
    "gallery.descriptions.editor",
    "gallery.descriptions.feedback",
    "gallery.descriptions.name",
    "gallery.descriptions.save",
    "gallery.descriptions.columns",
    "gallery.descriptions.span",
    "gallery.descriptions.visible",
    "gallery.descriptions.vertical",
    "gallery.descriptions.bordered",
    "gallery.descriptions.size",
    "gallery.descriptions.focus",
    "gallery.skeleton.preview",
    "gallery.skeleton.feedback",
    "gallery.skeleton.draft",
    "gallery.skeleton.save",
    "gallery.skeleton.loading",
    "gallery.skeleton.title",
    "gallery.skeleton.paragraph",
    "gallery.skeleton.avatar",
    "gallery.skeleton.round",
    "gallery.skeleton.standalone",
    "gallery.skeleton.shape",
    "gallery.skeleton.block",
    "gallery.skeleton.focus",
    "gallery.popover.editor",
    "gallery.popover.feedback",
    "gallery.popover.trigger",
    "gallery.popover.draft",
    "gallery.popover.save",
    "gallery.popover.cancel",
    "gallery.popover.placement",
    "gallery.popover.arrow",
    "gallery.popover.disabled",
    "gallery.popover.restore",
    "gallery.notices.workflow",
    "gallery.notices.feedback",
    "gallery.message.open",
    "gallery.message.update",
    "gallery.message.close",
    "gallery.notification.open",
    "gallery.notification.update",
    "gallery.notification.close",
    "gallery.notification.placement",
    "gallery.notification.status",
    "gallery.notification.draft",
    "gallery.notification.action",
    "gallery.notices.clear",
    "gallery.timeline.workflow",
    "gallery.timeline.feedback",
    "gallery.timeline.draft",
    "gallery.timeline.save",
    "gallery.timeline.mode",
    "gallery.timeline.reverse",
    "gallery.timeline.loading",
    "gallery.timeline.visible",
    "gallery.timeline.focus",
    "gallery.avatar.members",
    "gallery.avatar.feedback",
    "gallery.avatar.name",
    "gallery.avatar.save",
    "gallery.avatar.icon",
    "gallery.avatar.shape",
    "gallery.avatar.size",
    "gallery.avatar.limit",
    "gallery.avatar.visible",
    "gallery.avatar.restore",
    "gallery.statistic.metrics",
    "gallery.statistic.feedback",
    "gallery.statistic.value",
    "gallery.statistic.precision",
    "gallery.statistic.separators",
    "gallery.statistic.formatter",
    "gallery.statistic.loading",
    "gallery.statistic.draft",
    "gallery.statistic.save",
    "gallery.statistic.restore",
    "gallery.rate.service",
    "gallery.rate.feedback",
    "gallery.rate.authority",
    "gallery.rate.disabled",
    "gallery.rate.readonly",
    "gallery.rate.visible",
    "gallery.rate.restore",
    "gallery.rate.draft",
    "gallery.rate.save",
    "gallery.auto-complete.contacts",
    "gallery.auto-complete.feedback",
    "gallery.auto-complete.input",
    "gallery.auto-complete.authority",
    "gallery.auto-complete.options",
    "gallery.auto-complete.disabled",
    "gallery.auto-complete.readonly",
    "gallery.auto-complete.restore",
    "gallery.auto-complete.note",
    "gallery.auto-complete.save-note",
    "gallery.auto-complete.form",
    "gallery.auto-complete.form-feedback",
    "gallery.auto-complete.submit",
    "gallery.auto-complete.reset",
    "gallery.tree.project",
    "gallery.tree.feedback",
    "gallery.tree.note",
    "gallery.tree.save-note",
    "gallery.tree.disabled",
    "gallery.tree.visible",
    "gallery.tree.locked",
    "gallery.tree.restore",
});

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
}

void filter_button(const std::shared_ptr<GalleryState>& state, std::string_view caption, GallerySupportFilter filter) {
    const auto text = utf8(caption);
    ryn::Button(ryn::ButtonProps{}.size(ryn::ControlSize::Small).onClick([state, filter] {
        if (state->support_filter.set(filter)) {
            ++state->telemetry.filter_updates;
        }
    }),
                [text] { ryn::Text(text); });
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

void scrollbar_surface(ReferenceSurfaceRole role, const ryn::Signal<ryn::LogicalLength>& height,
                       const ryn::Signal<bool>& visible) {
    ReferenceSurface(ReferenceSurfaceProps{}.role(role).visible(visible).layout(
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
               ryn::ThemeContent{[state, title, identity, swatch, elevated] {
                   ++state->telemetry.theme_content_runs;
                   ReferenceSurface(ReferenceSurfaceProps{}
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
                         if (identity == "ant.component.typography") {
                             for (auto level :
                                  {ryn::TypographyLevel::H1, ryn::TypographyLevel::H2, ryn::TypographyLevel::H3,
                                   ryn::TypographyLevel::H4, ryn::TypographyLevel::H5}) {
                                 ryn::Title(ryn::TitleProps{}.level(level).content(u8"RynUI 标题层级"));
                             }
                             ryn::Text(ryn::TypographyProps{}
                                           .content(u8"Strong / Italic · 强调与斜体")
                                           .strong(true)
                                           .italic(true)
                                           .type(ryn::TypographyType::Danger));
                             ryn::Text(ryn::TypographyProps{}
                                           .content(u8"code: a += b; 中文回退")
                                           .code(true)
                                           .type(ryn::TypographyType::Success));
                             ryn::Text(ryn::TypographyProps{}
                                           .content(u8"Ctrl + C / 键帽")
                                           .keyboard(true)
                                           .type(ryn::TypographyType::Warning));
                             ryn::Text(ryn::TypographyProps{}
                                           .content(u8"高亮、下划线、删除线")
                                           .mark(true)
                                           .underline(true)
                                           .strikethrough(true)
                                           .type(ryn::TypographyType::Secondary));
                             ryn::Link(ryn::LinkProps{}.content(u8"Link · 指针与键盘激活").onClick([state] {
                                 ++state->telemetry.activations;
                             }));
                             state->telemetry.live_samples += 10;
                         }
                         if (identity == "ant.component.divider") {
                             ryn::Divider();
                             ryn::Divider(ryn::DividerProps{}.content(u8"Divider 标签"));
                             ryn::Divider(ryn::DividerProps{}.dashed(true));
                             state->telemetry.live_samples += 3;
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
}

void add_basic_live_samples(const std::shared_ptr<GalleryState>& state) {
    section_surface(state, gallery_document_sections()[5]);
    ryn::Space(
        ryn::SpaceProps{}
            .align(ryn::SpaceAlign::Start)
            .wrap(true)
            .align(ryn::SpaceAlign::Center)
            .size(ryn::dp(8.0F), ryn::dp(8.0F))
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
                                  ++state->telemetry.live_samples;
                              }});
                   ryn::Theme(ryn::ThemeProps{}, ryn::ThemeContent{[state] {
                                  ++state->telemetry.theme_content_runs;
                                  ryn::Input(ryn::InputProps{}
                                                 .defaultValue(u8"非受控 / Input")
                                                 .placeholder(u8"清空后显示 placeholder")
                                                 .allowClear(true)
                                                 .disabled(state->clear_disabled)
                                                 .onChange([state](ryn::String next) {
                                                     ++state->telemetry.input_changes;
                                                     state->input_feedback.set(std::move(next));
                                                 })
                                                 .onSubmit([state](ryn::String next) {
                                                     ++state->telemetry.input_submits;
                                                     state->input_feedback.set(std::move(next));
                                                 })
                                                 .layout(ryn::LayoutStyle{}.width(state->cell_width)));
                                  ++state->telemetry.live_samples;
                              }});
               });
    ryn::Text(ryn::TextProps{}.content(state->input_feedback));
    ryn::Text(u8"Search / 搜索 · 原生单行功能：复用 Input、Button 与 Compact");
    ryn::Text(u8"支持：四变体、全部共用属性、自定义图标、清空来源与连接外观");
    ryn::Space(ryn::SpaceProps{}
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
                   ++state->telemetry.live_samples;
                   ryn::Search(ryn::SearchProps{}
                                   .defaultValue(u8"RynUI")
                                   .size(ryn::ControlSize::Large)
                                   .enterButton(true)
                                   .onSearch([state](ryn::String next, ryn::SearchSource) {
                                       ++state->telemetry.search_submits;
                                       state->search_feedback.set(std::move(next));
                                   })
                                   .layout(ryn::LayoutStyle{}.width(state->cell_width)),
                               ryn::SearchButtonContent{[] { ryn::Text(u8"查询"); }});
                   ++state->telemetry.live_samples;
                   ryn::Search(ryn::SearchProps{}
                                   .defaultValue(u8"Small / 小号")
                                   .size(ryn::ControlSize::Small)
                                   .layout(ryn::LayoutStyle{}.width(state->cell_width)));
                   ++state->telemetry.live_samples;
                   ryn::Search(ryn::SearchProps{}
                                   .defaultValue(u8"loading")
                                   .enterButton(true)
                                   .loading(state->loading)
                                   .layout(ryn::LayoutStyle{}.width(state->cell_width)));
                   ++state->telemetry.live_samples;
                   ryn::Search(ryn::SearchProps{}
                                   .defaultValue(u8"disabled")
                                   .disabled(state->disabled)
                                   .layout(ryn::LayoutStyle{}.width(state->cell_width)));
                   ++state->telemetry.live_samples;
               });
    ryn::Text(ryn::TextProps{}.content(state->search_feedback));
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
                   ++state->telemetry.live_samples;
                   ryn::Password(ryn::PasswordProps{}
                                     .defaultValue(u8"禁用密码")
                                     .disabled(true)
                                     .layout(ryn::LayoutStyle{}.width(state->cell_width)));
                   ++state->telemetry.live_samples;
               });
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
                   ryn::Switch(ryn::SwitchProps{}.size(ryn::SwitchSize::Small).defaultChecked(true));
                   ++state->telemetry.live_samples;
                   ryn::Text(u8"禁用");
                   ryn::Switch(ryn::SwitchProps{}.disabled(true));
                   ++state->telemetry.live_samples;
                   ryn::Text(u8"加载");
                   ryn::Switch(ryn::SwitchProps{}.loading(state->loading));
                   ++state->telemetry.live_samples;
               });
    ryn::Text(u8"Checkbox / 多选框 · checked、indeterminate、disabled");
    ryn::Space(ryn::SpaceProps{}
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
                   ryn::Checkbox(ryn::CheckboxProps{}.defaultChecked(true),
                                 ryn::CheckboxLabel{[] { ryn::Text(u8"已选 / Checked"); }});
                   ++state->telemetry.live_samples;
                   ryn::Checkbox(ryn::CheckboxProps{}.indeterminate(true),
                                 ryn::CheckboxLabel{[] { ryn::Text(u8"半选 / Mixed"); }});
                   ++state->telemetry.live_samples;
                   ryn::Checkbox(ryn::CheckboxProps{}.disabled(true),
                                 ryn::CheckboxLabel{[] { ryn::Text(u8"禁用 / Disabled"); }});
                   ++state->telemetry.live_samples;
               });
    ryn::Text(u8"Radio / 单选框 · 单独使用、互斥分组、disabled");
    ryn::Space(ryn::SpaceProps{}
                   .align(ryn::SpaceAlign::Start)
                   .wrap(true)
                   .align(ryn::SpaceAlign::Center)
                   .size(ryn::dp(12.0F))
                   .layout(ryn::LayoutStyle{}.width(state->document_width)),
               [state] {
                   ryn::Radio(ryn::RadioProps{}.defaultChecked(true),
                              ryn::RadioLabel{[] { ryn::Text(u8"单独已选 / Standalone"); }});
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

void add_typography_layout_slider_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Typography · 省略、复制与原地编辑");
    const ryn::String full{u8"原始全文保持完整：省略显示时复制仍写入全部内容；点击展开查看全文，再次点击收起。"};
    ryn::Text(ryn::TypographyProps{}
                  .content(full)
                  .ellipsis(ryn::TypographyEllipsis{.expandable = true, .tooltip = full})
                  .copyable(ryn::TypographyCopyable{})
                  .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::Paragraph(ryn::TypographyProps{}
                       .content(u8"多行省略保留字素边界，英文 Latin "
                                u8"和中文均复用同一文本测量通道。\n显式换行也计入行数。\n展开后显示全部行。")
                       .ellipsis(ryn::TypographyEllipsis{.rows = 2, .expandable = true})
                       .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::Paragraph(ryn::TypographyProps{}
                       .content(state->typography_value)
                       .editable(ryn::TypographyEditable{.max_length = 64, .auto_size = {true, 1, 4}})
                       .onEdit([state](ryn::String value) { state->typography_value.set(std::move(value)); })
                       .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::Title(ryn::TitleProps{}
                   .level(ryn::TypographyLevel::H3)
                   .content(state->typography_title)
                   .editable(ryn::TypographyEditable{})
                   .onEdit([state](ryn::String value) { state->typography_title.set(std::move(value)); })
                   .layout(ryn::LayoutStyle{}.width(state->document_width)));
    ryn::Link(ryn::LinkProps{}.content(u8"Link · 激活计数").onClick([state] { ++state->telemetry.activations; }));
    ryn::Link(ryn::LinkProps{}.content(u8"禁用 Link").disabled(true));
    state->telemetry.live_samples += 6;
    ryn::Text(u8"Divider · 水平、虚线、plain、None 朝向间距与垂直线");
    ryn::Divider(ryn::DividerProps{}.orientation(ryn::DividerOrientation::Left).content(u8"Left"));
    ryn::Divider(ryn::DividerProps{}.orientation(ryn::DividerOrientation::Right).content(u8"Right").dashed(true));
    ryn::Divider(ryn::DividerProps{}.content(u8"Plain").plain(true));
    ryn::Divider(ryn::DividerProps{}
                     .content(u8"None / 显式无朝向间距")
                     .orientation(ryn::DividerOrientation::Left)
                     .orientationMargin(ryn::DividerOrientationMargin::none()));
    ryn::Space(ryn::SpaceProps{}.align(ryn::SpaceAlign::Center), [] {
        ryn::Text(u8"文字");
        ryn::Divider(ryn::DividerProps{}.type(ryn::DividerType::Vertical));
        ryn::Text(u8"文字");
    });
    state->telemetry.live_samples += 5;
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
                    .defaultValue(35)
                    .limits(ryn::SliderLimits{0, 100, 10})
                    .dots(true)
                    .included(false)
                    .reverse(true)
                    .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::Slider(ryn::SliderProps{}.defaultValue(60).disabled(true).layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::Slider(ryn::SliderProps{}
                    .defaultValue(40)
                    .orientation(ryn::SliderOrientation::Vertical)
                    .layout(ryn::LayoutStyle{}.height(ryn::dp(120))));
    state->telemetry.live_samples += 5;
    ryn::RangeSlider(ryn::RangeSliderProps{}.defaultValue({20, 80}).draggableTrack(true).layout(
        ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::MultiSlider(ryn::MultiSliderProps{}
                         .defaultValue({20, 50, 80})
                         .rangeOptions(ryn::SliderRangeOptions{false, true, 0, 6})
                         .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::MultiSlider(ryn::MultiSliderProps{}
                         .defaultValue({20, 50, 80})
                         .handleDisabled(ryn::SliderDisabledHandles{false, true, false})
                         .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    state->telemetry.live_samples += 3;
}

void add_button_and_divider_variant_samples(const std::shared_ptr<GalleryState>& state) {
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
                               ryn::ButtonContent{[] { ryn::Text(u8"gallery.button.icon-end · End / Loading"); }},
                               ryn::ButtonIcon{[] { ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CheckOutlined)); }},
                               ryn::ButtonLoadingIcon{[] { ryn::Text(u8"…"); }});
                   ryn::Button(ryn::ButtonProps{}.shape(ryn::ButtonShape::Circle),
                               ryn::ButtonSlots{.icon = ryn::ButtonIcon{[] {
                                                    ryn::Icon(ryn::IconProps{}.name(ryn::IconName::SearchOutlined));
                                                }}});
                   ryn::Button(ryn::ButtonProps{}.shape(ryn::ButtonShape::Square),
                               [] { ryn::Text(u8"gallery.button.square · Square"); });
                   ryn::Button(
                       ryn::ButtonProps{}.color(ryn::ButtonColor::Blue).variant(ryn::ButtonVariant::Dashed).ghost(true),
                       [] { ryn::Text(u8"gallery.button.ghost · Ghost dashed"); });
                   ryn::Button(ryn::ButtonProps{}.onClick([state] {
                       state->button_loading.set(!state->button_loading.get());
                       static_cast<void>(state->button_ref.focus());
                       ++state->telemetry.activations;
                   }),
                               [] { ryn::Text(u8"gallery.button.loading-focus · 切换加载并聚焦"); });
                   state->telemetry.live_samples += 5;
               });
    ryn::Button(ryn::ButtonProps{}.block(true).shape(ryn::ButtonShape::Round),
                [] { ryn::Text(u8"gallery.button.round-block · Round block / resize"); });
    ++state->telemetry.live_samples;
    ryn::Divider(ryn::DividerProps{}
                     .variant(ryn::DividerVariant::Dotted)
                     .size(ryn::ControlSize::Small)
                     .orientation(ryn::DividerOrientation::Start)
                     .direction(ryn::DividerDirection::RightToLeft)
                     .content(u8"gallery.divider.small-dotted-rtl · Small / Start / RTL"));
    ryn::Divider(ryn::DividerProps{}
                     .variant(ryn::DividerVariant::Dashed)
                     .size(ryn::ControlSize::Middle)
                     .orientation(ryn::DividerOrientation::End)
                     .orientationMargin(ryn::DividerOrientationMargin::length(ryn::dp(20)))
                     .content(u8"gallery.divider.middle-length · Middle / End / 20 dp"));
    ryn::Space(ryn::SpaceProps{}.align(ryn::SpaceAlign::Center), [] {
        ryn::Text(u8"gallery.divider.vertical-dotted · Vertical");
        ryn::Divider(ryn::DividerProps{}.type(ryn::DividerType::Vertical).variant(ryn::DividerVariant::Dotted));
        ryn::Text(u8"Dotted");
    });
    state->telemetry.live_samples += 3;
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
            ryn::Text(u8"gallery.switch.content-ref");
            ryn::Switch(
                ryn::SwitchProps{}.checked(state->switch_checked).ref(state->switch_ref).onChange([state](bool value) {
                    state->switch_checked.set(value);
                }),
                ryn::SwitchSlots{ryn::SwitchCheckedContent{[] { ryn::Text(u8"开启"); }},
                                 ryn::SwitchUncheckedContent{[] { ryn::Text(u8"关闭"); }}});
            ryn::Text(u8"gallery.switch.small-icon-rtl");
            ryn::Switch(ryn::SwitchProps{}.size(ryn::SwitchSize::Small).direction(ryn::SwitchDirection::RightToLeft),
                        ryn::SwitchSlots{ryn::SwitchCheckedContent{
                                             [] { ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CheckOutlined)); }},
                                         ryn::SwitchUncheckedContent{[] {
                                             ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CloseCircleFilled));
                                         }}});
            ryn::ThemeConfig custom;
            custom.switch_.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
            custom.switch_.algorithm = true;
            ryn::Theme(ryn::ThemeProps{}.config(custom), ryn::ThemeContent{[state] {
                           ++state->telemetry.theme_content_runs;
                           ryn::Text(u8"gallery.switch.component-theme");
                           ryn::Switch(ryn::SwitchProps{}.defaultChecked(true),
                                       ryn::SwitchSlots{ryn::SwitchCheckedContent{[] { ryn::Text(u8"紫色"); }},
                                                        ryn::SwitchUncheckedContent{[] { ryn::Text(u8"关"); }}});
                       }});
            ryn::Button(ryn::ButtonProps{}.onClick([state] {
                static_cast<void>(state->switch_ref.focus());
                ++state->telemetry.activations;
            }),
                        [] { ryn::Text(u8"gallery.switch.focus · 聚焦开关"); });
            state->telemetry.live_samples += 4;
        });
    ryn::Flex(ryn::FlexProps{}.align(ryn::FlexAlign::Start).layout(ryn::LayoutStyle{}.height(ryn::dp(48.0F))),
              ryn::FlexContent{[] {}});
}

void add_checkbox_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Checkbox · 多选、动态保留选项、RTL、ref 与独立主题");
    ryn::Text(u8"gallery.checkbox.group-controlled · 三类值");
    ryn::CheckboxGroup(
        ryn::CheckboxGroupProps{}
            .options(std::vector<ryn::CheckboxOption>{{ryn::String{u8"desktop"}, ryn::String{u8"桌面"}},
                                                      {1.0, ryn::String{u8"数字"}},
                                                      {true, ryn::String{u8"禁用布尔"}, true}})
            .value(state->checkbox_values)
            .onChange([state](const ryn::CheckboxValues& values) { state->checkbox_values.set(values); }));
    ryn::Text(u8"gallery.checkbox.group-dynamic · 按 value 保留 identity");
    ryn::CheckboxGroup(ryn::CheckboxGroupProps{}.options(state->checkbox_options).defaultValue({ryn::String{u8"a"}}));
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
                [] { ryn::Text(u8"gallery.checkbox.options-update · 重排 / 增删"); });
    ryn::Checkbox(ryn::CheckboxProps{}
                      .ref(state->checkbox_ref)
                      .indeterminate(true)
                      .direction(ryn::CheckboxDirection::RightToLeft),
                  ryn::CheckboxLabel{[] {
                      ryn::Icon(ryn::IconProps{}.name(ryn::IconName::CheckOutlined));
                      ryn::Text(u8"gallery.checkbox.rich-rtl-ref · 半选与富标签");
                  }});
    ryn::ThemeConfig custom;
    custom.checkbox.algorithm = true;
    custom.checkbox.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
    custom.checkbox.tokens.size = ryn::dp(20);
    ryn::Theme(ryn::ThemeProps{}.config(custom), ryn::ThemeContent{[state] {
                   ++state->telemetry.theme_content_runs;
                   ryn::Checkbox(ryn::CheckboxProps{}.defaultChecked(true), ryn::CheckboxLabel{[] {
                                     ryn::Text(u8"gallery.checkbox.component-theme · 紫色 / 20 dp");
                                 }});
               }});
    ryn::Button(ryn::ButtonProps{}.onClick([state] {
        static_cast<void>(state->checkbox_ref.focus());
        ++state->telemetry.activations;
    }),
                [] { ryn::Text(u8"gallery.checkbox.focus · 聚焦多选框"); });
    state->telemetry.live_samples += 6;
}

void add_radio_themed_sample(const std::shared_ptr<GalleryState>& state) {
    ryn::ThemeConfig custom;
    custom.radio.algorithm = true;
    custom.radio.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
    ryn::Theme(ryn::ThemeProps{}.config(custom), ryn::ThemeContent{[state] {
                   ++state->telemetry.theme_content_runs;
                   ryn::Text(u8"gallery.radio.component-theme / gallery.radio.ref");
                   ryn::RadioButton(ryn::RadioProps{}.defaultChecked(true).ref(state->radio_ref),
                                    ryn::RadioLabel{[] { ryn::Text(u8"独立紫色 / 富标签 / ref"); }});
               }});
}

void add_radio_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Radio · 动态选项、键盘组导航、按钮连接边、三尺寸、block、RTL、ref 与独立主题");
    ryn::Text(u8"gallery.radio.dynamic-outline · 按 value 保留 identity");
    ryn::RadioGroup(ryn::RadioGroupProps{}
                        .options(state->radio_options)
                        .defaultValue(ryn::String{u8"a"})
                        .optionType(ryn::RadioOptionType::Button));
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
                [] { ryn::Text(u8"gallery.radio.options-update · 重排 / 增删"); });
    ryn::Text(u8"gallery.radio.solid-large-block · 三类值 / 禁用");
    ryn::RadioGroup(ryn::RadioGroupProps{}
                        .options({{ryn::String{u8"desktop"}, ryn::String{u8"桌面"}},
                                  {1.0, ryn::String{u8"数字"}},
                                  {true, ryn::String{u8"禁用布尔"}, true}})
                        .defaultValue(1.0)
                        .optionType(ryn::RadioOptionType::Button)
                        .buttonStyle(ryn::RadioButtonStyle::Solid)
                        .size(ryn::RadioSize::Large)
                        .block(true)
                        .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::Text(u8"gallery.radio.vertical-small-rtl");
    ryn::RadioGroup(
        ryn::RadioGroupProps{}
            .options({{false, ryn::String{u8"false"}}, {true, ryn::String{u8"true"}}, {3.0, ryn::String{u8"数字"}}})
            .defaultValue(true)
            .optionType(ryn::RadioOptionType::Button)
            .size(ryn::RadioSize::Small)
            .orientation(ryn::RadioGroupOrientation::Vertical)
            .direction(ryn::RadioDirection::RightToLeft));
    add_radio_themed_sample(state);
    ryn::Button(ryn::ButtonProps{}.onClick([state] {
        static_cast<void>(state->radio_ref.focus());
        ++state->telemetry.activations;
    }),
                [] { ryn::Text(u8"gallery.radio.focus · 聚焦单选按钮"); });
    state->telemetry.live_samples += 6;
}

void add_flex_samples(const std::shared_ptr<GalleryState>& state) {
    ryn::Text(u8"Flex · 真实文字基线、反向换行、RTL 与默认拉伸");
    ryn::Text(u8"gallery.flex.baseline · 14 / 28 dp 与控件标签");
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
    ryn::Text(u8"gallery.flex.wrap-reverse · 从下边堆叠");
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
    ryn::Text(u8"gallery.flex.vertical-rtl · 纵向换列");
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
    ryn::Text(u8"gallery.flex.default-stretch · 自动高度 64 dp");
    ryn::Flex(ryn::FlexProps{}.gap(ryn::dp(8)).layout(ryn::LayoutStyle{}.height(ryn::dp(64))), ryn::FlexContent{[] {
                  ryn::Text(u8"默认 Stretch");
                  ryn::Text(ryn::TextProps{}.content(u8"固定 22 dp").layout(ryn::LayoutStyle{}.height(ryn::dp(22))));
              }});
    state->telemetry.live_samples += 4;
}

void add_space_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"Space · separator / baseline / Compact / mixed / Addon");
    Text(u8"gallery.space.separator · 富分隔随内容交错布局、绘制和 Tab");
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
    Text(u8"gallery.space.baseline · 大尺寸按钮、输入与文字对齐");
    Space(SpaceProps{}.align(SpaceAlign::Baseline), SpaceContent{[] {
              Text(u8"Ag 基线");
              Button(ButtonProps{}.size(ControlSize::Large), [] { Text(u8"Ag 大按钮"); });
              Input(
                  InputProps{}.size(ControlSize::Large).defaultValue(u8"Ag 输入").layout(LayoutStyle{}.width(dp(150))));
          }});
    for (const auto size : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
        Text(size == ControlSize::Small   ? String{u8"gallery.space.compact.small"}
             : size == ControlSize::Large ? String{u8"gallery.space.compact.large"}
                                          : String{u8"gallery.space.compact.middle"});
        SpaceCompact(SpaceCompactProps{}.size(size), SpaceCompactContent{[] {
                         Button(ButtonProps{}.variant(ButtonVariant::Dashed), [] { Text(u8"虚线"); });
                         Button(ButtonProps{}.type(ButtonType::Primary), [] { Text(u8"连接操作"); });
                     }});
    }
    Text(u8"gallery.space.compact.vertical-rtl · block / 纵向 / RTL");
    SpaceCompact(SpaceCompactProps{}
                     .orientation(SpaceOrientation::Vertical)
                     .direction(FlexDirection::RightToLeft)
                     .block(true)
                     .layout(LayoutStyle{}.width(state->cell_width)),
                 SpaceCompactContent{[] {
                     Button(ButtonProps{}, [] { Text(u8"上方"); });
                     Button(ButtonProps{}.danger(true), [] { Text(u8"下方危险操作"); });
                 }});
    Text(u8"gallery.space.compact.mixed · Input / Password / Search / Radio / nested");
    SpaceCompact(SpaceCompactProps{}
                     .size(ControlSize::Small)
                     .orientation(bind([narrow = state->narrow_layout] {
                         return narrow.get() ? SpaceOrientation::Vertical : SpaceOrientation::Horizontal;
                     }))
                     .block(true)
                     .layout(LayoutStyle{}.width(state->document_width)),
                 SpaceCompactContent{[] {
                     SpaceAddon(SpaceAddonProps{}, SpaceAddonContent{[] { Text(u8"https://"); }});
                     Input(InputProps{}.defaultValue(u8"输入").layout(LayoutStyle{}.width(dp(120))));
                     Password(PasswordProps{}.defaultValue(u8"秘密").layout(LayoutStyle{}.width(dp(100))));
                     Search(SearchProps{}.defaultValue(u8"搜索").layout(LayoutStyle{}.flex_grow(1).min_width(dp(0))));
                     RadioButton(RadioProps{}, RadioLabel{[] { Text(u8"选择"); }});
                     SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[] {
                                      Button(ButtonProps{}, [] { Text(u8"嵌套 A"); });
                                      Button(ButtonProps{}.disabled(true), [] { Text(u8"嵌套 B"); });
                                  }});
                 }});
    Text(u8"gallery.space.addon.variants · 四变体 / 错误与警告 / 禁用");
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
                        Icon(IconProps{}.name(IconName::HomeFilled));
                        Icon(IconProps{}.name(IconName::HeartTwoTone));
                        Icon(IconProps{}
                                 .name(IconName::WalletTwoTone)
                                 .twoToneColor(
                                     IconTwoToneColor{Color::rgba8(114, 46, 209), Color::rgba8(239, 219, 255)}));
                        Icon(IconProps{}.source(state->icon_source).rotate(state->icon_angle));
                        Icon(IconProps{}.name(IconName::LoadingOutlined).spin(state->icon_spin));
                        Icon(IconProps{}.source(icon_vector_sample()));
                        Icon(IconProps{}.source(icon_wide_sample()));
                    }});
          }});
    Space(SpaceProps{}.wrap(true), SpaceContent{[state] {
              Button(ButtonProps{}.onClick([state] { state->icon_spin.set(!state->icon_spin.get()); }),
                     ButtonContent{[] { Text(u8"开始 / 停止 spin"); }});
              Button(ButtonProps{}.onClick(
                         [state] { state->icon_angle.set(std::fmod(state->icon_angle.get() + 45, 360.0F)); }),
                     ButtonContent{[] { Text(u8"旋转 45°"); }});
              Button(ButtonProps{}.onClick([state] {
                  const auto step = ++state->icon_source_step % 3;
                  state->icon_source.set(step == 0   ? IconSource{IconName::HeartTwoTone}
                                         : step == 1 ? IconSource{IconName::WalletTwoTone}
                                                     : icon_vector_sample());
              }),
                     ButtonContent{[] { Text(u8"切换内置 / 自定义"); }});
          }});
    state->telemetry.live_samples += 8;
}

void add_tabs_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"Tabs · 键盘导航 / 保留草稿 / 溢出滚动");
    const auto sample = [&](const char8_t* caption, TabsProps props, std::vector<TabItem> items) {
        Text(String::from_utf8(std::u8string_view{caption}).value());
        Tabs(props.layout(LayoutStyle{}.width(state->cell_width)), std::move(items));
        ++state->telemetry.live_samples;
    };
    const auto basic = [] {
        return std::vector<TabItem>{TabItem{u8"draft", u8"草稿", TabContent{[] { Text(u8"内容 / Content"); }}},
                                    TabItem{u8"settings", u8"设置", TabContent{[] { Text(u8"Settings content"); }}}};
    };
    sample(
        u8"gallery.tabs.controlled-retained · 受控 / 切换保留编辑",
        TabsProps{}.activeKey(state->tabs_active).onChange([state](String key) {
            state->tabs_active.set(std::move(key));
            ++state->telemetry.activations;
        }),
        {TabItem{
             u8"draft", u8"草稿", TabContent{[] {
                 Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[] {
                          Input(InputProps{}.defaultValue(u8"切换后保留 / Draft").layout(LayoutStyle{}.width(dp(140))));
                          Select(SelectProps{}
                                     .options({{1, u8"桌面"}, {2, u8"服务"}})
                                     .defaultValue(1)
                                     .layout(LayoutStyle{}.width(dp(140))));
                      }});
             }}},
         TabItem{u8"settings", u8"设置", TabContent{[] {
                     Input(InputProps{}.defaultValue(u8"另一份草稿").layout(LayoutStyle{}.width(dp(140))));
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
              Text(u8"gallery.tabs.component-theme · 局部颜色 / 16 dp 间距");
              Tabs(TabsProps{}.layout(LayoutStyle{}.width(state->cell_width)), basic());
              ++state->telemetry.live_samples;
          }});
    Button(ButtonProps{}.onClick([state] {
        const bool disabled = !state->tabs_disabled.get();
        state->tabs_disabled.set(disabled);
        state->tabs_label.set(disabled ? String{u8"暂不可用"} : String{u8"设置 / Settings"});
        ++state->telemetry.activations;
    }),
           [] { Text(u8"gallery.tabs.toggle-label · 切换标签 / 禁用"); });
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
        state->telemetry.live_samples += 2; // Dialog and its page trigger.
    };
    sample(0, "gallery.modal.basic", String{u8"基础确认"}, ModalProps{}, ModalContent{[] {
               Text(u8"确认、取消、右上角关闭、Escape或点击遮罩。");
               Text(u8"关闭后会返回原来的有效入口。");
           }});
    sample(1, "gallery.modal.retained-edit", String{u8"保留编辑草稿"}, ModalProps{}, ModalContent{[] {
               Text(u8"修改内容后关闭再打开，草稿和选项继续保留。");
               Input(InputProps{}.defaultValue(u8"RynUI account"));
               TextArea(TextAreaProps{}.rows(3).defaultValue(u8"保留的多行备注\n中文 / English 🙂"));
               Select(SelectProps{}
                          .options({{u8"desktop", u8"桌面"}, {u8"service", u8"服务"}})
                          .defaultValue(u8"desktop")
                          .showSearch()
                          .allowClear());
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
               Text(u8"确认在加载时禁用，取消仍可使用。");
               Button(ButtonProps{}.onClick([state] { state->modal_loading.set(!state->modal_loading.get()); }),
                      [] { Text(u8"切换确认加载状态"); });
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
               Input(InputProps{}.defaultValue(u8"末尾保留的输入"));
               Select(SelectProps{}.options({{1, u8"末尾选项一"}, {2, u8"末尾选项二"}}));
               Tabs(TabsProps{}, {TabItem{u8"first", u8"保留面板一", TabContent{[] { Text(u8"面板一"); }}},
                                  TabItem{u8"second", u8"保留面板二", TabContent{[] { Text(u8"面板二"); }}}});
           }});
    sample(9, "gallery.modal.nested", String{u8"嵌套对话框"}, ModalProps{}, ModalContent{[state] {
               Input(InputProps{}.defaultValue(u8"父层保留的草稿"));
               Button(ButtonProps{}.onClick([state] { state->modal_nested.set(true); }), [] { Text(u8"打开下一层"); });
               Modal(ModalProps{}
                         .open(state->modal_nested)
                         .title(u8"二级确认")
                         .onOk([state] { state->modal_nested.set(false); })
                         .onCancel([state](ModalCancelReason) { state->modal_nested.set(false); }),
                     ModalContent{[] {
                         Text(u8"gallery.modal.nested-child · 关闭后返回父层有效入口");
                         Input(InputProps{}.defaultValue(u8"子层保留的草稿"));
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

void add_common_component_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"Tag / Badge · 状态、选择与计数");
    Text(TextProps{}.content(state->common_feedback));
    Space(SpaceProps{}.wrap(true).size(dp(12)), SpaceContent{[state] {
              Tag(TagProps{}, TagContent{[] { Text(u8"默认标签"); }});
              Tag(TagProps{}.color(Status::Success), TagContent{[] { Text(u8"运行正常"); }});
              Tag(TagProps{}.color(PresetColor::Blue).closable().visible(state->tag_visible).onClose([state] {
                  state->tag_visible.set(false);
              }),
                  TagContent{[] { Text(u8"gallery.tag.closable · 关闭标签"); }});
              CheckableTag(CheckableTagProps{}.checked(state->tag_checked).onChange([state](bool checked) {
                  state->tag_checked.set(checked);
              }),
                           TagContent{[] { Text(u8"gallery.tag.checkable · 选择标签"); }});
              Button(ButtonProps{}.onClick([state] { state->tag_visible.set(true); }),
                     [] { Text(u8"gallery.tag.restore · 恢复标签"); });
          }});
    Space(SpaceProps{}.wrap(true).size(dp(20)), SpaceContent{[state] {
              Badge(BadgeProps{}.count(state->badge_count), BadgeContent{[state] {
                        Button(ButtonProps{}.onClick([state] { state->badge_count.set(0); }),
                               [] { Text(u8"gallery.badge.clear · 已读"); });
                    }});
              Badge(BadgeProps{}.count(120));
              Badge(BadgeProps{}.count(0).showZero());
              Badge(BadgeProps{}.dot(), BadgeContent{[] { Button(ButtonProps{}, [] { Text(u8"圆点通知"); }); }});
              Badge(BadgeProps{}.status(Status::Success).text(u8"服务正常"));
              Button(ButtonProps{}.onClick([state] { state->badge_count.set(state->badge_count.get() + 1); }),
                     [] { Text(u8"gallery.badge.increment · 添加通知"); });
          }});

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
               Input(InputProps{}.defaultValue(u8"gallery.drawer.draft · 保留的抽屉草稿"));
               TextArea(TextAreaProps{}.defaultValue(u8"抽屉中的多行备注\n关闭后重新打开仍然保留"));
               Select(SelectProps{}.options({{1, u8"桌面"}, {2, u8"服务"}}).defaultValue(1));
               Popconfirm(PopconfirmProps{}.title(u8"保存当前草稿？").onConfirm([state] {
                   state->common_feedback.set(String{u8"抽屉草稿已确认保存"});
               }),
                          PopconfirmTrigger{
                              [] { Button(ButtonProps{}, [] { Text(u8"gallery.drawer.confirm · 确认保存"); }); }});
               for (int row = 1; row <= 20; ++row) {
                   Text(utf8("抽屉正文 " + std::to_string(row) + " · 标题和页脚固定，正文独立滚动。"));
               }
               Input(InputProps{}.defaultValue(u8"gallery.drawer.end · 末尾编辑"));
           }},
           {.footer = DrawerFooter{[state] {
                Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] { state->drawer_open.set(false); }),
                       [] { Text(u8"gallery.drawer.done · 完成并关闭"); });
            }}});

    Text(u8"Popconfirm · 确认 / 加载 / 取消");
    Popconfirm(
        PopconfirmProps{}
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
        PopconfirmTrigger{[] { Button(ButtonProps{}, [] { Text(u8"gallery.popconfirm.controlled · 执行操作"); }); }},
        {.description = PopconfirmDescription{[state] {
             Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state] {
                      Text(u8"点击确定进入加载状态，再点击下方按钮完成。");
                      Button(ButtonProps{}.onClick([state] {
                          state->popconfirm_loading.set(false);
                          state->popconfirm_open.set(false);
                          state->common_feedback.set(String{u8"操作已完成"});
                      }),
                             [] { Text(u8"gallery.popconfirm.finish · 完成操作"); });
                  }});
         }}});
    Popconfirm(
        PopconfirmProps{}.title(u8"只显示确认按钮").showCancel(false).onConfirm([state] {
            state->badge_count.set(state->badge_count.get() + 1);
        }),
        PopconfirmTrigger{[] { Button(ButtonProps{}, [] { Text(u8"gallery.popconfirm.single · 快速确认"); }); }});

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
                      [] { Text(u8"gallery.alert.retry · 重试"); });
           }}});
    Alert(AlertProps{}.status(Status::Success).message(u8"数据已保存").showIcon());
    Alert(AlertProps{}.status(Status::Error).message(u8"操作失败，请稍后重试").showIcon());
    Button(ButtonProps{}.onClick([state] { state->alert_visible.set(true); }),
           [] { Text(u8"gallery.alert.restore · 恢复提示"); });
    Button(ButtonProps{}.onClick([state] { state->feedback_loading.set(!state->feedback_loading.get()); }),
           [] { Text(u8"gallery.spin.toggle · 切换加载"); });
    Spin(SpinProps{}.spinning(state->feedback_loading).description(u8"正在读取"), SpinContent{[state] {
             Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state] {
                      Input(InputProps{}.defaultValue(u8"gallery.spin.draft · 加载前后保留内容"));
                      Button(ButtonProps{}.onClick(
                                 [state] { state->common_feedback.set(String{u8"加载区域中的操作已执行"}); }),
                             [] { Text(u8"gallery.spin.action · 内容操作"); });
                  }});
         }});
    Button(ButtonProps{}.onClick([state] {
        state->feedback_percent.set(state->feedback_percent.get() >= 100 ? 0 : state->feedback_percent.get() + 25);
    }),
           [] { Text(u8"gallery.progress.advance · 推进进度"); });
    Button(ButtonProps{}.onClick([state] {
        state->feedback_status.set(state->feedback_status.get() == ProgressStatus::Exception
                                       ? ProgressStatus::Normal
                                       : ProgressStatus::Exception);
    }),
           [] { Text(u8"gallery.progress.status · 切换异常状态"); });
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
    state->telemetry.live_samples += 20;
}

void add_data_and_selection_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"gallery.table.editable · Table / Pagination · 编辑、排序、选择与分页");
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
                     [] { Text(u8"gallery.table.update · 更新首行"); });
              Button(ButtonProps{}.onClick([state] {
                  auto rows = state->records.get();
                  std::ranges::reverse(rows);
                  state->records.set(std::move(rows));
              }),
                     [] { Text(u8"gallery.table.reverse · 反转数据顺序"); });
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
                     [] { Text(u8"gallery.table.remove · 删除末行"); });
              Button(ButtonProps{}.onClick([state] {
                  state->records.set(gallery_records());
                  state->table_page.set(1);
                  state->table_sort.set(TableSort{});
                  state->table_selected.set({});
              }),
                     [] { Text(u8"gallery.table.restore · 恢复数据"); });
              Button(ButtonProps{}.onClick([state] { state->table_loading.set(!state->table_loading.get()); }),
                     [] { Text(u8"gallery.table.loading · 切换加载"); });
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
    Text(u8"gallery.pagination.controlled · 每页四项");
    Pagination(PaginationProps{}
                   .total(bind([state] { return state->records.get().size(); }))
                   .current(state->table_page)
                   .pageSize(std::size_t{4})
                   .hideOnSinglePage()
                   .onChange([state](std::size_t page, std::size_t) { state->table_page.set(page); }));

    Text(u8"gallery.menu.inline · Menu / Dropdown · 导航与命令");
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
    Space(SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
              Button(ButtonProps{}.onClick([state] {
                  state->menu_label.set(state->menu_label.get() == String{u8"任务"} ? String{u8"任务 / 已更新"}
                                                                                    : String{u8"任务"});
              }),
                     [] { Text(u8"gallery.menu.label · 更新菜单文字"); });
              Button(ButtonProps{}.onClick([state] { state->menu_disabled.set(!state->menu_disabled.get()); }),
                     [] { Text(u8"gallery.menu.disabled · 切换菜单禁用"); });
          }});
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
             DropdownTrigger{
                 [] { Button(ButtonProps{}, [] { Text(u8"gallery.dropdown.actions · 点击或右键打开命令"); }); }});

    Text(u8"InputNumber · 小数、范围、禁用与只读");
    Text(TextProps{}.content(state->data_feedback));
    Space(
        SpaceProps{}.wrap(true).size(dp(16)), SpaceContent{[state] {
            Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[state] {
                     Text(u8"gallery.number.controlled · 受控小数 0..10");
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
                     Text(u8"gallery.number.disabled · 禁用切换");
                     InputNumber(
                         InputNumberProps{}.defaultValue(5).disabled(state->number_disabled).size(ControlSize::Small));
                 }});
            Flex(FlexProps{}.vertical(true).gap(dp(4)), FlexContent{[] {
                     Text(u8"gallery.number.readonly · 只读");
                     InputNumber(
                         InputNumberProps{}.defaultValue(10).readOnly(true).controls(false).size(ControlSize::Large));
                 }});
        }});
    Space(SpaceProps{}.wrap(true).size(dp(8)), SpaceContent{[state] {
              Button(ButtonProps{}.onClick([state] { state->number_disabled.set(!state->number_disabled.get()); }),
                     [] { Text(u8"gallery.number.toggle-disabled · 切换数值禁用"); });
              Button(ButtonProps{}.onClick([state] { static_cast<void>(state->number_ref.focus()); }),
                     [] { Text(u8"gallery.number.focus · 聚焦小数编辑"); });
          }});
    state->telemetry.live_samples += 7;
}

void add_panel_and_selection_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"gallery.collapse.retained · Collapse · 保留编辑与独立操作");
    Text(TextProps{}.content(state->panel_feedback));
    Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)), SpaceContent{[state] {
              Button(
                  ButtonProps{}.onClick([state] { state->collapse_accordion.set(!state->collapse_accordion.get()); }),
                  [] { Text(u8"gallery.collapse.accordion · 切换手风琴模式"); });
              Button(ButtonProps{}.onClick([state] { state->collapse_disabled.set(!state->collapse_disabled.get()); }),
                     [] { Text(u8"gallery.collapse.disabled · 切换高级面板禁用"); });
              Button(ButtonProps{}.onClick([state] { state->collapse_keys.set({}); }),
                     [] { Text(u8"gallery.collapse.close · 收起全部"); });
              Button(ButtonProps{}.onClick([state] {
                  state->collapse_keys.set(CollapseKeys{String{u8"general"}});
                  state->collapse_select_open.set(true);
              }),
                     [] { Text(u8"gallery.collapse.options · 展示面板内选项"); });
          }});
    Collapse(
        CollapseProps{}
            .activeKeys(state->collapse_keys)
            .accordion(state->collapse_accordion)
            .onChange([state](CollapseKeys keys) { state->collapse_keys.set(std::move(keys)); }),
        {CollapsePanel{u8"general", CollapseTitle{[] { Text(u8"基本设置"); }}, CollapseBody{[state] {
                           Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[state] {
                                    Input(InputProps{}.defaultValue(u8"gallery.collapse.draft · 保留的设置草稿"));
                                    Select(SelectProps{}
                                               .options({{u8"draft", u8"草稿"}, {u8"service", u8"服务"}})
                                               .defaultValue(u8"draft")
                                               .open(state->collapse_select_open)
                                               .onOpenChange(
                                                   [state](bool open) { state->collapse_select_open.set(open); }));
                                }});
                       }}}
             .extra(CollapseExtra{[state] {
                 Button(ButtonProps{}.onClick([state] { state->panel_feedback.set(String{u8"已执行独立帮助操作"}); }),
                        [] { Text(u8"gallery.collapse.help · 帮助"); });
             }}),
         CollapsePanel{u8"advanced", CollapseTitle{[] { Text(u8"高级设置"); }},
                       CollapseBody{[] { Input(InputProps{}.defaultValue(u8"gallery.collapse.advanced · 高级草稿")); }}}
             .disabled(state->collapse_disabled)});

    Text(u8"gallery.segmented.controlled · Segmented · 横向等宽视图");
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
    Text(u8"gallery.segmented.vertical · 纵向小尺寸，共享受控值");
    Segmented(SegmentedProps{}
                  .value(state->segment_value)
                  .vertical()
                  .size(ControlSize::Small)
                  .layout(LayoutStyle{}.width(dp(240)))
                  .onChange([state](String value) { state->segment_value.set(std::move(value)); }),
              {{u8"day", u8"日视图"},
               SegmentedOption{u8"week", u8"周视图"}.disabled(state->segment_disabled),
               {u8"month", u8"月视图"}});
    Text(u8"gallery.segmented.rich · 大尺寸富标签");
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
                     [] { Text(u8"gallery.segmented.label · 更新分段标签"); });
              Button(ButtonProps{}.onClick([state] { state->segment_disabled.set(!state->segment_disabled.get()); }),
                     [] { Text(u8"gallery.segmented.disabled · 切换周视图禁用"); });
          }});
    state->telemetry.live_samples += 4;
}

void add_empty_and_result_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"gallery.empty.default · Empty · 空状态与创建操作");
    Empty(EmptyProps{}.description(state->empty_description).image(state->empty_image),
          {.extra = EmptyExtra{[state] {
               Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                   state->empty_description.set(String{u8"已创建示例任务，可继续编辑详情"});
               }),
                      [] { Text(u8"gallery.empty.create · 创建示例任务"); });
           }}});
    Button(ButtonProps{}.onClick([state] {
        state->empty_image.set(state->empty_image.get() == EmptyImage::None ? EmptyImage::Default : EmptyImage::None);
    }),
           [] { Text(u8"gallery.empty.image · 切换空状态图示"); });
    Text(u8"gallery.empty.compact · 紧凑空状态");
    Empty(EmptyProps{}.image(EmptyImage::Compact).description(u8"暂无筛选结果，请调整查询条件"));
    Text(u8"gallery.empty.custom · 自定义图示与外边距");
    Empty(EmptyProps{}.description(u8"自定义图示使用原生 Icon"),
          {.image = EmptyImageContent{[] {
               Icon(IconProps{}
                        .name(IconName::SearchOutlined)
                        .layout(LayoutStyle{}.width(dp(100)).margin_left(dp(50)).margin_right(dp(50))));
           }}});

    Text(u8"gallery.result.controlled · Result · 状态、详情与保留草稿");
    Text(TextProps{}.content(state->result_feedback));
    Result(ResultProps{}
               .status(state->result_status)
               .title(state->result_title)
               .subtitle(u8"切换状态后继续编辑同一份草稿"),
           {.content = ResultContent{[] {
                Text(u8"任务详情：编辑草稿后可切换上方结果状态。");
                Input(InputProps{}
                          .defaultValue(u8"gallery.result.draft · 保留的结果详情草稿")
                          .layout(LayoutStyle{}.width(dp(280))));
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
                       [] { Text(u8"gallery.result.status · 切换结果状态"); });
                Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                    state->result_feedback.set(String{u8"已执行结果页继续操作"});
                }),
                       [] { Text(u8"gallery.result.action · 继续操作"); });
            }}});
    Text(u8"gallery.result.exception · 404 异常结果");
    Result(ResultProps{}
               .status(ResultException::NotFound)
               .title(u8"404 · 页面未找到")
               .subtitle(u8"请检查任务地址或返回列表"));
    Text(u8"gallery.result.custom · 自定义结果图标");
    Result(ResultProps{}.title(u8"自定义结果展示").subtitle(u8"图示可通过 typed slot 组合现有控件"),
           {.icon = ResultIcon{[] {
                Flex(FlexProps{}.gap(dp(8)).align(FlexAlign::Center), FlexContent{[] {
                         Icon(IconProps{}.name(IconName::CheckCircleFilled));
                         Text(u8"READY");
                     }});
            }}});
    state->telemetry.live_samples += 6;
}

void add_form_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    const auto form_width = bind([state] { return dp(state->document_width.get().value() - 2); });
    Text(TextProps{}
             .content(String{u8"gallery.form.workflow · Form · 填写、校验与保存"})
             .layout(LayoutStyle{}.margin_left(dp(1))));
    Text(TextProps{}
             .content(bind([state] {
                 return utf8("gallery.form.status · " + std::string{state->form_feedback.get().bytes()} +
                             (state->form_disabled.get() ? " · 已禁用" : " · 已启用") +
                             (state->form_read_only.get() ? " · 只读" : " · 可编辑"));
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
                          .label(String{u8"gallery.form.name · 名称"})
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
                          .label(String{u8"gallery.form.role · 角色"})
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
                          .label(String{u8"gallery.form.count · 数量"})
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
                          .label(String{u8"gallery.form.consent · 确认"})
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
                     [] { Text(u8"gallery.form.submit · 保存"); });
              Button(ButtonProps{}.onClick([state] {
                  if (const auto result = state->form_ref.validate()) {
                      state->form_feedback.set(result->valid() ? String{u8"校验通过，尚未保存"}
                                                               : String{u8"校验失败，请查看字段错误"});
                  }
              }),
                     [] { Text(u8"gallery.form.validate · 校验"); });
              Button(ButtonProps{}.onClick([state] {
                  if (state->form_ref.reset()) {
                      state->form_feedback.set(String{u8"已重置"});
                  }
              }),
                     [] { Text(u8"gallery.form.reset · 重置"); });
              Button(ButtonProps{}.onClick([state] { state->form_disabled.set(!state->form_disabled.get()); }),
                     [] { Text(u8"gallery.form.disabled · 切换禁用"); });
              Button(ButtonProps{}.onClick([state] { state->form_read_only.set(!state->form_read_only.get()); }),
                     [] { Text(u8"gallery.form.readonly · 切换只读"); });
              Button(ButtonProps{}.onClick([state] {
                  state->form_help.set(state->form_help.get() == String{u8"填写用于保存的名称"}
                                           ? String{u8"名称与其他字段一起保存"}
                                           : String{u8"填写用于保存的名称"});
              }),
                     [] { Text(u8"gallery.form.help · 更新帮助"); });
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
                      .content(String{u8"gallery.date-picker.controlled · DatePicker · 日期选择"})
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          const auto value = state->date_value.get();
                          return utf8("gallery.date-picker.value · " +
                                      (value ? std::string{format_iso_date(*value).bytes()} : "未选择日期"));
                      }))
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             DatePicker(
                 DatePickerProps{}
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
                              Text(u8"gallery.date-picker.footer · 选择备注");
                              Input(InputProps{}.defaultValue(u8"保留的日期备注").layout(LayoutStyle{}.width(dp(220))));
                          }});
                 }});
             Button(ButtonProps{}.onClick([state] {
                 state->date_value.set(CivilDate{2026, 10, 12});
                 state->date_open.set(false);
             }),
                    [] { Text(u8"gallery.date-picker.restore · 恢复示例日期"); });
         }});
    Text(TextProps{}
             .content(String{u8"gallery.date-picker.form · Form String 日期适配"})
             .layout(LayoutStyle{}.margin_left(dp(1))));
    Text(TextProps{}
             .content(bind([state] {
                 return utf8("gallery.date-picker.status · " + std::string{state->date_feedback.get().bytes()} +
                             (state->date_form_disabled.get() ? " · 已禁用" : " · 已启用") +
                             (state->date_form_read_only.get() ? " · 只读" : " · 可编辑"));
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
                          .label(String{u8"gallery.date-picker.field · 日期"})
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
                     [] { Text(u8"gallery.date-picker.submit · 保存日期"); });
              Button(ButtonProps{}.onClick([state] {
                  if (state->date_form_ref.reset()) {
                      state->date_feedback.set(String{u8"日期已重置"});
                  }
              }),
                     [] { Text(u8"gallery.date-picker.reset · 重置日期"); });
              Button(
                  ButtonProps{}.onClick([state] { state->date_form_disabled.set(!state->date_form_disabled.get()); }),
                  [] { Text(u8"gallery.date-picker.disabled · 切换禁用"); });
              Button(
                  ButtonProps{}.onClick([state] { state->date_form_read_only.set(!state->date_form_read_only.get()); }),
                  [] { Text(u8"gallery.date-picker.readonly · 切换只读"); });
              Button(ButtonProps{}.onClick([state] {
                  state->date_help.set(state->date_help.get() ==
                                               String{u8"允许 2026-10-05 至 2026-11-25，每月 13 日不可选"}
                                           ? String{u8"日期以 YYYY-MM-DD 字符串保存"}
                                           : String{u8"允许 2026-10-05 至 2026-11-25，每月 13 日不可选"});
              }),
                     [] { Text(u8"gallery.date-picker.help · 更新日期帮助"); });
          }});
    state->telemetry.live_samples += 2;
}

void add_input_feature_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"Input 单行收尾：变体 / 统计 / 焦点 / 清空 / Password / Search");
    Space(SpaceProps{}.wrap(true).align(SpaceAlign::Start).layout(LayoutStyle{}.width(state->document_width)),
          SpaceContent{[state] {
              for (const auto variant :
                   {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
                  const String id = utf8(variant == InputVariant::Outlined     ? "gallery.input.outlined"
                                         : variant == InputVariant::Filled     ? "gallery.input.filled"
                                         : variant == InputVariant::Borderless ? "gallery.input.borderless"
                                                                               : "gallery.input.underlined");
                  Flex(FlexProps{}.vertical(true).gap(dp(4)).layout(LayoutStyle{}.width(state->cell_width)),
                       FlexContent{[variant, id, state] {
                           Text(id);
                           Input(InputProps{}
                                     .variant(variant)
                                     .defaultValue(u8"中文 / RynUI 🙂")
                                     .allowClear(true)
                                     .showCount()
                                     .count(InputCountOptions{20, InputCountUnit::Grapheme})
                                     .layout(LayoutStyle{}.width(state->cell_width)));
                       }});
              }
              Text(u8"gallery.input.grapheme-count · 原文超限只提示");
              Input(InputProps{}
                        .defaultValue(u8"é🙂中文")
                        .showCount()
                        .count(InputCountOptions{3, InputCountUnit::Grapheme})
                        .layout(LayoutStyle{}.width(state->cell_width)));
              Text(u8"gallery.input.exceed-formatter · 字节统计 / 去除空格");
              Input(InputProps{}
                        .defaultValue(u8"RynUI")
                        .showCount()
                        .count(InputCountOptions{8, InputCountUnit::Scalar})
                        .countStrategy([](StringView value) { return value.bytes().size(); })
                        .exceedFormatter([](String value, std::size_t) {
                            auto bytes = std::string{value.bytes()};
                            std::erase(bytes, ' ');
                            return utf8(bytes);
                        })
                        .layout(LayoutStyle{}.width(state->cell_width)));
              Text(u8"gallery.input.focus-hints · Email / 原生提示");
              Input(InputProps{}
                        .ref(state->input_ref)
                        .defaultValue(u8"desktop@example.com")
                        .purpose(InputPurpose::Email)
                        .capitalization(InputCapitalization::None)
                        .autocorrect(false)
                        .onFocus([state] { state->input_feedback.set(String{u8"引用输入已聚焦"}); })
                        .layout(LayoutStyle{}.width(state->cell_width)));
              Text(u8"gallery.input.clear-vector · 禁用动作保留图标");
              Input(InputProps{}
                        .defaultValue(u8"自定义清空图标")
                        .allowClear(true)
                        .clearDisabled(state->input_clear_disabled)
                        .clearIcon(icon_vector_sample())
                        .onClear([state] { state->input_feedback.set(String{u8"onClear 已通知"}); })
                        .layout(LayoutStyle{}.width(state->cell_width)));
              Text(u8"gallery.password.hover-vector · 悬停 / prefix / count");
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
              Text(u8"gallery.password.controlled-toggle · 受控 / 显隐动作开关");
              Password(PasswordProps{}
                           .defaultValue(u8"controlled")
                           .visible(state->password_visible)
                           .visibilityToggle(state->password_toggle)
                           .onVisibleChange([state](bool value) { state->password_visible.set(value); })
                           .layout(LayoutStyle{}.width(state->cell_width)));
              ThemeConfig dark;
              dark.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
              Theme(ThemeProps{}.config(dark), ThemeContent{[state] {
                        ++state->telemetry.theme_content_runs;
                        Text(u8"gallery.search.filled-vector · Dark Compact / Clear 来源");
                        Search(SearchProps{}
                                   .variant(InputVariant::Filled)
                                   .size(ControlSize::Small)
                                   .defaultValue(u8"RynUI")
                                   .searchIcon(icon_vector_sample())
                                   .allowClear(true)
                                   .showCount()
                                   .onSearch([state](String value, SearchSource source) {
                                       ++state->telemetry.search_submits;
                                       state->search_feedback.set(source == SearchSource::Clear ? String{u8"Clear"}
                                                                                                : std::move(value));
                                   })
                                   .layout(LayoutStyle{}.width(state->cell_width)));
                    }});
              Text(u8"gallery.search.underlined-small · 连接高度 / typed content");
              Search(SearchProps{}
                         .variant(InputVariant::Underlined)
                         .size(ControlSize::Small)
                         .defaultValue(u8"Search")
                         .layout(LayoutStyle{}.width(state->cell_width)),
                     SearchButtonContent{[] { Text(u8"查询"); }});
              Button(ButtonProps{}.onClick(
                         [state] { static_cast<void>(state->input_ref.focus({InputFocusCursor::All})); }),
                     ButtonContent{[] { Text(u8"gallery.input.focus-all"); }});
              Button(ButtonProps{}.onClick(
                         [state] { state->input_clear_disabled.set(!state->input_clear_disabled.get()); }),
                     ButtonContent{[] { Text(u8"gallery.input.toggle-clear-disabled"); }});
              Button(ButtonProps{}.onClick([state] { state->password_toggle.set(!state->password_toggle.get()); }),
                     ButtonContent{[] { Text(u8"gallery.password.toggle-action"); }});
          }});
    state->telemetry.live_samples += 15;
}

void add_text_area_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"TextArea / 多行输入：换行、选择、统计、autoSize、滚动与 resize");
    Space(SpaceProps{}.wrap(true).align(SpaceAlign::Start).layout(LayoutStyle{}.width(state->document_width)),
          SpaceContent{[state] {
              for (const auto variant :
                   {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
                  const auto id = utf8(variant == InputVariant::Outlined     ? "gallery.text-area.outlined"
                                       : variant == InputVariant::Filled     ? "gallery.text-area.filled"
                                       : variant == InputVariant::Borderless ? "gallery.text-area.borderless"
                                                                             : "gallery.text-area.underlined");
                  Flex(FlexProps{}.vertical(true).gap(dp(4)).layout(LayoutStyle{}.width(state->cell_width)),
                       FlexContent{[state, id, variant] {
                           Text(id);
                           TextArea(TextAreaProps{}
                                        .variant(variant)
                                        .rows(3)
                                        .defaultValue(u8"中文 / RynUI 🙂\n第二行\nEnter 换行")
                                        .allowClear(true)
                                        .showCount()
                                        .count(InputCountOptions{40, InputCountUnit::Grapheme})
                                        .onChange([state](String value) {
                                            ++state->telemetry.input_changes;
                                            state->input_feedback.set(std::move(value));
                                        })
                                        .layout(LayoutStyle{}.width(state->cell_width)));
                       }});
              }
              Text(u8"gallery.text-area.autosize · minRows=2 / maxRows=5");
              TextArea(TextAreaProps{}
                           .defaultValue(u8"autoSize 随编辑与宽度换行扩展\n保留空行\n\n")
                           .autoSize(TextAreaAutoSize{true, 2, 5})
                           .allowClear(true)
                           .showCount()
                           .layout(LayoutStyle{}.width(state->cell_width)));
              Text(u8"gallery.text-area.resize-nowrap · 双向拖动 / wheel");
              TextArea(
                  TextAreaProps{}
                      .defaultValue(
                          u8"No wrap: abcdefghijklmnopqrstuvwxyz 0123456789\n第二行\n第三行\n第四行\n第五行\n第六行")
                      .rows(2)
                      .wrap(false)
                      .resize(TextAreaResize::Both)
                      .layout(LayoutStyle{}.width(state->cell_width)));
              ThemeConfig dark;
              dark.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
              Theme(ThemeProps{}.config(dark), ThemeContent{[state] {
                        ++state->telemetry.theme_content_runs;
                        Text(u8"gallery.text-area.readonly · Dark Compact / 可选择复制");
                        TextArea(TextAreaProps{}
                                     .defaultValue(u8"只读可以选择、复制和滚动\n中文 / Latin 🙂\n第三行\n第四行")
                                     .rows(2)
                                     .readOnly(true)
                                     .variant(InputVariant::Filled)
                                     .size(ControlSize::Small)
                                     .showCount()
                                     .layout(LayoutStyle{}.width(state->cell_width)));
                    }});
              Text(u8"gallery.text-area.disabled · Warning / 禁用");
              TextArea(TextAreaProps{}
                           .defaultValue(u8"禁用输入\n不响应编辑或拖动")
                           .rows(2)
                           .disabled(true)
                           .status(InputStatus::Warning)
                           .layout(LayoutStyle{}.width(state->cell_width)));
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
                           Text(id);
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
                       }});
              }
              Text(u8"gallery.otp.controlled · 六格 / 填满通知");
              OTP(OTPProps{}
                      .value(state->otp_value)
                      .ref(state->otp_ref)
                      .onInput([state](const auto&) { ++state->telemetry.input_changes; })
                      .onChange([state](String value) {
                          state->otp_value.set(value);
                          state->input_feedback.set(std::move(value));
                      }));
              Text(u8"gallery.otp.formatter-mask · 大写 / bullet / Large");
              OTP(OTPProps{}.defaultValue(u8"rYnUI!").mask(true).size(ControlSize::Large).formatter([](String value) {
                  std::string bytes{value.bytes()};
                  for (auto& byte : bytes) {
                      if (byte >= 'a' && byte <= 'z') {
                          byte = static_cast<char>(byte - 'a' + 'A');
                      }
                  }
                  return String::from_utf8(bytes).value();
              }));
              Text(u8"gallery.otp.separator-rtl · typed separator / RightToLeft");
              OTP(OTPProps{}.length(4).defaultValue(u8"1234").direction(OTPDirection::RightToLeft),
                  OTPSeparator{[](std::size_t index) -> std::optional<OTPSeparatorContent> {
                      if (index != 1) {
                          return {};
                      }
                      return OTPSeparatorContent{[] { Text(u8"-"); }};
                  }});
              Text(u8"gallery.otp.dynamic-length · 四/六格 / 前缀身份保留");
              OTP(OTPProps{}.length(state->otp_length).defaultValue(u8"12345678"));
              ThemeConfig dark;
              dark.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
              Theme(ThemeProps{}.config(dark), ThemeContent{[state] {
                        ++state->telemetry.theme_content_runs;
                        Text(u8"gallery.otp.readonly-mask · Dark Compact / Small / 自定义 mask");
                        OTP(OTPProps{}
                                .length(4)
                                .defaultValue(u8"只读验证码")
                                .mask(u8"*")
                                .readOnly(true)
                                .variant(InputVariant::Filled)
                                .size(ControlSize::Small));
                    }});
              Text(u8"gallery.otp.disabled · Warning / 禁用");
              OTP(OTPProps{}.length(4).defaultValue(u8"1234").disabled(true).status(InputStatus::Warning));
              Button(ButtonProps{}.onClick([state] { static_cast<void>(state->otp_ref.focus()); }),
                     ButtonContent{[] { Text(u8"gallery.otp.focus-all"); }});
              Button(ButtonProps{}.onClick([state] { state->otp_length.set(state->otp_length.get() == 6 ? 4 : 6); }),
                     ButtonContent{[] { Text(u8"gallery.otp.toggle-length"); }});
          }});
    state->telemetry.live_samples += 12;
}

void add_bidi_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"双向文字：Arabic / Hebrew / 数字 / 括号 · Auto / LTR / RTL");
    Text(TextProps{}.content(u8"gallery.bidi.text · مرحبا (12) אבג RynUI").direction(state->text_direction));
    Paragraph(TypographyProps{}
                  .content(u8"gallery.bidi.paragraph · مرحبا 123 (אבג)\nאבג 12 مرحبا 中文")
                  .direction(state->text_direction)
                  .underline(true)
                  .layout(LayoutStyle{}.width(state->cell_width)));
    Text(u8"gallery.bidi.input");
    Input(InputProps{}
              .defaultValue(u8"A אבג 12 مرحبا")
              .direction(state->text_direction)
              .layout(LayoutStyle{}.width(state->cell_width)));
    Text(u8"gallery.bidi.text-area");
    TextArea(TextAreaProps{}
                 .defaultValue(u8"אבג (12) مرحبا\nمرحبا 34 אבג 中文\n\n尾行")
                 .direction(state->text_direction)
                 .rows(3)
                 .layout(LayoutStyle{}.width(state->cell_width)));
    Text(u8"gallery.bidi.password");
    Password(PasswordProps{}
                 .defaultValue(u8"אבג مرحبا 👩‍💻")
                 .direction(state->text_direction)
                 .layout(LayoutStyle{}.width(state->cell_width)));
    Button(ButtonProps{}.onClick([state] {
        const auto current = state->text_direction.get();
        state->text_direction.set(current == TextDirection::Auto          ? TextDirection::LeftToRight
                                  : current == TextDirection::LeftToRight ? TextDirection::RightToLeft
                                                                          : TextDirection::Auto);
    }),
           ButtonContent{[] { Text(u8"gallery.bidi.toggle-direction · 切换段落方向"); }});
    state->telemetry.live_samples += 6;
}

void add_select_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"Select · 单选 / Unicode 搜索 / 清空 / 动态选项 / 键盘");
    const std::vector<SelectOption> choices{
        {u8"desktop", u8"桌面 / Desktop"}, {u8"service", u8"服务 / Service"}, {u8"unavailable", u8"禁用选项", true}};
    const auto field = [&](SelectProps props, const char8_t* caption) {
        Text(String::from_utf8(std::u8string_view{caption}).value());
        Select(props.layout(LayoutStyle{}.width(state->cell_width)));
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
    field(SelectProps{}.options({{1, u8"优先级 1"}, {42, u8"优先级 42"}, {99, u8"禁用", true}}).defaultValue(42),
          u8"gallery.select.numeric · int64 value");
    field(SelectProps{}.options(choices).defaultValue(u8"desktop").disabled(true), u8"gallery.select.disabled");
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
        field(SelectProps{}.options(choices).defaultValue(u8"desktop").variant(variant).allowClear(), caption);
    }
    for (const auto [size, caption] : std::array{std::pair{ControlSize::Small, u8"gallery.select.small"},
                                                 std::pair{ControlSize::Middle, u8"gallery.select.middle"},
                                                 std::pair{ControlSize::Large, u8"gallery.select.large"}}) {
        field(SelectProps{}.options(choices).defaultValue(u8"desktop").size(size), caption);
    }
    field(SelectProps{}.options(choices).status(InputStatus::Warning).defaultValue(u8"desktop"),
          u8"gallery.select.warning");
    field(SelectProps{}.options(choices).status(InputStatus::Error).defaultValue(u8"desktop"),
          u8"gallery.select.error");
    ThemeConfig custom;
    custom.select.algorithm = true;
    custom.select.seed.color_primary = Color::rgba8(114, 46, 209);
    custom.select.tokens.option_height = dp(36);
    Theme(ThemeProps{}.config(custom), ThemeContent{[state, choices] {
              ++state->telemetry.theme_content_runs;
              Text(u8"gallery.select.component-theme · 独立颜色 / 36 dp 行高");
              Select(SelectProps{}
                         .options(choices)
                         .defaultValue(u8"service")
                         .showSearch()
                         .layout(LayoutStyle{}.width(state->cell_width)),
                     SelectPrefix{[] { Icon(IconProps{}.name(IconName::SearchOutlined)); }});
              ++state->telemetry.live_samples;
          }});
    Button(ButtonProps{}.onClick([state] { static_cast<void>(state->select_ref.focus()); }),
           ButtonContent{[] { Text(u8"gallery.select.focus"); }});
    Button(ButtonProps{}.onClick([state] { state->select_open.set(!state->select_open.get()); }),
           ButtonContent{[] { Text(u8"gallery.select.toggle-open"); }});
    Button(ButtonProps{}.onClick([state] {
        state->select_options_expanded = !state->select_options_expanded;
        auto options = std::vector<SelectOption>{{u8"desktop", u8"桌面 / Desktop"}, {u8"service", u8"服务 / Service"}};
        if (state->select_options_expanded) {
            options.push_back({u8"device", u8"设备 / Device"});
        }
        state->select_options.set(std::move(options));
    }),
           ButtonContent{[] { Text(u8"gallery.select.dynamic-options"); }});
    state->telemetry.live_samples += 3;
}

void add_card_samples(const std::shared_ptr<GalleryState>& state) {
    using namespace ryn;
    Text(u8"Card · 标题 / 区域组合 / 保留编辑状态 / 加载");
    const auto props = [state] {
        return CardProps{}.layout(LayoutStyle{}.width(state->cell_width));
    };
    Text(u8"gallery.card.basic · 无标题内容容器");
    Card(props(), CardContent{[] { Text(u8"Business content / 业务内容"); }});
    Text(u8"gallery.card.account · 标题 / extra / 编辑状态");
    Card(props().title(u8"Account / 账户").loading(state->card_loading), CardContent{[] {
             Input(InputProps{}.defaultValue(u8"保留草稿 / Draft").layout(LayoutStyle{}.width(dp(140))));
         }},
         CardSlots{.extra = CardExtra{[state] {
                       Button(ButtonProps{}.onClick([state] { ++state->telemetry.activations; }),
                              [] { Text(u8"Edit"); });
                   }}});
    Text(u8"gallery.card.cover-actions · 富标题 / cover / actions");
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
    Text(u8"gallery.card.small · 小号尺寸");
    Card(props().title(u8"Small / 小号").size(CardSize::Small), CardContent{[] { Text(u8"12 dp padding"); }});
    Text(u8"gallery.card.borderless · 无边框 / 子控件 hover");
    Card(props().title(u8"Hover / 悬停").variant(CardVariant::Borderless).hoverable(), CardContent{[state] {
             Button(ButtonProps{}.onClick([state] { ++state->telemetry.activations; }),
                    [] { Text(u8"Nested action"); });
         }});
    Text(u8"gallery.card.loading · 静态占位");
    Card(props().title(u8"Loading / 加载").loading(), CardContent{[] { Text(u8"Data remains mounted"); }});
    ThemeConfig custom;
    custom.card.tokens.background = Color::rgba8(238, 246, 255);
    custom.card.tokens.header_background = Color::rgba8(220, 235, 255);
    custom.card.tokens.text = Color(0, 0, 0, 0.88F);
    custom.card.tokens.header_text = Color(0, 0, 0, 0.88F);
    custom.card.tokens.body_padding = dp(16);
    Theme(ThemeProps{}.config(custom), ThemeContent{[state, props] {
              ++state->telemetry.theme_content_runs;
              Text(u8"gallery.card.component-theme · 局部背景 / 16 dp padding");
              Card(props().title(u8"Component Token"), CardContent{[] { Text(u8"Scoped presentation / 局部主题"); }});
          }});
    Button(ButtonProps{}.onClick([state] {
        state->card_loading.set(!state->card_loading.get());
        ++state->telemetry.activations;
    }),
           [] { Text(u8"gallery.card.toggle-loading · 切换账户卡片加载"); });
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
                      .content(String{u8"gallery.breadcrumb.trail · Breadcrumb · 项目层级"})
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}.content(bind([state] {
                 return utf8("gallery.breadcrumb.feedback · " + std::string{state->breadcrumb_feedback.get().bytes()});
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
                                             .defaultValue(u8"Breadcrumb draft")
                                             .placeholder(u8"gallery.breadcrumb.draft")
                                             .ref(state->breadcrumb_ref)
                                             .onChange([state](String value) { state->breadcrumb_draft.set(value); })
                                             .layout(LayoutStyle{}.width(dp(150))));
                                   Button(ButtonProps{}.onClick([state] {
                                       state->breadcrumb_feedback.set(
                                           utf8("应用 " + std::string{state->breadcrumb_draft.get().bytes()}));
                                   }),
                                          [] { Text(u8"应用草稿"); });
                               }});
                      }})});
             Text(u8"gallery.breadcrumb.display · 展示层级中的独立编辑");
             Breadcrumb(BreadcrumbProps{}.layout(LayoutStyle{}.margin_left(dp(8))),
                        {BreadcrumbItem{u8"preview", u8"预览"}.richTitle(BreadcrumbTitle{[] {
                             Input(InputProps{}.defaultValue(u8"Display draft").layout(LayoutStyle{}.width(dp(150))));
                         }}),
                         BreadcrumbItem{u8"read", u8"只展示层级"}});
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}.onClick([state] {
                           state->breadcrumb_title.set(
                               state->breadcrumb_title.get() == String{u8"项目"} ? String{u8"归档"} : String{u8"项目"});
                       }),
                              [] { Text(u8"gallery.breadcrumb.rename · 改名"); });
                       Button(ButtonProps{}.onClick(
                                  [state] { state->breadcrumb_visible.set(!state->breadcrumb_visible.get()); }),
                              [] { Text(u8"gallery.breadcrumb.visible · 显隐祖先"); });
                       Button(ButtonProps{}.onClick([state] {
                           state->breadcrumb_draft_visible.set(!state->breadcrumb_draft_visible.get());
                       }),
                              [] { Text(u8"gallery.breadcrumb.draft-visible · 显隐草稿"); });
                       Button(ButtonProps{}.onClick(
                                  [state] { state->breadcrumb_disabled.set(!state->breadcrumb_disabled.get()); }),
                              [] { Text(u8"gallery.breadcrumb.disabled · 锁定层级"); });
                       Button(ButtonProps{}.onClick([state] {
                           state->breadcrumb_title.set(String{u8"项目"});
                           state->breadcrumb_visible.set(true);
                           state->breadcrumb_draft_visible.set(true);
                           state->breadcrumb_disabled.set(false);
                       }),
                              [] { Text(u8"gallery.breadcrumb.restore · 恢复层级"); });
                       Button(ButtonProps{}.onClick([state] { static_cast<void>(state->breadcrumb_ref.focus()); }),
                              [] { Text(u8"gallery.breadcrumb.focus · 编辑草稿"); });
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
                      .content(String{u8"gallery.steps.workflow · Steps · 发布流程"})
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}.content(bind([state] {
                 return utf8("gallery.steps.feedback · current=" + std::to_string(state->steps_current.get()) + " · " +
                             std::string{state->steps_feedback.get().bytes()});
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
                                     Input(InputProps{}
                                               .defaultValue(u8"Steps draft")
                                               .placeholder(u8"gallery.steps.draft")
                                               .ref(state->steps_ref)
                                               .onChange([state](String value) { state->steps_draft.set(value); })
                                               .layout(LayoutStyle{}.width(dp(120))));
                                     Button(ButtonProps{}.onClick([state] {
                                         state->steps_feedback.set(
                                             utf8("保存 " + std::string{state->steps_draft.get().bytes()}));
                                     }),
                                            [] { Text(u8"保存备注"); });
                                 }});
                        }}),
                    StepsItem{u8"review", u8"审核"}
                        .description(u8"核对资料")
                        .visible(state->steps_visible)
                        .disabled(state->steps_disabled)
                        .icon(StepsIcon{[] { Icon(IconProps{}.name(IconName::SearchOutlined)); }}),
                    StepsItem{u8"publish", u8"发布"}.description(u8"确认后发布")});
             Space(
                 SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                 SpaceContent{[state] {
                     Button(ButtonProps{}.onClick([state] {
                         state->steps_current.set(state->steps_current.get() > 0 ? state->steps_current.get() - 1 : 0);
                         state->steps_feedback.set(String{u8"上一步"});
                     }),
                            [] { Text(u8"gallery.steps.previous · 上一步"); });
                     Button(ButtonProps{}.type(ButtonType::Primary).onClick([state] {
                         state->steps_current.set(std::min<std::size_t>(3, state->steps_current.get() + 1));
                         state->steps_feedback.set(state->steps_current.get() == 3 ? String{u8"发布完成"}
                                                                                   : String{u8"下一步"});
                     }),
                            [] { Text(u8"gallery.steps.next · 下一步"); });
                     Button(ButtonProps{}.onClick([state] {
                         state->steps_current.set(0);
                         state->steps_visible.set(true);
                         state->steps_disabled.set(false);
                         state->steps_status.set(StepsStatus::Process);
                         state->steps_feedback.set(String{u8"流程已重置，备注保留"});
                     }),
                            [] { Text(u8"gallery.steps.reset · 重置流程"); });
                     Button(ButtonProps{}.onClick([state] { state->steps_vertical.set(!state->steps_vertical.get()); }),
                            [] { Text(u8"gallery.steps.vertical · 横纵切换"); });
                     Button(ButtonProps{}.onClick([state] {
                         state->steps_size.set(state->steps_size.get() == StepsSize::Default ? StepsSize::Small
                                                                                             : StepsSize::Default);
                     }),
                            [] { Text(u8"gallery.steps.size · 切换尺寸"); });
                     Button(ButtonProps{}.onClick([state] {
                         state->steps_status.set(state->steps_status.get() == StepsStatus::Process
                                                     ? StepsStatus::Error
                                                     : StepsStatus::Process);
                     }),
                            [] { Text(u8"gallery.steps.status · 切换错误状态"); });
                     Button(ButtonProps{}.onClick([state] { state->steps_visible.set(!state->steps_visible.get()); }),
                            [] { Text(u8"gallery.steps.visible · 显隐审核"); });
                     Button(ButtonProps{}.onClick([state] { state->steps_disabled.set(!state->steps_disabled.get()); }),
                            [] { Text(u8"gallery.steps.disabled · 锁定资料与审核"); });
                     Button(ButtonProps{}.onClick([state] { static_cast<void>(state->steps_ref.focus()); }),
                            [] { Text(u8"gallery.steps.focus · 编辑备注"); });
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
                      .content(String{u8"gallery.time-picker.controlled · TimePicker · 时间选择"})
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          const auto value = state->time_value.get();
                          return utf8(
                              "gallery.time-picker.value · " +
                              (value ? std::string{format_time_of_day(*value, TimePickerPrecision::Minute).bytes()}
                                     : "未选择时间") +
                              (state->time_confirm.get() ? " · 确认模式" : " · 即时模式") +
                              (state->time_accept.get() ? " · 接受选择" : " · 锁定已保存值") +
                              (state->time_input_read_only.get() ? " · 文本只读" : " · 可输入") + " · " +
                              std::string{state->time_request.get().bytes()});
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
                              Text(u8"gallery.time-picker.footer · 选择备注");
                              Input(InputProps{}.defaultValue(u8"保留的时间备注").layout(LayoutStyle{}.width(dp(220))));
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
                              [] { Text(u8"gallery.time-picker.restore · 恢复时间示例"); });
                       Button(ButtonProps{}.onClick([state] { state->time_confirm.set(!state->time_confirm.get()); }),
                              [] { Text(u8"gallery.time-picker.mode · 确认/即时模式"); });
                       Button(ButtonProps{}.onClick([state] { state->time_accept.set(!state->time_accept.get()); }),
                              [] { Text(u8"gallery.time-picker.authority · 切换锁定已保存值"); });
                       Button(ButtonProps{}.onClick(
                                  [state] { state->time_input_read_only.set(!state->time_input_read_only.get()); }),
                              [] { Text(u8"gallery.time-picker.input-readonly · 切换文本只读"); });
                   }});
         }});
    Text(TextProps{}
             .content(String{u8"gallery.time-picker.form · Form String 时间适配"})
             .layout(LayoutStyle{}.margin_left(dp(1))));
    Text(TextProps{}
             .content(bind([state] {
                 return utf8("gallery.time-picker.status · " + std::string{state->time_feedback.get().bytes()} +
                             (state->time_form_disabled.get() ? " · 已禁用" : " · 已启用") +
                             (state->time_form_read_only.get() ? " · 只读" : " · 可编辑"));
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
                          .label(String{u8"gallery.time-picker.field · 时间"})
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
                     [] { Text(u8"gallery.time-picker.submit · 保存时间"); });
              Button(ButtonProps{}.onClick([state] {
                  if (state->time_form_ref.reset()) {
                      state->time_feedback.set(String{u8"时间已重置"});
                  }
              }),
                     [] { Text(u8"gallery.time-picker.reset · 重置时间"); });
              Button(
                  ButtonProps{}.onClick([state] { state->time_form_disabled.set(!state->time_form_disabled.get()); }),
                  [] { Text(u8"gallery.time-picker.disabled · 切换禁用"); });
              Button(
                  ButtonProps{}.onClick([state] { state->time_form_read_only.set(!state->time_form_read_only.get()); }),
                  [] { Text(u8"gallery.time-picker.readonly · 切换只读"); });
              Button(ButtonProps{}.onClick([state] {
                  state->time_help.set(state->time_help.get() == String{u8"HH:mm:ss，08:00:00 至 18:00:00，12 时不可选"}
                                           ? String{u8"仅确认后的 HH:mm:ss 字符串进入保存结果"}
                                           : String{u8"HH:mm:ss，08:00:00 至 18:00:00，12 时不可选"});
              }),
                     [] { Text(u8"gallery.time-picker.help · 更新时间帮助"); });
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
                      .content(u8"gallery.descriptions.editor · Descriptions · 项目资料")
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}.content(bind([state] {
                 return utf8("gallery.descriptions.feedback · " + std::string{state->details_feedback.get().bytes()});
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
                                                 .defaultValue(u8"晨间项目")
                                                 .placeholder(u8"gallery.descriptions.name")
                                                 .ref(state->details_ref)
                                                 .onChange([state](String value) { state->details_name.set(value); })
                                                 .layout(LayoutStyle{}
                                                             .width(dp(156))
                                                             .margin_left(dp(6))
                                                             .margin_right(dp(6))
                                                             .margin_top(dp(6))
                                                             .margin_bottom(dp(6))));
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
                              [] { Text(u8"gallery.descriptions.columns · 单列/多列"); });
                       Button(ButtonProps{}.onClick(
                                  [state] { state->details_span.set(state->details_span.get() == 1 ? 2 : 1); }),
                              [] { Text(u8"gallery.descriptions.span · 名称跨列"); });
                       Button(ButtonProps{}.onClick(
                                  [state] { state->details_visible.set(!state->details_visible.get()); }),
                              [] { Text(u8"gallery.descriptions.visible · 显隐资料"); });
                       Button(ButtonProps{}.onClick(
                                  [state] { state->details_vertical.set(!state->details_vertical.get()); }),
                              [] { Text(u8"gallery.descriptions.vertical · 横纵标签"); });
                       Button(ButtonProps{}.onClick(
                                  [state] { state->details_bordered.set(!state->details_bordered.get()); }),
                              [] { Text(u8"gallery.descriptions.bordered · 切换边框"); });
                       Button(ButtonProps{}.onClick([state] {
                           state->details_size.set(
                               state->details_size.get() == DescriptionsSize::Default  ? DescriptionsSize::Middle
                               : state->details_size.get() == DescriptionsSize::Middle ? DescriptionsSize::Small
                                                                                       : DescriptionsSize::Default);
                       }),
                              [] { Text(u8"gallery.descriptions.size · 切换尺寸"); });
                       Button(ButtonProps{}.onClick([state] {
                           state->details_visible.set(true);
                           static_cast<void>(state->details_ref.focus());
                       }),
                              [] { Text(u8"gallery.descriptions.focus · 编辑名称"); });
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
                      .content(u8"gallery.skeleton.preview · Skeleton · 正文加载")
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}.content(bind([state] {
                 return utf8("gallery.skeleton.feedback · " + std::string{state->skeleton_feedback.get().bytes()});
             })));
             Space(
                 SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                 SpaceContent{[state] {
                     Button(ButtonProps{}.onClick(
                                [state] { state->skeleton_loading.set(!state->skeleton_loading.get()); }),
                            [] { Text(u8"gallery.skeleton.loading · 加载/显示正文"); });
                     Button(ButtonProps{}.onClick([state] { state->skeleton_title.set(!state->skeleton_title.get()); }),
                            [] { Text(u8"gallery.skeleton.title · 标题"); });
                     Button(ButtonProps{}.onClick(
                                [state] { state->skeleton_paragraph.set((state->skeleton_paragraph.get() + 1) % 3); }),
                            [] { Text(u8"gallery.skeleton.paragraph · 正文行数"); });
                     Button(
                         ButtonProps{}.onClick([state] { state->skeleton_avatar.set(!state->skeleton_avatar.get()); }),
                         [] { Text(u8"gallery.skeleton.avatar · 头像"); });
                     Button(ButtonProps{}.onClick([state] { state->skeleton_round.set(!state->skeleton_round.get()); }),
                            [] { Text(u8"gallery.skeleton.round · 圆角"); });
                     Button(ButtonProps{}.onClick([state] {
                         state->skeleton_loading.set(false);
                         static_cast<void>(state->skeleton_ref.focus());
                     }),
                            [] { Text(u8"gallery.skeleton.focus · 编辑正文"); });
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
                                             .defaultValue(u8"今天整理项目资料")
                                             .placeholder(u8"gallery.skeleton.draft")
                                             .ref(state->skeleton_ref)
                                             .onChange([state](String value) { state->skeleton_draft.set(value); })
                                             .layout(LayoutStyle{}
                                                         .width(dp(260))
                                                         .margin_left(dp(6))
                                                         .margin_right(dp(6))
                                                         .margin_top(dp(6))
                                                         .margin_bottom(dp(6))));
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
             Text(u8"gallery.skeleton.standalone · 独立占位");
             Space(SpaceProps{}.wrap(true).size(dp(8)).layout(LayoutStyle{}.width(state->document_width)),
                   SpaceContent{[state] {
                       Button(ButtonProps{}.onClick(
                                  [state] { state->skeleton_shape.set((state->skeleton_shape.get() + 1) % 4); }),
                              [] { Text(u8"gallery.skeleton.shape · 切换形态与尺寸"); });
                       Button(
                           ButtonProps{}.onClick([state] { state->skeleton_block.set(!state->skeleton_block.get()); }),
                           [] { Text(u8"gallery.skeleton.block · 填满宽度"); });
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
                      .content(u8"gallery.popover.editor · Popover · 项目备注")
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return utf8("gallery.popover.feedback · " +
                                      std::string{state->popover_feedback.get().bytes()});
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
                                      [] { Text(u8"gallery.popover.trigger · 编辑备注"); });
                           }},
                           {.title = PopoverTitle{[] { Text(u8"项目备注"); }},
                            .content = PopoverContent{[state] {
                                Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)),
                                     FlexContent{[state] {
                                         Input(InputProps{}
                                                   .defaultValue(u8"下次会议前补齐项目备注")
                                                   .placeholder(u8"gallery.popover.draft")
                                                   .ref(state->popover_ref)
                                                   .onChange([state](String value) { state->popover_draft.set(value); })
                                                   .layout(LayoutStyle{}
                                                               .width(dp(180))
                                                               .margin_left(dp(6))
                                                               .margin_right(dp(6))
                                                               .margin_top(dp(6))
                                                               .margin_bottom(dp(6))));
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
                              [] { Text(u8"gallery.popover.placement · 切换方位"); });
                       Button(ButtonProps{}.onClick([state] { state->popover_arrow.set(!state->popover_arrow.get()); }),
                              [] { Text(u8"gallery.popover.arrow · 切换箭头"); });
                       Button(ButtonProps{}.onClick(
                                  [state] { state->popover_disabled.set(!state->popover_disabled.get()); }),
                              [] { Text(u8"gallery.popover.disabled · 禁用/恢复"); });
                       Button(ButtonProps{}.onClick([state] {
                           state->popover_disabled.set(false);
                           state->popover_open.set(true);
                       }),
                              [] { Text(u8"gallery.popover.restore · 重新打开"); });
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
                      .content(u8"gallery.notices.workflow · Message / Notification · 处理提示")
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return utf8("gallery.notices.feedback · " +
                                      std::string{state->notice_feedback.get().bytes()});
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
                            [] { Text(u8"gallery.message.open · 处理提示"); });
                     Button(ButtonProps{}.onClick([state] {
                         static_cast<void>(state->messages.open(
                             u8"project-save",
                             MessageProps{}.content(u8"项目处理成功").status(NoticeStatus::Success).duration(0)));
                         state->notice_feedback.set(u8"同一提示已更新为处理成功");
                     }),
                            [] { Text(u8"gallery.message.update · 更新成功"); });
                     Button(
                         ButtonProps{}.onClick([state] { static_cast<void>(state->messages.close(u8"project-save")); }),
                         [] { Text(u8"gallery.message.close · 关闭提示"); });
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
                                  Input(InputProps{}
                                            .defaultValue(state->notification_draft.get())
                                            .placeholder(u8"gallery.notification.draft")
                                            .ref(state->notification_ref)
                                            .onChange([state](String value) { state->notification_draft.set(value); })
                                            .layout(LayoutStyle{}
                                                        .width(dp(180))
                                                        .margin_left(dp(6))
                                                        .margin_right(dp(6))
                                                        .margin_top(dp(6))
                                                        .margin_bottom(dp(6))));
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
                            [] { Text(u8"gallery.notification.open · 打开通知"); });
                     Button(ButtonProps{}
                                .disabled(bind([state] { return !state->notification_open.get(); }))
                                .onClick([state] {
                                    static_cast<void>(state->notifications.open(
                                        u8"project-review", NotificationProps{}.title(u8"项目资料已更新")));
                                    state->notice_feedback.set(u8"已更新通知标题，编辑内容保持");
                                }),
                            [] { Text(u8"gallery.notification.update · 更新标题"); });
                     Button(ButtonProps{}.onClick(
                                [state] { static_cast<void>(state->notifications.close(u8"project-review")); }),
                            [] { Text(u8"gallery.notification.close · 关闭通知"); });
                     Button(ButtonProps{}.onClick([state] {
                         state->notification_placement.set(static_cast<NotificationPlacement>(
                             (static_cast<int>(state->notification_placement.get()) + 1) % 4));
                     }),
                            [] { Text(u8"gallery.notification.placement · 四角位置"); });
                     Button(ButtonProps{}.onClick([state] {
                         state->notification_status.set(
                             static_cast<NoticeStatus>((static_cast<int>(state->notification_status.get()) + 1) % 5));
                     }),
                            [] { Text(u8"gallery.notification.status · 切换状态"); });
                     Button(ButtonProps{}.onClick([state] {
                         static_cast<void>(state->messages.clear());
                         static_cast<void>(state->notifications.clear());
                         state->notice_feedback.set(u8"已清空消息与通知");
                     }),
                            [] { Text(u8"gallery.notices.clear · 清空提示"); });
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
                      .content(u8"gallery.timeline.workflow · Timeline · 项目处理记录")
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return utf8("gallery.timeline.feedback · " +
                                      std::string{state->timeline_feedback.get().bytes()});
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
                                                  .defaultValue(u8"记录发布后的跟进事项")
                                                  .placeholder(u8"gallery.timeline.draft")
                                                  .ref(state->timeline_ref)
                                                  .onChange([state](String value) { state->timeline_draft.set(value); })
                                                  .layout(LayoutStyle{}
                                                              .width(dp(160))
                                                              .margin_left(dp(6))
                                                              .margin_right(dp(6))
                                                              .margin_top(dp(6))
                                                              .margin_bottom(dp(6))));
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
                              [] { Text(u8"gallery.timeline.mode · 切换位置"); });
                       Button(ButtonProps{}.onClick(
                                  [state] { state->timeline_reverse.set(!state->timeline_reverse.get()); }),
                              [] { Text(u8"gallery.timeline.reverse · 正序/反序"); });
                       Button(ButtonProps{}.onClick(
                                  [state] { state->timeline_loading.set(!state->timeline_loading.get()); }),
                              [] { Text(u8"gallery.timeline.loading · 切换处理中"); });
                       Button(ButtonProps{}.onClick(
                                  [state] { state->timeline_visible.set(!state->timeline_visible.get()); }),
                              [] { Text(u8"gallery.timeline.visible · 显隐备注"); });
                       Button(ButtonProps{}.onClick([state] {
                           state->timeline_visible.set(true);
                           static_cast<void>(state->timeline_ref.focus());
                       }),
                              [] { Text(u8"gallery.timeline.focus · 编辑备注"); });
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
                      .content(u8"gallery.avatar.members · Avatar · 成员头像")
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return utf8("gallery.avatar.feedback · " + std::string{state->member_feedback.get().bytes()});
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
                                            .defaultValue(u8"江")
                                            .placeholder(u8"gallery.avatar.name")
                                            .ref(state->member_ref)
                                            .onChange([state](String value) { state->member_name.set(value); })
                                            .layout(member_control_margin().width(dp(160))));
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
                              [] { Text(u8"gallery.avatar.icon · 文字/图标"); });
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->member_shape.set(state->member_shape.get() == AvatarShape::Circle
                                                                  ? AvatarShape::Square
                                                                  : AvatarShape::Circle);
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.avatar.shape · 圆形/方形"); });
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->member_size.set(state->member_size.get() ==
                                                                     AvatarSizeValue{AvatarSize::Large}
                                                                 ? AvatarSizeValue{AvatarSize::Small}
                                                                 : AvatarSizeValue{AvatarSize::Large});
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.avatar.size · 大/小头像"); });
                       Button(ButtonProps{}
                                  .onClick([state] { state->member_limit.set(state->member_limit.get() == 2 ? 0 : 2); })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.avatar.limit · 显示2/0人"); });
                       Button(ButtonProps{}
                                  .onClick([state] { state->members_visible.set(!state->members_visible.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.avatar.visible · 显隐成员"); });
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->members_visible.set(true);
                                      static_cast<void>(state->member_ref.focus());
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.avatar.restore · 恢复编辑"); });
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
                      .content(u8"gallery.statistic.metrics · Statistic · 统计指标")
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return utf8("gallery.statistic.feedback · 原值=" +
                                      std::string{metric_raw_value(state->metric_value.get()).bytes()} + " · " +
                                      std::string{state->metric_feedback.get().bytes()});
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
                                                  .defaultValue(u8"核对本期指标")
                                                  .placeholder(u8"gallery.statistic.draft")
                                                  .ref(state->metric_ref)
                                                  .onChange([state](String value) { state->metric_note.set(value); })
                                                  .layout(member_control_margin().width(dp(180))));
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
                              [] { Text(u8"gallery.statistic.value · 切换业务值"); });
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      const auto p = state->metric_precision.get();
                                      state->metric_precision.set(p == 2   ? std::optional{8}
                                                                  : p == 8 ? std::nullopt
                                                                           : std::optional{2});
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.statistic.precision · 2/8/原精度"); });
                       Button(ButtonProps{}
                                  .onClick([state] { state->metric_separators.set(!state->metric_separators.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.statistic.separators · 切换分隔符"); });
                       Button(ButtonProps{}
                                  .onClick([state] { state->metric_formatter.set(!state->metric_formatter.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.statistic.formatter · 切换formatter"); });
                       Button(ButtonProps{}
                                  .onClick([state] { state->metric_loading.set(!state->metric_loading.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.statistic.loading · 加载/完成"); });
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->metric_loading.set(false);
                                      static_cast<void>(state->metric_ref.focus());
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.statistic.restore · 恢复指标编辑"); });
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
                      .content(u8"gallery.rate.service · Rate · 服务评分")
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return utf8(
                              "gallery.rate.feedback · 已接受=" +
                              std::string{
                                  metric_raw_value(StatisticValue{static_cast<double>(state->rating.get())}).bytes()} +
                              (state->rating_accept.get() ? " · 接受修改" : " · 拒绝修改") +
                              (state->rating_disabled.get() ? " · 禁用" : "") +
                              (state->rating_read_only.get() ? " · 只读" : "") + " · " +
                              std::string{state->rating_request.get().bytes()} + " · " +
                              std::string{state->rating_feedback.get().bytes()});
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
                                                  .defaultValue(u8"补充服务评价")
                                                  .placeholder(u8"gallery.rate.draft")
                                                  .ref(state->rating_ref)
                                                  .onChange([state](String value) { state->rating_note.set(value); })
                                                  .layout(member_control_margin().width(dp(180))));
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
                              [] { Text(u8"gallery.rate.authority · 接受/拒绝修改"); });
                       Button(ButtonProps{}
                                  .onClick([state] { state->rating_disabled.set(!state->rating_disabled.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.rate.disabled · 禁用/启用评分"); });
                       Button(ButtonProps{}
                                  .onClick([state] { state->rating_read_only.set(!state->rating_read_only.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.rate.readonly · 只读/编辑评分"); });
                       Button(ButtonProps{}
                                  .onClick([state] { state->rating_visible.set(!state->rating_visible.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.rate.visible · 显隐评分"); });
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->rating_visible.set(true);
                                      state->rating_disabled.set(false);
                                      state->rating_read_only.set(false);
                                      state->rating_accept.set(true);
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.rate.restore · 恢复评分"); });
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
                      .content(u8"gallery.auto-complete.contacts · AutoComplete · 联系人建议")
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          return utf8("gallery.auto-complete.feedback · 接受值=" +
                                      std::string{state->contact_value.get().bytes()} + " · " +
                                      std::string{state->contact_request.get().bytes()} + " · " +
                                      std::string{state->contact_saved.get().bytes()});
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
                              [] { Text(u8"gallery.auto-complete.authority · 接受/拒绝修改"); });
                       Button(ButtonProps{}
                                  .onClick([state] { state->contact_extra.set(!state->contact_extra.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.auto-complete.options · 更新建议"); });
                       Button(ButtonProps{}
                                  .onClick([state] { state->contact_disabled.set(!state->contact_disabled.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.auto-complete.disabled · 禁用/启用"); });
                       Button(ButtonProps{}
                                  .onClick([state] { state->contact_read_only.set(!state->contact_read_only.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.auto-complete.readonly · 只读/编辑"); });
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->contact_accept.set(true);
                                      state->contact_disabled.set(false);
                                      state->contact_read_only.set(false);
                                      static_cast<void>(state->contact_ref.focus());
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.auto-complete.restore · 恢复联系人输入"); });
                   }});
             Text(TextProps{}
                      .content(bind([state] {
                          return utf8("gallery.auto-complete.form-feedback · " +
                                      std::string{state->contact_form_feedback.get().bytes()});
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
                                       [] { Text(u8"gallery.auto-complete.submit · 保存表单联系人"); });
                                Button(ButtonProps{}
                                           .onClick([state] {
                                               static_cast<void>(state->contact_form.reset());
                                               state->contact_form_feedback.set(String{u8"已重置联系人"});
                                           })
                                           .layout(member_control_margin()),
                                       [] { Text(u8"gallery.auto-complete.reset · 重置联系人"); });
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
                      .content(u8"gallery.tree.project · Tree · 项目目录")
                      .layout(LayoutStyle{}.margin_left(dp(1))));
             Text(TextProps{}
                      .content(bind([state] {
                          const auto [checked, half] =
                              gallery_project_checks(state->project_checked.get(), state->project_locked.get());
                          return utf8("gallery.tree.feedback · 选择=" +
                                      std::string{gallery_project_keys(state->project_selected.get()).bytes()} +
                                      " · 勾选=" + std::string{gallery_project_keys(checked).bytes()} +
                                      " · 半选=" + std::string{gallery_project_keys(half).bytes()} + " · " +
                                      std::string{state->project_saved.get().bytes()});
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
                              [] { Text(u8"gallery.tree.disabled · 禁用/启用项目树"); });
                       Button(ButtonProps{}
                                  .onClick([state] { state->project_visible.set(!state->project_visible.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.tree.visible · 显隐项目树"); });
                       Button(ButtonProps{}
                                  .onClick([state] { state->project_locked.set(!state->project_locked.get()); })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.tree.locked · 归档分支禁用/恢复"); });
                       Button(ButtonProps{}
                                  .onClick([state] {
                                      state->project_visible.set(true);
                                      state->project_disabled.set(false);
                                      state->project_expanded.set(TreeKeys{});
                                      static_cast<void>(state->project_note_ref.focus());
                                  })
                                  .layout(member_control_margin()),
                              [] { Text(u8"gallery.tree.restore · 恢复项目备注"); });
                   }});
         }});
    ++state->telemetry.live_samples;
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

TokenGalleryDefinition make_token_gallery_definition() {
    auto state = std::make_shared<GalleryState>();
    auto set_theme = [state](const ryn::ThemeConfig& config, bool brand) {
        state->set_theme(config, brand);
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
                                                    });
                                      });
                            scrollbar_surface(ReferenceSurfaceRole::scrollbar_track, state->navigation_track_height,
                                              state->navigation_bar_visible);
                            scrollbar_surface(ReferenceSurfaceRole::scrollbar_thumb, state->navigation_thumb_height,
                                              state->navigation_bar_visible);
                            scrollbar_surface(ReferenceSurfaceRole::scrollbar_track, state->document_track_height,
                                              state->document_bar_visible);
                            scrollbar_surface(ReferenceSurfaceRole::scrollbar_thumb, state->document_thumb_height,
                                              state->document_bar_visible);
                            ReferenceSurface(
                                ReferenceSurfaceProps{}
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
            const auto snapshot = ryn::resolve_theme(state->theme.get());
            result.snapshot_identity = snapshot.identity();
            result.snapshot_diagnostic = snapshot.diagnostic_json();
            return result;
        },
        {stable_test_ids.begin(), stable_test_ids.end()},
        navigation_control_count,
        [state] { return state->background_color; },
    };
    return definition;
}

} // namespace rynui::example
