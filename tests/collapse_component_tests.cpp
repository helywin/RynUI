#include "component/collapse_component.hpp"
#include "component/feedback_component.hpp"
#include "component/modal_component.hpp"
#include "component/select_component.hpp"
#include "support/input_fixture.hpp"
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

void sync(Fixture& f) {
    check(f.services.layout_and_synchronize({500, 1200}, {0, 0, 500, 1200}), "Collapse layout failed");
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
    throw std::runtime_error("Collapse header has no interaction");
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, r.x + 3, r.y + r.height / 2});
    }
    sync(f);
}

void key(Fixture& f, Key value, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({value, KeyAction::down, modifiers});
    f.services.focus().dispatch({value, KeyAction::up, modifiers});
    sync(f);
}

CollapsePanel plain(String key) {
    return {key, CollapseTitle{[key] { Text(TextProps{}.content(key)); }}, CollapseBody{[] { Text(u8"Details"); }}};
}

IconSource arrow_source(Fixture& f, runtime::NodeId node) {
    for (const auto& text : f.services.text().mounted_texts()) {
        if (f.services.components().root(text.component) == node) {
            check(f.scene.primitive(text.scene).instances.count > 0, "Collapse arrow has no visible glyphs");
            return f.services.text().icon_snapshot(text.component).source;
        }
    }
    throw std::runtime_error("Collapse arrow is not retained");
}

void controlled_and_accordion() {
    Fixture f;
    Signal<CollapseKeys> active{CollapseKeys{String{u8"a"}}};
    Signal<bool> accordion{false};
    bool echo{};
    std::vector<CollapseKeys> changes;
    f.services.mount(Content{[&] {
        Collapse(CollapseProps{}.activeKeys(active).accordion(accordion).onChange([&](CollapseKeys keys) {
            changes.push_back(keys);
            if (echo) {
                active.set(keys);
            }
        }),
                 {plain(u8"a"), plain(u8"b"), plain(u8"locked").disabled()});
    }});
    sync(f);
    const auto id = f.services.collapse().mounted()[0];
    const auto initial = f.services.collapse().snapshot(id);
    check(arrow_source(f, initial.panels[0].arrow) == IconSource{IconName::DownOutlined} &&
              arrow_source(f, initial.panels[1].arrow) == IconSource{IconName::RightOutlined},
          "Collapse arrow state drifted");
    click(f, initial.panels[1].header);
    check(f.services.collapse().snapshot(id).active_keys == CollapseKeys{String{u8"a"}} && changes.size() == 1 &&
              changes[0] == CollapseKeys{String{u8"a"}, String{u8"b"}},
          "controlled refusal changed Collapse");
    echo = true;
    click(f, initial.panels[1].header);
    check(f.services.collapse().snapshot(id).panels[0].expanded &&
              f.services.collapse().snapshot(id).panels[1].expanded,
          "multiple Collapse failed");
    accordion.set(true);
    sync(f);
    check(f.services.collapse().snapshot(id).active_keys == CollapseKeys{String{u8"a"}},
          "accordion failed to normalize");
    click(f, initial.panels[1].header);
    check(f.services.collapse().snapshot(id).active_keys == CollapseKeys{String{u8"b"}},
          "accordion kept previous panel");
    key(f, Key::space);
    check(f.services.collapse().snapshot(id).active_keys.empty(), "accordion cannot close last panel");
    check(arrow_source(f, initial.panels[1].arrow) == IconSource{IconName::RightOutlined},
          "closing did not update retained arrow");
    active.set({String{u8"unknown"}, String{u8"locked"}, String{u8"locked"}, String{u8"a"}});
    sync(f);
    check(f.services.collapse().snapshot(id).active_keys == CollapseKeys{String{u8"locked"}} &&
              f.services.collapse().snapshot(id).panels[2].expanded,
          "explicit disabled key lost authority");
    const auto count = changes.size();
    click(f, initial.panels[2].header);
    check(changes.size() == count &&
              !f.services.focus().request_focus(interaction(f, initial.panels[2].header), FocusModality::keyboard),
          "disabled header activated or accepted focus");
}

