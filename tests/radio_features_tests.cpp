#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"

#include <ryn/rynui.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void key(Fixture& fixture, Key value, KeyAction action = KeyAction::down, bool repeat = false) {
    fixture.services.focus().dispatch({value, action, KeyModifier::none, repeat});
}

void click(Fixture& fixture, const detail::MountedSelectionComponent& item) {
    const auto bounds = fixture.nodes.require(item.node).bounds;
    const float x = bounds.x + bounds.width / 2;
    const float y = bounds.y + bounds.height / 2;
    fixture.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, x, y});
    fixture.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, x, y});
}

void dynamic_and_keyboard() {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    Signal<std::vector<RadioOption>> options{std::vector<RadioOption>{
        {String{u8"a"}, String{u8"甲"}}, {42.0, String{u8"数字"}}, {true, String{u8"禁用"}, true}}};
    Signal<RadioDirection> direction{RadioDirection::LeftToRight};
    int calls{};
    host.mount(Content{[&] {
        Radio(RadioProps{}, RadioLabel{[] { Text(u8"Before"); }});
        RadioGroup(RadioGroupProps{}
                       .options(options)
                       .defaultValue(String{u8"a"})
                       .direction(direction)
                       .onValueChange([&](const RadioValue&) { ++calls; }));
        Radio(RadioProps{}, RadioLabel{[] { Text(u8"After"); }});
    }});
    fixture.synchronize();
    const auto before = host.mounted()[0];
    const auto a = host.mounted()[1];
    const auto number = host.mounted()[2];
    const auto disabled = host.mounted()[3];
    const auto after = host.mounted()[4];
    const auto group = host.radio_groups().front();
    key(fixture, Key::tab);
    require(fixture.services.focus().state().focused == before.interaction, "Tab before failed");
    key(fixture, Key::tab);
    require(fixture.services.focus().state().focused == a.interaction, "selected Tab entry failed");
    key(fixture, Key::right);
    require(fixture.services.focus().state().focused == number.interaction && calls == 1 &&
                host.radio_group_value(group) == RadioSelection{42.0},
            "typed arrow selection failed");
    key(fixture, Key::right);
    require(fixture.services.focus().state().focused == a.interaction && calls == 2, "arrow skip/wrap failed");
    key(fixture, Key::tab);
    require(fixture.services.focus().state().focused == after.interaction, "group has multiple Tab stops");
    require(fixture.services.focus().request_focus(number.interaction, FocusModality::keyboard), "refocus failed");
    options.set(std::vector<RadioOption>{
        {42.0, String{u8"数字"}}, {String{u8"a"}, String{u8"甲"}}, {false, String{u8"新选项"}}});
    fixture.synchronize();
    require(host.mounted()[1].component == a.component && host.mounted()[2].surface == number.surface &&
                fixture.services.focus().state().focused == number.interaction &&
                !fixture.services.components().contains(disabled.component),
            "reorder replaced identity/focus");
    require(fixture.nodes.require(number.node).bounds.x < fixture.nodes.require(a.node).bounds.x,
            "options layout order did not update");
    direction.set(RadioDirection::RightToLeft);
    key(fixture, Key::left);
    require(fixture.services.focus().state().focused == a.interaction && calls == 2,
            "RTL arrow did not use logical order/current selection");
    click(fixture, number);
    require(calls == 3 && host.radio_group_value(group) == RadioSelection{42.0}, "number click failed");
    const auto bounds = fixture.nodes.require(number.node).bounds;
    fixture.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                         bounds.x + bounds.width / 2, bounds.y + bounds.height / 2});
    require(fixture.services.pointer().state(PointerIdentity::mouse())->capture.has_value(), "capture did not start");
    options.set(std::vector<RadioOption>{{false, String{u8"恢复"}}});
    fixture.synchronize();
    require(!fixture.services.pointer().state(PointerIdentity::mouse())->capture &&
                !fixture.services.focus().state().focused && !host.radio_group_value(group) && calls == 3,
            "deletion did not clear focus/capture/value silently");
    options.set(std::vector<RadioOption>{});
    fixture.synchronize();
    require(host.mounted().size() == 2, "empty options leaked members");
    options.set(std::vector<RadioOption>{{true, String{u8"恢复"}}});
    fixture.synchronize();
    click(fixture, host.mounted().back());
    require(host.radio_group_value(group) == RadioSelection{true} && calls == 4, "empty group did not recover");
}

