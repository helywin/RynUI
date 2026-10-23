#include "component/popconfirm_component.hpp"
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

void sync(Fixture& f, float width = 700, float height = 600) {
    for (int i = 0; i < 3; ++i) {
        check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}), "Popconfirm layout failed");
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
    throw std::runtime_error("missing interaction");
}

runtime::ComponentId trigger_button(Fixture& f, runtime::ComponentId owner) {
    const auto trigger = f.services.popconfirm().snapshot(owner).trigger;
    return f.services.components().children(trigger).front();
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

void key(Fixture& f, Key value) {
    f.services.focus().dispatch({value, KeyAction::down});
    f.services.focus().dispatch({value, KeyAction::up});
    sync(f);
}

void retained_buttons_loading_and_focus() {
    Signal<bool> loading{false};
    Signal<bool> show_cancel{true};
    Signal<bool> disabled{false};
    Signal<String> label{String{u8"Confirm"}};
    int trigger_runs{};
    int title_runs{};
    int description_runs{};
    int confirms{};
    int cancels{};
    int outside_clicks{};
    std::vector<bool> changes;
    InputRef retained;
    ButtonRef trigger_ref;
    Fixture f;
    f.services.mount(Content{[&] {
        Popconfirm(PopconfirmProps{}
                       .confirmLoading(loading)
                       .showCancel(show_cancel)
                       .disabled(disabled)
                       .okText(label)
                       .onConfirm([&] {
                           ++confirms;
                           loading.set(true);
                       })
                       .onCancel([&] { ++cancels; })
                       .onOpenChange([&](bool value) { changes.push_back(value); }),
                   PopconfirmTrigger{[&] {
                       ++trigger_runs;
                       Button(ButtonProps{}.ref(trigger_ref), [] { Text(u8"Delete"); });
                   }},
                   {.title = PopconfirmTitle{[&] {
                        ++title_runs;
                        Text(u8"Delete draft?");
                    }},
                    .description = PopconfirmDescription{[&] {
                        ++description_runs;
                        Input(InputProps{}.defaultValue(u8"retained").ref(retained).autoFocus());
                    }}});
        Button(ButtonProps{}.layout(LayoutStyle{}.margin_top(dp(300))).onClick([&] { ++outside_clicks; }),
               [] { Text(u8"Outside"); });
    }});
    sync(f);
    const auto owner = f.services.popconfirm().mounted()[0];
    auto snapshot = f.services.popconfirm().snapshot(owner);
    const auto trigger = trigger_button(f, owner);
    const auto outside = f.buttons.mounted_buttons().back().component;
    const auto count = f.services.components().component_count();
    check(!snapshot.geometry.visible && changes.empty() && !retained.focus() && !f.inputs.sessions().active().valid(),
          "hidden content acquired input");
    click(f, trigger);
    snapshot = f.services.popconfirm().snapshot(owner);
    check(snapshot.geometry.visible && f.services.focus().state().focused == interaction(f, snapshot.confirm) &&
              changes == std::vector<bool>{true},
          "opening did not transfer focus to real confirm Button");
    check(f.buttons.snapshot(snapshot.confirm).type == ButtonType::Primary &&
              f.buttons.snapshot(snapshot.confirm).size == ControlSize::Small,
          "Popconfirm did not compose small primary Button");
    key(f, Key::enter);
    check(confirms == 1 && loading.get() && f.services.popconfirm().snapshot(owner).geometry.visible,
          "synchronous loading did not retain confirmation");
    click(f, snapshot.confirm);
    check(confirms == 1, "loading allowed repeated confirmation");
    click(f, snapshot.cancel);
    check(cancels == 1 && !f.services.popconfirm().snapshot(owner).geometry.visible &&
              f.services.focus().state().focused == interaction(f, trigger),
          "cancel/focus restoration failed");
    loading.set(false);
    label.set(String{u8"Proceed"});
    show_cancel.set(false);
    sync(f);
    check(!f.services.components().branch_active(snapshot.cancel), "showCancel=false left an active button");
    check(trigger_ref.focus(), "trigger focus failed");
    key(f, Key::space);
    check(f.services.popconfirm().snapshot(owner).geometry.visible, "keyboard trigger failed");
    key(f, Key::escape);
    check(!f.services.popconfirm().snapshot(owner).geometry.visible &&
              f.services.focus().state().focused == interaction(f, trigger) && cancels == 1,
          "Escape failed or invoked cancel button callback");
    click(f, trigger);
    click(f, outside);
    check(outside_clicks == 1 && !f.services.popconfirm().snapshot(owner).geometry.visible &&
              f.services.focus().state().focused == interaction(f, outside),
          "outside close stole external focus");
    click(f, trigger);
    disabled.set(true);
    sync(f);
    check(!f.services.popconfirm().snapshot(owner).geometry.visible, "disabled popup stayed visible");
    click(f, trigger);
    check(!f.services.popconfirm().snapshot(owner).geometry.visible, "disabled trigger reopened");
    check(trigger_runs == 1 && title_runs == 1 && description_runs == 1 &&
              f.services.components().component_count() == count,
          "property change rebuilt retained slots");
}

void controlled_boundary_suppression() {
    Signal<bool> open{true};
    Signal<bool> modal_open{false};
    std::vector<bool> requests;
    Fixture f;
    f.services.mount(Content{[&] {
        Popconfirm(PopconfirmProps{}.open(open).title(u8"Outside").onOpenChange([&](bool value) {
            requests.push_back(value);
        }),
                   PopconfirmTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Outside trigger"); }); }});
        Modal(ModalProps{}.open(modal_open), ModalContent{[] { Text(u8"Modal"); }});
    }});
    sync(f);
    const auto owner = f.services.popconfirm().mounted()[0];
    check(f.services.popconfirm().snapshot(owner).geometry.visible, "default controlled open failed");
    key(f, Key::escape);
    check(open.get() && f.services.popconfirm().snapshot(owner).geometry.visible &&
              requests == std::vector<bool>{false},
          "controlled request mutated visibility");
    modal_open.set(true);
    sync(f);
    check(!f.services.popconfirm().snapshot(owner).geometry.visible && requests == std::vector<bool>{false},
          "deduplicated request bypassed boundary suppression");
    modal_open.set(false);
    sync(f);
    check(!f.services.popconfirm().snapshot(owner).geometry.visible && open.get(), "stale controlled popup reappeared");
    click(f, trigger_button(f, owner));
    check(f.services.popconfirm().snapshot(owner).geometry.visible && requests == std::vector<bool>({false, true}),
          "explicit trigger did not release suppression");
    f.services.set_window_active(false);
    sync(f);
    f.services.set_window_active(true);
    sync(f);
    check(!f.services.popconfirm().snapshot(owner).geometry.visible, "window reactivation resurrected popup");
    open.set(false);
    open.set(true);
    sync(f);
    check(f.services.popconfirm().snapshot(owner).geometry.visible, "explicit open reconciliation failed");
}

void modal_children_and_retained_cleanup() {
    Signal<bool> modal_open{true};
    Signal<bool> pop_open{false};
    Signal<bool> child_open{false};
    int modal_cancel{};
    Fixture f;
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(modal_open).onCancel([&](ModalCancelReason) { ++modal_cancel; }), ModalContent{[&] {
                  Popconfirm(
                      PopconfirmProps{}.open(pop_open).title(u8"Confirm within Modal").onOpenChange([&](bool value) {
                          pop_open.set(value);
                      }),
                      PopconfirmTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Nested"); }); }},
                      {.description = PopconfirmDescription{[&] {
                           Select(SelectProps{}
                                      .options({{1, u8"One"}, {2, u8"Two"}})
                                      .open(child_open)
                                      .onOpenChange([&](bool value) { child_open.set(value); }));
                       }}});
              }});
    }});
    sync(f);
    const auto owner = f.services.popconfirm().mounted()[0];
    const auto modal = f.services.modal().mounted()[0];
    const auto child = f.services.select().mounted()[0];
    click(f, trigger_button(f, owner));
    auto snapshot = f.services.popconfirm().snapshot(owner);
    check(snapshot.geometry.visible && f.services.components().in_window_layer_stack(snapshot.popup),
          "Popconfirm escaped its Modal layer group");
    click(f, f.services.select().snapshot(child).selector);
    check(f.services.select().snapshot(child).visible && pop_open.get(), "child Select click closed Popconfirm");
    const auto row = f.services.select().snapshot(child).rows[0];
    click(f, row);
    check(pop_open.get() && !f.services.select().snapshot(child).visible &&
              f.services.select().snapshot(child).value == SelectSelection{1},
          "child popup row was obscured or closed parent");
    click(f, f.services.select().snapshot(child).selector);
    key(f, Key::escape);
    check(!f.services.select().snapshot(child).visible && pop_open.get() && modal_cancel == 0,
          "Escape did not close child Select first");
    key(f, Key::escape);
    check(!pop_open.get() && f.services.modal().snapshot(modal).visible && modal_cancel == 0,
          "Popconfirm Escape cancelled Modal");
    click(f, trigger_button(f, owner));
    click(f, f.services.select().snapshot(child).selector);
    pop_open.set(false);
    sync(f);
    check(!f.services.select().snapshot(child).visible && !child_open.get(), "hidden parent retained child popup");
    pop_open.set(true);
    sync(f);
    check(!f.services.select().snapshot(child).visible, "child popup resurrected with parent");
    modal_open.set(false);
    sync(f);
    check(!f.services.popconfirm().snapshot(owner).geometry.visible && !pop_open.get(),
          "hidden owner retained Popconfirm");
}

