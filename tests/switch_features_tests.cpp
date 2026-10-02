#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"

#include <ryn/rynui.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

bool near(float a, float b) {
    return std::abs(a - b) < 0.02F;
}

void activate(Fixture& f, input::InteractionId id) {
    if (f.services.focus().state().focused != id) {
        require(f.services.focus().request_focus(id, input::FocusModality::keyboard), "Switch focus failed");
    }
    f.services.focus().dispatch({input::Key::space, input::KeyAction::down});
    f.services.focus().dispatch({input::Key::space, input::KeyAction::up});
}

void retained_content_and_direction() {
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<bool> checked{false};
    Signal<SwitchDirection> direction{SwitchDirection::LeftToRight};
    Signal<String> caption{u8"关闭 Disabled"};
    int on_runs{};
    int off_runs{};
    f.buttons.mount(Content{[&] {
        Switch(SwitchProps{}.checked(checked).direction(direction), SwitchSlots{SwitchCheckedContent{[&] {
                                                                                    ++on_runs;
                                                                                    Text(u8"开");
                                                                                }},
                                                                                SwitchUncheckedContent{[&] {
                                                                                    ++off_runs;
                                                                                    Text(TextProps{}.content(caption));
                                                                                }}});
        Checkbox(CheckboxProps{});
        Radio(RadioProps{});
    }});
    f.synchronize();
    const auto sw = host.mounted().front();
    const auto texts = f.services.text().mounted_texts();
    require(texts.size() == 2 && on_runs == 1 && off_runs == 1, "Switch content was not retained once");
    const auto width = f.nodes.require(sw.node).bounds.width;
    require(width > 44 && f.services.interactions().size() == 3, "Switch content did not contribute stable width");
    const auto on = texts[0];
    const auto off = texts[1];
    require(!f.services.components().branch_active(on.component) &&
                f.services.components().branch_active(off.component),
            "Switch painted the wrong initial branch");
    const auto shapes = f.scene.text_state(off.scene).counters().shape_count;
    const auto mounts = f.services.components().mount_runs();
    const auto before = f.nodes.require(sw.node).measure_count;
    f.dirty.clear();
    checked.set(true);
    f.synchronize();
    require(near(f.nodes.require(sw.node).bounds.width, width) && f.nodes.require(sw.node).measure_count == before &&
                f.services.components().mount_runs() == mounts && on_runs == 1 && off_runs == 1 &&
                f.scene.text_state(off.scene).counters().shape_count == shapes &&
                f.services.components().branch_active(on.component) &&
                !f.services.components().branch_active(off.component),
            "Switch toggle changed width, measure, shaping or retained content");
    const auto range = f.services.surfaces().visual_range(sw.surface);
    const auto ltr = f.services.surfaces().instances().at(range.first + 1).bounds;
    f.dirty.clear();
    direction.set(SwitchDirection::RightToLeft);
    f.synchronize();
    const auto rtl = f.services.surfaces().instances().at(range.first + 1).bounds;
    require(rtl[0] < ltr[0] && near(rtl[0], 2) && f.nodes.require(sw.node).measure_count == before,
            "Switch RTL did not mirror handle without measure");
    const auto on_node = f.services.components().root(on.component);
    const auto clip = f.nodes.content_clip(on_node, {0, 0, 320, 240});
    require(clip.x >= 24 && clip.x + clip.width <= width - 9 + 0.02F, "Switch RTL content budget is wrong");
    f.nodes.require(sw.node).translation = {13, 7};
    f.synchronize();
    require(f.nodes.require(on_node).translation == runtime::Point{13, 7} &&
                near(f.nodes.content_clip(on_node, {0, 0, 320, 240}).x, clip.x + 13),
            "Switch translation did not follow content and clip");
    f.nodes.require(sw.node).translation = {};
    f.synchronize();
    caption.set(u8"很长的关闭内容 Long content changes width");
    f.synchronize();
    require(f.nodes.require(sw.node).bounds.width > width && on_runs == 1 && off_runs == 1,
            "Inactive Switch content update did not resize retained layout");
    f.synchronize(40, {0, 0, 40, 240});
    const auto primitive = f.scene.primitive(on.scene).instances;
    require(primitive.count > 0, "Switch visible text has no glyphs");
    const auto glyph_clip = f.scene.glyph_scene().instances().at(primitive.first).clip_bounds;
    require(glyph_clip[0] >= 24 && glyph_clip[2] <= 31.02F, "Switch glyph escaped narrow content clip");
    f.synchronize(10, {0, 0, 10, 240});
    require(!f.services.components().branch_active(on.component), "Zero content viewport retained visible branch");
    f.synchronize();
    require(f.services.components().branch_active(on.component), "Switch did not recover content from zero width");
}