void controlled_manual_and_callbacks() {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    Signal<RadioSelection> selected{RadioSelection{String{u8"a"}}};
    Signal<bool> disabled{false};
    std::vector<int> order;
    int labels{};
    host.mount(Content{[&] {
        RadioGroup(
            RadioGroupProps{}.selection(selected).disabled(disabled).onValueChange([&](const RadioValue& value) {
                require(value == RadioValue{false}, "group candidate differed");
                order.push_back(2);
            }),
            RadioGroupContent{[&] {
                Flex(FlexProps{}, FlexContent{[&] {
                         Radio(RadioProps{}.value(String{u8"a"}), RadioLabel{[&] {
                                   ++labels;
                                   Text(u8"A");
                               }});
                         Radio(RadioProps{}
                                   .value(false)
                                   .onChange([&](bool value) {
                                       require(value, "Radio member unchecked");
                                       order.push_back(1);
                                   })
                                   .onClick([&](bool value) {
                                       require(value, "Radio click candidate differed");
                                       order.push_back(3);
                                   }),
                               RadioLabel{[] { Text(u8"Bool"); }});
                     }});
                RadioGroup(
                    RadioGroupProps{}.options({{String{u8"a"}, String{u8"Nested"}}}).defaultValue(String{u8"a"}));
            }});
    }});
    fixture.synchronize();
    const auto a = host.mounted()[0];
    const auto boolean = host.mounted()[1];
    const auto nested = host.mounted()[2];
    click(fixture, boolean);
    require(order == std::vector<int>{1, 2, 3} && host.snapshot(a.component).checked &&
                !host.snapshot(boolean.component).checked && host.snapshot(nested.component).checked,
            "controlled candidate order or nearest group failed");
    selected.set(RadioSelection{false});
    require(host.snapshot(boolean.component).checked && !host.snapshot(a.component).checked && labels == 1 &&
                order.size() == 3,
            "external echo remounted/repeated");
    click(fixture, boolean);
    require(order == std::vector<int>{1, 2, 3, 3}, "checked Radio did not emit click only");
    disabled.set(true);
    require(host.snapshot(a.component).disabled && host.snapshot(boolean.component).disabled &&
                !host.snapshot(nested.component).disabled,
            "nested disabled isolation failed");
    disabled.set(false);
    selected.set(RadioSelection{String{u8"a"}});
    fixture.services.focus().request_focus(a.interaction, FocusModality::keyboard);
    key(fixture, Key::right);
    require(fixture.services.focus().state().focused == boolean.interaction && host.snapshot(a.component).checked,
            "controlled navigation did not retain authority/move focus");
}

