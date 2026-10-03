#include "token_gallery_definition.hpp"

#include "ant_design_reference_catalog.hpp"
#include "gallery_document_model.hpp"
#include "gallery_layout.hpp"
#include "reference_surface.hpp"
#include "icon_samples.hpp"

#include <ryn/rynui.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace rynui::example {
namespace {

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

    void set_theme(ryn::ThemeConfig config, bool brand = false) {
        background_color = ryn::resolve_theme(config).alias().color_background_container;
        theme.set(std::move(config));
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
                    ryn::ThemeConfig config = {}, std::optional<ryn::Color> swatch = std::nullopt,
                    bool elevated = false) {
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

void add_palette_cells(const std::shared_ptr<GalleryState>& state) {
    reference_cell(state, "ant.map.colorPrimary", "Primary / 主色", {}, ryn::Color::rgba8(22, 119, 255));
    reference_cell(state, "ant.map.colorSuccess", "Success / 成功", {}, ryn::Color::rgba8(82, 196, 26));
    reference_cell(state, "ant.map.colorWarning", "Warning / 警告", {}, ryn::Color::rgba8(250, 173, 20));
    reference_cell(state, "ant.map.colorError", "Error / 错误", {}, ryn::Color::rgba8(255, 77, 79));
    reference_cell(state, "ant.map.colorInfo", "Info / 信息", {}, ryn::Color::rgba8(22, 119, 255));
    reference_cell(state, "ant.map.blue6", "Blue 6", {}, ryn::Color::rgba8(22, 119, 255));
    reference_cell(state, "ant.map.green6", "Green 6", {}, ryn::Color::rgba8(82, 196, 26));
    reference_cell(state, "ant.map.red5", "Red 5", {}, ryn::Color::rgba8(255, 77, 79));
    reference_cell(state, "ant.map.gold6", "Gold 6", {}, ryn::Color::rgba8(250, 173, 20));
    reference_cell(state, "ant.map.purple6", "Purple 6", {}, ryn::Color::rgba8(114, 46, 209));
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
    ryn::Theme(ryn::ThemeProps{}.config(config),
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
            themed_button(state, "gallery.state.default", "Default", {});
            themed_button(state, "gallery.state.primary", "Primary", {}, ryn::ButtonType::Primary);
            themed_button(state, "gallery.state.danger", "Danger", {}, ryn::ButtonType::Danger);
            themed_button(state, "gallery.state.hover", "Hover me / 悬停", {});
            themed_button(state, "gallery.state.active", "Press me / 按下", {});
            themed_button(state, "gallery.state.focus-visible", "Tab focus / 键盘焦点", {});
            themed_button(state, "gallery.state.disabled", "Disabled / 禁用", {}, ryn::ButtonType::Default,
                          ryn::ControlSize::Middle, state->disabled);
            themed_button(state, "gallery.state.loading", "Loading / 加载", {}, ryn::ButtonType::Primary,
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
    ryn::Text(ryn::TypographyProps{}
                  .content(u8"原始全文保持完整：省略显示时复制仍写入全部内容；点击展开查看全文，再次点击收起。")
                  .ellipsis(ryn::TypographyEllipsis{.expandable = true})
                  .copyable(ryn::TypographyCopyable{})
                  .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::Paragraph(ryn::TypographyProps{}
                       .content(u8"多行省略保留字素边界，英文 Latin "
                                u8"和中文均复用同一文本测量通道。\n显式换行也计入行数。\n展开后显示全部行。")
                       .ellipsis(ryn::TypographyEllipsis{.rows = 2, .expandable = true})
                       .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::Text(ryn::TypographyProps{}
                  .content(state->typography_value)
                  .editable(ryn::TypographyEditable{.max_length = 64})
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
    auto set_theme = [state](ryn::ThemeConfig config, bool brand) {
        state->set_theme(std::move(config), brand);
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
