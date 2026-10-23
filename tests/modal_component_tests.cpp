#include "component/modal_component.hpp"
#include "component/card_component.hpp"
#include "component/select_component.hpp"
#include "component/tooltip_component.hpp"
#include "component/tabs_component.hpp"
#include "component/slider_component.hpp"
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

void sync(Fixture& f, float width = 700, float height = 600) {
    for (int frame = 0; frame < 3; ++frame) {
        check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}), "Modal layout failed");
    }
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
    throw std::runtime_error("component has no interaction");
}

void click_at(Fixture& f, float x, float y) {
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary, x, y});
    }
    sync(f);
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto rect = bounds(f, id);
    click_at(f, rect.x + rect.width / 2, rect.y + rect.height / 2);
}

void key(Fixture& f, Key value, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({value, KeyAction::down, modifiers});
    f.services.focus().dispatch({value, KeyAction::up, modifiers});
    sync(f);
}

void retained_editing_and_requests() {
    Signal<bool> open{false}, loading{false}, closable{true}, keyboard{true};
    Signal<ModalMask> mask{ModalMask{}};
    InputRef outside, inside;
    int root_runs{}, body_runs{}, ok{}, closed{};
    std::vector<bool> changes;
    std::vector<ModalCancelReason> cancellations;
    Fixture f;
    f.services.mount(Content{[&] {
        ++root_runs;
        Input(InputProps{}.defaultValue(u8"outside").ref(outside));
        Modal(ModalProps{}
                  .open(open)
                  .title(u8"Edit account")
                  .confirmLoading(loading)
                  .closable(closable)
                  .keyboard(keyboard)
                  .mask(mask)
                  .onOk([&] { ++ok; })
                  .onCancel([&](ModalCancelReason reason) { cancellations.push_back(reason); })
                  .afterOpenChange([&](bool value) { changes.push_back(value); })
                  .afterClose([&] { ++closed; }),
              ModalContent{[&] {
                  ++body_runs;
                  Input(InputProps{}.defaultValue(u8"draft").ref(inside).autoFocus());
                  Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}));
              }});
    }});
    sync(f);
    const auto id = f.services.modal().mounted()[0];
    const auto editor = f.inputs.mounted_inputs()[1];
    const auto initial = f.services.modal().snapshot(id);
    const auto count = f.services.components().component_count();
    check(body_runs == 1 && !f.inputs.sessions().active().valid() && changes.empty() && closed == 0 &&
              !initial.visible && !inside.focus(),
          "hidden Modal started focus or lifecycle");
    check(outside.focus(), "outside focus failed");
    open.set(true);
    sync(f);
    auto shown = f.services.modal().snapshot(id);
    check(shown.visible && shown.topmost && shown.bounds.width == 520 && shown.bounds.y == 100 &&
              f.services.focus().state().focused == editor.interaction && !outside.focus() &&
              changes == std::vector{true},
          "Modal opening geometry/focus/isolation failed");
    const auto session = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"!"}, session})), "Modal input edit failed");
    sync(f);
    const auto draft = f.inputs.editors().require(editor.editor).value();
    click(f, shown.ok);
    click(f, shown.cancel);
    click(f, shown.close);
    click_at(f, 2, 2);
    key(f, Key::escape);
    check(ok == 1 &&
              cancellations == std::vector{ModalCancelReason::CancelButton, ModalCancelReason::CloseButton,
                                           ModalCancelReason::Mask, ModalCancelReason::Escape} &&
              f.services.modal().snapshot(id).visible && changes == std::vector{true} && closed == 0,
          "controlled Modal optimistically closed or duplicated a request");
    loading.set(true);
    sync(f);
    click(f, shown.ok);
    click(f, shown.cancel);
    check(ok == 1 && cancellations.back() == ModalCancelReason::CancelButton,
          "loading blocked cancellation or repeated OK");
    keyboard.set(false);
    closable.set(false);
    mask.set({false, false});
    sync(f);
    const auto requests = cancellations.size();
    click_at(f, 2, 2);
    key(f, Key::escape);
    check(cancellations.size() == requests && !outside.focus(),
          "disabled cancel path or transparent mask leaked input");
    const auto select_id = f.services.select().mounted()[0];
    click(f, f.services.select().snapshot(select_id).selector);
    check(f.services.select().snapshot(select_id).visible, "Modal Select failed to open");
    key(f, Key::escape);
    check(!f.services.select().snapshot(select_id).visible && f.services.modal().snapshot(id).visible,
          "child popup Escape escaped Modal");
    check(inside.focus(), "Modal ref focus failed");
    open.set(false);
    sync(f);
    check(!f.services.modal().snapshot(id).visible && changes == std::vector({true, false}) && closed == 1 &&
              f.services.focus().state().focused == f.inputs.mounted_inputs()[0].interaction &&
              f.inputs.editors().require(editor.editor).value() == draft && !inside.focus(),
          "close did not clean and restore while retaining the editor");
    check(!bool(f.inputs.dispatch(TextCommitted{String{u8"stale"}, session})), "old Modal session remained usable");
    open.set(true);
    sync(f);
    check(f.inputs.editors().require(editor.editor).value() == draft && body_runs == 1 && root_runs == 1 &&
              f.services.components().component_count() == count &&
              f.services.modal().snapshot(id).body == initial.body && !f.services.select().snapshot(select_id).visible,
          "reopen rebuilt content or lost draft/popup cleanup");
    for (int step = 0; step < 16; ++step) {
        key(f, Key::tab, step % 2 ? KeyModifier::shift : KeyModifier::none);
        const auto focused = f.services.focus().state().focused;
        check(focused && f.services.interactions().branch_active(*focused), "Modal Tab escaped");
    }
    f.services.dispose();
    check(!inside.bound() && !outside.bound() && f.services.interactions().size() == 0 &&
              f.services.surfaces().size() == 0 && f.services.rounded_effects().live_count() == 0 &&
              !f.inputs.sessions().active().valid() && f.services.modal().mounted().empty(),
          "Modal disposal leaked retained resources");
}