void references_and_destruction() {
    RadioRef reference;
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    Signal<bool> disabled{false};
    int changes{};
    int clicks{};
    runtime::ComponentId id;
    host.mount(Content{[&] {
        Radio(RadioProps{}
                  .ref(reference)
                  .autoFocus(true)
                  .disabled(disabled)
                  .onChange([&](bool value) {
                      require(value, "standalone candidate false");
                      ++changes;
                      require(fixture.services.destroy(id), "self destroy failed");
                  })
                  .onClick([&](bool value) {
                      require(value, "copied click false");
                      ++clicks;
                  }));
    }});
    fixture.synchronize();
    id = host.mounted().front().component;
    require(reference.bound() && fixture.services.focus().state().focused == host.mounted().front().interaction,
            "Radio autoFocus/ref failed");
    require(reference.blur() && !reference.blur() && reference.focus(), "ref focus/blur failed");
    bool wrong_thread{};
    std::thread thread{[&] {
        try {
            static_cast<void>(reference.bound());
        } catch (const std::logic_error&) {
            wrong_thread = true;
        }
    }};
    thread.join();
    require(wrong_thread, "ref accepted wrong thread");
    disabled.set(true);
    require(!reference.focus() && !fixture.services.focus().state().focused, "disabled ref retained focus");
    disabled.set(false);
    require(reference.focus(), "ref did not recover");
    key(fixture, Key::space);
    key(fixture, Key::space, KeyAction::down, true);
    key(fixture, Key::space, KeyAction::up);
    require(changes == 1 && clicks == 1 && !reference.bound() && !reference.focus() && host.mounted().empty(),
            "self destroy callback/ref cleanup failed");
    Fixture recovery;
    detail::SelectionComponentHost recovery_host{recovery.services};
    recovery_host.mount(Content{[&] { Radio(RadioProps{}.ref(reference)); }});
    recovery.synchronize();
    require(reference.bound() && reference.focus(), "ref could not rebind after destroy");
}

