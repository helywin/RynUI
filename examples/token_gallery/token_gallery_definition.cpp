#include "token_gallery_definition.hpp"

#include "ant_design_reference_catalog.hpp"
#include "gallery_document_model.hpp"
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
    ryn::Signal<ryn::LogicalLength> gallery_width{ryn::dp(1120.0F)};
    ryn::Signal<ryn::LogicalLength> cell_width{ryn::dp(260.0F)};
    ryn::Signal<bool> disabled{true};
    ryn::Signal<bool> loading{true};
    TokenGalleryTelemetry telemetry;
};

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
    if (test_id.starts_with("ant.")
            && ryn::find_ant_design_token(test_id) == nullptr) {
        throw std::logic_error(
            "Token Gallery label identity is not in the locked catalog");
    }
    return utf8(caption);
}

std::string joined_scope(
    std::string_view prefix,
    std::string_view value) {
    std::string result(prefix);
    if (value.empty()) {
        result += "无";
        return result;
    }
    result += value;
    return result;
}

ryn::ThemeConfig algorithm_config(ryn::ThemeAlgorithm algorithm) {
    ryn::ThemeConfig config;
    if (algorithm != ryn::ThemeAlgorithm::Default) {
        config.algorithms.push_back(algorithm);
    }
    return config;
}

void section_surface(
    const std::shared_ptr<GalleryState>& state,
    const GalleryDocumentSection& section) {
    const auto title = utf8(section.title);
    const auto summary = utf8(section.summary);
    ReferenceSurface(
        ReferenceSurfaceProps{}
            .status(GallerySupportStatus::partial)
            .elevated(true)
            .layout(ryn::LayoutStyle{}.width(state->gallery_width)),
        [state, title, summary] {
            ++state->telemetry.reference_content_runs;
            ryn::Text(title);
            ryn::Text(ryn::TextProps{}
                .content(summary)
                .tone(ryn::TextTone::Secondary));
        });
    ++state->telemetry.document_sections;
    ++state->telemetry.reference_surfaces;
}

void source_section(const std::shared_ptr<GalleryState>& state) {
    section_surface(state, gallery_document_sections()[0]);
    for (const auto& source : ant_design_reference_sources()) {
        const auto title = utf8(source.title);
        const auto url = utf8(source.official_url);
        ReferenceSurface(
            ReferenceSurfaceProps{}
                .status(GallerySupportStatus::planned)
                .layout(ryn::LayoutStyle{}.width(state->gallery_width)),
            [state, title, url] {
                ++state->telemetry.reference_content_runs;
                ryn::Text(title);
                ryn::Text(ryn::TextProps{}
                    .content(url)
                    .tone(ryn::TextTone::Secondary));
            });
        ++state->telemetry.reference_surfaces;
    }
}

void design_values(const std::shared_ptr<GalleryState>& state) {
    section_surface(state, gallery_document_sections()[2]);
    ryn::Flex(
        ryn::FlexProps{}
            .wrap(true)
            .gap(ryn::dp(8.0F), ryn::dp(8.0F))
            .layout(ryn::LayoutStyle{}.width(state->gallery_width)),
        [state] {
            for (const auto& value : gallery_design_values()) {
                const auto title = utf8(
                    std::string(value.english_name) + " / "
                    + std::string(value.chinese_name));
                const auto summary = utf8(value.summary);
                ReferenceSurface(
                    ReferenceSurfaceProps{}
                        .status(GallerySupportStatus::planned)
                        .layout(ryn::LayoutStyle{}.width(state->cell_width)),
                    [state, title, summary] {
                        ++state->telemetry.reference_content_runs;
                        ryn::Text(title);
                        ryn::Text(ryn::TextProps{}
                            .content(summary)
                            .tone(ryn::TextTone::Secondary));
                    });
                ++state->telemetry.reference_surfaces;
            }
        });
}

void reference_cell(
    const std::shared_ptr<GalleryState>& state,
    std::string_view test_id,
    std::string_view caption,
    ryn::ThemeConfig config = {},
    std::optional<ryn::Color> swatch = std::nullopt,
    bool elevated = false) {
    const auto title = label(test_id, caption);
    const auto identity = utf8(test_id);
    ryn::Theme(
        ryn::ThemeProps{}.config(std::move(config)),
        ryn::ThemeContent{[state, title, identity, swatch, elevated] {
            ++state->telemetry.theme_content_runs;
            ReferenceSurface(
                ReferenceSurfaceProps{}
                    .status(GallerySupportStatus::implemented)
                    .swatch(swatch)
                    .elevated(elevated)
                    .layout(ryn::LayoutStyle{}.width(state->cell_width)),
                [state, title, identity] {
                    ++state->telemetry.reference_content_runs;
                    ryn::Text(title);
                    ryn::Text(ryn::TextProps{}
                        .content(identity)
                        .tone(ryn::TextTone::Secondary));
                });
            ++state->telemetry.reference_surfaces;
        }});
}

