#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"

#include <ryn/rynui.hpp>

#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <class Function> void rejected(Function function) {
    bool failed = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    require(failed, "Checkbox invalid input was accepted");
}

void activate(Fixture& f, input::InteractionId id) {
    if (f.services.focus().state().focused != id) {
        require(f.services.focus().request_focus(id, input::FocusModality::keyboard), "Checkbox focus failed");
    }
    f.services.focus().dispatch({input::Key::space, input::KeyAction::down});
    f.services.focus().dispatch({input::Key::space, input::KeyAction::up});
}

void controlled_and_values() {
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<CheckboxValues> value{CheckboxValues{String{u8"one"}}};
    Signal<bool> disabled{false};
    CheckboxValues candidate;
    int changes{};
    f.buttons.mount(Content{[&] {
        CheckboxGroup(CheckboxGroupProps{}
                          .options(std::vector<CheckboxOption>{{String{u8"one"}, String{u8"文字"}},
                                                               {1.0, String{u8"数字"}},
                                                               {true, String{u8"布尔"}, true}})
                          .value(value)
                          .disabled(disabled)
                          .onChange([&](const CheckboxValues& next) {
                              candidate = next;
                              ++changes;
                          }));
        Switch(SwitchProps{});
        Radio(RadioProps{});
    }});
    f.synchronize();
    const auto first = host.mounted()[0];
    const auto second = host.mounted()[1];
    const auto third = host.mounted()[2];
    require(host.checkbox_groups().size() == 1 && host.mounted().size() == 5 && host.snapshot(first.component).checked,
            "Checkbox typed Group inventory/value incorrect");
    const auto mounts = f.services.components().mount_runs();
    const auto group = host.checkbox_groups()[0];
    require(!f.services.interactions().require(group.interaction).focusable, "CheckboxGroup is a Tab target");
    activate(f, second.interaction);
    require(changes == 1 && candidate == CheckboxValues{String{u8"one"}, 1.0} &&
                !host.snapshot(second.component).checked,
            "Checkbox controlled Group did not preserve authority/candidate order");
    value.set(candidate);
    f.synchronize();
    require(host.snapshot(second.component).checked && changes == 1 && f.services.components().mount_runs() == mounts,
            "Checkbox external controlled update remounted or emitted change");
    disabled.set(true);
    f.synchronize();
    require(host.snapshot(first.component).disabled && host.snapshot(second.component).disabled &&
                host.snapshot(third.component).disabled,
            "Checkbox Group disabled did not propagate");
    disabled.set(false);
    f.synchronize();
    require(!host.snapshot(first.component).disabled && host.snapshot(third.component).disabled,
            "Checkbox own disabled was lost after Group enabled");
    rejected([&] { value.set({1.0, 1.0}); });
    require(host.checkbox_group_value(group.component) == candidate && host.snapshot(second.component).checked,
            "Checkbox invalid controlled value mutated published state");
    value.set({true});
    f.synchronize();
    require(host.snapshot(third.component).checked && !host.snapshot(first.component).checked && changes == 1,
            "Checkbox disabled value cannot be set externally");
}