template <class Declare> void rejected(Declare declare) {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    bool failed{};
    try {
        host.mount(Content{declare});
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    require(failed && host.mounted().empty() && fixture.services.interactions().size() == 0,
            "invalid Radio configuration leaked resources");
}

void invalid_and_reactive_rollback() {
    rejected([] { RadioGroup(RadioGroupProps{}.options({{1.0, String{u8"A"}}, {1.0, String{u8"B"}}})); });
    rejected([] { RadioGroup(RadioGroupProps{}.options({{std::numeric_limits<double>::infinity(), String{u8"A"}}})); });
    rejected([] { RadioGroup(RadioGroupProps{}.value(std::optional<String>{}).selection(RadioSelection{})); });
    rejected([] { RadioGroup(RadioGroupProps{}.selection(RadioSelection{}).defaultValue(false)); });
    rejected(
        [] { RadioGroup(RadioGroupProps{}.options({}), RadioGroupContent{[] { Radio(RadioProps{}.value(false)); }}); });
    rejected([] { RadioGroup(RadioGroupProps{}, RadioGroupContent{[] { Radio(RadioProps{}); }}); });
    rejected([] {
        RadioGroup(RadioGroupProps{}, RadioGroupContent{[] {
                       Radio(RadioProps{}.value(false));
                       Radio(RadioProps{}.value(false));
                   }});
    });
    rejected([] { Radio(RadioProps{}, RadioLabel{[] { Button(ButtonProps{}, [] { Text(u8"Interactive"); }); }}); });
    rejected([] {
        RadioRef ref;
        Radio(RadioProps{}.ref(ref));
        Radio(RadioProps{}.ref(ref));
    });
    std::vector<RadioOption> excessive;
    for (std::size_t i = 0; i < 1025; ++i) {
        excessive.push_back({double(i), String{u8"A"}});
    }
    rejected([&] { RadioGroup(RadioGroupProps{}.options(excessive)); });
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    Signal<std::vector<RadioOption>> options{std::vector<RadioOption>{{1.0, String{u8"A"}}}};
    host.mount(Content{[&] { RadioGroup(RadioGroupProps{}.options(options).defaultValue(1.0)); }});
    fixture.synchronize();
    const auto item = host.mounted().front();
    bool failed{};
    try {
        options.set(std::vector<RadioOption>{{2.0, String{u8"B"}}, {2.0, String{u8"C"}}});
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    require(failed && host.mounted().front().component == item.component && host.snapshot(item.component).checked,
            "invalid reactive options changed mounted state");
    options.set(std::vector<RadioOption>{{1.0, String{u8"A"}}, {false, String{u8"B"}}});
    fixture.synchronize();
    require(host.mounted().size() == 2 && host.mounted().front().component == item.component,
            "rollback did not recover");
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.02F;
}

std::array<float, 3> effect_color_at(Fixture& fixture, runtime::Point point, float scale = 1) {
    std::array<float, 3> result{1, 1, 1};
    const auto instances = fixture.services.surfaces().effects().packed_instances();
    for (const auto command : fixture.services.scene_composer().ordered_scene().commands()) {
        if (command.kind != graphics::SceneDrawKind::rounded_effect) {
            continue;
        }
        for (std::uint32_t i = 0; i < command.instance_count; ++i) {
            const auto& effect = instances[command.first_instance + i];
            if (effect.geometry.ancestor_clip) {
                const auto clip = effect.geometry.ancestor_clip->bounds;
                if (point.x < clip.x || point.y < clip.y || point.x >= clip.x + clip.width ||
                    point.y >= clip.y + clip.height) {
                    continue;
                }
            }
            const float alpha = graphics::rounded_effect_coverage(point, effect, 1 / scale) * effect.material.opacity *
                                effect.material.color.alpha();
            const std::array color{effect.material.color.red(), effect.material.color.green(),
                                   effect.material.color.blue()};
            for (std::size_t channel = 0; channel < result.size(); ++channel) {
                result[channel] = color[channel] * alpha + result[channel] * (1 - alpha);
            }
        }
    }
    return result;
}

void buttons_and_local_theme() {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    ThemeConfig config;
    Signal<ThemeConfig> theme{config};
    Signal<RadioSize> size{RadioSize::Middle};
    Signal<RadioOptionType> type{RadioOptionType::Button};
    Signal<RadioButtonStyle> style{RadioButtonStyle::Outline};
    Signal<RadioGroupOrientation> orientation{RadioGroupOrientation::Horizontal};
    Signal<RadioDirection> direction{RadioDirection::LeftToRight};
    Signal<bool> block{false};
    int labels{};
    fixture.buttons.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                           RadioGroup(RadioGroupProps{}
                                          .size(size)
                                          .optionType(type)
                                          .buttonStyle(style)
                                          .orientation(orientation)
                                          .direction(direction)
                                          .block(block)
                                          .defaultValue(1.0),
                                      RadioGroupContent{[&] {
                                          for (double value : {1.0, 2.0, 3.0}) {
                                              Radio(RadioProps{}.value(value).disabled(value == 3.0), RadioLabel{[&] {
                                                        ++labels;
                                                        Text(u8"选项");
                                                    }});
                                          }
                                      }});
                           Switch(SwitchProps{});
                           RadioButton(RadioProps{}.defaultChecked(true), RadioLabel{[] { Text(u8"Standalone"); }});
                       }});
              }});
    }});
    fixture.synchronize(420);
    const auto first = host.mounted()[0];
    const auto middle = host.mounted()[1];
    const auto last = host.mounted()[2];
    const auto switch_item = host.mounted()[3];
    const auto standalone = host.mounted()[4];
    require(host.snapshot(first.component).radio_button && host.snapshot(standalone.component).radio_button &&
                labels == 3,
            "button appearance or labels missing");
    require(host.snapshot(first.component).rounded_corners == std::array{true, false, false, true} &&
                host.snapshot(middle.component).rounded_corners == std::array{false, false, false, false} &&
                host.snapshot(last.component).rounded_corners == std::array{false, true, true, false},
            "horizontal buttons did not join corners");
    auto a = fixture.nodes.require(first.node).bounds;
    auto b = fixture.nodes.require(middle.node).bounds;
    require(near(a.height, 32) && near(b.x, a.x + a.width - 1), "button height or single shared edge mismatch");
    const auto seam = effect_color_at(fixture, {b.x + 0.5F, b.y + b.height / 2});
    const auto primary = resolve_theme().radio().primary;
    require(near(seam[0], primary.red()) && near(seam[1], primary.green()) && near(seam[2], primary.blue()),
            "unchecked neighbor overwrote selected shared border");
    const auto range = host.snapshot(first.component).button_range;
    require(range.valid(), "button retained effect range missing");
    const auto mount_runs = fixture.services.components().mount_runs();
    const auto surface_creates = fixture.services.surfaces().diagnostics().creates;
    const auto label_scene = fixture.services.text().mounted_texts().front().scene;
    const auto reshapes = fixture.scene.text_state(label_scene).counters().shape_count;
    const auto measures = fixture.nodes.require(first.node).measure_count;
    fixture.services.surfaces().instances().clear_dirty_ranges();
    config.radio.tokens.primary = Color::rgba8(114, 46, 209);
    config.radio.tokens.button_solid_checked_background = Color::rgba8(114, 46, 209);
    theme.set(config);
    fixture.synchronize(420);
    require(labels == 3 && fixture.services.components().mount_runs() == mount_runs &&
                fixture.scene.text_state(label_scene).counters().shape_count == reshapes &&
                fixture.nodes.require(first.node).measure_count == measures &&
                fixture.services.surfaces().diagnostics().creates == surface_creates &&
                host.snapshot(first.component).button_range == range,
            "Radio recolor reshaped/remounted/recreated");
    const auto switch_range = fixture.services.surfaces().visual_range(switch_item.surface);
    for (const auto dirty : fixture.services.surfaces().instances().material_dirty_ranges()) {
        require(dirty.first + dirty.count <= switch_range.first ||
                    dirty.first >= switch_range.first + switch_range.count,
                "Radio token dirtied Switch material");
    }
    style.set(RadioButtonStyle::Solid);
    size.set(RadioSize::Large);
    block.set(true);
    fixture.synchronize(420);
    a = fixture.nodes.require(first.node).bounds;
    b = fixture.nodes.require(middle.node).bounds;
    const auto c = fixture.nodes.require(last.node).bounds;
    require(near(a.height, 40) && near(a.width, b.width) && near(b.width, c.width) && near(c.x + c.width - a.x, 420),
            "large block buttons did not distribute width");
    direction.set(RadioDirection::RightToLeft);
    fixture.synchronize(420);
    require(fixture.nodes.require(first.node).bounds.x > fixture.nodes.require(last.node).bounds.x &&
                host.snapshot(first.component).rounded_corners == std::array{false, true, true, false},
            "RTL joined buttons did not reverse position/corners");
    orientation.set(RadioGroupOrientation::Vertical);
    size.set(RadioSize::Small);
    fixture.synchronize(420);
    a = fixture.nodes.require(first.node).bounds;
    b = fixture.nodes.require(middle.node).bounds;
    require(near(a.height, 24) && near(a.width, 420) && near(b.y, a.y + a.height - 1) &&
                host.snapshot(first.component).rounded_corners == std::array{true, true, false, false} &&
                host.snapshot(last.component).rounded_corners == std::array{false, false, true, true},
            "vertical small block corners/edge mismatch");
    type.set(RadioOptionType::Default);
    fixture.synchronize(420);
    require(!host.snapshot(first.component).radio_button && host.snapshot(standalone.component).radio_button &&
                host.mounted()[0].surface == first.surface && labels == 3,
            "option type remounted/changed standalone button");
    for (const auto algorithm : {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
        config.algorithms = {algorithm};
        theme.set(config);
        fixture.synchronize(420);
        require(labels == 3 && host.mounted()[0].component == first.component, "theme algorithm replaced Radio");
    }
}

