#include "component/segmented_component.hpp"
#include "support/input_fixture.hpp"

#include <cmath>
#include <iostream>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void sync(Fixture& f, float width = 320) {
    check(f.services.layout_and_synchronize({width, 800}, {0, 0, width, 800}), "Segmented layout failed");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& node = f.nodes.require(f.services.components().root(id));
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}

InteractionId interaction(Fixture& f, runtime::ComponentId id) {
    for (auto hit : f.services.interactions().declaration_order()) {
        if (f.services.interactions().require(hit).component == id) {
            return hit;
        }
    }
    throw std::runtime_error("Segmented item has no interaction");
}

void pointer(Fixture& f, runtime::ComponentId id, PointerAction action) {
    const auto r = bounds(f, id);
    f.services.pointer().dispatch({PointerIdentity::mouse(), action,
                                   action == PointerAction::move ? PointerButton::none : PointerButton::primary,
                                   r.x + r.width / 2, r.y + r.height / 2});
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    }
    sync(f);
}

void key(Fixture& f, Key value, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({value, KeyAction::down, modifiers});
    f.services.focus().dispatch({value, KeyAction::up, modifiers});
    sync(f);
}

std::size_t stops(Fixture& f) {
    return std::ranges::count_if(f.services.interactions().declaration_order(), [&](auto hit) {
        const auto& item = f.services.interactions().require(hit);
        return item.eligible && item.focusable && item.tab_stop && f.services.interactions().branch_active(hit);
    });
}

std::array<float, 4> text_color(Fixture& f, runtime::ComponentId owner) {
    for (const auto& text : f.services.text().mounted_texts()) {
        for (auto current = std::optional{text.component}; current;
             current = f.services.components().parent(*current)) {
            if (*current == owner) {
                const auto range = f.scene.primitive(text.scene).instances;
                if (range.count) {
                    return f.scene.glyph_scene().instances().at(range.first).color;
                }
            }
        }
    }
    throw std::runtime_error("Segmented text has no glyph color");
}

std::vector<SegmentedOption> options() {
    return {{u8"a", u8"A"}, SegmentedOption{u8"locked", u8"Locked"}.disabled(), {u8"b", u8"Longer label"}};
}

void controlled_and_keyboard() {
    Signal<String> value{String{u8"a"}};
    Signal<bool> vertical{false};
    bool echo{};
    std::vector<String> changes;
    Fixture f;
    f.services.mount(Content{[&] {
        Segmented(SegmentedProps{}.value(value).vertical(vertical).onChange([&](String next) {
            changes.push_back(next);
            if (echo) {
                value.set(next);
            }
        }),
                  options());
        Button(ButtonProps{}, [] { Text(u8"Next"); });
    }});
    sync(f);
    const auto id = f.services.segmented().mounted()[0];
    const auto s = f.services.segmented().snapshot(id);
    click(f, s.items[2]);
    check(f.services.segmented().snapshot(id).selected == 0 && changes == std::vector<String>{String{u8"b"}} &&
              f.services.focus().state().focused == interaction(f, s.items[2]) && stops(f) == 2,
          "controlled refusal changed selection or lost one group entry");
    key(f, Key::space);
    key(f, Key::enter);
    check(changes.size() == 3 && f.services.segmented().snapshot(id).selected == 0, "controlled activation failed");
    const auto before = changes.size();
    f.services.focus().dispatch({Key::enter, KeyAction::down, KeyModifier::none, true});
    f.services.focus().dispatch({Key::enter, KeyAction::up});
    check(changes.size() == before, "repeat Enter emitted duplicate activation");
    echo = true;
    key(f, Key::end);
    check(f.services.segmented().snapshot(id).selected == 2, "controlled echo failed");
    key(f, Key::right);
    check(f.services.segmented().snapshot(id).selected == 0, "right did not wrap");
    key(f, Key::right);
    check(f.services.segmented().snapshot(id).selected == 2, "right did not skip disabled");
    key(f, Key::home);
    key(f, Key::left);
    check(f.services.segmented().snapshot(id).selected == 2, "left did not wrap");
    vertical.set(true);
    sync(f);
    key(f, Key::up);
    check(f.services.segmented().snapshot(id).selected == 0, "vertical up failed");
    key(f, Key::down);
    check(f.services.segmented().snapshot(id).selected == 2, "vertical down failed");
    const auto count = changes.size();
    key(f, Key::left);
    check(changes.size() == count, "vertical consumed horizontal navigation");
    key(f, Key::tab);
    check(f.services.focus().state().focused == f.buttons.mounted_buttons()[0].interaction, "Tab did not leave group");
    key(f, Key::tab, KeyModifier::shift);
    check(f.services.focus().state().focused == interaction(f, s.items[2]), "Shift Tab lost group entry");
    value.set(String{u8"unknown"});
    sync(f);
    check(!f.services.segmented().snapshot(id).selected &&
              f.services.segmented().snapshot(id).value == String{u8"unknown"},
          "unknown controlled key lost authority");
    value.set(String{u8"locked"});
    sync(f);
    check(f.services.segmented().snapshot(id).selected == 1 &&
              !f.services.interactions().require(interaction(f, s.items[1])).eligible,
          "disabled explicit key changed");
}