void dynamic_options() {
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<std::vector<CheckboxOption>> options{
        std::vector<CheckboxOption>{{String{u8"a"}, String{u8"A"}}, {String{u8"b"}, String{u8"B"}}}};
    int changes{};
    CheckboxValues candidate;
    f.buttons.mount(Content{[&] {
        CheckboxGroup(CheckboxGroupProps{}
                          .options(options)
                          .defaultValue({String{u8"b"}})
                          .onChange([&](const CheckboxValues& next) {
                              candidate = next;
                              ++changes;
                          }));
        Checkbox(CheckboxProps{}, CheckboxLabel{[] { Text(u8"following control"); }});
    }});
    f.synchronize();
    const auto a = host.mounted()[0];
    const auto b = host.mounted()[1];
    const auto following = host.mounted()[2];
    const auto group = host.checkbox_groups()[0];
    const auto label = f.services.text().mounted_texts()[0];
    const auto label_shapes = f.scene.text_state(label.scene).counters().shape_count;
    require(f.services.focus().request_focus(b.interaction, input::FocusModality::keyboard),
            "Checkbox dynamic focus failed");
    options.set({{String{u8"b"}, String{u8"B updated"}}, {String{u8"a"}, String{u8"A"}}, {2.0, String{u8"C"}}});
    f.synchronize();
    const auto c = host.mounted()[3];
    require(host.mounted()[0].component == a.component && host.mounted()[1].component == b.component &&
                f.services.focus().state().focused == b.interaction &&
                f.nodes.require(b.node).bounds.x < f.nodes.require(a.node).bounds.x &&
                f.scene.text_state(label.scene).counters().shape_count == label_shapes,
            "Checkbox options reorder remounted unchanged options/labels or lost focus");
    f.services.focus().dispatch({input::Key::tab, input::KeyAction::down});
    require(f.services.focus().state().focused == a.interaction, "Checkbox options Tab order did not follow data");
    f.services.focus().dispatch({input::Key::tab, input::KeyAction::down});
    require(f.services.focus().state().focused == c.interaction,
            "Checkbox new option appended after following controls");
    f.services.focus().dispatch({input::Key::tab, input::KeyAction::down});
    require(f.services.focus().state().focused == following.interaction, "Checkbox Group escaped keyboard boundary");
    activate(f, a.interaction);
    require(candidate == CheckboxValues{String{u8"b"}, String{u8"a"}} && changes == 1,
            "Checkbox candidate ordering incorrect");
    rejected([&] { options.set({{String{u8"a"}, String{u8"wrong"}}, {String{u8"a"}, String{u8"duplicate"}}}); });
    require(host.mounted().size() == 4 && host.snapshot(b.component).checked,
            "Checkbox invalid options replaced old scene");
    const auto bounds = f.nodes.require(a.node).bounds;
    f.services.pointer().dispatch({input::PointerIdentity::mouse(), input::PointerAction::down,
                                   input::PointerButton::primary, bounds.x + bounds.width / 2,
                                   bounds.y + bounds.height / 2});
    require(f.services.pointer().state(input::PointerIdentity::mouse())->capture == a.interaction,
            "Checkbox option pointer capture failed");
    options.set({{String{u8"b"}, String{u8"B updated"}}, {2.0, String{u8"C"}, true}});
    f.synchronize();
    require(!f.services.components().contains(a.component) && !f.services.focus().state().focused &&
                host.checkbox_group_value(group.component) == CheckboxValues{String{u8"b"}} &&
                host.snapshot(c.component).disabled && changes == 1,
            "Checkbox removed selected/focused option retained resources/value or emitted change");
    require(!f.services.pointer().state(input::PointerIdentity::mouse())->capture,
            "Checkbox removed option retained pointer capture");
    options.set({});
    f.synchronize();
    require(host.mounted().size() == 1 && host.checkbox_group_value(group.component).empty() &&
                f.services.interactions().size() == 2,
            "Checkbox empty Group leaked option interactions");
    options.set({{false, String{u8"false"}}});
    f.synchronize();
    require(host.mounted().size() == 2, "Checkbox empty options did not recover");
}

void manual_content_and_lifetime() {
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<bool> disabled{false};
    int content_runs{};
    Signal<CheckboxDirection> direction{CheckboxDirection::LeftToRight};
    int label_runs{};
    int own_changes{};
    int group_changes{};
    f.buttons.mount(Content{[&] {
        CheckboxGroup(
            CheckboxGroupProps{}.disabled(disabled).direction(direction).onChange([&](const CheckboxValues& value) {
                require(value == CheckboxValues{true}, "Checkbox copied Group callback candidate wrong");
                ++group_changes;
            }),
            CheckboxGroupContent{[&] {
                ++content_runs;
                Space(SpaceProps{}, SpaceContent{[&] {
                          Checkbox(CheckboxProps{}.value(true).onChange([&](bool value) {
                              require(value, "Checkbox own candidate false");
                              ++own_changes;
                              f.services.destroy(host.checkbox_groups().back().component);
                          }),
                                   CheckboxLabel{[&] {
                                       ++label_runs;
                                       Icon(IconProps{}.name(IconName::CheckOutlined));
                                       Text(u8"rich");
                                   }});
                          Checkbox(CheckboxProps{}.skipGroup(true).defaultChecked(true));
                          CheckboxGroup(
                              CheckboxGroupProps{}.options(std::vector<CheckboxOption>{{1.0, String{u8"nested"}}}));
                      }});
            }});
    }});
    f.synchronize();
    const auto item = host.mounted()[0];
    const auto range = f.services.surfaces().visual_range(item.surface);
    const auto initial_x = f.services.surfaces().instances().at(range.first).bounds[0];
    direction.set(CheckboxDirection::RightToLeft);
    f.synchronize();
    require(f.services.surfaces().instances().at(range.first).bounds[0] > initial_x,
            "Checkbox member did not inherit reactive Group RTL");
    require(host.checkbox_groups().size() == 2 && host.mounted().size() == 3 &&
                host.snapshot(host.mounted()[1].component).checked,
            "Checkbox nearest Group/skipGroup failed");
    disabled.set(true);
    f.synchronize();
    require(host.snapshot(item.component).disabled && !host.snapshot(host.mounted()[1].component).disabled &&
                !host.snapshot(host.mounted()[2].component).disabled && content_runs == 1 && label_runs == 1,
            "Checkbox Group propagated into skip/nested Group or reran rich content");
    disabled.set(false);
    f.synchronize();
    activate(f, item.interaction);
    require(own_changes == 1 && group_changes == 1 && host.mounted().empty() && host.checkbox_groups().empty() &&
                f.services.interactions().size() == 0 && f.scene.size() == 0,
            "Checkbox callback destruction leaked state");
}