void dynamic_button_edges() {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    Signal<std::vector<RadioOption>> options{std::vector<RadioOption>{
        {String{u8"a"}, String{u8"A"}}, {String{u8"b"}, String{u8"B"}}, {String{u8"c"}, String{u8"C"}}}};
    host.mount(Content{[&] { RadioGroup(RadioGroupProps{}.options(options).optionType(RadioOptionType::Button)); }});
    fixture.synchronize();
    const auto middle = host.mounted()[1];
    const auto last = host.mounted()[2];
    const auto range = host.snapshot(middle.component).button_range;
    options.set(std::vector<RadioOption>{{String{u8"b"}, String{u8"B"}}, {String{u8"c"}, String{u8"C"}}});
    fixture.synchronize();
    require(host.snapshot(middle.component).rounded_corners == std::array{true, false, false, true} &&
                host.snapshot(last.component).rounded_corners == std::array{false, true, true, false} &&
                host.snapshot(middle.component).button_range == range,
            "dynamic removal did not preserve identity/restore first edge");
    fixture.services.destroy(last.component);
    fixture.synchronize();
    require(host.snapshot(middle.component).rounded_corners == std::array{true, true, true, true},
            "manual deletion did not restore standalone corners");
    options.set(std::vector<RadioOption>{});
    fixture.synchronize();
    require(host.mounted().empty() && fixture.services.surfaces().effects().live_count() == 0,
            "empty button options retained effects");
}

