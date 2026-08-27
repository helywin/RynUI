#include "token_gallery_definition.hpp"

#include <ryn/rynui.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace rynui::example {
namespace {

struct GalleryState final {
    ryn::Signal<ryn::ThemeConfig> theme{ryn::ThemeConfig{}};
    ryn::Signal<ryn::LogicalLength> gallery_width{ryn::dp(1120.0F)};
    ryn::Signal<ryn::LogicalLength> cell_width{ryn::dp(126.0F)};
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

ryn::String label(std::string_view test_id, std::string_view caption) {
    if (test_id.starts_with("ant.") && ryn::find_ant_design_token(test_id) == nullptr) {
        throw std::logic_error("Token Gallery label identity is not in the locked catalog");
    }
    auto parsed = ryn::String::from_utf8(caption);
    if (!parsed) {
        throw std::logic_error("Token Gallery label is not valid UTF-8");
    }
    return std::move(parsed).value();
}

ryn::ThemeConfig primary_surface(ryn::Color color) {
    ryn::ThemeConfig config;
    config.button.tokens.primary_background = color;
    config.button.tokens.primary_color = ryn::Color::rgba8(255, 255, 255);
    config.button.tokens.primary_shadow = ryn::ShadowList{};
    return config;
}

ryn::ThemeConfig shadow_surface(const ryn::ShadowList& shadow) {
    ryn::ThemeConfig config;
    config.button.tokens.default_shadow = shadow;
    config.button.tokens.primary_shadow = shadow;
    config.button.tokens.danger_shadow = shadow;
    return config;
}

ryn::ThemeConfig radius_surface(float radius) {
    ryn::ThemeConfig config;
    config.button.tokens.border_radius = ryn::dp(radius);
    return config;
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
                .layout(
                    ryn::LayoutStyle{}
                        .width(state->cell_width)
                        .flex_shrink(1.0F));
            if (on_click) {
                props.onClick(std::move(on_click));
            }
            ryn::Button(std::move(props), [text] { ryn::Text(text); });
        }});
}

void add_palette_cells(const std::shared_ptr<GalleryState>& state) {
    themed_button(state, "ant.map.colorPrimary", "Primary / 主色",
                  primary_surface(ryn::Color::rgba8(22, 119, 255)),
                  ryn::ButtonType::Primary);
    themed_button(state, "ant.map.colorSuccess", "Success / 成功",
                  primary_surface(ryn::Color::rgba8(82, 196, 26)),
                  ryn::ButtonType::Primary);
    themed_button(state, "ant.map.colorWarning", "Warning / 警告",
                  primary_surface(ryn::Color::rgba8(250, 173, 20)),
                  ryn::ButtonType::Primary);
    themed_button(state, "ant.map.colorError", "Error / 错误",
                  primary_surface(ryn::Color::rgba8(255, 77, 79)),
                  ryn::ButtonType::Primary);
    themed_button(state, "ant.map.colorInfo", "Info / 信息",
                  primary_surface(ryn::Color::rgba8(22, 119, 255)),
                  ryn::ButtonType::Primary);
    themed_button(state, "ant.map.blue6", "Blue 6",
                  primary_surface(ryn::Color::rgba8(22, 119, 255)),
                  ryn::ButtonType::Primary);
    themed_button(state, "ant.map.green6", "Green 6",
                  primary_surface(ryn::Color::rgba8(82, 196, 26)),
                  ryn::ButtonType::Primary);
    themed_button(state, "ant.map.red5", "Red 5",
                  primary_surface(ryn::Color::rgba8(255, 77, 79)),
                  ryn::ButtonType::Primary);
    themed_button(state, "ant.map.gold6", "Gold 6",
                  primary_surface(ryn::Color::rgba8(250, 173, 20)),
                  ryn::ButtonType::Primary);
    themed_button(state, "ant.map.purple6", "Purple 6",
                  primary_surface(ryn::Color::rgba8(114, 46, 209)),
                  ryn::ButtonType::Primary);
}