void nested_and_callback_lifetime() {
    Signal<bool> first{false}, second{false};
    InputRef outside, parent, child;
    runtime::ComponentId child_owner;
    int closes{};
    bool reopen{};
    Fixture f;
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(outside));
        Modal(ModalProps{}.open(first).title(u8"Parent"), ModalContent{[&] {
                  Input(InputProps{}.ref(parent));
                  Modal(ModalProps{}
                            .open(second)
                            .title(u8"Child")
                            .onCancel([&](ModalCancelReason) { second.set(false); })
                            .afterClose([&] {
                                ++closes;
                                if (reopen) {
                                    reopen = false;
                                    second.set(true);
                                }
                            }),
                        ModalContent{[&] { Input(InputProps{}.ref(child)); }});
              }});
    }});
    sync(f);
    const auto first_id = f.services.modal().mounted()[0];
    child_owner = f.services.modal().mounted()[1];
    check(outside.focus(), "nested outside focus failed");
    first.set(true);
    sync(f);
    check(parent.focus(), "parent initial focus failed");
    second.set(true);
    sync(f);
    check(child.focus() && !parent.focus() && !outside.focus() && f.services.modal().snapshot(child_owner).topmost &&
              !f.services.modal().snapshot(first_id).topmost,
          "nested Modal did not isolate the latest layer");
    key(f, Key::escape);
    check(!second.get() && !f.services.modal().snapshot(child_owner).visible &&
              f.services.focus().state().focused == f.inputs.mounted_inputs()[1].interaction && closes == 1,
          "nested close did not restore parent");
    second.set(true);
    sync(f);
    reopen = true;
    second.set(false);
    sync(f);
    check(second.get() && f.services.modal().snapshot(child_owner).topmost && closes == 2 && child.focus(),
          "afterClose reentry failed");
    check(f.services.destroy(child_owner), "nested owner destroy failed");
    sync(f);
    check(!child.bound() && parent.focus() && !outside.focus() && f.services.modal().snapshot(first_id).topmost,
          "nested destruction left stale boundary/focus");
    first.set(false);
    sync(f);
    check(outside.focus() && !f.services.interactions().input_boundary(), "parent close did not release boundary");
}

void no_entries_custom_slots_and_flow() {
    Signal<bool> open{true};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<float> width{420};
    int title_runs{}, body_runs{}, footer_runs{};
    Fixture f;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true).gap(dp(10)), FlexContent{[&] {
                 Button(ButtonProps{}, [] { Text(u8"Before"); });
                 Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                           Modal(ModalProps{}
                                     .open(open)
                                     .closable(false)
                                     .footer(ModalFooterMode::None)
                                     .width(width)
                                     .centered(),
                                 ModalContent{[&] {
                                     ++body_runs;
                                     Text(u8"Retained body");
                                 }},
                                 ModalSlots{.title = ModalTitle{[&] {
                                                ++title_runs;
                                                Text(u8"Custom title");
                                            }},
                                            .footer = ModalFooter{[&] {
                                                ++footer_runs;
                                                Text(u8"Custom footer");
                                            }}});
                       }});
                 Button(ButtonProps{}, [] { Text(u8"After"); });
             }});
    }});
    sync(f);
    const auto id = f.services.modal().mounted()[0];
    const auto snapshot = f.services.modal().snapshot(id);
    const auto focus = interaction(f, snapshot.dialog);
    check(f.services.focus().state().focused == focus && f.services.interactions().require(focus).tab_stop &&
              snapshot.bounds.width == 420 && std::abs(snapshot.bounds.y - (600 - snapshot.bounds.height) / 2) < .01F,
          "no-entry dialog fallback or centered geometry failed");
    const auto before = bounds(f, f.buttons.mounted_buttons()[0].component);
    const auto after = bounds(f, f.buttons.mounted_buttons().back().component);
    check(std::abs(after.y - before.y - before.height - 10) < .01F, "Modal owner consumed normal-flow space/gap");
    for (int step = 0; step < 4; ++step) {
        key(f, Key::tab, step % 2 ? KeyModifier::shift : KeyModifier::none);
        check(f.services.focus().state().focused == focus, "no-entry Tab lost dialog focus");
    }
    const auto body_node = f.services.components().root(snapshot.body);
    const auto measures = f.nodes.require(body_node).measure_count;
    ThemeConfig custom;
    custom.modal.tokens.background = Color::rgba8(90, 60, 30);
    custom.modal.tokens.title = Color::rgba8(250, 240, 230);
    custom.modal.tokens.close = Color::rgba8(220, 210, 200);
    theme.set(custom);
    sync(f);
    check(f.nodes.require(body_node).measure_count == measures && title_runs == 1 && body_runs == 1 && footer_runs == 1,
          "Modal color update measured/reran content");
    bool rejected{};
    try {
        width.set(std::numeric_limits<float>::infinity());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.services.modal().snapshot(id).bounds == snapshot.bounds, "invalid width was not atomic");
    width.set(300);
    sync(f, 200, 100);
    auto small = f.services.modal().snapshot(id);
    check(small.bounds.width >= 0 && small.bounds.height >= 0 && small.bounds.x >= 0 && small.bounds.y >= 0 &&
              small.bounds.x + small.bounds.width <= 200 && small.bounds.y + small.bounds.height <= 100,
          "small viewport did not clamp dialog");
    sync(f, 1, 1);
    sync(f);
    check(f.services.modal().snapshot(id).bounds.width == 300 && title_runs == 1 && footer_runs == 1,
          "viewport recovery remounted content");
}