void reactive_identity_and_capture() {
    Signal<bool> disabled{false};
    Signal<bool> all_disabled{false};
    Signal<String> label{String{u8"Initial"}};
    int runs{};
    int changes{};
    Fixture f;
    f.services.mount(Content{[&] {
        ++runs;
        Segmented(SegmentedProps{}.disabled(all_disabled).defaultValue(u8"b").onChange([&](String) { ++changes; }),
                  {SegmentedOption{u8"a", u8"A"}.label(label), SegmentedOption{u8"b", u8"B"}.disabled(disabled)});
    }});
    sync(f);
    const auto id = f.services.segmented().mounted()[0];
    const auto s = f.services.segmented().snapshot(id);
    const auto components = f.services.components().component_count();
    const auto texts = f.scene.size();
    check(s.selected == 1, "defaultValue ignored");
    pointer(f, s.items[1], PointerAction::down);
    check(f.services.pointer().state(PointerIdentity::mouse())->capture == interaction(f, s.items[1]),
          "Segmented did not capture pointer");
    disabled.set(true);
    sync(f);
    pointer(f, s.items[1], PointerAction::up);
    sync(f);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture && !f.services.focus().state().focused &&
              f.services.segmented().snapshot(id).selected == 1 && !changes,
          "disabled captured item activated, changed selection or retained focus");
    label.set(String{u8"Updated / 已更新"});
    sync(f);
    check(runs == 1 && f.services.segmented().snapshot(id).items == s.items &&
              f.services.components().component_count() == components && f.scene.size() == texts &&
              bounds(f, s.items[0]).width > bounds(f, s.items[1]).width,
          "reactive label remounted or did not affect real geometry");
    all_disabled.set(true);
    sync(f);
    click(f, s.items[0]);
    check(!changes && stops(f) == 0, "group disabled did not block activation");
    all_disabled.set(false);
    disabled.set(false);
    sync(f);
    pointer(f, s.items[0], PointerAction::down);
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, 319, 799});
    sync(f);
    check(!changes && f.services.segmented().snapshot(id).selected == 1, "outside release selected item");
    pointer(f, s.items[0], PointerAction::down);
    f.services.set_window_active(false);
    sync(f);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "window blur retained capture");
    f.services.set_window_active(true);
    click(f, s.items[0]);
    check(changes == 1 && f.services.segmented().snapshot(id).selected == 0, "uncontrolled selection failed");
    pointer(f, s.items[1], PointerAction::down);
    check(f.services.destroy(id), "Segmented destroy failed");
    sync(f);
    check(f.services.segmented().mounted().empty() && !f.services.pointer().state(PointerIdentity::mouse())->capture &&
              f.services.interactions().size() == 0 && f.services.rounded_effects().live_count() == 0 &&
              f.nodes.size() == 0 && f.scene.size() == 0,
          "Segmented teardown leaked retained resources");
}