void retained_drafts_reflow_and_cleanup() {
    Fixture f;
    Signal<CollapseKeys> active{CollapseKeys{String{u8"a"}}};
    int title_runs{};
    int body_runs{};
    int extra_runs{};
    InputRef ref;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true).gap(dp(10)), FlexContent{[&] {
                 Collapse(CollapseProps{}.activeKeys(active),
                          {CollapsePanel{u8"a", CollapseTitle{[&] {
                                             ++title_runs;
                                             Text(u8"Draft");
                                         }},
                                         CollapseBody{[&] {
                                             ++body_runs;
                                             Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                                      Input(InputProps{}.defaultValue(u8"original").ref(ref));
                                                      Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}));
                                                  }});
                                         }}}
                               .extra(CollapseExtra{[&] {
                                   ++extra_runs;
                                   Text(u8"Extra");
                               }}),
                           CollapsePanel{u8"b", CollapseTitle{[] { Text(u8"Second"); }}, CollapseBody{[] {
                                             Input(InputProps{}.autoFocus().defaultValue(u8"hidden"));
                                         }}}});
                 Button(ButtonProps{}, [] { Text(u8"Following"); });
             }});
    }});
    sync(f);
    const auto id = f.services.collapse().mounted()[0];
    const auto original = f.services.collapse().snapshot(id);
    const auto editor = f.inputs.mounted_inputs()[0];
    const auto select = f.services.select().mounted()[0];
    const auto sibling = f.buttons.mounted_buttons()[0].component;
    const auto old_y = bounds(f, sibling).y;
    const auto body_height = bounds(f, original.panels[0].body).height;
    const auto count = f.services.components().component_count();
    check(!f.inputs.sessions().active().valid(), "hidden autofocus started session");
    check(ref.focus({InputFocusCursor::All}), "retained Input ref focus failed");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"保留草稿"}, f.inputs.sessions().active()})),
          "draft commit failed");
    sync(f);
    const auto session = f.inputs.sessions().active();
    active.set({});
    sync(f);
    check(bounds(f, sibling).y == old_y - body_height && !f.services.focus().state().focused &&
              !f.inputs.sessions().active().valid() && !ref.focus() &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, session})),
          "collapsed body retained layout, focus, ref or session");
    active.set({String{u8"a"}});
    sync(f);
    check(bounds(f, sibling).y == old_y && f.inputs.mounted_inputs()[0].editor == editor.editor &&
              f.inputs.editors().require(editor.editor).value() == String{u8"保留草稿"}.bytes(),
          "collapse remounted editor or lost draft");
    click(f, f.services.select().snapshot(select).selector);
    check(f.services.select().snapshot(select).visible, "body Select did not open");
    active.set({});
    sync(f);
    active.set({String{u8"a"}});
    sync(f);
    check(!f.services.select().snapshot(select).visible, "hidden body reopened old popup");
    // Hide during an actual text-selection capture, without a release event.
    const auto r = f.inputs.layout_snapshot(editor.component).viewport;
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, r.x + 4, r.y + r.height / 2});
    check(f.services.pointer().state(PointerIdentity::mouse())->capture == editor.interaction, "Input did not capture");
    active.set({});
    sync(f);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "hidden body retained pointer capture");
    check(title_runs == 1 && body_runs == 1 && extra_runs == 1 && f.services.components().component_count() == count &&
              f.services.collapse().snapshot(id).panels[0].body == original.panels[0].body,
          "slots were remounted");
    check(f.services.destroy(id), "Collapse destroy failed");
    sync(f);
    check(f.inputs.mounted_inputs().empty() && f.services.select().mounted().empty() &&
              f.services.collapse().mounted().empty() && f.services.interactions().size() == 1,
          "Collapse leaked descendants");
}