void scrolling_focus_and_layers(float scale) {
    Signal<bool> open{true}, nested_open{false};
    Signal<ThemeConfig> config{ThemeConfig{}};
    InputRef last;
    Fixture f;
    f.font_scale = scale;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  Modal(ModalProps{}.open(open).title(u8"Long document"), ModalContent{[&] {
                            for (int index = 0; index < 16; ++index) {
                                Text(u8"A long retained paragraph for scrolling and clipping.");
                            }
                            Input(InputProps{}.ref(last));
                            Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}));
                            Modal(ModalProps{}.open(nested_open).title(u8"Nested chooser"), ModalContent{[] {
                                      Select(SelectProps{}.options({{1, u8"Nested One"},
                                                                    {2, u8"Nested Two"},
                                                                    {3, u8"Nested Three"},
                                                                    {4, u8"Nested Four"},
                                                                    {5, u8"Nested Five"},
                                                                    {6, u8"Nested Six"}}));
                                  }});
                        }});
              }});
    }});
    sync(f, 700, 300);
    const auto id = f.services.modal().mounted()[0];
    auto shown = f.services.modal().snapshot(id);
    check(shown.maximum_scroll > 0 && shown.body_bounds.height > 0 && shown.bounds.height <= 268,
          "long Modal body did not become scrollable");
    const auto input = f.inputs.mounted_inputs()[0];
    check(last.focus(), "clipped input could not receive logical focus");
    sync(f, 700, 300);
    shown = f.services.modal().snapshot(id);
    const auto input_rect = bounds(f, input.component);
    check(shown.scroll > 0 && input_rect.y >= shown.body_bounds.y - .01F &&
              input_rect.y + input_rect.height <= shown.body_bounds.y + shown.body_bounds.height + .01F,
          "focus did not reveal clipped body input");
    check(std::abs(shown.scroll * scale - std::round(shown.scroll * scale)) < .01F, "Modal scroll lost pixel phase");
    const auto body_node = f.services.components().root(shown.body);
    const auto measures = f.nodes.require(body_node).measure_count;
    const auto old_scroll = shown.scroll;

    struct Counts {
        detail::TextSceneId scene;
        std::uint64_t shapes, instances, geometry;
    };

    std::vector<Counts> counts;
    for (const auto& text : f.services.text().mounted_texts()) {
        if (f.scene.primitive(text.scene).instances.count > 0) {
            const auto& record = f.scene.record_counters(text.scene);
            counts.push_back({text.scene, f.scene.text_state(text.scene).counters().shape_count,
                              record.instance_rebuilds, record.geometry_rebuilds});
        }
    }
    check(f.inputs.dispatch(ScrollInputEvent{0, 1, shown.body_bounds.x + 2, shown.body_bounds.y + 2}),
          "Modal body wheel not consumed");
    sync(f, 700, 300);
    shown = f.services.modal().snapshot(id);
    if (!(shown.scroll < old_scroll && f.nodes.require(body_node).measure_count == measures)) {
        std::cerr << "scroll=" << old_scroll << " -> " << shown.scroll << ", measures=" << measures << " -> "
                  << f.nodes.require(body_node).measure_count << '\n';
        throw std::runtime_error("body wheel was undone by focus or remeasured content");
    }
    check(!counts.empty(), "scroll locality fixture did not realize glyphs");
    for (const auto& old : counts) {
        const auto& record = f.scene.record_counters(old.scene);
        check(f.scene.text_state(old.scene).counters().shape_count == old.shapes &&
                  record.instance_rebuilds == old.instances && record.geometry_rebuilds == old.geometry,
              "Modal scroll reshaped or rebuilt existing glyph geometry");
    }
    sync(f, 1, 1);
    sync(f, 700, 300);
    nested_open.set(true);
    sync(f, 700, 300);
    const auto nested_id = f.services.modal().mounted()[1];
    const auto select = f.services.select().mounted()[1];
    auto themed = ThemeConfig{};
    themed.modal.tokens.z_index_popup = std::numeric_limits<int>::max();
    themed.select.tokens.z_index_popup = 1;
    config.set(themed);
    sync(f, 700, 300);
    const auto selector = f.services.select().snapshot(select).selector;
    const auto selector_bounds = bounds(f, selector);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                       selector_bounds.x + selector_bounds.width / 2,
                                       selector_bounds.y + selector_bounds.height / 2});
    }
    sync(f, 700, 300);
    const auto menu = f.services.select().snapshot(select);
    check(menu.visible && f.services.modal().snapshot(nested_id).topmost, "nested low-priority owned popup failed");
    const auto row_bounds = bounds(f, menu.rows[0]);
    const auto dialog = f.services.modal().snapshot(nested_id).bounds;
    check(row_bounds.y < dialog.y || row_bounds.y + row_bounds.height > dialog.y + dialog.height,
          "popup escape fixture did not cross the dialog edge");
    const auto hit = f.services.hit_test().hit_test({row_bounds.x + 2, row_bounds.y + row_bounds.height / 2});
    check(hit == interaction(f, menu.rows[0]), "owned popup was painted/hit below its high-priority Modal");
}