void real_geometry(float scale) {
    Signal<bool> block{false};
    Signal<bool> vertical{false};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<LogicalLength> width{dp(300)};
    Fixture f;
    f.font_scale = scale;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true).layout(LayoutStyle{}.width(width)), FlexContent{[&] {
                 Segmented(SegmentedProps{}.block(block).vertical(vertical).size(size),
                           {{u8"a", u8"A"}, {u8"b", u8"Longer"}, {u8"c", u8"CC"}});
             }});
    }});
    sync(f);
    const auto id = f.services.segmented().mounted()[0];
    const auto s = f.services.segmented().snapshot(id);
    check(bounds(f, s.items[1]).width > bounds(f, s.items[0]).width, "natural widths unexpectedly equal");
    block.set(true);
    for (auto control_size : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
        size.set(control_size);
        sync(f);
        const auto root = bounds(f, id);
        const auto first = bounds(f, s.items[0]);
        const auto last = bounds(f, s.items[2]);
        const float height = control_size == ControlSize::Small   ? 24.0F
                             : control_size == ControlSize::Large ? 40.0F
                                                                  : 32.0F;
        check(std::abs(first.width - (root.width - 4) / 3) < .001F && root.width == 300 &&
                  std::abs(last.x + last.width - (root.x + root.width - 2)) < .001F &&
                  std::abs(root.height - height) < .001F,
              "actual block item bounds or three-size height drifted");
        for (auto item : s.items) {
            check(std::abs(bounds(f, item).width - first.width) < .001F, "placed block items differ in width");
        }
    }
    vertical.set(true);
    sync(f);
    const auto first = bounds(f, s.items[0]);
    const auto second = bounds(f, s.items[1]);
    check(first.width == 296 && second.width == first.width && second.y == first.y + first.height,
          "vertical items did not fill available width or stack");
    vertical.set(false);
    width.set(dp(180));
    sync(f);
    check(std::abs(bounds(f, s.items[0]).width - 176.0F / 3) < .001F, "block resize did not reallocate real bounds");
    width.set(dp(0));
    sync(f);
    check(bounds(f, id).width == 0 && bounds(f, s.items[0]).width == 0, "zero-width geometry escaped allocation");
}

void rich_labels_and_theme_locality() {
    Signal<bool> disabled{false};
    Signal<LogicalLength> width{dp(190)};
    Signal<String> text{String{u8"Wrapped rich label with several words / 多行标签"}};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int rich_runs{};
    int icon_runs{};
    Fixture f;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Segmented(SegmentedProps{}.block().layout(LayoutStyle{}.width(width)),
                            {SegmentedOption{u8"a", u8""}
                                 .content(SegmentedLabel{[&] {
                                     ++rich_runs;
                                     Flex(FlexProps{}.vertical(true),
                                          FlexContent{[&] { Text(TextProps{}.content(text)); }});
                                 }})
                                 .icon(SegmentedIcon{[&] {
                                     ++icon_runs;
                                     Icon(IconProps{}.name(IconName::SearchOutlined));
                                 }}),
                             SegmentedOption{u8"b", u8"B"}.disabled(disabled)});
                  Button(ButtonProps{}, [] { Text(u8"Sibling"); });
              }});
    }});
    sync(f);
    const auto id = f.services.segmented().mounted()[0];
    const auto s = f.services.segmented().snapshot(id);
    const auto initial = bounds(f, s.items[0]);
    check(initial.height > 32 && rich_runs == 1 && icon_runs == 1, "rich label did not wrap beside icon");
    const auto components = f.services.components().component_count();
    const auto effects = f.services.rounded_effects().live_count();
    const auto root = f.services.components().root(id);
    const auto sibling = f.services.components().root(f.buttons.mounted_buttons()[0].component);
    const auto measures = f.nodes.require(root).measure_count;
    const auto sibling_measures = f.nodes.require(sibling).measure_count;
    const auto geometry = f.services.rounded_effects().diagnostics().geometry_updates;
    std::vector<std::uint64_t> shapes;
    for (auto item : f.services.text().mounted_texts()) {
        shapes.push_back(f.scene.text_state(item.scene).counters().shape_count);
    }
    ThemeConfig custom;
    custom.segmented.tokens.selected_background = Color::rgba8(100, 20, 50);
    custom.segmented.tokens.selected = Color::rgba8(245, 240, 230);
    custom.segmented.tokens.hover_background = Color::rgba8(10, 90, 40);
    custom.segmented.tokens.active_background = Color::rgba8(30, 40, 90);
    custom.segmented.tokens.disabled = Color::rgba8(80, 80, 80);
    theme.set(custom);
    sync(f);
    check(f.nodes.require(root).measure_count == measures &&
              f.nodes.require(sibling).measure_count == sibling_measures &&
              f.services.rounded_effects().diagnostics().geometry_updates == geometry,
          "material-only Theme update remeasured or changed geometry");
    std::size_t index = 0;
    for (auto item : f.services.text().mounted_texts()) {
        check(f.scene.text_state(item.scene).counters().shape_count == shapes[index++], "Theme color reshaped text");
    }
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [](const auto& effect) {
                                  return effect.material.opacity > 0 &&
                                         effect.material.color == Color::rgba8(100, 20, 50);
                              }),
          "selected Theme color absent from logical effects");
    check(text_color(f, s.items[0]) == std::array<float, 4>{245.0F / 255, 240.0F / 255, 230.0F / 255, 1},
          "rich label did not inherit selected Theme foreground");
    pointer(f, s.items[1], PointerAction::move);
    sync(f);
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [](const auto& effect) {
                                  return effect.material.opacity > 0 &&
                                         effect.material.color == Color::rgba8(10, 90, 40);
                              }),
          "hover Theme color absent");
    pointer(f, s.items[1], PointerAction::down);
    sync(f);
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [](const auto& effect) {
                                  return effect.material.opacity > 0 &&
                                         effect.material.color == Color::rgba8(30, 40, 90);
                              }),
          "pressed Theme background absent");
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, 319, 799});
    sync(f);
    check(f.services.focus().request_focus(interaction(f, s.items[1]), FocusModality::keyboard),
          "rich item focus failed");
    sync(f);
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [](const auto& effect) {
                                  return effect.material.opacity > 0 &&
                                         effect.geometry.kind == graphics::RoundedEffectKind::outline &&
                                         effect.geometry.outline_width == 3;
                              }),
          "focus-visible ring absent");
    disabled.set(true);
    sync(f);
    check(text_color(f, s.items[1]) == std::array<float, 4>{80.0F / 255, 80.0F / 255, 80.0F / 255, 1} &&
              !f.services.focus().state().focused,
          "disabled Theme foreground/focus did not update");
    text.set(String{u8"Updated label, still retained"});
    width.set(dp(280));
    sync(f);
    check(rich_runs == 1 && icon_runs == 1 && f.services.segmented().snapshot(id).items == s.items &&
              f.services.components().component_count() == components &&
              f.services.rounded_effects().live_count() == effects,
          "rich label/theme/resize rebuilt slots or effects");
    for (auto item : s.items) {
        const auto b = bounds(f, item);
        const auto row = f.services.components().root(item);
        for (auto child : f.nodes.require(row).children) {
            const auto c = f.nodes.require(child).bounds;
            check(c.x >= b.x && c.x + c.width <= b.x + b.width + .001F && c.y >= b.y &&
                      c.y + c.height <= b.y + b.height + .001F,
                  "rich wrapper escaped real item allocation");
        }
    }
    static_cast<void>(f.frames.consume_request());
    const auto stable = f.services.rounded_effects().diagnostics();
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline() &&
              f.services.rounded_effects().diagnostics().material_updates == stable.material_updates &&
              f.services.rounded_effects().diagnostics().geometry_updates == stable.geometry_updates,
          "idle Segmented requested frames or changed retained effects");
}