void invalid_mounts() {
    for (int kind = 0; kind < 8; ++kind) {
        Fixture f;
        detail::SelectionComponentHost host{f.services};
        rejected([&] {
            f.buttons.mount(Content{[&] {
                if (kind == 0) {
                    CheckboxGroup(CheckboxGroupProps{}.options(
                        std::vector<CheckboxOption>{{std::numeric_limits<double>::quiet_NaN(), String{u8"bad"}}}));
                } else if (kind == 1) {
                    CheckboxGroup(CheckboxGroupProps{}.value(CheckboxValues{}).defaultValue({true}));
                } else if (kind == 2) {
                    CheckboxGroup(CheckboxGroupProps{}.options(std::vector<CheckboxOption>{}),
                                  CheckboxGroupContent{[] {}});
                } else if (kind == 3) {
                    CheckboxGroup(CheckboxGroupProps{}, CheckboxGroupContent{[] { Checkbox(CheckboxProps{}); }});
                } else if (kind == 4) {
                    Checkbox(CheckboxProps{},
                             CheckboxLabel{[] { Button(ButtonProps{}, [] { Text(u8"interactive"); }); }});
                } else if (kind == 5) {
                    CheckboxGroup(CheckboxGroupProps{}.defaultValue({true, true}));
                } else if (kind == 6) {
                    CheckboxGroup(CheckboxGroupProps{}, CheckboxGroupContent{[] {
                                      Checkbox(CheckboxProps{}.value(true));
                                      Checkbox(CheckboxProps{}.value(true));
                                  }});
                } else {
                    CheckboxGroup(CheckboxGroupProps{}.options(std::vector<CheckboxOption>(1025)));
                }
            }});
        });
        require(f.nodes.size() == 0 && f.services.interactions().size() == 0 && host.mounted().empty() &&
                    host.checkbox_groups().empty() && f.scene.size() == 0,
                "Checkbox invalid mount leaked transaction resources");
    }
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.001F;
}