void child_input_priority_and_window_activity() {
    Signal<bool> open{true};
    InputRef area;
    runtime::ComponentId tip_button;
    int cancellations{};
    Fixture f;
    f.services.mount(Content{[&] {
        Modal(
            ModalProps{}.open(open).title(u8"Editors and popups").onCancel([&](ModalCancelReason) { ++cancellations; }),
            ModalContent{[&] {
                TextArea(TextAreaProps{}
                             .ref(area)
                             .defaultValue(u8"one\ntwo\nthree\nfour\nfive\nsix\nseven\neight")
                             .layout(LayoutStyle{}.height(dp(64))));
                Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}).showSearch());
                std::vector<TabItem> items;
                for (int index = 0; index < 10; ++index) {
                    const auto key = String::from_utf8("item-" + std::to_string(index)).value();
                    items.emplace_back(key, String{u8"A wide retained tab"}, TabContent{[] { Text(u8"Panel"); }});
                }
                Tabs(TabsProps{}, std::move(items));
                Tooltip(TooltipProps{}.title(String{u8"Owned tip"}).trigger(TooltipTriggerMode::Click),
                        TooltipTrigger{[&] {
                            Button(ButtonProps{}, [] { Text(u8"Show tip"); });
                            tip_button = f.buttons.mounted_buttons().back().component;
                        }});
                for (int row = 0; row < 20; ++row) {
                    Text(u8"Long body after the controls.");
                }
            }});
    }});
    sync(f);
    const auto id = f.services.modal().mounted()[0];
    const auto editor = f.inputs.mounted_inputs()[0];
    check(area.focus({InputFocusCursor::Start}), "TextArea focus failed");
    sync(f);
    const auto old_body_scroll = f.services.modal().snapshot(id).scroll;
    const auto area_rect = f.inputs.layout_snapshot(editor.component).viewport;
    check(f.inputs.dispatch(ScrollInputEvent{0, -1, area_rect.x + 4, area_rect.y + 4}), "TextArea wheel unhandled");
    sync(f);
    check(f.inputs.layout_snapshot(editor.component).vertical_scroll > 0 &&
              f.services.modal().snapshot(id).scroll == old_body_scroll,
          "Modal stole TextArea wheel");
    const auto select = f.services.select().mounted()[0];
    click(f, f.services.select().snapshot(select).selector);
    check(f.services.select().snapshot(select).visible, "search Select did not open in Modal");
    const auto covered_tabs = f.services.tabs().mounted()[0];
    const auto popup = f.services.select().snapshot(select).popup;
    const auto header = f.services.tabs().snapshot(covered_tabs).viewport;
    check(popup.y + 12 >= header.y && popup.y + 12 < header.y + header.height,
          "popup wheel fixture did not overlap Tabs header");
    const auto header_scroll = f.services.tabs().snapshot(covered_tabs).scroll;
    check(f.inputs.dispatch(ScrollInputEvent{1, 0, popup.x + 50, popup.y + 12}), "popup wheel unhandled");
    sync(f);
    check(f.services.tabs().snapshot(covered_tabs).scroll == header_scroll,
          "Tabs consumed wheel through the covering Select popup");
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"ni"}, {2, 0}, f.inputs.sessions().active()})),
          "Modal search preedit failed");
    key(f, Key::escape);
    check(f.services.select().snapshot(select).visible && cancellations == 0, "IME lost first Escape");
    key(f, Key::escape);
    check(!f.services.select().snapshot(select).visible && cancellations == 0, "Select lost second Escape");
    key(f, Key::escape);
    check(cancellations == 1, "Modal did not receive third Escape");
    const auto tabs = f.services.tabs().mounted()[0];
    const auto navigation = f.services.tabs().snapshot(tabs);
    const auto body_scroll = f.services.modal().snapshot(id).scroll;
    check(f.inputs.dispatch(ScrollInputEvent{1, 0, navigation.viewport.x + 4, navigation.viewport.y + 4}),
          "Tabs wheel unhandled");
    sync(f);
    check(f.services.tabs().snapshot(tabs).scroll > 0 && f.services.modal().snapshot(id).scroll == body_scroll,
          "Modal stole Tabs header wheel");
    click(f, tip_button);
    const auto tooltip = f.services.tooltip().mounted()[0];
    check(f.services.tooltip().snapshot(tooltip).visible, "owned Tooltip did not open");
    key(f, Key::escape);
    check(!f.services.tooltip().snapshot(tooltip).visible && cancellations == 1, "Tooltip lost Escape priority");
    check(area.focus(), "area refocus failed");
    const auto old_session = f.inputs.sessions().active();
    f.inputs.set_window_active(false);
    sync(f);
    check(f.services.modal().snapshot(id).visible && !f.inputs.sessions().active().valid() &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"stale"}, old_session})),
          "window deactivation retained input or hid controlled Modal");
    f.inputs.set_window_active(true);
    sync(f);
    check(f.services.focus().state().focused &&
              f.services.interactions().branch_active(*f.services.focus().state().focused),
          "window reactivation lost Modal focus");
}