void add_scale_cells(const std::shared_ptr<GalleryState>& state) {
    const auto text_cell = [&](std::string_view id, std::string_view caption, float size) {
        ryn::ThemeConfig config;
        config.text.tokens.font_size = ryn::dp(size);
        config.text.tokens.line_height = ryn::dp(size + 8.0F);
        themed_button(state, id, caption, std::move(config));
    };
    text_cell("ant.map.fontSizeSM", "Font 12", 12.0F);
    text_cell("ant.map.fontSize", "Font 14", 14.0F);
    text_cell("ant.map.fontSizeLG", "Font 16", 16.0F);
    themed_button(state, "ant.map.sizeXS", "Gap 8 / 间距",
                  ryn::ThemeConfig{}, ryn::ButtonType::Default,
                  ryn::ControlSize::Small);
    themed_button(state, "ant.map.size", "Gap 16 / 间距",
                  ryn::ThemeConfig{}, ryn::ButtonType::Default,
                  ryn::ControlSize::Middle);
    themed_button(state, "ant.map.sizeLG", "Gap 24 / 间距",
                  ryn::ThemeConfig{}, ryn::ButtonType::Default,
                  ryn::ControlSize::Large);
    themed_button(state, "ant.map.controlHeightSM", "Control 24",
                  ryn::ThemeConfig{}, ryn::ButtonType::Default,
                  ryn::ControlSize::Small);
    themed_button(state, "ant.seed.controlHeight", "Control 32",
                  ryn::ThemeConfig{}, ryn::ButtonType::Default,
                  ryn::ControlSize::Middle);
    themed_button(state, "ant.map.controlHeightLG", "Control 40",
                  ryn::ThemeConfig{}, ryn::ButtonType::Default,
                  ryn::ControlSize::Large);
    themed_button(state, "ant.map.borderRadiusSM", "Radius 4", radius_surface(4.0F));
    themed_button(state, "ant.seed.borderRadius", "Radius 6", radius_surface(6.0F));
    themed_button(state, "ant.map.borderRadiusLG", "Radius 8", radius_surface(8.0F));
}

void add_shadow_cells(const std::shared_ptr<GalleryState>& state) {
    const auto& shadows = ryn::ant_design_default_shadows();
    themed_button(state, "ant.alias.boxShadowTertiary", "Elevation 1",
                  shadow_surface(shadows.box_shadow_tertiary));
    themed_button(state, "ant.alias.boxShadowSecondary", "Elevation 2",
                  shadow_surface(shadows.box_shadow_secondary));
    themed_button(state, "ant.alias.boxShadow", "Elevation 3",
                  shadow_surface(shadows.box_shadow));
    themed_button(state, "ant.component.Button.defaultShadow", "Button Default",
                  shadow_surface(shadows.button_default));
    themed_button(state, "ant.component.Button.primaryShadow", "Button Primary",
                  shadow_surface(shadows.button_primary), ryn::ButtonType::Primary);
    themed_button(state, "ant.component.Button.dangerShadow", "Button Danger",
                  shadow_surface(shadows.button_danger), ryn::ButtonType::Danger);
    themed_button(state, "ant.alias.boxShadowDrawerLeft", "Drawer Left",
                  shadow_surface(shadows.drawer_left));
    themed_button(state, "ant.alias.boxShadowDrawerRight", "Drawer Right",
                  shadow_surface(shadows.drawer_right));
    themed_button(state, "ant.alias.boxShadowDrawerUp", "Drawer Up",
                  shadow_surface(shadows.drawer_up));
    themed_button(state, "ant.alias.boxShadowDrawerDown", "Drawer Down",
                  shadow_surface(shadows.drawer_down));
    themed_button(state, "ant.alias.boxShadowPopoverArrow", "Popover Arrow",
                  shadow_surface(shadows.popover_arrow));
    themed_button(state, "ant.alias.dropShadowPopover", "Popover",
                  shadow_surface(shadows.popover_drop));
    themed_button(state, "ant.alias.boxShadowCard", "Card",
                  shadow_surface(shadows.card));
    themed_button(state, "ant.alias.boxShadowTabsOverflowLeft", "Tabs Left inset",
                  shadow_surface(shadows.tabs_overflow_left));
    themed_button(state, "ant.alias.boxShadowTabsOverflowRight", "Tabs Right inset",
                  shadow_surface(shadows.tabs_overflow_right));
    themed_button(state, "ant.alias.boxShadowTabsOverflowTop", "Tabs Top inset",
                  shadow_surface(shadows.tabs_overflow_top));
    themed_button(state, "ant.alias.boxShadowTabsOverflowBottom", "Tabs Bottom inset",
                  shadow_surface(shadows.tabs_overflow_bottom));
}