void add_palette_cells(const std::shared_ptr<GalleryState>& state) {
    reference_cell(state, "ant.map.colorPrimary", "Primary / 主色", {},
                   ryn::Color::rgba8(22, 119, 255));
    reference_cell(state, "ant.map.colorSuccess", "Success / 成功", {},
                   ryn::Color::rgba8(82, 196, 26));
    reference_cell(state, "ant.map.colorWarning", "Warning / 警告", {},
                   ryn::Color::rgba8(250, 173, 20));
    reference_cell(state, "ant.map.colorError", "Error / 错误", {},
                   ryn::Color::rgba8(255, 77, 79));
    reference_cell(state, "ant.map.colorInfo", "Info / 信息", {},
                   ryn::Color::rgba8(22, 119, 255));
    reference_cell(state, "ant.map.blue6", "Blue 6", {},
                   ryn::Color::rgba8(22, 119, 255));
    reference_cell(state, "ant.map.green6", "Green 6", {},
                   ryn::Color::rgba8(82, 196, 26));
    reference_cell(state, "ant.map.red5", "Red 5", {},
                   ryn::Color::rgba8(255, 77, 79));
    reference_cell(state, "ant.map.gold6", "Gold 6", {},
                   ryn::Color::rgba8(250, 173, 20));
    reference_cell(state, "ant.map.purple6", "Purple 6", {},
                   ryn::Color::rgba8(114, 46, 209));
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
    for (const auto [id, caption, radius] : std::array{
            std::tuple{"ant.map.borderRadiusSM", "Radius 4", 4.0F},
            std::tuple{"ant.seed.borderRadius", "Radius 6", 6.0F},
            std::tuple{"ant.map.borderRadiusLG", "Radius 8", 8.0F}}) {
        ryn::ThemeConfig config;
        config.seed.border_radius = ryn::dp(radius);
        reference_cell(state, id, caption, std::move(config));
    }
}

void shadow_cell(
    const std::shared_ptr<GalleryState>& state,
    std::string_view id,
    std::string_view caption,
    const ryn::ShadowList& shadow) {
    ryn::ThemeConfig config;
    config.alias.box_shadow_tertiary = shadow;
    reference_cell(state, id, caption, std::move(config), std::nullopt, true);
}

void add_shadow_cells(const std::shared_ptr<GalleryState>& state) {
    const auto& shadows = ryn::ant_design_default_shadows();
    shadow_cell(state, "ant.alias.boxShadowTertiary", "Elevation 1",
                shadows.box_shadow_tertiary);
    shadow_cell(state, "ant.alias.boxShadowSecondary", "Elevation 2",
                shadows.box_shadow_secondary);
    shadow_cell(state, "ant.alias.boxShadow", "Elevation 3", shadows.box_shadow);
    shadow_cell(state, "ant.component.Button.defaultShadow", "Button Default",
                shadows.button_default);
    shadow_cell(state, "ant.component.Button.primaryShadow", "Button Primary",
                shadows.button_primary);
    shadow_cell(state, "ant.component.Button.dangerShadow", "Button Danger",
                shadows.button_danger);
    shadow_cell(state, "ant.alias.boxShadowDrawerLeft", "Drawer Left",
                shadows.drawer_left);
    shadow_cell(state, "ant.alias.boxShadowDrawerRight", "Drawer Right",
                shadows.drawer_right);
    shadow_cell(state, "ant.alias.boxShadowDrawerUp", "Drawer Up", shadows.drawer_up);
    shadow_cell(state, "ant.alias.boxShadowDrawerDown", "Drawer Down",
                shadows.drawer_down);
    shadow_cell(state, "ant.alias.boxShadowPopoverArrow", "Popover Arrow",
                shadows.popover_arrow);
    shadow_cell(state, "ant.alias.dropShadowPopover", "Popover",
                shadows.popover_drop);
    shadow_cell(state, "ant.alias.boxShadowCard", "Card", shadows.card);
    shadow_cell(state, "ant.alias.boxShadowTabsOverflowLeft", "Tabs Left inset",
                shadows.tabs_overflow_left);
    shadow_cell(state, "ant.alias.boxShadowTabsOverflowRight", "Tabs Right inset",
                shadows.tabs_overflow_right);
    shadow_cell(state, "ant.alias.boxShadowTabsOverflowTop", "Tabs Top inset",
                shadows.tabs_overflow_top);
    shadow_cell(state, "ant.alias.boxShadowTabsOverflowBottom", "Tabs Bottom inset",
                shadows.tabs_overflow_bottom);
}