void callback_destroy_capture_and_invalid_triggers() {
    Signal<bool> open{false}, disabled{false}, restore{true};
    InputRef outside;
    ButtonRef trigger;
    runtime::ComponentId modal;
    int ok{}, closed{};
    bool destroy_on_cancel{};
    Fixture f;
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(outside).disabled(disabled));
        Button(ButtonProps{}.ref(trigger).onClick([&] { open.set(true); }), [] { Text(u8"Open"); });
        Modal(ModalProps{}
                  .open(open)
                  .title(u8"Callback lifetime")
                  .restoreFocusAfterClose(restore)
                  .onOk([&] { ++ok; })
                  .onCancel([&](ModalCancelReason) {
                      if (destroy_on_cancel) {
                          f.services.destroy(modal);
                      } else {
                          open.set(false);
                      }
                  })
                  .afterClose([&] { ++closed; }),
              ModalContent{[] { Input(InputProps{}); }});
    }});
    sync(f);
    modal = f.services.modal().mounted()[0];
    check(trigger.focus(), "trigger focus failed");
    key(f, Key::enter);
    check(f.services.modal().snapshot(modal).visible, "Modal did not open inside keyboard callback");
    auto shown = f.services.modal().snapshot(modal);
    auto rect = bounds(f, shown.ok);
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                   rect.x + rect.width / 2, rect.y + rect.height / 2});
    check(f.services.pointer().state(PointerIdentity::mouse())->capture.has_value(), "Modal button did not capture");
    open.set(false);
    sync(f);
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary,
                                   rect.x + rect.width / 2, rect.y + rect.height / 2});
    check(ok == 0 && !f.services.pointer().state(PointerIdentity::mouse())->capture && closed == 1,
          "closing Modal retained capture or activated hidden OK");
    check(outside.focus(), "outside focus before disabled trigger failed");
    open.set(true);
    sync(f);
    disabled.set(true);
    open.set(false);
    sync(f);
    check(!f.services.focus().state().focused, "disabled trigger was restored");
    disabled.set(false);
    check(outside.focus(), "outside focus before inactive close failed");
    open.set(true);
    sync(f);
    f.inputs.set_window_active(false);
    open.set(false);
    sync(f);
    check(!f.inputs.sessions().active().valid(), "inactive close retained an editor session");
    f.inputs.set_window_active(true);
    sync(f);
    check(f.services.focus().state().focused == f.inputs.mounted_inputs()[0].interaction,
          "inactive close did not restore its trigger after reactivation");
    check(outside.focus(), "outside focus before opt-out failed");
    open.set(true);
    sync(f);
    restore.set(false);
    open.set(false);
    sync(f);
    check(!f.services.focus().state().focused, "restoreFocusAfterClose opt-out was ignored");
    restore.set(true);
    check(outside.focus(), "outside focus before stale trigger failed");
    open.set(true);
    sync(f);
    check(f.services.destroy(f.inputs.mounted_inputs()[0].component), "trigger destroy failed");
    open.set(false);
    sync(f);
    check(!f.services.focus().state().focused, "stale trigger was restored");
    open.set(true);
    sync(f);
    destroy_on_cancel = true;
    shown = f.services.modal().snapshot(modal);
    click(f, shown.cancel);
    check(f.services.modal().mounted().empty() && !f.services.interactions().input_boundary() &&
              !f.inputs.sessions().active().valid(),
          "onCancel owner destruction leaked boundary/input");
}

