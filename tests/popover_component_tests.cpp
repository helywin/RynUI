#include "support/popover_fixture.hpp"
#include "component/modal_component.hpp"
#include "component/select_component.hpp"
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace ryn_test::popover;

void retained() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<bool> disabled{false};
    Signal<String> label{String{u8"Title"}};
    auto config = std::make_unique<ThemeConfig>();
    Signal<ThemeConfig> theme{*config};
    int triggers{};
    int titles{};
    int contents{};
    int trigger_clicks{};
    int clicks{};
    int outside_clicks{};
    ButtonRef trigger_ref;
    ButtonRef content_ref;
    InputRef input_ref;
    std::vector<bool> changes;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Popover(
                      PopoverProps{}.disabled(disabled).trigger(PopoverTriggerMode::Click).onOpenChange([&](bool v) {
                          changes.push_back(v);
                      }),
                      PopoverTrigger{[&] {
                          ++triggers;
                          Button(ButtonProps{}.ref(trigger_ref).onClick([&] { ++trigger_clicks; }),
                                 [] { Text(u8"Open"); });
                      }},
                      {.title = PopoverTitle{[&] {
                           ++titles;
                           Text(TextProps{}.content(label));
                       }},
                       .content = PopoverContent{[&] {
                           ++contents;
                           Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[&] {
                                    Input(InputProps{}.ref(input_ref).defaultValue(u8"Draft").autoFocus().layout(
                                        LayoutStyle{}.width(dp(220))));
                                    Button(ButtonProps{}.ref(content_ref).onClick([&] { ++clicks; }),
                                           [] { Text(u8"Save"); });
                                }});
                       }}});
                  Button(ButtonProps{}.layout(LayoutStyle{}.margin_top(dp(350))).onClick([&] { ++outside_clicks; }),
                         [] { Text(u8"Outside"); });
              }});
    }});
    sync(f);
    const auto owner = f.services.popover().mounted()[0];
    const auto original = f.services.popover().snapshot(owner);
    const auto input = f.inputs.mounted_inputs()[0];
    const auto count = f.services.components().component_count();
    const auto outside = f.buttons.mounted_buttons().back().component;
    check(!original.geometry.visible && !input_ref.focus() && !f.inputs.sessions().active().valid(),
          "hidden content took focus");
    click(f, trigger(f, owner));
    check(trigger_clicks == 1 && f.services.popover().snapshot(owner).geometry.visible,
          "native click did not open once");
    check(input_ref.focus({InputFocusCursor::All}), "content Input ref failed");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"Retained draft"}, f.inputs.sessions().active()})),
          "editor text failed");
    sync(f);
    const auto session = f.inputs.sessions().active();
    label.set(String{u8"Updated title"});
    config->popover.tokens.background = Color::rgba8(230, 240, 250);
    theme.set(*config);
    sync(f);
    check(f.inputs.sessions().active() == session &&
              f.inputs.editors().require(input.editor).value() == "Retained draft",
          "ordinary update lost draft/session");
    check(content_ref.focus(), "content Button focus failed");
    key(f, Key::enter);
    key(f, Key::space);
    check(clicks == 2 && f.services.popover().snapshot(owner).geometry.visible,
          "content keyboard activation swallowed");
    for (const auto& b : f.buttons.mounted_buttons()) {
        if (descendant(f, b.component, original.content)) {
            click(f, b.component);
            break;
        }
    }
    check(clicks == 3, "content pointer activation swallowed");
    check(input_ref.focus(), "editor focus before close");
    key(f, Key::escape);
    check(!f.services.popover().snapshot(owner).geometry.visible && !f.inputs.sessions().active().valid() &&
              f.services.focus().state().focused == interaction(f, trigger(f, owner)),
          "Escape close/focus failed");
    key(f, Key::space);
    check(trigger_clicks == 2 && f.services.popover().snapshot(owner).geometry.visible &&
              f.services.focus().state().focused == input.interaction,
          "native Space activation/focus failed");
    check(f.inputs.editors().require(input.editor).value() == "Retained draft", "close reopened new editor");
    click(f, outside);
    check(outside_clicks == 1 && !f.services.popover().snapshot(owner).geometry.visible &&
              f.services.focus().state().focused == interaction(f, outside),
          "outside click lost native action/focus");
    click(f, trigger(f, owner));
    disabled.set(true);
    sync(f);
    disabled.set(false);
    sync(f);
    check(!f.services.popover().snapshot(owner).geometry.visible, "disabled stale open revived");
    check(triggers == 1 && titles == 1 && contents == 1 && f.services.components().component_count() == count &&
              f.inputs.mounted_inputs()[0].editor == input.editor && input_ref.bound(),
          "retained slots/refs changed");
}