void references_callbacks_and_rtl() {
    CheckboxRef reference;
    require(!reference.bound() && !reference.focus() && !reference.blur(), "Unbound CheckboxRef was usable");
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<bool> disabled{false};
    Signal<CheckboxDirection> direction{CheckboxDirection::LeftToRight};
    std::vector<int> calls;
    f.buttons.mount(Content{[&] {
        Checkbox(CheckboxProps{}
                     .checked(false)
                     .disabled(disabled)
                     .direction(direction)
                     .ref(reference)
                     .autoFocus(true)
                     .onChange([&](bool value) { calls.push_back(value ? 1 : -1); })
                     .onClick([&](bool value) { calls.push_back(value ? 2 : -2); }),
                 CheckboxLabel{[] { Text(u8"label"); }});
    }});
    const auto item = host.mounted().front();
    require(reference.bound() && f.services.focus().state().focused == item.interaction,
            "Checkbox autofocus/ref failed");
    f.synchronize();
    const auto range = f.services.surfaces().visual_range(item.surface);
    const auto initial_x = f.services.surfaces().instances().at(range.first).bounds[0];
    activate(f, item.interaction);
    require(calls == std::vector<int>{1, 2} && !host.snapshot(item.component).checked,
            "Checkbox controlled click order wrong");
    direction.set(CheckboxDirection::RightToLeft);
    f.synchronize();
    const auto quad = f.services.surfaces().instances().at(range.first);
    const auto bounds = f.nodes.require(item.node).bounds;
    require(quad.bounds[0] > initial_x && near(quad.bounds[0] + quad.bounds[2], bounds.x + bounds.width),
            "Checkbox RTL indicator did not follow label");
    require(reference.blur() && !reference.blur() && reference.focus(), "Checkbox ref focus/blur failed");
    disabled.set(true);
    require(!reference.focus() && !f.services.focus().state().focused, "Disabled Checkbox retained focus");
    bool wrong_thread{};
    std::thread thread{[&] {
        try {
            static_cast<void>(reference.bound());
        } catch (const std::logic_error&) {
            wrong_thread = true;
        }
    }};
    thread.join();
    require(wrong_thread, "CheckboxRef accepted wrong thread");
    f.services.destroy(item.component);
    require(!reference.bound() && !reference.focus(), "CheckboxRef stayed bound after destroy");
    Fixture rebound;
    detail::SelectionComponentHost rebound_host{rebound.services};
    rebound.buttons.mount(Content{[&] {
        Checkbox(CheckboxProps{}
                     .ref(reference)
                     .onChange([&](bool) {
                         calls.push_back(3);
                         rebound.services.destroy(rebound_host.mounted().front().component);
                     })
                     .onClick([&](bool value) {
                         require(value && !reference.bound(), "Checkbox copied click/ref state stale");
                         calls.push_back(4);
                     }));
    }});
    rebound.synchronize();
    activate(rebound, rebound_host.mounted().front().interaction);
    require(calls == std::vector<int>{1, 2, 3, 4} && rebound_host.mounted().empty() &&
                rebound.services.surfaces().size() == 0,
            "Checkbox copied callbacks or rebound cleanup failed");
    Fixture duplicate;
    detail::SelectionComponentHost duplicate_host{duplicate.services};
    rejected([&] {
        duplicate.buttons.mount(Content{[&] {
            Checkbox(CheckboxProps{}.ref(reference));
            Checkbox(CheckboxProps{}.ref(reference));
        }});
    });
    require(!reference.bound() && duplicate_host.mounted().empty() && duplicate.nodes.size() == 0,
            "Checkbox duplicate ref leaked binding");
}