void root_only_layout_and_idle() {
    Signal<String> text{String{u8"Short"}};
    Signal<bool> open{true};
    int runs{};
    Fixture f;
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(open).closable(false).footer(ModalFooterMode::None), ModalContent{[&] {
                  ++runs;
                  Text(TextProps{}.content(text));
              }});
    }});
    sync(f);
    const auto id = f.services.modal().mounted()[0];
    const auto before = f.services.modal().snapshot(id);
    text.set(String{u8"First line\nSecond line\nThird line"});
    sync(f);
    const auto after = f.services.modal().snapshot(id);
    check(after.bounds.height > before.bounds.height && runs == 1, "root-only Modal skipped descendant layout changes");
    while (f.frames.consume_request()) {
    }
    const auto count = f.frames.counters().requests;
    const auto node = f.services.components().root(after.body);
    const auto measured = f.nodes.require(node).measure_count;
    sync(f);
    check(f.frames.counters().requests == count && !f.frames.pending() && !f.services.next_frame_deadline() &&
              f.nodes.require(node).measure_count == measured,
          "settled Modal requested frames/timers or remeasured");
}

void tall_editor_caret_reveal_and_parent_close() {
    Signal<bool> first{false}, second{false};
    InputRef outside, area;
    Fixture f;
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(outside));
        Modal(ModalProps{}.open(first).title(u8"Tall editor"), ModalContent{[&] {
                  TextArea(
                      TextAreaProps{}
                          .ref(area)
                          .defaultValue(u8"one\ntwo\nthree\nfour\nfive\nsix\nseven\neight\nnine\nten\neleven\ntwelve\nt"
                                        u8"hirteen\nfourteen\nfifteen\nsixteen\nseventeen\neighteen\nnineteen\ntwenty")
                          .layout(LayoutStyle{}.height(dp(400))));
                  Modal(ModalProps{}.open(second), ModalContent{[] { Input(InputProps{}); }});
              }});
    }});
    sync(f, 700, 300);
    check(outside.focus(), "tall editor external focus failed");
    first.set(true);
    sync(f, 700, 300);
    check(area.focus({InputFocusCursor::End}), "tall editor End focus failed");
    sync(f, 700, 300);
    const auto id = f.services.modal().mounted()[0];
    const auto body = f.services.modal().snapshot(id).body_bounds;
    const auto editor = f.inputs.mounted_inputs()[1];
    auto caret = f.inputs.layout_snapshot(editor.component).caret;
    if (!(caret.y >= body.y - .01F && caret.y + caret.height <= body.y + body.height + .01F)) {
        std::cerr << "body=" << body.y << ',' << body.height << " caret=" << caret.y << ',' << caret.height
                  << " scroll=" << f.services.modal().snapshot(id).scroll
                  << " max=" << f.services.modal().snapshot(id).maximum_scroll
                  << " editor=" << bounds(f, editor.component).y << ',' << bounds(f, editor.component).height << '\n';
    }
    check(caret.y >= body.y - .01F && caret.y + caret.height <= body.y + body.height + .01F &&
              f.services.modal().snapshot(id).scroll > 0,
          "tall editor end caret stayed outside body clip");
    const auto old_scroll = f.services.modal().snapshot(id).scroll;
    const auto old_inner_scroll = f.inputs.layout_snapshot(editor.component).vertical_scroll;
    check(f.inputs.dispatch(ScrollInputEvent{0, 1, body.x + 20, body.y + 8}), "tall TextArea wheel unhandled");
    sync(f, 700, 300);
    check(f.inputs.layout_snapshot(editor.component).vertical_scroll < old_inner_scroll &&
              f.services.modal().snapshot(id).scroll == old_scroll,
          "caret reveal undid tall TextArea's own wheel");
    check(area.focus({InputFocusCursor::Start}), "tall editor Start focus failed");
    sync(f, 700, 300);
    caret = f.inputs.layout_snapshot(editor.component).caret;
    check(caret.y >= body.y - .01F && caret.y + caret.height <= body.y + body.height + .01F,
          "caret movement did not reveal the start of a tall editor");
    second.set(true);
    sync(f, 700, 300);
    first.set(false);
    sync(f, 700, 300);
    check(!f.services.modal().snapshot(id).visible &&
              !f.services.modal().snapshot(f.services.modal().mounted()[1]).visible &&
              f.services.focus().state().focused == f.inputs.mounted_inputs()[0].interaction,
          "closing an ancestor Modal did not restore its external trigger");
    first.set(true);
    sync(f, 700, 300);
    check(f.services.modal().snapshot(f.services.modal().mounted()[1]).topmost,
          "retained controlled child did not resume with parent");
    check(f.services.destroy(id), "active parent destruction failed");
    sync(f, 700, 300);
    check(f.services.modal().mounted().empty() &&
              f.services.focus().state().focused == f.inputs.mounted_inputs()[0].interaction &&
              !f.services.interactions().input_boundary(),
          "destroying nested parent lost external restoration");
}