void reference_and_callback_lifecycle() {
    SwitchRef reference;
    require(!reference.bound() && !reference.focus() && !reference.blur(), "Unbound SwitchRef was usable");
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<bool> loading{false};
    Signal<bool> disabled{false};
    std::vector<int> calls;
    f.buttons.mount(Content{[&] {
        Switch(SwitchProps{}
                   .checked(false)
                   .loading(loading)
                   .disabled(disabled)
                   .ref(reference)
                   .autoFocus(true)
                   .onChange([&](bool value) { calls.push_back(value ? 1 : -1); })
                   .onClick([&](bool value) { calls.push_back(value ? 2 : -2); }));
    }});
    const auto sw = host.mounted().front();
    require(reference.bound() && f.services.focus().state().focused == sw.interaction, "Switch autoFocus/ref failed");
    f.synchronize();
    activate(f, sw.interaction);
    require(calls == std::vector<int>{1, 2} && !host.snapshot(sw.component).checked,
            "Switch callbacks changed controlled value or callback order");
    loading.set(true);
    require(reference.focus() && f.services.focus().state().focused == sw.interaction,
            "Loading changed focus contract");
    activate(f, sw.interaction);
    require(calls.size() == 2, "Loading Switch called activation callbacks");
    loading.set(false);
    require(reference.blur() && !f.services.focus().state().focused && !reference.blur(), "SwitchRef blur failed");
    disabled.set(true);
    require(!reference.focus(), "Disabled SwitchRef focused");
    bool cross_thread_rejected{};
    std::thread thread{[&] {
        try {
            static_cast<void>(reference.bound());
        } catch (const std::logic_error&) {
            cross_thread_rejected = true;
        }
    }};
    thread.join();
    require(cross_thread_rejected, "SwitchRef allowed wrong thread");
    require(f.services.destroy(sw.component) && !reference.bound() && !reference.focus(),
            "Destroyed SwitchRef stayed bound");
    Fixture rebound;
    detail::SelectionComponentHost rebound_host{rebound.services};
    rebound.buttons.mount(Content{[&] {
        Switch(SwitchProps{}
                   .ref(reference)
                   .onChange([&](bool) {
                       calls.push_back(3);
                       require(rebound.services.destroy(rebound_host.mounted().front().component),
                               "Callback destroy failed");
                   })
                   .onClick([&](bool value) {
                       require(value && !reference.bound(), "Click after destroy retained invalid ref/value");
                       calls.push_back(4);
                   }));
    }});
    rebound.synchronize();
    activate(rebound, rebound_host.mounted().front().interaction);
    require(calls == std::vector<int>{1, 2, 3, 4} && rebound_host.mounted().empty() &&
                rebound.services.interactions().size() == 0 && rebound.services.surfaces().size() == 0,
            "Switch callback destruction leaked resources");
}