void keyboard_extra_and_disabled_capture() {
    Fixture f;
    Signal<bool> disabled{false};
    int extra_actions{};
    f.services.mount(Content{[&] {
        Collapse(CollapseProps{},
                 {plain(u8"a"), plain(u8"b").disabled(disabled), plain(u8"locked").disabled().extra(CollapseExtra{[&] {
                      Button(ButtonProps{}.onClick([&] { ++extra_actions; }), [] { Text(u8"Action"); });
                  }}),
                  plain(u8"c")});
    }});
    sync(f);
    const auto id = f.services.collapse().mounted()[0];
    const auto initial = f.services.collapse().snapshot(id);
    check(f.services.focus().request_focus(interaction(f, initial.panels[0].header), FocusModality::keyboard),
          "header focus failed");
    key(f, Key::down);
    check(f.services.focus().state().focused == interaction(f, initial.panels[1].header) &&
              f.services.collapse().snapshot(id).active_keys.empty(),
          "Down changed expansion or missed header");
    key(f, Key::down);
    check(f.services.focus().state().focused == interaction(f, initial.panels[3].header), "Down did not skip disabled");
    key(f, Key::home);
    key(f, Key::enter);
    key(f, Key::tab);
    check(f.services.focus().state().focused == interaction(f, initial.panels[1].header), "header Tab order failed");
    key(f, Key::end);
    key(f, Key::space);
    check(f.services.collapse().snapshot(id).active_keys.size() == 2, "keyboard multi expansion failed");
    click(f, f.buttons.mounted_buttons()[0].component);
    key(f, Key::enter);
    check(extra_actions == 2 && f.services.collapse().snapshot(id).active_keys.size() == 2,
          "disabled panel extra was disabled or toggled header");
    const auto r = bounds(f, initial.panels[1].header);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, r.x + 3, r.y + 3});
    check(f.services.pointer().state(PointerIdentity::mouse())->capture == interaction(f, initial.panels[1].header),
          "header capture failed");
    disabled.set(true);
    sync(f);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, r.x + 3, r.y + 3});
    sync(f);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture &&
              f.services.collapse().snapshot(id).active_keys.size() == 2,
          "disabled captured header activated");
}

void logical_modal_and_spin_boundaries() {
    Fixture f;
    Signal<bool> modal{false};
    Signal<bool> spinning{false};
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(spinning), SpinContent{[] { Collapse(CollapseProps{}, {plain(u8"outside")}); }});
        Modal(ModalProps{}.open(modal).footer(ModalFooterMode::None),
              ModalContent{[] { Collapse(CollapseProps{}, {plain(u8"inside")}); }});
    }});
    sync(f);
    const auto outside = f.services.collapse().mounted()[0];
    const auto inside = f.services.collapse().mounted()[1];
    const auto out_header = f.services.collapse().snapshot(outside).panels[0].header;
    const auto in_header = f.services.collapse().snapshot(inside).panels[0].header;
    check(f.services.focus().request_focus(interaction(f, out_header), FocusModality::keyboard),
          "outside focus failed");
    spinning.set(true);
    sync(f);
    key(f, Key::enter);
    click(f, out_header);
    check(f.services.collapse().snapshot(outside).active_keys.empty() &&
              !f.services.focus().request_focus(interaction(f, out_header), FocusModality::keyboard),
          "Spin allowed Collapse activation");
    spinning.set(false);
    sync(f);
    modal.set(true);
    sync(f);
    click(f, out_header);
    check(f.services.collapse().snapshot(outside).active_keys.empty() &&
              !f.services.focus().request_focus(interaction(f, out_header), FocusModality::keyboard),
          "Modal allowed background Collapse");
    static_cast<void>(f.services.focus().request_focus(interaction(f, in_header), FocusModality::keyboard));
    check(f.services.focus().state().focused == interaction(f, in_header), "Modal inner Collapse blocked");
    key(f, Key::space);
    check(f.services.collapse().snapshot(inside).panels[0].expanded, "Modal inner Collapse keyboard failed");
    modal.set(false);
    sync(f);
    click(f, out_header);
    check(f.services.collapse().snapshot(outside).panels[0].expanded, "Collapse did not recover after boundary");
}