void layer_base_and_notification_retirement() {
    Signal<bool> open{false};
    Signal<ThemeConfig> config{ThemeConfig{}};
    runtime::ComponentId lower, upper, modal;
    int closed{};
    bool retire{};
    Fixture f;
    f.services.mount(Content{[&] {
        const auto layer = [&](int priority) {
            auto& build = runtime::require_component_build_context();
            const auto id = build.mount_component<int>(0);
            const auto node = build.root(id);
            f.layout.set_layout(node, layout::LeafLayout{});
            build.on_resource_cleanup(id, [&f, node] { f.layout.remove_layout(node); });
            f.services.components().set_window_layer(id, priority);
            const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
            f.services.scene_composer().set_fragment(fragment, {});
            return id;
        };
        lower = layer(900);
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  Modal(ModalProps{}
                            .open(open)
                            .afterOpenChange([&](bool value) {
                                if (!value && retire) {
                                    f.services.destroy(modal);
                                }
                            })
                            .afterClose([&] { ++closed; }),
                        ModalContent{[] { Text(u8"Layer base"); }});
              }});
        upper = layer(1100);
    }});
    sync(f);
    modal = f.services.modal().mounted()[0];
    open.set(true);
    sync(f);
    const auto dialog = f.services.modal().snapshot(modal).dialog;
    const auto index = [&](runtime::ComponentId id) {
        const auto traversal = f.services.components().paint_traversal();
        const auto found = std::ranges::find_if(traversal, [id](const auto& entry) { return entry.component == id; });
        check(found != traversal.end(), "layer fragment missing");
        return found - traversal.begin();
    };
    check(index(lower) < index(dialog) && index(dialog) < index(upper), "Modal ignored its global layer base");
    auto custom = ThemeConfig{};
    custom.modal.tokens.z_index_popup = 2000;
    config.set(custom);
    sync(f);
    check(index(upper) < index(dialog), "Modal layer Token did not reorder the group base");
    retire = true;
    open.set(false);
    sync(f);
    check(f.services.modal().mounted().empty() && closed == 0 && !f.services.interactions().input_boundary(),
          "retired owner received a later lifecycle callback");
}

void window_layer_escapes_document_clip() {
    Signal<bool> open{true};
    InputRef inside;
    int clicks{}, cancels{};
    runtime::ComponentId action;
    Fixture f;
    f.services.mount(Content{[&] {
        Input(InputProps{}.defaultValue(u8"Clipped background"));
        Modal(ModalProps{}.open(open).title(u8"Window layer").onCancel([&](ModalCancelReason) { ++cancels; }),
              ModalContent{[&] {
                  Input(InputProps{}.ref(inside).defaultValue(u8"Visible modal input"));
                  Button(ButtonProps{}.layout(LayoutStyle{}.width(dp(100))).onClick([&] { ++clicks; }),
                         [] { Text(u8"Left action"); });
                  action = f.buttons.mounted_buttons().back().component;
              }});
    }});
    const auto clipped_sync = [&] {
        for (int frame = 0; frame < 3; ++frame) {
            check(f.services.layout_and_synchronize({700, 600}, {300, 200, 350, 300}), "clipped document sync failed");
        }
    };
    clipped_sync();
    const auto snapshot = f.services.modal().snapshot(f.services.modal().mounted()[0]);
    const auto modal_input = f.inputs.layout_snapshot(f.inputs.mounted_inputs()[1].component);
    check(modal_input.viewport.x < 300 && modal_input.clip.x == modal_input.viewport.x &&
              modal_input.clip.width == modal_input.viewport.width,
          "document clip cropped the Modal editor");
    const auto rect = bounds(f, action);
    check(rect.x + rect.width / 2 < 300, "clipped action fixture was inside document clip");
    for (auto event : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), event, PointerButton::primary, rect.x + rect.width / 2,
                                       rect.y + rect.height / 2});
    }
    clipped_sync();
    check(clicks == 1, "document clip blocked the Modal action");
    for (const auto& button : f.buttons.mounted_buttons()) {
        if (button.component != action) {
            continue;
        }
        const auto range = f.services.surfaces().visual_range(button.scene);
        for (const auto command : f.services.scene_composer().ordered_scene().commands()) {
            check(command.kind != graphics::SceneDrawKind::quad ||
                      command.first_instance >= range.first + range.count ||
                      command.first_instance + command.instance_count <= range.first,
                  "body button still emitted quads without a rectangular clip");
        }
        const auto& focus = f.services.surfaces().focus_effect(button.scene);
        check(focus.geometry.ancestor_clip && focus.geometry.ancestor_clip->bounds == snapshot.body_bounds,
              "body button focus/shadow did not follow the body clip");
    }
    for (auto event : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), event, PointerButton::primary, 2, 2});
    }
    clipped_sync();
    check(cancels == 1, "mask did not cover the entire window outside document clip");
    for (int frame = 0; frame < 3; ++frame) {
        check(f.services.layout_and_synchronize({1000, 800}, {300, 200, 350, 300}), "expanded window sync failed");
    }
    for (auto event : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), event, PointerButton::primary, 998, 798});
    }
    check(cancels == 2, "resized mask retained the old hit-test window clip");
    open.set(false);
    clipped_sync();
    const auto background = f.inputs.layout_snapshot(f.inputs.mounted_inputs()[0].component);
    check(background.clip.x >= 300, "window layer expanded background clip");
}