void solid_fill_has_no_inner_frame() {
    for (const auto algorithm : {ThemeAlgorithm::Default, ThemeAlgorithm::Dark}) {
        for (const auto direction : {RadioDirection::LeftToRight, RadioDirection::RightToLeft}) {
            for (const auto orientation : {RadioGroupOrientation::Horizontal, RadioGroupOrientation::Vertical}) {
                Fixture f;
                detail::SelectionComponentHost host{f.services};
                ThemeConfig config;
                config.algorithms = {algorithm};
                const auto primary = resolve_theme(config).radio().button_solid_checked_background;
                host.mount(Content{[&] {
                    Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                              RadioGroup(RadioGroupProps{}
                                             .options({{1.0, String{u8"A"}}, {2.0, String{u8"B"}}})
                                             .defaultValue(1.0)
                                             .optionType(RadioOptionType::Button)
                                             .buttonStyle(RadioButtonStyle::Solid)
                                             .direction(direction)
                                             .orientation(orientation));
                          }});
                }});
                f.synchronize();
                const auto item = host.mounted().front();
                const auto rect = f.nodes.require(item.node).bounds;
                const auto corners = host.snapshot(item.component).rounded_corners;
                const auto radius = resolve_theme(config).radio().button_radius;
                for (const float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
                    std::size_t samples{};
                    for (float y = rect.y + 0.5F / scale; y < rect.y + rect.height; y += 0.25F / scale) {
                        for (float x = rect.x + 0.5F / scale; x < rect.x + rect.width; x += 0.25F / scale) {
                            const auto corner = y < rect.y + rect.height / 2 ? (x < rect.x + rect.width / 2 ? 0 : 1)
                                                                             : (x < rect.x + rect.width / 2 ? 3 : 2);
                            if (graphics::rounded_rect_signed_distance({x, y}, {rect, corners[corner] ? radius : 0}) >
                                -0.5F / scale) {
                                continue;
                            }
                            const auto color = effect_color_at(f, {x, y}, scale);
                            require(std::abs(color[0] - primary.red()) < 0.0001F &&
                                        std::abs(color[1] - primary.green()) < 0.0001F &&
                                        std::abs(color[2] - primary.blue()) < 0.0001F,
                                    "Solid RadioButton exposed an inner frame or quadrant seam");
                            ++samples;
                        }
                    }
                    require(samples > 100, "Solid RadioButton fill regression missed its interior");
                }
            }
        }
    }
}

