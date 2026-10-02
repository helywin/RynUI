#include "token_gallery_definition.hpp"

#include "ant_design_reference_catalog.hpp"
#include "gallery_document_model.hpp"
#include "gallery_layout.hpp"
#include "reference_surface.hpp"

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
    ryn::Signal<bool> clear_disabled{false};
    ryn::Signal<bool> switch_checked{false};
    ryn::Signal<bool> checkbox_checked{false};
    ryn::Signal<std::optional<ryn::String>> radio_selected{std::optional<ryn::String>{ryn::String{u8"a"}}};
    ryn::Signal<ryn::String> input_value{ryn::String{u8""}};
    ryn::Signal<ryn::String> input_feedback{ryn::String{u8"Enter 提交；支持选择、剪贴板、撤销/重做"}};
    ryn::Signal<ryn::String> search_value{ryn::String{}};
    ryn::Signal<ryn::String> search_feedback{ryn::String{u8"Enter 或按钮提交搜索"}};
    ryn::Signal<ryn::String> typography_value{ryn::String{u8"点击编辑 · 受控正文"}};
    ryn::Signal<ryn::String> typography_title{ryn::String{u8"标题编辑继承字号"}};
    ryn::Signal<double> slider_value{30};
    ryn::Signal<ryn::SliderRange> slider_range{ryn::SliderRange{20, 80}};
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
    "gallery.input.controlled",
    "gallery.input.uncontrolled",
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
                   ryn::ThemeConfig config, ryn::ButtonType type = ryn::ButtonType::Default,
                   ryn::ControlSize size = ryn::ControlSize::Small, ryn::Prop<bool> disabled = false,
                   ryn::Prop<bool> loading = false, std::function<void()> on_click = {}) {
    const auto text = label(test_id, caption);
    ryn::Theme(ryn::ThemeProps{}.config(std::move(config)),
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

void add_live_samples(const std::shared_ptr<GalleryState>& state) {
    section_surface(state, gallery_document_sections()[5]);
    ryn::Space(
        ryn::SpaceProps{}
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
    ryn::Text(u8"Input / 单行输入 · partial");
    ryn::Text(u8"支持：受控/非受控、prefix/suffix、Unicode 编辑、IME 事件桥接、Theme/status");
    ryn::Text(u8"支持：allowClear 清空操作；TextArea 与更多组合能力仍待实现");
    ryn::Space(ryn::SpaceProps{}.wrap(true).size(ryn::dp(8.0F)).layout(ryn::LayoutStyle{}.width(state->document_width)),
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
    ryn::Text(u8"Search / 搜索 · partial：复用 Input、Button 与 Flex");
    ryn::Text(u8"支持：受控/非受控、Enter/按钮提交、loading/disabled；暂缺 clear、自定义图标与紧凑边角");
    ryn::Space(ryn::SpaceProps{}.wrap(true).size(ryn::dp(8.0F)).layout(ryn::LayoutStyle{}.width(state->document_width)),
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
    ryn::Space(ryn::SpaceProps{}.wrap(true).size(ryn::dp(8.0F)).layout(ryn::LayoutStyle{}.width(state->document_width)),
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
    ryn::Text(u8"Tooltip · 悬停 / 键盘焦点 / Escape / 边缘翻转 / 禁用触发项");
    ryn::Space(ryn::SpaceProps{}.wrap(true), [] {
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
    ryn::Text(u8"Slider · 单值、范围、反向、禁用、纵向（Tooltip / marks 待实现）");
    ryn::Slider(ryn::SliderProps{}
                    .value(state->slider_value)
                    .onChange([state](double value) { state->slider_value.set(value); })
                    .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::RangeSlider(ryn::RangeSliderProps{}
                         .value(state->slider_range)
                         .onChange([state](ryn::SliderRange value) { state->slider_range.set(value); })
                         .layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::Slider(ryn::SliderProps{}.defaultValue(35).reverse(true).layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::Slider(ryn::SliderProps{}.defaultValue(60).disabled(true).layout(ryn::LayoutStyle{}.width(state->cell_width)));
    ryn::Slider(ryn::SliderProps{}
                    .defaultValue(40)
                    .orientation(ryn::SliderOrientation::Vertical)
                    .layout(ryn::LayoutStyle{}.height(ryn::dp(120))));
    state->telemetry.live_samples += 5;
    ryn::Flex(ryn::FlexProps{}.layout(ryn::LayoutStyle{}.height(ryn::dp(48.0F))), ryn::FlexContent{[] {}});
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
                            .vertical(true)
                            .gap(ryn::dp(0.0F))
                            .layout(ryn::LayoutStyle{}.width(state->gallery_width)),
                        [state] {
                            ryn::Flex(ryn::FlexProps{}
                                          .vertical(state->narrow_layout)
                                          .gap(ryn::dp(GalleryLayoutMetrics::column_gap))
                                          .layout(ryn::LayoutStyle{}
                                                      .width(state->gallery_width)
                                                      .margin_top(ryn::dp(GalleryLayoutMetrics::body_gap))
                                                      .order(1)),
                                      [state] {
                                          ryn::Flex(ryn::FlexProps{}
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
                                    ryn::Flex(ryn::FlexProps{}.gap(ryn::dp(24.0F)), [state] {
                                        document_text(utf8("RynUI"), 20.0F, 28.0F, 600);
                                        document_text(utf8("组件"), 14.0F, 28.0F, 500);
                                        document_text(utf8("Design Tokens"), 14.0F, 28.0F, 400,
                                                      ryn::TextTone::Secondary);
                                        document_text(utf8("Ant Design 6.6.5"), 12.0F, 28.0F, 400,
                                                      ryn::TextTone::Secondary);
                                        ryn::Button(ryn::ButtonProps{}.size(ryn::ControlSize::Small).onClick([state] {
                                            state->navigation_request = GalleryNavigationTarget::to_navigation();
                                            ++state->telemetry.navigation_requests;
                                        }),
                                                    [] { ryn::Text(u8"目录"); });
                                        ryn::Button(
                                            ryn::ButtonProps{}
                                                .type(ryn::ButtonType::Text)
                                                .size(ryn::ControlSize::Small)
                                                .onClick([state] {
                                                    const auto config = state->theme.get();
                                                    const bool dark =
                                                        std::find(config.algorithms.begin(), config.algorithms.end(),
                                                                  ryn::ThemeAlgorithm::Dark) != config.algorithms.end();
                                                    state->set_theme(algorithm_config(dark
                                                                                          ? ryn::ThemeAlgorithm::Default
                                                                                          : ryn::ThemeAlgorithm::Dark));
                                                }),
                                            [state] {
                                                ryn::Icon(ryn::IconProps{}.name(ryn::bind([theme = state->theme] {
                                                    const auto config = theme.get();
                                                    return std::find(config.algorithms.begin(), config.algorithms.end(),
                                                                     ryn::ThemeAlgorithm::Dark) !=
                                                                   config.algorithms.end()
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