void body_controls_clip_and_background_hover() {
    Signal<bool> open{false};
    std::vector<runtime::ComponentId> actions;
    int clicks{};
    Fixture f;
    f.services.mount(Content{[&] {
        Card(CardProps{}.title(u8"Background owner").hoverable(), CardContent{[&] {
                 Modal(ModalProps{}.open(open).title(u8"Scrollable actions"), ModalContent{[&] {
                           for (int row = 0; row < 15; ++row) {
                               Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Retained action"); });
                               actions.push_back(f.buttons.mounted_buttons().back().component);
                           }
                       }});
             }});
    }});
    sync(f, 700, 300);
    const auto card = f.services.card().mounted()[0];
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none, 20, 20});
    check(f.services.card().snapshot(card).hovered, "background Card hover fixture failed");
    open.set(true);
    sync(f, 700, 300);
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none, 2, 2});
    check(!f.services.card().snapshot(card).hovered, "logical Modal owner Card received background hover");
    const auto id = f.services.modal().mounted()[0];
    const auto initial = f.services.modal().snapshot(id);
    const auto footer_y = bounds(f, initial.ok).y;
    check(f.services.focus().request_focus(interaction(f, initial.ok), FocusModality::keyboard), "footer focus failed");
    sync(f, 700, 300);
    for (const auto& button : f.buttons.mounted_buttons()) {
        if (button.component != initial.ok) {
            continue;
        }
        const auto& effect = f.services.surfaces().focus_effect(button.scene);
        check(effect.geometry.ancestor_clip && effect.geometry.ancestor_clip->bounds == initial.bounds,
              "default footer clipped its focus outline to the button row");
    }
    check(f.inputs.dispatch(ScrollInputEvent{0, -1, initial.body_bounds.x + 20, initial.body_bounds.y + 20}),
          "action body scroll unhandled");
    sync(f, 700, 300);
    const auto first = bounds(f, actions[0]);
    check(first.y + first.height / 2 < initial.body_bounds.y, "action clip fixture did not cross header");
    for (auto event : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), event, PointerButton::primary,
                                       first.x + first.width / 2, first.y + first.height / 2});
    }
    sync(f, 700, 300);
    check(clicks == 0 && bounds(f, initial.ok).y == footer_y,
          "clipped action activated over header or footer moved with body");
    f.services.dispose();
    check(f.services.rounded_effects().live_count() == 0, "clipped surface fills leaked after disposal");
}

void slider_dots_clip_with_body() {
    Fixture f;
    f.services.mount(Content{[] {
        Modal(ModalProps{}.open(true).title(u8"Clipped dots"), ModalContent{[] {
                  Slider(
                      SliderProps{}.dots(true).limits(SliderLimits{0, 100, 25}).layout(LayoutStyle{}.width(dp(240))));
                  for (int row = 0; row < 15; ++row) {
                      Text(u8"Retained scroll content");
                  }
              }});
    }});
    sync(f, 700, 300);
    const auto id = f.services.modal().mounted()[0];
    const auto modal = f.services.modal().snapshot(id);
    const auto slider = f.services.slider().mounted()[0];
    const auto dot_size = f.services.components().theme_scope(slider.component)->snapshot().slider().metrics.dot_size;
    const auto verify_dots = [&] {
        std::size_t dots{};
        const auto& instances = f.services.surfaces().instances();
        for (std::uint32_t index = 0; index < instances.size(); ++index) {
            const auto& visual = instances.at(index);
            if (visual.bounds[2] != dot_size || visual.bounds[3] != dot_size || visual.corner_radius != dot_size / 2 ||
                visual.opacity == 0) {
                continue;
            }
            ++dots;
            for (const auto command : f.services.scene_composer().ordered_scene().commands()) {
                check(command.kind != graphics::SceneDrawKind::quad || index < command.first_instance ||
                          index >= command.first_instance + command.instance_count,
                      "Slider dot emitted an unclipped content quad inside Modal");
            }
        }
        check(dots >= 5, "Slider clipping fixture had no dots");
    };
    verify_dots();
    check(f.inputs.dispatch(ScrollInputEvent{0, -1, modal.body_bounds.x + 4, modal.body_bounds.y + 4}),
          "Slider body scroll failed");
    sync(f, 700, 300);
    verify_dots();
    f.services.dispose();
    check(f.services.rounded_effects().live_count() == 0, "clipped Slider dot effects leaked");
}

void hidden_select_does_not_request_frames() {
    Signal<bool> open{false};
    Fixture f;
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(open), ModalContent{[] { Select(SelectProps{}.options({{1, u8"Retained option"}})); }});
    }});
    for (int cycle = 0; cycle < 2; ++cycle) {
        open.set(cycle == 0);
        sync(f);
    }
    static_cast<void>(f.frames.consume_request());
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline(),
          "hidden Modal Select requested an idle geometry frame");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        retained_editing_and_requests();
        nested_and_callback_lifetime();
        no_entries_custom_slots_and_flow();
        for (float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
            scrolling_focus_and_layers(scale);
        }
        child_input_priority_and_window_activity();
        callback_destroy_capture_and_invalid_triggers();
        root_only_layout_and_idle();
        tall_editor_caret_reveal_and_parent_close();
        layer_base_and_notification_retirement();
        window_layer_escapes_document_clip();
        body_controls_clip_and_background_hover();
        slider_dots_clip_with_body();
        hidden_select_does_not_request_frames();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