void theme_sizes_and_retained_material() {
    Fixture f;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<ControlSize> size{ControlSize::Middle};
    int runs{};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Collapse(CollapseProps{}.size(size).defaultActiveKeys({String{u8"a"}}),
                           {CollapsePanel{u8"a", CollapseTitle{[] { Text(u8"Title"); }}, CollapseBody{[&] {
                                              ++runs;
                                              Text(u8"Body");
                                          }}}});
              }});
    }});
    sync(f);
    const auto id = f.services.collapse().mounted()[0];
    const auto original = f.services.collapse().snapshot(id);
    for (const auto value : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
        size.set(value);
        sync(f);
        const float expected = value == ControlSize::Small ? 38.0F : value == ControlSize::Middle ? 46.0F : 56.0F;
        check(bounds(f, original.panels[0].header).height == expected, "Collapse size recipe failed");
    }
    const auto node = f.services.components().root(id);
    const auto measures = f.nodes.require(node).measure_count;
    const auto effects = f.services.rounded_effects().diagnostics();
    ThemeConfig custom;
    const auto red = Color::rgba8(120, 20, 30);
    custom.collapse.tokens.header_background = red;
    theme.set(custom);
    sync(f);
    check(f.nodes.require(node).measure_count == measures && runs == 1 &&
              f.services.rounded_effects().diagnostics().material_updates > effects.material_updates &&
              std::ranges::any_of(
                  f.services.rounded_effects().packed_instances(),
                  [&](const auto& effect) { return effect.material.color == red && effect.material.opacity > 0; }),
          "Collapse Theme material update remeasured or omitted scene");
    custom.collapse.tokens.header_block_large = dp(20);
    theme.set(custom);
    sync(f);
    check(bounds(f, original.panels[0].header).height == 64 && runs == 1 &&
              f.services.collapse().snapshot(id).panels[0].arrow == original.panels[0].arrow,
          "Collapse metrics remounted slots");
    const auto before = f.services.rounded_effects().diagnostics();
    static_cast<void>(f.frames.consume_request());
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline() &&
              f.services.rounded_effects().diagnostics().geometry_updates == before.geometry_updates &&
              f.services.rounded_effects().diagnostics().material_updates == before.material_updates,
          "Collapse idle scheduled work");
}

void callbacks_and_validation() {
    for (int mode = 0; mode < 3; ++mode) {
        Fixture f;
        runtime::ComponentId owner;
        Signal<CollapseKeys> active{CollapseKeys{String{u8"a"}}};
        bool retire{};
        f.services.mount(Content{[&] {
            CollapseProps props;
            if (mode != 0) {
                props.activeKeys(active);
            }
            props.onChange([&](CollapseKeys) { static_cast<void>(f.services.destroy(owner)); });
            Collapse(props, {CollapsePanel{u8"a", CollapseTitle{[] { Text(u8"A"); }}, CollapseBody{[&] {
                                               Input(InputProps{}.onBlur([&] {
                                                   if (retire) {
                                                       static_cast<void>(f.services.destroy(owner));
                                                   }
                                               }));
                                           }}}});
        }});
        sync(f);
        owner = f.services.collapse().mounted()[0];
        if (mode == 2) {
            check(f.services.focus().request_focus(f.inputs.mounted_inputs()[0].interaction, FocusModality::keyboard),
                  "blur fixture focus failed");
            retire = true;
            active.set({});
            sync(f);
        } else {
            click(f, f.services.collapse().snapshot(owner).panels[0].header);
        }
        check(f.services.collapse().mounted().empty() && f.inputs.mounted_inputs().empty() &&
                  f.services.interactions().size() == 0 && !f.inputs.sessions().active().valid() &&
                  f.services.rounded_effects().live_count() == 0,
              "callback disposal retained Collapse resources");
    }
    for (int mode = 0; mode < 4; ++mode) {
        Fixture f;
        int runs{};
        bool rejected{};
        try {
            f.services.mount(Content{[&] {
                CollapseProps props;
                if (mode == 0) {
                    props.size(static_cast<ControlSize>(99));
                }
                if (mode == 1) {
                    props.activeKeys(CollapseKeys{}).defaultActiveKeys({});
                }
                Collapse(props, {CollapsePanel{mode == 2 ? String{} : String{u8"a"}, CollapseTitle{[&] { ++runs; }},
                                               CollapseBody{[] {}}},
                                 plain(mode == 3 ? String{u8"a"} : String{u8"b"})});
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && runs == 0 && f.nodes.size() == 0 && f.services.collapse().mounted().empty(),
              "invalid Collapse partially mounted");
    }
    Fixture empty;
    empty.services.mount(Content{[] { Collapse(CollapseProps{}, {}); }});
    sync(empty);
    check(empty.services.interactions().size() == 0, "empty Collapse has interactions");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        controlled_and_accordion();
        retained_drafts_reflow_and_cleanup();
        keyboard_extra_and_disabled_capture();
        logical_modal_and_spin_boundaries();
        theme_sizes_and_retained_material();
        callbacks_and_validation();
        std::cout << "Collapse retained component contracts passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