void radio_tokens_and_corner_math() {
    ThemeConfig config;
    const auto baseline = resolve_theme(config);
    require(near(baseline.radio().size, 16) && near(baseline.radio().dot_size, 6) &&
                near(baseline.radio().button_padding_inline, 15),
            "locked Radio token derivation mismatch");
    config.seed.wireframe = true;
    const auto wire = resolve_theme(config);
    require(near(wire.radio().dot_size, 8) && wire.radio().dot == wire.map().color_primary &&
                wire.radio().checked_background == wire.alias().color_background_container,
            "wireframe mapping mismatch");
    config = {};
    config.radio.tokens.size = dp(20);
    config.radio.tokens.dot_size = dp(8);
    config.radio.tokens.button_padding_inline = dp(19);
    const auto overridden = resolve_theme(config);
    require(overridden.identity() != baseline.identity() &&
                overridden.diagnostic_json().find("\"radio\"") != std::string::npos,
            "Radio missing hash/JSON");
    require(resolve_theme(ThemeConfig{}, &overridden).radio() == overridden.radio(), "Radio inheritance failed");
    config.radio.algorithm = true;
    config.radio.seed.color_primary = Color::rgba8(114, 46, 209);
    require(resolve_theme(config).radio().primary != baseline.radio().primary &&
                resolve_theme(config).switch_token() == baseline.switch_token(),
            "Radio algorithm leaked to siblings");
    for (int invalid = 0; invalid < 4; ++invalid) {
        ThemeConfig bad;
        if (invalid == 0) {
            bad.radio.tokens.dot_size = dp(30);
        }
        if (invalid == 1) {
            bad.radio.tokens.line_width = dp(20);
        }
        if (invalid == 2) {
            bad.radio.tokens.wave_opacity = 1.1F;
        }
        if (invalid == 3) {
            bad.radio.tokens.button_height_small = dp(0);
        }
        bool failed{};
        try {
            static_cast<void>(resolve_theme(bad));
        } catch (const std::invalid_argument&) {
            failed = true;
        }
        require(failed, "invalid Radio token accepted");
    }
    const graphics::LogicalRoundedRect shape{{10, 20, 100, 40}, 6};
    const auto outlines = graphics::make_corner_outline_effects(shape, {true, false, false, true}, 2, 1,
                                                                Color::rgba8(22, 119, 255), 1, {5, 7});
    require(outlines[0].geometry.shape.radius == 6 && outlines[1].geometry.shape.radius == 0 &&
                outlines[0].geometry.ancestor_clip->bounds.x + outlines[0].geometry.ancestor_clip->bounds.width == 65 &&
                outlines[1].geometry.ancestor_clip->bounds.x == 65,
            "corner clips/radii mismatch");
    const auto fill =
        graphics::make_corner_fill_effects(shape, {true, false, false, true}, Color::rgba8(255, 255, 255));
    require(graphics::rounded_effect_coverage({10.5F, 20.5F}, fill[0]) == 0 &&
                graphics::rounded_effect_coverage({109.5F, 20.5F}, fill[1]) == 1,
            "joined fill did not square only inner corners");
}

void constrained_button_geometry() {
    Fixture fixture;
    detail::SelectionComponentHost host{fixture.services};
    Signal<LogicalLength> width{dp(0)};
    host.mount(Content{[&] {
        RadioButton(RadioProps{}.layout(LayoutStyle{}.width(width)), RadioLabel{[] { Text(u8"Constrained"); }});
    }});
    fixture.synchronize();
    const auto item = host.mounted().front();
    require(fixture.nodes.require(item.node).bounds.width == 0, "button ignored zero-width constraint");
    width.set(dp(4));
    fixture.synchronize();
    fixture.services.focus().request_focus(item.interaction, FocusModality::keyboard);
    require(host.snapshot(item.component).focus.focus_visible, "narrow button lost focus");
}