void foundation_tokens(const std::shared_ptr<GalleryState>& state) {
    section_surface(state, gallery_document_sections()[3]);
    ryn::Flex(
        ryn::FlexProps{}
            .wrap(true)
            .gap(ryn::dp(8.0F), ryn::dp(8.0F))
            .layout(ryn::LayoutStyle{}.width(state->gallery_width)),
        [state] {
            add_palette_cells(state);
            add_scale_cells(state);
            add_shadow_cells(state);
        });
}

void component_entry(
    const std::shared_ptr<GalleryState>& state,
    const AntDesignReferenceEntry& entry) {
    const auto name = utf8(
        std::string(entry.english_name) + " / "
        + std::string(entry.chinese_name));
    const auto summary = utf8(entry.summary);
    const auto supported = utf8(joined_scope("支持：", entry.supported_scope));
    const auto missing = utf8(joined_scope("缺失：", entry.missing_scope));
    const auto evidence = utf8(
        std::string("证据：") + std::string(entry.evidence_identifiers)
        + " · Source: " + std::string(entry.source_path));
    ReferenceSurface(
        ReferenceSurfaceProps{}
            .status(entry.support_status)
            .layout(ryn::LayoutStyle{}.width(state->cell_width)),
        [state, name, summary, supported, missing, evidence] {
            ++state->telemetry.reference_content_runs;
            ryn::Text(name);
            ryn::Text(ryn::TextProps{}
                .content(summary)
                .tone(ryn::TextTone::Secondary));
            ryn::Text(ryn::TextProps{}
                .content(supported)
                .tone(ryn::TextTone::Secondary));
            ryn::Text(ryn::TextProps{}
                .content(missing)
                .tone(ryn::TextTone::Secondary));
            ryn::Text(ryn::TextProps{}
                .content(evidence)
                .tone(ryn::TextTone::Secondary));
        });
    ++state->telemetry.component_entries;
    ++state->telemetry.reference_surfaces;
}

void component_overview(const std::shared_ptr<GalleryState>& state) {
    section_surface(state, gallery_document_sections()[4]);
    const auto entries = ant_design_reference_entries();
    for (const auto& category : ant_design_reference_categories()) {
        ryn::Text(utf8(gallery_category_title(category.category)));
        ryn::Flex(
            ryn::FlexProps{}
                .wrap(true)
                .gap(ryn::dp(8.0F), ryn::dp(8.0F))
                .layout(ryn::LayoutStyle{}.width(state->gallery_width)),
            [state, entries, category] {
                for (const auto& entry : entries) {
                    if (entry.category == category.category
                            && gallery_support_filter_matches(
                                GallerySupportFilter::all, entry.support_status)) {
                        component_entry(state, entry);
                    }
                }
            });
    }
}