void triggers() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<PopoverTriggers> actions{PopoverTriggers{true, true, true}};
    ButtonRef entry;
    InputRef body;
    f.services.mount(Content{[&] {
        Popover(PopoverProps{}.content(u8"Hover").triggers(actions),
                PopoverTrigger{[&] { Button(ButtonProps{}.ref(entry), [] { Text(u8"Entry"); }); }});
    }});
    sync(f);
    const auto owner = f.services.popover().mounted()[0];
    const auto b = bounds(f, trigger(f, owner));
    pointer(f, PointerAction::move, b.x + 5, b.y + 5);
    tick(f, 99000);
    check(!f.services.popover().snapshot(owner).geometry.visible, "hover opened before delay");
    tick(f, 100000);
    check(f.services.popover().snapshot(owner).geometry.visible, "hover deadline not opened");
    const auto p = f.services.popover().snapshot(owner).geometry.bounds;
    pointer(f, PointerAction::move, 699, 599);
    tick(f, 150000);
    pointer(f, PointerAction::move, p.x + 5, p.y + 5);
    tick(f, 200000);
    check(f.services.popover().snapshot(owner).geometry.visible, "moving into popup failed to cancel leave");
    pointer(f, PointerAction::move, 699, 599);
    tick(f, 299000);
    check(f.services.popover().snapshot(owner).geometry.visible, "leave delay closed early");
    tick(f, 300000);
    check(!f.services.popover().snapshot(owner).geometry.visible, "leave deadline did not close");
    check(entry.focus(), "focus entry");
    sync(f);
    check(f.services.popover().snapshot(owner).geometry.visible, "focus trigger not immediate");
    key(f, Key::escape);
    tick(f, 900000);
    check(!f.services.popover().snapshot(owner).geometry.visible, "Escape reopened under same focus");
    actions.set(PopoverTriggers{});
    sync(f);
    click(f, trigger(f, owner));
    check(!f.services.popover().snapshot(owner).geometry.visible, "manual trigger activated");
}