void geometry_theme_and_lifetime() {
    Signal<PopconfirmPlacement> placement{PopconfirmPlacement::Top};
    Signal<bool> arrow{true};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int title_runs{};
    Fixture f;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Popconfirm(PopconfirmProps{}.defaultOpen().placement(placement).pointAtCenter().arrow(arrow),
                             PopconfirmTrigger{[] { Text(u8"Plain keyboard trigger"); }},
                             {.title = PopconfirmTitle{[&] {
                                  ++title_runs;
                                  Text(u8"Delete?");
                              }}});
              }});
    }});
    sync(f);
    const auto owner = f.services.popconfirm().mounted()[0];
    for (int p = 0; p < 12; ++p) {
        placement.set(static_cast<PopconfirmPlacement>(p));
        sync(f, 300, 180);
        const auto geometry = f.services.popconfirm().snapshot(owner).geometry;
        check(geometry.visible && geometry.bounds.x >= 0 && geometry.bounds.y >= 0 &&
                  geometry.bounds.x + geometry.bounds.width <= 300 && geometry.bounds.y + geometry.bounds.height <= 180,
              "Popconfirm placement overflowed viewport");
    }
    sync(f);
    f.services.text().reset_sync_profile();
    f.services.text().set_sync_profiling_enabled(true);
    ThemeConfig config;
    config.popconfirm.tokens.background = Color::rgba8(240, 250, 255);
    theme.set(config);
    sync(f);
    check(title_runs == 1 && f.services.text().sync_profile().layout_calls == 0,
          "color theme rebuilt rich title or remeasured layout");
    key(f, Key::escape);
    check(!f.services.popconfirm().snapshot(owner).geometry.visible &&
              f.services.focus().state().focused == interaction(f, owner),
          "non-interactive trigger fallback focus failed");
    key(f, Key::enter);
    check(f.services.popconfirm().snapshot(owner).geometry.visible, "plain trigger keyboard opening failed");
    arrow.set(false);
    sync(f);
    check(!f.services.components().branch_active(f.services.components().children(owner).back()),
          "arrow=false retained a visible arrow");
    click(f, f.services.popconfirm().snapshot(owner).confirm);
    check(!f.services.popconfirm().snapshot(owner).geometry.visible, "default confirmation did not close");
    key(f, Key::enter);
    check(f.services.destroy(owner) && f.services.popconfirm().mounted().empty(),
          "Popconfirm resources did not retire");
    sync(f);
    check(!f.services.focus().state().focused, "destroyed owner retained focus");

    Fixture destroy_on_confirm;
    runtime::ComponentId destroyed;
    destroy_on_confirm.services.mount(Content{[&] {
        Popconfirm(PopconfirmProps{}.defaultOpen().title(u8"Destroy").onConfirm([&] {
            destroy_on_confirm.services.destroy(destroyed);
        }),
                   PopconfirmTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Destroy"); }); }});
    }});
    sync(destroy_on_confirm);
    destroyed = destroy_on_confirm.services.popconfirm().mounted()[0];
    click(destroy_on_confirm, destroy_on_confirm.services.popconfirm().snapshot(destroyed).confirm);
    check(!destroy_on_confirm.services.components().contains(destroyed), "confirm callback destruction failed");

    Fixture destroy_on_open;
    runtime::ComponentId opening;
    destroy_on_open.services.mount(Content{[&] {
        Popconfirm(PopconfirmProps{}.title(u8"Open callback").onOpenChange([&](bool value) {
            if (value) {
                destroy_on_open.services.destroy(opening);
            }
        }),
                   PopconfirmTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Open"); }); }});
    }});
    sync(destroy_on_open);
    opening = destroy_on_open.services.popconfirm().mounted()[0];
    click(destroy_on_open, trigger_button(destroy_on_open, opening));
    check(!destroy_on_open.services.components().contains(opening), "open callback destruction failed");
}

void invalid_declarations() {
    for (int variant = 0; variant < 3; ++variant) {
        Fixture f;
        bool rejected{};
        try {
            f.services.mount(Content{[&] {
                auto props = PopconfirmProps{};
                PopconfirmSlots slots;
                if (variant == 0) {
                    props.open(true).defaultOpen(false);
                } else if (variant == 1) {
                    props.title(u8"Duplicate");
                    slots.title = PopconfirmTitle{[] { Text(u8"Duplicate"); }};
                } else {
                    props.placement(static_cast<PopconfirmPlacement>(99));
                }
                Popconfirm(props, PopconfirmTrigger{[] { Text(u8"Trigger"); }}, slots);
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && f.services.popconfirm().mounted().empty(),
              "invalid declarations were not rejected atomically");
    }
}

void initially_open_inside_modal() {
    for (bool controlled : {false, true}) {
        Signal<bool> modal_open{true}, popup_open{true};
        Fixture f;
        f.services.mount(Content{[&] {
            Modal(ModalProps{}.open(modal_open), ModalContent{[&] {
                      auto props = PopconfirmProps{}.title(u8"Initially open");
                      if (controlled) {
                          props.open(popup_open);
                      } else {
                          props.defaultOpen();
                      }
                      Popconfirm(props, PopconfirmTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Action"); }); }});
                  }});
        }});
        sync(f);
        const auto id = f.services.popconfirm().mounted()[0];
        const auto opened = f.services.popconfirm().snapshot(id);
        check(opened.geometry.visible && f.services.focus().state().focused == interaction(f, opened.confirm),
              "initial Modal boundary did not activate its open Popconfirm");
        modal_open.set(false);
        sync(f);
        modal_open.set(true);
        sync(f);
        check(!f.services.popconfirm().snapshot(id).geometry.visible,
              "Modal reactivation revived suppressed old Popconfirm");
        if (controlled) {
            popup_open.set(false);
            popup_open.set(true);
            sync(f);
            check(f.services.popconfirm().snapshot(id).geometry.visible,
                  "explicit Popconfirm reopen stayed suppressed");
        }
    }
}

void keyboard_preserves_native_trigger_activation() {
    int clicks{};
    ButtonRef trigger;
    Fixture f;
    f.services.mount(Content{[&] {
        Popconfirm(PopconfirmProps{}.title(u8"Confirm action"), PopconfirmTrigger{[&] {
                       Button(ButtonProps{}.ref(trigger).onClick([&] { ++clicks; }), [] { Text(u8"Action"); });
                   }});
    }});
    sync(f);
    const auto id = f.services.popconfirm().mounted()[0];
    check(trigger.focus(), "keyboard trigger fixture focus failed");
    key(f, Key::enter);
    check(clicks == 1 && f.services.popconfirm().snapshot(id).geometry.visible,
          "Popconfirm swallowed native Button Enter activation");
    key(f, Key::escape);
    f.services.focus().dispatch({Key::space, KeyAction::down});
    sync(f);
    check(clicks == 1 && !f.services.popconfirm().snapshot(id).geometry.visible &&
              f.services.focus().state().keyboard_pressed,
          "Popconfirm transferred focus before native Space release");
    f.services.focus().dispatch({Key::space, KeyAction::up});
    sync(f);
    check(clicks == 2 && f.services.popconfirm().snapshot(id).geometry.visible,
          "Popconfirm swallowed native Button Space activation");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        retained_buttons_loading_and_focus();
        controlled_boundary_suppression();
        modal_children_and_retained_cleanup();
        geometry_theme_and_lifetime();
        invalid_declarations();
        initially_open_inside_modal();
        keyboard_preserves_native_trigger_activation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