void modal_and_spin() {
    Signal<bool> open{false};
    Signal<bool> loading{false};
    int outside_changes{};
    int inside_changes{};
    Fixture f;
    f.services.mount(Content{[&] {
        Segmented(SegmentedProps{}.onChange([&](String) { ++outside_changes; }), options());
        Modal(ModalProps{}.open(open).width(300).footer(ModalFooterMode::None), ModalContent{[&] {
                  Spin(SpinProps{}.spinning(loading), SpinContent{[&] {
                           Segmented(SegmentedProps{}.block().onChange([&](String) { ++inside_changes; }), options());
                       }});
              }});
    }});
    sync(f, 640);
    const auto outside = f.services.segmented().mounted()[0];
    const auto inside = f.services.segmented().mounted()[1];
    const auto a = f.services.segmented().snapshot(outside);
    const auto b = f.services.segmented().snapshot(inside);
    check(f.services.focus().request_focus(interaction(f, a.items[0]), FocusModality::keyboard),
          "outside focus failed");
    pointer(f, a.items[2], PointerAction::down);
    open.set(true);
    sync(f, 640);
    pointer(f, a.items[2], PointerAction::up);
    check(!outside_changes && !f.services.interactions().component_active(outside) &&
              !f.services.focus().request_focus(interaction(f, a.items[0]), FocusModality::keyboard) &&
              f.services.interactions().require(interaction(f, a.items[0])).eligible,
          "Modal boundary changed disabled value or admitted background");
    static_cast<void>(f.services.focus().request_focus(interaction(f, b.items[0]), FocusModality::keyboard));
    check(f.services.focus().state().focused == interaction(f, b.items[0]), "Modal child focus blocked");
    key(f, Key::right);
    check(inside_changes == 1 && f.services.segmented().snapshot(inside).selected == 2,
          "Modal Segmented keyboard blocked");
    loading.set(true);
    sync(f, 640);
    check(f.services.focus().state().focused != interaction(f, b.items[0]) &&
              f.services.focus().state().focused != interaction(f, b.items[2]) &&
              !f.services.interactions().component_active(inside),
          "Spin did not clear Segmented focus");
    check(!f.services.segmented().on_keyboard_input({Key::home, KeyAction::down}), "blocked host consumed key");
    pointer(f, b.items[0], PointerAction::down);
    pointer(f, b.items[0], PointerAction::up);
    sync(f, 640);
    check(inside_changes == 1, "Spin admitted Segmented pointer");
    loading.set(false);
    sync(f, 640);
    check(f.services.focus().request_focus(interaction(f, b.items[2]), FocusModality::keyboard),
          "Spin recovery lost focus");
    key(f, Key::home);
    check(inside_changes == 2, "Spin recovery lost keyboard");
    open.set(false);
    sync(f, 640);
    check(f.services.interactions().component_active(outside), "Modal close did not restore background");
}