void boundaries() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<bool> open{true};
    Signal<bool> modal{false};
    Signal<bool> spin{false};
    Signal<bool> child_open{false};
    std::vector<bool> changes;
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(spin), SpinContent{[&] {
                 Popover(PopoverProps{}.open(open).trigger(PopoverTriggerMode::Click).onOpenChange([&](bool value) {
                     changes.push_back(value);
                 }),
                         PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Entry"); }); }},
                         {.content = PopoverContent{
                              [&] { Select(SelectProps{}.open(child_open).options({{1, u8"One"}, {2, u8"Two"}})); }}});
             }});
        Modal(ModalProps{}.open(modal), ModalContent{[] { Text(u8"Boundary"); }});
        Button(ButtonProps{}, [] { Text(u8"Foreign"); });
    }});
    sync(f);
    const auto owner = f.services.popover().mounted()[0];
    check(f.services.popover().snapshot(owner).geometry.visible && !f.services.focus().state().focused,
          "unfocused controlled default open failed");
    key(f, Key::escape);
    check(open.get() && f.services.popover().snapshot(owner).geometry.visible && changes == std::vector<bool>{false},
          "controlled authority bypassed");
    child_open.set(true);
    sync(f);
    const auto child = f.services.select().mounted()[0];
    check(f.services.select().snapshot(child).visible, "child Select not opened");
    modal.set(true);
    sync(f);
    check(!f.services.popover().snapshot(owner).geometry.visible && !f.services.select().snapshot(child).visible &&
              changes == std::vector<bool>{false},
          "dedupe skipped forced suppression");
    modal.set(false);
    sync(f);
    check(!f.services.popover().snapshot(owner).geometry.visible, "Modal exit revived old open");
    click(f, trigger(f, owner));
    check(f.services.popover().snapshot(owner).geometry.visible && !f.services.select().snapshot(child).visible,
          "explicit reopen/child stale state failed");
    spin.set(true);
    sync(f);
    spin.set(false);
    sync(f);
    check(!f.services.popover().snapshot(owner).geometry.visible, "Spin revive");
    open.set(false);
    open.set(true);
    sync(f);
    check(f.services.popover().snapshot(owner).geometry.visible, "authority reconcile failed");
    f.services.components().set_branch_active(owner, false);
    sync(f);
    f.services.components().set_branch_active(owner, true);
    sync(f);
    check(!f.services.popover().snapshot(owner).geometry.visible, "hidden revive");
    open.set(false);
    open.set(true);
    sync(f);
    f.services.set_window_active(false);
    f.services.set_window_active(true);
    sync(f);
    check(!f.services.popover().snapshot(owner).geometry.visible, "inactive revive");
    const auto foreign = f.buttons.mounted_buttons().back().component;
    f.services.set_input_blocked(foreign, true);
    const auto popup = f.services.popover().snapshot(owner).popup;
    check(f.services.destroy(owner), "destroy owner failed");
    sync(f);
    check(!f.services.interactions().component_active(foreign) && !f.services.components().contains(popup),
          "cleanup erased foreign block");
    f.services.set_input_blocked(foreign, false);
    check(f.services.interactions().component_active(foreign), "foreign block release failed");
}

void lifecycle() {
    for (int mode = 0; mode < 5; ++mode) {
        auto fixture = std::make_unique<Fixture>();
        auto& f = *fixture;
        runtime::ComponentId owner;
        InputRef input;
        ButtonRef entry;
        bool armed{};
        Signal<bool> disabled{false};
        f.services.mount(Content{[&] {
            auto gate = Binding<bool>{[&] {
                if (armed && mode == 4) {
                    f.services.destroy(owner);
                }
                return disabled.get();
            }};
            Popover(PopoverProps{}.trigger(PopoverTriggerMode::Click).onOpenChange([&](bool value) {
                if (armed && ((mode == 0 && value) || (mode == 1 && !value))) {
                    f.services.destroy(owner);
                }
            }),
                    PopoverTrigger{[&] {
                        Button(ButtonProps{}.ref(entry).disabled(gate).onClick([&] {
                            if (armed && mode == 2) {
                                f.services.destroy(owner);
                            }
                        }),
                               [] { Text(u8"Entry"); });
                    }},
                    {.content = PopoverContent{[&] {
                         Input(InputProps{}.ref(input).onBlur([&] {
                             if (armed && mode == 3) {
                                 f.services.destroy(owner);
                             }
                         }));
                     }}});
            Flex(FlexProps{}, FlexContent{[] {}});
        }});
        sync(f);
        owner = f.services.popover().mounted()[0];
        armed = mode == 0 || mode == 2;
        click(f, trigger(f, owner));
        if (mode == 1 || mode == 3) {
            check(input.focus(), "lifetime focus");
            armed = true;
            key(f, Key::escape);
        }
        if (mode == 4) {
            check(entry.focus(), "gate focus");
            armed = true;
            disabled.set(true);
            sync(f);
        }
        check(f.services.popover().mounted().empty() && !input.bound() && !entry.bound() &&
                  f.services.interactions().size() == 0,
              "callback unmount retained stale resources");
        // Reuse the same refs and likely store indices with a different generation.
        const auto replacement_parent = f.services.components().root_components().back();
        f.services.append_slot(replacement_parent, Content{[&] {
                                   Popover(PopoverProps{}.defaultOpen().content(u8"Replacement"), PopoverTrigger{[&] {
                                               Button(ButtonProps{}.ref(entry), [] { Text(u8"Replacement"); });
                                           }});
                               }});
        sync(f);
        check(entry.bound() && f.services.popover().mounted()[0] != owner, "new generation was damaged");
    }
}