void theme_and_finite_wave() {
    const auto defaults = resolve_theme();
    const auto token = defaults.checkbox();
    require(near(token.size, 16) && near(token.indeterminate_size, 8) && near(token.line_width, 1) &&
                near(token.check_width, 2) && near(token.label_gap, 8),
            "Checkbox default native token drift");
    ThemeConfig config;
    config.checkbox.algorithm = true;
    config.checkbox.seed.color_primary = Color::rgba8(120, 40, 180);
    config.checkbox.tokens.size = dp(20);
    const auto isolated = resolve_theme(config);
    const auto inherited = resolve_theme({}, &isolated);
    require(isolated.checkbox() == inherited.checkbox() && isolated.checkbox().primary != token.primary &&
                isolated.switch_token() == defaults.switch_token() && isolated.button() == defaults.button() &&
                isolated.identity() != defaults.identity() &&
                isolated.diagnostic_json().find("\"checkbox\"") != std::string::npos,
            "Checkbox theme isolation/inheritance/identity failed");
    const auto scope = theme_runtime::ThemeScope::create_default();
    theme_runtime::DirtyPhase checkbox_phase{};
    int switch_invalidations{};
    auto checkbox_subscription = scope->capture([&](theme_runtime::DirtyPhase phase) { checkbox_phase = phase; },
                                                [&] {
                                                    static_cast<void>(scope->checkbox_metrics());
                                                    static_cast<void>(scope->checkbox_colors());
                                                    static_cast<void>(scope->checkbox_effects());
                                                });
    auto switch_subscription = scope->capture([&](theme_runtime::DirtyPhase) { ++switch_invalidations; },
                                              [&] {
                                                  static_cast<void>(scope->switch_geometry());
                                                  static_cast<void>(scope->switch_colors());
                                                  static_cast<void>(scope->switch_effects());
                                              });
    ThemeConfig local_color;
    local_color.checkbox.tokens.primary = Color::rgba8(80, 120, 160);
    require(scope->update(local_color) && checkbox_phase == theme_runtime::DirtyPhase::paint_material &&
                switch_invalidations == 0,
            "Checkbox token identity invalidated independent Switch or non-material phases");
    for (const float invalid : {-1.0F, 1.1F, std::numeric_limits<float>::infinity()}) {
        auto bad = config;
        bad.checkbox.tokens.wave_opacity = invalid;
        rejected([&] { static_cast<void>(resolve_theme(bad)); });
    }
    auto bad = config;
    bad.checkbox.tokens.indeterminate_size = dp(21);
    rejected([&] { static_cast<void>(resolve_theme(bad)); });
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    Signal<ThemeConfig> theme{config};
    Signal<bool> disabled{false};
    Signal<bool> wave{true};
    Signal<bool> indeterminate{true};
    int runs{};
    f.buttons.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Checkbox(CheckboxProps{}.disabled(disabled).wave(wave).indeterminate(indeterminate),
                           CheckboxLabel{[&] {
                               ++runs;
                               Text(u8"the complete label");
                           }});
                  Switch(SwitchProps{});
              }});
    }});
    f.synchronize();
    const auto item = host.mounted().front();
    const auto range = f.services.surfaces().visual_range(item.surface);
    const auto label = f.services.text().mounted_texts().front().scene;
    const auto targets = f.services.animations().diagnostics().targets;
    const auto shape_count = f.scene.text_state(label).counters().shape_count;
    const auto measures = f.nodes.require(item.node).measure_count;
    config.checkbox.tokens.primary = Color::rgba8(200, 100, 0);
    theme.set(config);
    f.synchronize();
    require(f.nodes.require(item.node).measure_count == measures &&
                f.scene.text_state(label).counters().shape_count == shape_count && runs == 1,
            "Checkbox color-only theme update measured/shaped/remounted label");
    require(f.services.focus().request_focus(item.interaction, input::FocusModality::keyboard),
            "Checkbox wave focus failed");
    f.synchronize();
    f.services.set_motion_preference(animation::MotionPreference::normal);
    const auto effects = f.services.surfaces().effects().live_count();
    activate(f, item.interaction);
    require(host.snapshot(item.component).checked && host.snapshot(item.component).indeterminate &&
                host.snapshot(item.component).wave_active &&
                f.services.surfaces().effects().live_count() == effects + 1 && f.services.next_frame_deadline(),
            "Checkbox indeterminate activation/wave failed");
    const auto wave_range = host.snapshot(item.component).wave_range;
    f.synchronize();
    const auto indicator = f.services.surfaces().instances().at(range.first).bounds;
    const auto focus = f.services.surfaces().focus_effect(item.surface);
    require(near(indicator[2], 20) && near(focus.geometry.shape.rect.width, indicator[2]) &&
                indicator[2] < f.nodes.require(item.node).bounds.width,
            "Checkbox focus geometry includes the label");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(100000)));
    f.synchronize();
    require(host.snapshot(item.component).wave_progress > 0 && host.snapshot(item.component).wave_progress < 1 &&
                f.nodes.require(item.node).measure_count == measures && runs == 1,
            "Checkbox wave measured/remounted content");
    activate(f, item.interaction);
    require(host.snapshot(item.component).wave_range == wave_range &&
                near(host.snapshot(item.component).wave_progress, 0),
            "Checkbox wave restart replaced retained range");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(1000000)));
    f.synchronize();
    require(!host.snapshot(item.component).wave_active && !f.services.next_frame_deadline() &&
                f.services.surfaces().effects().live_count() == effects,
            "Checkbox wave never settled idle");
    for (int cancel = 0; cancel < 4; ++cancel) {
        activate(f, item.interaction);
        require(host.snapshot(item.component).wave_active, "Checkbox cancel setup failed");
        if (cancel == 0) {
            disabled.set(true);
            disabled.set(false);
        } else if (cancel == 1) {
            wave.set(false);
            wave.set(true);
        } else if (cancel == 2) {
            f.services.set_window_active(false);
            f.services.set_window_active(true);
        } else {
            config.seed.motion = false;
            theme.set(config);
        }
        require(!host.snapshot(item.component).wave_active && !f.services.next_frame_deadline(),
                "Checkbox cancellation retained wave/deadline");
    }
    config.seed.motion = true;
    theme.set(config);
    activate(f, item.interaction);
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    require(!host.snapshot(item.component).wave_active && !f.services.next_frame_deadline(),
            "Checkbox reduced motion retained wave");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    activate(f, item.interaction);
    f.services.destroy(item.component);
    require(f.services.animations().diagnostics().targets + 1 == targets && !f.services.next_frame_deadline(),
            "Checkbox destruction retained its wave target/deadline");
}
} // namespace

int main() {
    try {
        controlled_and_values();
        dynamic_options();
        manual_content_and_lifetime();
        invalid_mounts();
        references_callbacks_and_rtl();
        theme_and_finite_wave();
        std::cout << "Checkbox Group contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