void invalid_content_and_reference() {
    for (const bool duplicate : {false, true}) {
        Fixture f;
        detail::SelectionComponentHost host{f.services};
        SwitchRef reference;
        bool rejected{};
        try {
            f.buttons.mount(Content{[&] {
                if (duplicate) {
                    Switch(SwitchProps{}.ref(reference));
                    Switch(SwitchProps{}.ref(reference));
                } else {
                    Switch(SwitchProps{}.ref(reference),
                           SwitchSlots{SwitchCheckedContent{
                                           [] { Button(ButtonProps{}, ButtonContent{[] { Text(u8"invalid"); }}); }},
                                       {}});
                }
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected && !reference.bound() && host.mounted().empty() &&
                    f.services.components().component_count() == 0 && f.services.interactions().size() == 0 &&
                    f.services.surfaces().size() == 0,
                "Switch invalid mount did not roll back");
    }
}

void theme_and_handle_feedback() {
    const auto defaults = resolve_theme();
    const auto& token = defaults.switch_token();
    require(near(token.inner_min_margin, 9) && near(token.inner_max_margin, 24) &&
                near(token.inner_min_margin_small, 6) && near(token.inner_max_margin_small, 18) &&
                token.handle_shadow.size() == 1 && token.handle_shadow[0].offset == LogicalOffset{0, 2} &&
                near(token.handle_shadow[0].blur, 4) && near(token.loading_opacity, 0.65F),
            "Switch defaults differ from locked content/shadow/opacity tokens");
    ThemeConfig override;
    override.alias.opacity_loading = 0.3F;
    override.switch_.tokens.inner_min_margin = dp(12);
    override.switch_.tokens.inner_max_margin = dp(30);
    override.switch_.tokens.wave_width = dp(3);
    const auto parent = resolve_theme(override);
    const auto child = resolve_theme({}, &parent);
    require(parent.switch_token() == child.switch_token() && near(child.alias().opacity_loading, 0.3F) &&
                parent.identity() != defaults.identity() &&
                parent.diagnostic_json().find("\"handleShadow\"") != std::string::npos,
            "Switch tokens were not inherited/hashed/serialized");
    for (const float invalid : {-1.0F, 1.1F, std::numeric_limits<float>::infinity()}) {
        auto bad = override;
        bad.alias.opacity_loading = invalid;
        bool rejected{};
        try {
            static_cast<void>(resolve_theme(bad));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "Invalid opacityLoading was accepted");
        bad = override;
        bad.switch_.tokens.wave_opacity = invalid;
        rejected = false;
        try {
            static_cast<void>(resolve_theme(bad));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "Invalid Switch wave opacity was accepted");
    }
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<bool> loading{false};
    Signal<SwitchDirection> direction{SwitchDirection::LeftToRight};
    int runs{};
    f.buttons.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Switch(SwitchProps{}.loading(loading).direction(direction), SwitchSlots{SwitchCheckedContent{[&] {
                                                                                              ++runs;
                                                                                              Text(u8"开");
                                                                                          }},
                                                                                          SwitchUncheckedContent{[&] {
                                                                                              ++runs;
                                                                                              Text(u8"关");
                                                                                          }}});
              }});
    }});
    f.synchronize();
    const auto sw = host.mounted().front();
    const auto range = f.services.surfaces().visual_range(sw.surface);
    const auto handle_width = f.services.surfaces().instances().at(range.first + 1).bounds[2];
    const auto shadows = f.services.surfaces().shadow_effects(sw.surface);
    require(shadows.size() == 1, "Switch did not create handle shadow");
    const auto shadow_id = shadows[0];
    const auto& effect = f.services.surfaces().effects().at(shadow_id);
    require(near(effect.geometry.shape.rect.width, handle_width) && near(effect.geometry.shape.radius, 9),
            "Switch shadow uses track shape instead of handle");
    const auto commands = f.services.scene_composer().ordered_scene().commands();
    require(commands.size() >= 3 && commands[0].kind == graphics::SceneDrawKind::quad &&
                commands[0].instance_count == 1 && commands[1].kind == graphics::SceneDrawKind::rounded_effect &&
                commands[2].kind == graphics::SceneDrawKind::quad && commands[2].instance_count == 9,
            "Switch shadow is not between track and handle fill");
    require(f.services.focus().request_focus(sw.interaction, input::FocusModality::keyboard),
            "Switch press focus failed");
    f.services.focus().dispatch({input::Key::space, input::KeyAction::down});
    f.synchronize();
    require(near(f.services.surfaces().instances().at(range.first + 1).bounds[2], handle_width * 1.3F),
            "Switch keyboard press did not extend handle");
    loading.set(true);
    f.synchronize();
    require(near(f.services.surfaces().instances().at(range.first + 1).bounds[2], handle_width) &&
                f.services.focus().state().focused == sw.interaction &&
                !host.snapshot(sw.component).focus.keyboard_pressed,
            "Loading retained extension or lost focus");
    const auto scene = f.services.text().mounted_texts().back().scene;
    const auto shapes = f.scene.text_state(scene).counters().shape_count;
    const auto measures = f.nodes.require(sw.node).measure_count;
    f.dirty.clear();
    auto config = theme.get();
    config.alias.opacity_loading = 0.3F;
    theme.set(config);
    f.synchronize();
    const auto glyph = f.scene.primitive(scene).instances;
    require(near(f.services.surfaces().instances().at(range.first).opacity, 0.3F) && glyph.count > 0 &&
                near(f.scene.glyph_scene().instances().at(glyph.first).translation_opacity[2], 0.3F) &&
                near(f.services.surfaces().effects().at(shadow_id).material.opacity, 0) &&
                f.nodes.require(sw.node).measure_count == measures &&
                f.scene.text_state(scene).counters().shape_count == shapes && runs == 2,
            "opacityLoading did not fade track/content/shadow locally");
    f.dirty.clear();
    config.switch_.tokens.handle_shadow = ShadowList{{ShadowKind::outer, {1, 3}, 6, 0, Color::rgba8(5, 6, 7, 70)}};
    theme.set(config);
    f.synchronize();
    require(f.services.surfaces().shadow_effects(sw.surface)[0] == shadow_id &&
                f.services.surfaces().effects().at(shadow_id).geometry.offset == LogicalOffset{1, 3} &&
                f.nodes.require(sw.node).measure_count == measures,
            "Switch shadow override remounted or measured content");
    config.switch_.algorithm = true;
    config.switch_.seed.color_primary = Color::rgba8(120, 40, 180);
    theme.set(config);
    f.synchronize();
    const auto isolated = resolve_theme(config).switch_token();
    auto reference_theme = ThemeConfig{};
    reference_theme.seed.color_primary = *config.switch_.seed.color_primary;
    require(isolated.checked_background == resolve_theme(reference_theme).map().color_primary &&
                f.nodes.require(sw.node).measure_count == measures,
            "Switch component algorithm ignored local primary color or measured content");
    config.switch_.seed.focus_outline = false;
    theme.set(config);
    f.synchronize();
    require(near(resolve_theme(config).switch_token().focus_width, 0) &&
                f.services.surfaces().effects().live_count() == 1 && f.nodes.require(sw.node).measure_count == measures,
            "Switch component focusOutline=false left a ring or measured content");
    config.switch_.tokens.inner_min_margin = dp(20);
    config.switch_.tokens.inner_max_margin = dp(40);
    theme.set(config);
    f.synchronize();
    require(f.nodes.require(sw.node).bounds.width > 60 && runs == 2, "Switch content token did not update width");
    loading.set(false);
    require(
        near(f.services.surfaces().effects().at(f.services.surfaces().shadow_effects(sw.surface)[0]).material.opacity,
             1),
        "Enabled Switch did not restore handle shadow");
    direction.set(SwitchDirection::RightToLeft);
    f.services.focus().dispatch({input::Key::space, input::KeyAction::down});
    f.synchronize();
    const auto handle = f.services.surfaces().instances().at(range.first + 1).bounds;
    require(near(handle[0] + handle[2], f.nodes.require(sw.node).bounds.width - 2),
            "RTL press extension moved fixed outer edge");
    f.services.focus().clear_focus();
    f.synchronize();
    require(near(f.services.surfaces().instances().at(range.first + 1).bounds[2], handle_width),
            "Blur retained Switch extension");
}