void lifetime_and_validation() {
    for (int mode = 0; mode < 4; ++mode) {
        Fixture f;
        runtime::ComponentId owner;
        bool retire{};
        f.services.mount(Content{[&] {
            Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                     Segmented(SegmentedProps{}.onChange([&](String) { static_cast<void>(f.services.destroy(owner)); }),
                               {{u8"a", u8"A"}, {u8"b", u8"B"}});
                     if (mode == 3) {
                         Input(InputProps{}.onBlur([&] {
                             if (retire) {
                                 static_cast<void>(f.services.destroy(owner));
                             }
                         }));
                     }
                 }});
        }});
        sync(f);
        const auto id = f.services.segmented().mounted()[0];
        owner = mode == 0 ? id : f.services.components().parent(id).value();
        const auto s = f.services.segmented().snapshot(id);
        if (mode == 2) {
            check(f.services.focus().request_focus(interaction(f, s.items[0]), FocusModality::keyboard),
                  "lifetime focus failed");
            key(f, Key::right);
        } else {
            if (mode == 3) {
                check(
                    f.services.focus().request_focus(f.inputs.mounted_inputs()[0].interaction, FocusModality::keyboard),
                    "blur lifetime focus failed");
                retire = true;
            }
            click(f, s.items[1]);
        }
        check(f.services.segmented().mounted().empty() && f.services.interactions().size() == 0 &&
                  f.services.rounded_effects().live_count() == 0 && !f.services.focus().state().focused,
              "callback destruction leaked resources or focus");
    }
    for (int mode = 0; mode < 4; ++mode) {
        Fixture f;
        int slots{};
        bool rejected{};
        try {
            f.services.mount(Content{[&] {
                SegmentedProps props;
                if (mode == 0) {
                    props.value(u8"a").defaultValue(u8"a");
                } else if (mode == 1) {
                    props.size(static_cast<ControlSize>(99));
                }
                Segmented(props, {SegmentedOption{mode == 2 ? String{} : String{u8"a"}, u8"A"}.content(
                                      SegmentedLabel{[&] { ++slots; }}),
                                  {mode == 3 ? String{u8"a"} : String{u8"b"}, u8"B"}});
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && !slots && f.nodes.size() == 0 && f.services.interactions().size() == 0 &&
                  f.services.segmented().mounted().empty(),
              "invalid Segmented partially mounted");
    }
    Fixture f;
    f.services.mount(Content{[] {
        Segmented(SegmentedProps{}, {});
        Segmented(SegmentedProps{}, {SegmentedOption{u8"a", u8"A"}.disabled(), {u8"b", u8"B"}});
        Segmented(SegmentedProps{}, {SegmentedOption{u8"a", u8"A"}.disabled()});
    }});
    sync(f);
    check(!f.services.segmented().snapshot(f.services.segmented().mounted()[0]).selected &&
              f.services.segmented().snapshot(f.services.segmented().mounted()[1]).selected == 1 &&
              !f.services.segmented().snapshot(f.services.segmented().mounted()[2]).selected && stops(f) == 1,
          "empty/all-disabled/default first-enabled selection failed");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        std::cout << std::unitbuf;
        controlled_and_keyboard();
        std::cout << "controlled/default keyboard focus passed\n";
        reactive_identity_and_capture();
        std::cout << "reactive identity and capture passed\n";
        for (float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
            real_geometry(scale);
        }
        rich_labels_and_theme_locality();
        std::cout << "real geometry, rich labels/icons and Theme locality passed\n";
        modal_and_spin();
        std::cout << "Modal/Spin boundaries passed\n";
        lifetime_and_validation();
        std::cout << "Segmented component contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