void validation() {
    for (int mode = 0; mode < 5; ++mode) {
        auto fixture = std::make_unique<Fixture>();
        auto& f = *fixture;
        bool rejected{};
        try {
            f.services.mount(Content{[&] {
                auto props = PopoverProps{};
                if (mode == 0) {
                    props.open(true).defaultOpen();
                }
                if (mode == 1) {
                    props.title(u8"Text");
                }
                if (mode == 2) {
                    props.trigger(PopoverTriggerMode::Click).triggers(PopoverTriggers{});
                }
                if (mode == 3) {
                    props.placement(static_cast<PopoverPlacement>(99));
                }
                if (mode == 4) {
                    props.mouseLeaveDelay(Duration::milliseconds(86401000));
                }
                Popover(props, PopoverTrigger{[] { Text(u8"Entry"); }},
                        mode == 1 ? PopoverSlots{.title = PopoverTitle{[] { Text(u8"Title"); }}} : PopoverSlots{});
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && f.services.popover().mounted().empty(), "invalid declarations accepted");
    }
}

void gates() {
    for (int mode = 0; mode < 3; ++mode) {
        auto fixture = std::make_unique<Fixture>();
        auto& f = *fixture;
        runtime::ComponentId owner;
        ButtonRef ref;
        f.services.mount(Content{[&] {
            Popover(PopoverProps{}.trigger(PopoverTriggerMode::Click).content(u8"Body"),
                    PopoverTrigger{[&] { Button(ButtonProps{}.ref(ref), [] { Text(u8"Original"); }); }});
        }});
        sync(f);
        owner = f.services.popover().mounted()[0];
        const auto original = trigger(f, owner);
        const auto root = f.services.popover().snapshot(owner).trigger;
        const auto hit = interaction(f, original);
        if (mode == 1) {
            check(ref.focus(), "gate keyboard focus");
        }
        auto handlers = *f.services.interactions().require(hit).focus_handlers;
        handlers.activation_allowed = [&] {
            if (mode < 2) {
                f.services.destroy(owner);
            } else {
                f.services.destroy(original);
                f.services.append_slot(root,
                                       Content{[&] { Button(ButtonProps{}.ref(ref), [] { Text(u8"Replacement"); }); }});
            }
            return true;
        };
        f.services.interactions().set_focus_handlers(hit, handlers);
        if (mode == 1) {
            key(f, Key::enter);
        } else {
            click(f, original);
        }
        if (mode < 2) {
            check(f.services.popover().mounted().empty() && !ref.bound(), "activation getter retained stale owner");
        } else {
            check(ref.bound() && !f.services.popover().snapshot(owner).geometry.visible &&
                      trigger(f, owner) != original,
                  "activation getter rebound control then opened from stale generation");
            click(f, trigger(f, owner));
            check(f.services.popover().snapshot(owner).geometry.visible, "rebound native trigger failed");
        }
    }
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<bool> open{false};
    runtime::ComponentId owner;
    bool armed{};
    f.services.mount(Content{[&] {
        Popover(PopoverProps{}.content(u8"Body").open(Binding<bool>{[&] {
            const bool value = open.get();
            if (armed) {
                f.services.destroy(owner);
            }
            return value;
        }}),
                PopoverTrigger{[] { Text(u8"Entry"); }});
    }});
    sync(f);
    owner = f.services.popover().mounted()[0];
    armed = true;
    open.set(true);
    sync(f);
    check(f.services.popover().mounted().empty() && f.services.interactions().size() == 0,
          "open authority getter retained stale owner");
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        const std::string name = argc > 1 ? argv[1] : "retained";
        if (name == "retained") {
            retained();
        } else if (name == "triggers") {
            triggers();
        } else if (name == "boundaries") {
            boundaries();
        } else if (name == "lifecycle") {
            lifecycle();
        } else if (name == "validation") {
            validation();
        } else if (name == "gates") {
            gates();
        } else {
            throw std::runtime_error("unknown case");
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