ryn::ThemeConfig algorithm_config(ryn::ThemeAlgorithm algorithm) {
    ryn::ThemeConfig config;
    if (algorithm != ryn::ThemeAlgorithm::Default) {
        config.algorithms.push_back(algorithm);
    }
    return config;
}

void add_theme_and_state_cells(const std::shared_ptr<GalleryState>& state) {
    themed_button(
        state, "gallery.theme.default", "Default", algorithm_config(ryn::ThemeAlgorithm::Default),
        ryn::ButtonType::Default, ryn::ControlSize::Middle, false, false,
        [state] {
            state->theme.set(algorithm_config(ryn::ThemeAlgorithm::Default));
            ++state->telemetry.theme_updates;
            ++state->telemetry.activations;
        });
    themed_button(
        state, "gallery.theme.dark", "Dark", algorithm_config(ryn::ThemeAlgorithm::Dark),
        ryn::ButtonType::Default, ryn::ControlSize::Middle, false, false,
        [state] {
            state->theme.set(algorithm_config(ryn::ThemeAlgorithm::Dark));
            ++state->telemetry.theme_updates;
            ++state->telemetry.activations;
        });
    themed_button(
        state, "gallery.theme.compact", "Compact", algorithm_config(ryn::ThemeAlgorithm::Compact),
        ryn::ButtonType::Default, ryn::ControlSize::Middle, false, false,
        [state] {
            state->theme.set(algorithm_config(ryn::ThemeAlgorithm::Compact));
            ++state->telemetry.theme_updates;
            ++state->telemetry.activations;
        });
    ryn::ThemeConfig nested = algorithm_config(ryn::ThemeAlgorithm::Dark);
    nested.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
    themed_button(state, "gallery.theme.nested-brand", "Dark + Purple Seed", nested,
                  ryn::ButtonType::Primary);

    themed_button(state, "gallery.state.default", "Default", ryn::ThemeConfig{});
    themed_button(state, "gallery.state.primary", "Primary", ryn::ThemeConfig{},
                  ryn::ButtonType::Primary);
    themed_button(state, "gallery.state.danger", "Danger", ryn::ThemeConfig{},
                  ryn::ButtonType::Danger);
    themed_button(state, "gallery.state.hover", "Hover me / 悬停", ryn::ThemeConfig{});
    themed_button(state, "gallery.state.active", "Press me / 按下", ryn::ThemeConfig{});
    themed_button(state, "gallery.state.focus-visible", "Tab focus / 键盘焦点",
                  ryn::ThemeConfig{});
    themed_button(state, "gallery.state.disabled", "Disabled / 禁用",
                  ryn::ThemeConfig{}, ryn::ButtonType::Default,
                  ryn::ControlSize::Middle, state->disabled);
    themed_button(state, "gallery.state.loading", "Loading / 加载",
                  ryn::ThemeConfig{}, ryn::ButtonType::Primary,
                  ryn::ControlSize::Middle, false, state->loading);
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
                            .gap(ryn::SpaceSize::Small)
                            .layout(ryn::LayoutStyle{}.width(state->gallery_width)),
                        [state] {
                            ryn::Text(u8"RynUI Ant Design 6 Token Gallery / 设计令牌验收");
                            ryn::Text(
                                ryn::TextProps{}
                                    .content(u8"Default · Dark · Compact · Brand Seed · Nested Theme")
                                    .tone(ryn::TextTone::Secondary));
                            ryn::Flex(
                                ryn::FlexProps{}
                                    .wrap(true)
                                    .gap(ryn::dp(8.0F), ryn::dp(8.0F))
                                    .layout(ryn::LayoutStyle{}.width(state->gallery_width)),
                                [state] {
                                    add_theme_and_state_cells(state);
                                    add_palette_cells(state);
                                    add_scale_cells(state);
                                    add_shadow_cells(state);
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
            const float content_width = std::max(256.0F, viewport_width - 48.0F);
            const float next_cell_width = content_width < 720.0F
                ? std::max(150.0F, (content_width - 16.0F) / 3.0F)
                : 150.0F;
            if (state->gallery_width.get() != ryn::dp(content_width)) {
                state->gallery_width.set(ryn::dp(content_width));
                ++state->telemetry.viewport_updates;
            }
            if (state->cell_width.get() != ryn::dp(next_cell_width)) {
                state->cell_width.set(ryn::dp(next_cell_width));
                ++state->telemetry.viewport_updates;
            }
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