void finite_feedback() {
    for (const bool button : {false, true}) {
        Fixture fixture;
        detail::SelectionComponentHost host{fixture.services};
        ThemeConfig config;
        Signal<ThemeConfig> theme{config};
        Signal<bool> disabled{false};
        Signal<bool> wave{true};
        int labels{};
        fixture.buttons.mount(Content{[&] {
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      const auto props = RadioProps{}.checked(false).disabled(disabled).wave(wave);
                      const auto label = RadioLabel{[&] {
                          ++labels;
                          Text(u8"Wave 中文");
                      }};
                      if (button) {
                          RadioButton(props, label);
                      } else {
                          Radio(props, label);
                      }
                  }});
        }});
        fixture.synchronize();
        fixture.services.set_motion_preference(animation::MotionPreference::normal);
        const auto item = host.mounted().front();
        fixture.services.focus().request_focus(item.interaction, FocusModality::keyboard);
        fixture.synchronize();
        const auto effects = fixture.services.surfaces().effects().live_count();
        const auto activate = [&] {
            key(fixture, Key::space);
            key(fixture, Key::space, KeyAction::up);
        };
        activate();
        require(host.snapshot(item.component).wave_active && fixture.services.next_frame_deadline() &&
                    fixture.services.surfaces().effects().live_count() == effects + (button ? 4 : 1),
                "Radio wave did not start");
        const auto range = host.snapshot(item.component).wave_range;
        static_cast<void>(fixture.services.tick_animations(animation::AnimationTime::microseconds(100000)));
        fixture.synchronize();
        require(host.snapshot(item.component).wave_progress > 0 && host.snapshot(item.component).wave_progress < 1 &&
                    labels == 1,
                "wave did not advance or remounted label");
        activate();
        require(host.snapshot(item.component).wave_range == range &&
                    near(host.snapshot(item.component).wave_progress, 0),
                "wave restart replaced identity");
        static_cast<void>(fixture.services.tick_animations(animation::AnimationTime::microseconds(1000000)));
        fixture.synchronize();
        require(!host.snapshot(item.component).wave_active && !fixture.services.next_frame_deadline() &&
                    fixture.services.surfaces().effects().live_count() == effects,
                "wave did not finish idle");
        for (int cancel = 0; cancel < 4; ++cancel) {
            if (fixture.services.focus().state().focused != item.interaction) {
                fixture.services.focus().request_focus(item.interaction, FocusModality::keyboard);
            }
            activate();
            require(host.snapshot(item.component).wave_active, "wave cancellation setup failed");
            if (cancel == 0) {
                disabled.set(true);
                disabled.set(false);
            }
            if (cancel == 1) {
                wave.set(false);
                wave.set(true);
            }
            if (cancel == 2) {
                fixture.services.set_window_active(false);
                fixture.services.set_window_active(true);
            }
            if (cancel == 3) {
                config.seed.motion = false;
                theme.set(config);
            }
            require(!host.snapshot(item.component).wave_active && !fixture.services.next_frame_deadline(),
                    "Radio cancellation retained wave/deadline");
        }
        config.seed.motion = true;
        theme.set(config);
        activate();
        fixture.services.set_motion_preference(animation::MotionPreference::reduced);
        require(!host.snapshot(item.component).wave_active && !fixture.services.next_frame_deadline(),
                "reduced motion retained wave");
        fixture.services.set_motion_preference(animation::MotionPreference::normal);
        activate();
        fixture.services.destroy(item.component);
        require(!fixture.services.next_frame_deadline() &&
                    fixture.services.surfaces().effects().live_count() == effects - (button ? 13 : 1),
                "Radio destruction retained effects/deadline");
    }
}
} // namespace

int main() {
    try {
        dynamic_and_keyboard();
        controlled_manual_and_callbacks();
        references_and_destruction();
        invalid_and_reactive_rollback();
        buttons_and_local_theme();
        dynamic_button_edges();
        solid_fill_has_no_inner_frame();
        radio_tokens_and_corner_math();
        constrained_button_geometry();
        finite_feedback();
        std::cout << "radio features tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "radio features tests failed: " << error.what() << '\n';
        return 1;
    }
}