void wave_restarts_cancellation_and_idle() {
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<bool> disabled{false};
    Signal<bool> loading{false};
    Signal<bool> wave{true};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int runs{};
    f.buttons.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Switch(SwitchProps{}.disabled(disabled).loading(loading).wave(wave),
                         SwitchSlots{SwitchCheckedContent{[&] {
                                         ++runs;
                                         Text(u8"开");
                                     }},
                                     SwitchUncheckedContent{[&] {
                                         ++runs;
                                         Text(u8"关");
                                     }}});
              }});
    }});
    f.synchronize();
    const auto sw = host.mounted().front();
    f.services.set_motion_preference(animation::MotionPreference::normal);
    const auto effects = f.services.surfaces().effects().live_count();
    const auto measures = f.nodes.require(sw.node).measure_count;
    activate(f, sw.interaction);
    require(host.snapshot(sw.component).wave_active && near(host.snapshot(sw.component).wave_progress, 0) &&
                f.services.surfaces().effects().live_count() == effects + 1 && f.services.next_frame_deadline(),
            "Switch wave did not start");
    f.synchronize();
    const auto range = host.snapshot(sw.component).wave_range;
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(100000)));
    f.synchronize();
    require(host.snapshot(sw.component).wave_progress > 0 && host.snapshot(sw.component).wave_progress < 1 &&
                f.nodes.require(sw.node).measure_count == measures && runs == 2,
            "Switch wave measured or reran content");
    activate(f, sw.interaction);
    require(host.snapshot(sw.component).wave_range == range && near(host.snapshot(sw.component).wave_progress, 0) &&
                f.services.surfaces().effects().live_count() == effects + 1,
            "Switch wave restart leaked range/effect");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(1000000)));
    f.synchronize();
    require(!host.snapshot(sw.component).wave_active && f.services.surfaces().effects().live_count() == effects &&
                !f.services.next_frame_deadline(),
            "Switch wave did not settle idle");
    for (int cancel = 0; cancel < 5; ++cancel) {
        activate(f, sw.interaction);
        require(host.snapshot(sw.component).wave_active, "Switch cancellation fixture failed to start wave");
        if (cancel == 0) {
            disabled.set(true);
            require(!host.snapshot(sw.component).wave_active, "Disabled did not cancel wave");
            disabled.set(false);
        } else if (cancel == 1) {
            loading.set(true);
            require(!host.snapshot(sw.component).wave_active, "Loading did not cancel wave");
            loading.set(false);
        } else if (cancel == 2) {
            wave.set(false);
            require(!host.snapshot(sw.component).wave_active, "Wave prop did not cancel wave");
            wave.set(true);
        } else if (cancel == 3) {
            f.services.set_window_active(false);
            require(!host.snapshot(sw.component).wave_active, "Inactive window did not cancel wave");
            f.services.set_window_active(true);
        } else {
            auto config = theme.get();
            config.seed.motion = false;
            theme.set(config);
            require(!host.snapshot(sw.component).wave_active && !f.services.next_frame_deadline(),
                    "Theme motion=false left animation deadlines");
            config.seed.motion = true;
            theme.set(config);
        }
    }
    activate(f, sw.interaction);
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    require(!host.snapshot(sw.component).wave_active && !f.services.next_frame_deadline(),
            "Reduced motion did not cancel finite animations");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    activate(f, sw.interaction);
    require(f.services.destroy(sw.component) && f.services.surfaces().size() == 0 &&
                f.services.surfaces().effects().live_count() == 0 &&
                f.services.animations().diagnostics().targets == 0 && !f.services.next_frame_deadline(),
            "Switch wave destruction leaked resources");
}
} // namespace

int main() {
    try {
        retained_content_and_direction();
        reference_and_callback_lifecycle();
        invalid_content_and_reference();
        theme_and_handle_feedback();
        wave_restarts_cancellation_and_idle();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