void themed_button(
    const std::shared_ptr<GalleryState>& state,
    std::string_view test_id,
    std::string_view caption,
    ryn::ThemeConfig config,
    ryn::ButtonType type = ryn::ButtonType::Default,
    ryn::ControlSize size = ryn::ControlSize::Small,
    ryn::Prop<bool> disabled = false,
    ryn::Prop<bool> loading = false,
    std::function<void()> on_click = {}) {
    const auto text = label(test_id, caption);
    ryn::Theme(
        ryn::ThemeProps{}.config(std::move(config)),
        ryn::ThemeContent{[state, text, type, size, disabled = std::move(disabled),
                           loading = std::move(loading), on_click = std::move(on_click)]() mutable {
            ++state->telemetry.theme_content_runs;
            auto props = ryn::ButtonProps{}
                .type(type)
                .size(size)
                .disabled(std::move(disabled))
                .loading(std::move(loading))
                .layout(ryn::LayoutStyle{}
                    .width(state->cell_width)
                    .flex_shrink(1.0F));
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
            .layout(ryn::LayoutStyle{}.width(state->gallery_width)),
        [state] {
            themed_button(
                state, "gallery.theme.default", "Default",
                algorithm_config(ryn::ThemeAlgorithm::Default),
                ryn::ButtonType::Default, ryn::ControlSize::Middle, false, false,
                [state] {
                    state->theme.set(algorithm_config(ryn::ThemeAlgorithm::Default));
                    ++state->telemetry.theme_updates;
                    ++state->telemetry.activations;
                });
            themed_button(
                state, "gallery.theme.dark", "Dark",
                algorithm_config(ryn::ThemeAlgorithm::Dark),
                ryn::ButtonType::Default, ryn::ControlSize::Middle, false, false,
                [state] {
                    state->theme.set(algorithm_config(ryn::ThemeAlgorithm::Dark));
                    ++state->telemetry.theme_updates;
                    ++state->telemetry.activations;
                });
            themed_button(
                state, "gallery.theme.compact", "Compact",
                algorithm_config(ryn::ThemeAlgorithm::Compact),
                ryn::ButtonType::Default, ryn::ControlSize::Middle, false, false,
                [state] {
                    state->theme.set(algorithm_config(ryn::ThemeAlgorithm::Compact));
                    ++state->telemetry.theme_updates;
                    ++state->telemetry.activations;
                });
            auto nested = algorithm_config(ryn::ThemeAlgorithm::Dark);
            nested.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
            themed_button(
                state, "gallery.theme.nested-brand", "Dark + Purple Seed",
                nested, ryn::ButtonType::Primary);
            themed_button(state, "gallery.state.default", "Default", {});
            themed_button(state, "gallery.state.primary", "Primary", {},
                          ryn::ButtonType::Primary);
            themed_button(state, "gallery.state.danger", "Danger", {},
                          ryn::ButtonType::Danger);
            themed_button(state, "gallery.state.hover", "Hover me / 悬停", {});
            themed_button(state, "gallery.state.active", "Press me / 按下", {});
            themed_button(
                state, "gallery.state.focus-visible", "Tab focus / 键盘焦点", {});
            themed_button(
                state, "gallery.state.disabled", "Disabled / 禁用", {},
                ryn::ButtonType::Default, ryn::ControlSize::Middle,
                state->disabled);
            themed_button(
                state, "gallery.state.loading", "Loading / 加载", {},
                ryn::ButtonType::Primary, ryn::ControlSize::Middle,
                false, state->loading);
        });
}

} // namespace

TokenGalleryViewport token_gallery_logical_viewport(
    int pixel_width,
    int pixel_height,
    float render_scale) {
    if (pixel_width <= 0 || pixel_height <= 0
            || !std::isfinite(render_scale) || render_scale <= 0.0F) {
        throw std::invalid_argument(
            "Token Gallery pixel extent and render scale must be positive");
    }
    return {
        static_cast<float>(pixel_width) / render_scale,
        static_cast<float>(pixel_height) / render_scale,
    };
}

float token_gallery_pointer_to_render_logical(
    float host_logical_coordinate,
    float host_display_scale,
    float render_scale) {
    if (!std::isfinite(host_logical_coordinate)
            || !std::isfinite(host_display_scale) || host_display_scale <= 0.0F
            || !std::isfinite(render_scale) || render_scale <= 0.0F) {
        throw std::invalid_argument(
            "Token Gallery pointer coordinate and display scales must be finite and positive");
    }
    return host_logical_coordinate * host_display_scale / render_scale;
}

TokenGalleryDefinition make_token_gallery_definition() {
    auto state = std::make_shared<GalleryState>();
    auto set_theme = [state](ryn::ThemeConfig config, bool brand) {
        state->theme.set(std::move(config));
        ++state->telemetry.theme_updates;
        if (brand) {
            ++state->telemetry.brand_updates;
        }
    };

    TokenGalleryDefinition definition{
        ryn::Content{[state] {
            ++state->telemetry.content_runs;
            ryn::Theme(
                ryn::ThemeProps{}.config(state->theme),
                ryn::ThemeContent{[state] {
                    ++state->telemetry.theme_content_runs;
                    ryn::Flex(
                        ryn::FlexProps{}
                            .vertical(true)
                            .gap(ryn::dp(16.0F))
                            .layout(ryn::LayoutStyle{}.width(state->gallery_width)),
                        [state] {
                            source_section(state);
                            section_surface(state, gallery_document_sections()[1]);
                            design_values(state);
                            foundation_tokens(state);
                            component_overview(state);
                            add_live_samples(state);
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
            const float content_width = std::max(256.0F, viewport_width - 48.0F);
            const float next_cell_width = content_width < 720.0F
                ? std::max(220.0F, (content_width - 8.0F) / 2.0F)
                : 260.0F;
            if (state->gallery_width.get() != ryn::dp(content_width)) {
                state->gallery_width.set(ryn::dp(content_width));
                ++state->telemetry.viewport_updates;
            }
            if (state->cell_width.get() != ryn::dp(next_cell_width)) {
                state->cell_width.set(ryn::dp(next_cell_width));
                ++state->telemetry.viewport_updates;
            }
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
        [state] {
            auto result = state->telemetry;
            const auto snapshot = ryn::resolve_theme(state->theme.get());
            result.snapshot_identity = snapshot.identity();
            result.snapshot_diagnostic = snapshot.diagnostic_json();
            return result;
        },
        {stable_test_ids.begin(), stable_test_ids.end()},
    };
    return definition;
}

} // namespace rynui::example
