#include "support/auto_complete_fixture.hpp"
#include "component/popover_component.hpp"
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace auto_complete_test;

struct LabelFailure final {
    int code{};
};

struct BlurFailure final {};

runtime::ComponentId region(Fixture& f) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    f.layout.set_layout(node, layout::BoxLayout{});
    build.on_resource_cleanup(id, [&f, node] { f.layout.remove_layout(node); });
    return id;
}

void no_old_resources(Fixture& f, runtime::ComponentId safe) {
    check(f.services.auto_complete().mounted().empty() && f.inputs.mounted_inputs().empty() && f.nodes.size() == 1 &&
              f.services.components().contains(safe) && f.scene.size() == 0 &&
              f.scene.glyph_scene().instances().size() == 0 && f.services.interactions().size() == 0 &&
              f.services.surfaces().size() == 0 && blocked_roots(f.services.interactions()).empty() &&
              !f.inputs.sessions().active().valid() && !f.services.next_frame_deadline(),
          "quiet retirement retained an old resource");
}

void teardown(bool throwing) {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    InputRef rich;
    runtime::ComponentId owner;
    int blurs = 0;
    const AutoCompleteRichLabel kept{AutoCompleteOptionLabel{[&] {
        Input(InputProps{}.ref(rich).defaultValue(u8"Retained editor").onBlur([&] {
            ++blurs;
            if (throwing) {
                throw BlurFailure{};
            }
            check(f.services.destroy(owner), "blur reentered owner destroy");
        }));
    }}};
    Signal<std::vector<AutoCompleteOption>> source{
        std::vector<AutoCompleteOption>{{u8"keep", u8"keep", u8"", false, kept}}};
    f.services.mount(Content{[&] { AutoComplete(AutoCompleteProps{}.defaultOpen().options(source)); }});
    sync(f);
    owner = f.services.auto_complete().mounted()[0];
    check(rich.focus(), "focused rich teardown fixture");
    bool original = false;
    try {
        source.set({{u8"keep", u8"keep", u8"", false, kept},
                    {u8"throw", u8"throw", u8"", false,
                     AutoCompleteRichLabel{AutoCompleteOptionLabel{[] { throw LabelFailure{39}; }}}}});
    } catch (const LabelFailure& error) {
        original = error.code == 39;
    }
    sync(f);
    check(original && blurs == 0, "failure teardown delivered native blur or replaced original exception");
    check(f.services.auto_complete().mounted().empty() && f.inputs.mounted_inputs().empty() && f.nodes.size() == 0 &&
              f.scene.size() == 0 && f.services.interactions().size() == 0 && f.services.surfaces().size() == 0 &&
              !f.inputs.sessions().active().valid() && !rich.focus(),
          "failure teardown retained old resources");
    clean(f);
}

void cancel_sources() {
    for (int mode = 0; mode < 3; ++mode) {
        for (bool replacement_requested : {false, true}) {
            std::cout << "quiet cancel " << mode << replacement_requested << '\n';
            auto fixture = std::make_unique<Fixture>();
            auto& f = *fixture;
            InputRef rich;
            InputRef shared;
            runtime::ComponentId safe;
            runtime::ComponentId owner;
            runtime::ComponentId replacement;
            int blurs = 0;
            int pointer_callbacks = 0;
            int clicks = 0;
            int changes = 0;
            std::shared_ptr<theme_runtime::ThemeScope> theme;
            std::size_t subscribers = 0;
            const AutoCompleteRichLabel kept{AutoCompleteOptionLabel{[&] {
                Input(InputProps{}.ref(rich).defaultValue(u8"Retained").onBlur([&] {
                    ++blurs;
                    if (mode == 2) {
                        throw BlurFailure{};
                    }
                    (void)f.services.destroy(owner);
                    if (mode == 1) {
                        f.services.append_slot(safe, Content{[&] { AutoComplete(AutoCompleteProps{}.ref(shared)); }});
                    }
                }));
                Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Capture"); });
            }}};
            Signal<std::vector<AutoCompleteOption>> source{
                std::vector<AutoCompleteOption>{{u8"keep", u8"keep", u8"", false, kept}}};
            f.services.mount(Content{[&] {
                safe = region(f);
                theme = runtime::require_component_build_context().theme_scope();
                subscribers = theme->diagnostics().subscriber_count;
                AutoComplete(AutoCompleteProps{}.defaultOpen().ref(shared).options(source));
            }});
            sync(f);
            owner = f.services.auto_complete().mounted()[0];
            const auto old_main = f.inputs.mounted_inputs()[0];
            const auto old_rich = f.inputs.mounted_inputs()[1];
            const auto reset = detail::capture_form_input(shared).second;
            const auto button = f.buttons.mounted_buttons()[0];
            auto handlers = *f.services.interactions().require(button.interaction).handlers;
            const auto native = handlers.target;
            handlers.target = [&, native](PointerDispatchContext& event) {
                if (event.kind() == PointerEventKind::cancel || event.kind() == PointerEventKind::leave) {
                    ++pointer_callbacks;
                    throw BlurFailure{};
                }
                if (native) {
                    native(event);
                }
                if (event.kind() == PointerEventKind::down) {
                    (void)event.capture_pointer();
                }
            };
            f.services.interactions().set_handlers(button.interaction, std::move(handlers));
            f.services.interactions().set_pointer_focus_predicate(button.interaction, [] { return false; });
            check(rich.focus(), "quiet rich focus");
            const auto stale = f.inputs.sessions().active();
            check(bool(f.inputs.dispatch(CompositionChanged{String{u8"old preedit"}, {}, stale})), "quiet preedit");
            const auto rect = bounds(f, button.component);
            f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                           rect.x + rect.width / 2, rect.y + rect.height / 2});
            const auto pressed = f.services.pointer().state(PointerIdentity::mouse());
            check(pressed && pressed->capture == button.interaction && pressed->primary_down &&
                      f.services.focus().state().focused == old_rich.interaction,
                  "quiet capture fixture");
            bool original = false;
            try {
                source.set({{u8"keep", u8"keep", u8"", false, kept},
                            {u8"bad", u8"bad", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                                 if (replacement_requested) {
                                     check(f.services.retire_failed_component(owner),
                                           "quiet retirement before same-ref rebind");
                                     runtime::require_component_build_context().mount_slot(
                                         safe, Content{[&] {
                                             AutoComplete(AutoCompleteProps{}
                                                              .defaultOpen()
                                                              .defaultValue(u8"new")
                                                              .ref(shared)
                                                              .onChange([&](String) { ++changes; })
                                                              .options({{u8"new", u8"new", u8"New"}}));
                                         }});
                                     replacement = f.services.auto_complete().mounted().back();
                                 }
                                 throw LabelFailure{41};
                             }}}}});
            } catch (const LabelFailure& error) {
                original = error.code == 41;
            }
            const auto canceled = f.services.pointer().state(PointerIdentity::mouse());
            check(original && blurs == 0 && pointer_callbacks == 0 && clicks == 0 && canceled && !canceled->capture &&
                      !canceled->press_origin && !canceled->primary_down && !canceled->hover_target &&
                      !f.services.focus().state().focused && !f.inputs.sessions().active().valid() &&
                      !f.inputs.editors().find(old_rich.editor) && !f.inputs.editors().find(old_main.editor),
                  "quiet retirement delivered callbacks or retained event sources");
            sync(f);
            f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary,
                                           rect.x + rect.width / 2, rect.y + rect.height / 2});
            source.set(options());
            if (replacement_requested) {
                check(replacement != owner && snapshot(f, replacement).value == String{u8"new"} && changes == 0 &&
                          shared.focus(),
                      "quiet old cancellation damaged replacement generation");
                const auto fresh = f.inputs.sessions().active();
                check(bool(f.inputs.dispatch(CompositionChanged{String{u8"new preedit"}, {}, fresh})),
                      "quiet replacement preedit");
                reset();
                check(f.inputs.editors().require(fresh.owner).composition().active &&
                          !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stale})),
                      "quiet stale reset/session crossed generation");
                check(f.services.destroy(replacement), "quiet replacement cleanup");
            } else {
                check(!shared.focus(), "quiet retired ref stayed bound");
            }
            no_old_resources(f, safe);
            check(theme->diagnostics().subscriber_count == subscribers && blurs == 0 && pointer_callbacks == 0,
                  "quiet retirement retained subscriptions or late callbacks");
            clean(f);
        }
    }
}

void keyboard_cancel() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    ButtonRef button;
    runtime::ComponentId safe;
    int notifications = 0;
    int clicks = 0;
    const AutoCompleteRichLabel kept{AutoCompleteOptionLabel{
        [&] { Button(ButtonProps{}.ref(button).onClick([&] { ++clicks; }), [] { Text(u8"Keyboard press"); }); }}};
    Signal<std::vector<AutoCompleteOption>> source{
        std::vector<AutoCompleteOption>{{u8"keep", u8"keep", u8"", false, kept}}};
    f.services.mount(Content{[&] {
        safe = region(f);
        AutoComplete(AutoCompleteProps{}.defaultOpen().options(source));
    }});
    sync(f);
    check(button.focus(), "keyboard cancel focus");
    const auto hit = f.buttons.mounted_buttons()[0].interaction;
    f.services.focus().dispatch({Key::space, KeyAction::down});
    check(f.services.focus().state().keyboard_pressed, "keyboard press fixture");
    auto handlers = *f.services.interactions().require(hit).focus_handlers;
    const auto native = handlers.state_changed;
    handlers.state_changed = [&, native](FocusPresentation value) {
        ++notifications;
        if (native) {
            native(value);
        }
        throw BlurFailure{};
    };
    f.services.interactions().set_focus_handlers(hit, std::move(handlers));
    bool original = false;
    try {
        source.set({{u8"keep", u8"keep", u8"", false, kept},
                    {u8"bad", u8"bad", u8"", false,
                     AutoCompleteRichLabel{AutoCompleteOptionLabel{[] { throw LabelFailure{43}; }}}}});
    } catch (const LabelFailure& error) {
        original = error.code == 43;
    }
    f.services.focus().dispatch({Key::space, KeyAction::up});
    sync(f);
    check(original && notifications == 0 && clicks == 0 && !f.services.focus().state().keyboard_pressed,
          "quiet keyboard cancellation notified or activated the old interaction");
    no_old_resources(f, safe);
    clean(f);
}

void partial_focus_cancel() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    runtime::ComponentId safe;
    runtime::ComponentId owner;
    InputRef partial;
    int blurs = 0;
    Signal<std::vector<AutoCompleteOption>> source{std::vector<AutoCompleteOption>{{u8"plain", u8"plain", u8"Plain"}}};
    f.services.mount(Content{[&] {
        safe = region(f);
        AutoComplete(AutoCompleteProps{}.defaultOpen().options(source));
    }});
    sync(f);
    owner = f.services.auto_complete().mounted()[0];
    bool original = false;
    try {
        source.set({{u8"plain", u8"plain", u8"Plain"},
                    {u8"bad", u8"bad", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                         Input(InputProps{}.ref(partial).onBlur([&] {
                             ++blurs;
                             (void)f.services.destroy(owner);
                             throw BlurFailure{};
                         }));
                         check(partial.focus(), "partial row focus");
                         throw LabelFailure{45};
                     }}}}});
    } catch (const LabelFailure& error) {
        original = error.code == 45;
    }
    sync(f);
    check(original && blurs == 0 && !partial.focus(),
          "append rollback delivered a partial child's blur before quiet retirement");
    no_old_resources(f, safe);
    clean(f);
}

void damaged_owner_cancel() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    runtime::ComponentId safe;
    runtime::ComponentId owner;
    InputRef rich;
    int blurs = 0;
    f.services.mount(Content{[&] {
        safe = region(f);
        AutoComplete(AutoCompleteProps{}.defaultOpen().options(
            {{u8"rich", u8"rich", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                  Input(InputProps{}.ref(rich).onBlur([&] {
                      ++blurs;
                      (void)f.services.destroy(owner);
                  }));
              }}}}}));
    }});
    sync(f);
    owner = f.services.auto_complete().mounted()[0];
    check(rich.focus(), "damaged owner rich focus");
    check(f.services.destroy(snapshot(f, owner).input), "private Input retirement");
    sync(f);
    check(blurs == 0, "damaged owner retirement delivered a destructive blur");
    no_old_resources(f, safe);
    clean(f);
}

void restore() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    ButtonRef trigger;
    InputRef content;
    Signal<bool> child_open{false};
    int blurs = 0;
    f.services.mount(Content{[&] {
        AutoComplete(AutoCompleteProps{}.defaultOpen().options(
            {{u8"child", u8"child", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                  Popover(
                      PopoverProps{}.open(child_open).trigger(PopoverTriggerMode::Click).onOpenChange([&](bool value) {
                          child_open.set(value);
                      }),
                      PopoverTrigger{[&] { Button(ButtonProps{}.ref(trigger), [] { Text(u8"Open child"); }); }},
                      {.content = PopoverContent{[&] { Input(InputProps{}.ref(content).onBlur([&] { ++blurs; })); }}});
              }}}}}));
    }});
    sync(f);
    const auto owner = f.services.auto_complete().mounted()[0];
    click(f, f.buttons.mounted_buttons()[0].component);
    check(child_open.get() && content.focus(), "child popup content focus");
    key(f, Key::escape);
    check(!child_open.get() && blurs == 1 && snapshot(f, owner).visible &&
              f.services.focus().state().focused == f.buttons.mounted_buttons()[0].interaction,
          "deferred trigger restore was blocked by premature parent exit");
    clean(f);
}

void restore_cases() {
    for (int mode = 0; mode < 9; ++mode) {
        std::cout << "deferred restore " << mode << '\n';
        auto fixture = std::make_unique<Fixture>();
        auto& f = *fixture;
        runtime::ComponentId safe;
        runtime::ComponentId owner;
        runtime::ComponentId replacement;
        ButtonRef trigger;
        InputRef content;
        InputRef alternate;
        InputRef external;
        InputRef shared;
        Signal<bool> child_open{false};
        Signal<bool> disabled{false};
        std::vector<String> events;
        bool armed = false;
        int parent_closes = 0;
        const auto replace_owner = [&] {
            check(f.services.destroy(owner), "restore callback owner retirement");
            f.services.append_slot(
                safe, Content{[&] {
                    AutoComplete(AutoCompleteProps{}.defaultOpen().ref(shared).options({{u8"new", u8"new", u8"New"}}));
                }});
            replacement = f.services.auto_complete().mounted().back();
        };
        f.services.mount(Content{[&] {
            safe = region(f);
            AutoComplete(
                AutoCompleteProps{}
                    .defaultOpen()
                    .ref(shared)
                    .layout(LayoutStyle{}.width(dp(220)))
                    .onOpenChange([&](bool value) {
                        if (!value) {
                            ++parent_closes;
                        }
                    })
                    .options(
                        {{u8"child", u8"child", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                              Popover(PopoverProps{}
                                          .open(child_open)
                                          .trigger(PopoverTriggerMode::Click)
                                          .onOpenChange([&](bool value) { child_open.set(value); }),
                                      PopoverTrigger{[&] {
                                          Button(ButtonProps{}.ref(trigger).disabled(disabled),
                                                 [] { Text(u8"Trigger"); });
                                      }},
                                      {.content = PopoverContent{[&] {
                                           Input(InputProps{}
                                                     .ref(content)
                                                     .defaultValue(u8"Child editor")
                                                     .onFocus([&] { events.emplace_back(u8"content focus"); })
                                                     .onBlur([&] {
                                                         events.emplace_back(u8"content blur");
                                                         if (mode == 1) {
                                                             replace_owner();
                                                         }
                                                         if (mode == 2) {
                                                             check(f.services.destroy(
                                                                       f.buttons.mounted_buttons()[0].component),
                                                                   "restore target deletion");
                                                         }
                                                         if (mode == 3) {
                                                             disabled.set(true);
                                                         }
                                                         if (mode == 4) {
                                                             f.services.focus().defer_focus(
                                                                 f.inputs.mounted_inputs()[3].interaction,
                                                                 FocusModality::keyboard);
                                                         }
                                                         if (mode == 5 || mode == 7) {
                                                             f.services.focus().defer_focus(
                                                                 f.inputs.mounted_inputs()[2].interaction,
                                                                 FocusModality::keyboard);
                                                         }
                                                         if (mode == 8) {
                                                             throw BlurFailure{};
                                                         }
                                                     }));
                                       }}});
                          }}}},
                         {u8"alternate", u8"alternate", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                              Input(InputProps{}
                                        .ref(alternate)
                                        .variant(InputVariant::Borderless)
                                        .onFocus([&] {
                                            events.emplace_back(u8"alternate focus");
                                            if (mode == 7) {
                                                f.services.focus().defer_focus(
                                                    f.buttons.mounted_buttons()[0].interaction,
                                                    FocusModality::keyboard);
                                            }
                                        })
                                        .onBlur([&] { events.emplace_back(u8"alternate blur"); }));
                          }}}}}));
            Input(InputProps{}.ref(external).onFocus([&] { events.emplace_back(u8"external focus"); }));
        }});
        sync(f);
        owner = f.services.auto_complete().mounted()[0];
        const auto before = snapshot(f, owner);
        const auto old_content = f.inputs.mounted_inputs()[1];
        const auto reset = detail::capture_form_input(shared).second;
        const auto trigger_hit = f.buttons.mounted_buttons()[0].interaction;
        auto handlers = *f.services.interactions().require(trigger_hit).focus_handlers;
        const auto native = handlers.state_changed;
        handlers.state_changed = [&, native](FocusPresentation value) {
            if (native) {
                native(value);
            }
            if (armed && value.focused) {
                events.emplace_back(u8"trigger focus");
                if (mode == 6) {
                    armed = false;
                    replace_owner();
                }
            }
        };
        f.services.interactions().set_focus_handlers(trigger_hit, std::move(handlers));
        click(f, f.buttons.mounted_buttons()[0].component);
        check(child_open.get() && content.focus(), "deferred content fixture");
        const auto stale = f.inputs.sessions().active();
        armed = true;
        bool original_blur_exception = false;
        try {
            f.services.focus().dispatch({Key::escape, KeyAction::down});
        } catch (const BlurFailure&) {
            original_blur_exception = true;
        }
        check(original_blur_exception == (mode == 8), "native blur exception changed");
        sync(f);
        check(!child_open.get() && std::ranges::count(events, String{u8"content blur"}) == 1 &&
                  !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stale})),
              "deferred native blur/session semantics changed");
        if (mode == 0 || mode == 8) {
            check(events == std::vector<String>{u8"content focus", u8"content blur", u8"trigger focus"},
                  "internal restore changed native blur/focus order");
        }
        if (mode == 7) {
            check(events == std::vector<String>{u8"content focus", u8"content blur", u8"alternate focus",
                                                u8"alternate blur", u8"trigger focus"},
                  "chained restore changed native callback order");
        }
        if (mode == 4) {
            check(events == std::vector<String>{u8"content focus", u8"content blur", u8"external focus"},
                  "external deferred target changed native callback order");
        }
        if (mode == 1 || mode == 6) {
            check(!f.services.components().contains(owner) && replacement != owner &&
                      snapshot(f, replacement).visible && shared.focus(),
                  "deferred stale restore damaged replacement generation");
            const auto fresh = f.inputs.sessions().active();
            check(bool(f.inputs.dispatch(CompositionChanged{String{u8"fresh"}, {}, fresh})),
                  "deferred replacement preedit");
            reset();
            check(f.inputs.editors().require(fresh.owner).composition().active,
                  "deferred old reset touched replacement");
        } else if (mode == 2 || mode == 3 || mode == 4) {
            check(!snapshot(f, owner).visible && parent_closes == 1,
                  "failed/external restoration did not close parent once");
            check(mode == 4 ? f.services.focus().state().focused == f.inputs.mounted_inputs()[3].interaction
                            : !f.services.focus().state().focused,
                  "failed/external restore target state");
        } else {
            const auto expected = mode == 5 ? f.inputs.mounted_inputs()[2].interaction : trigger_hit;
            check(snapshot(f, owner).visible && parent_closes == 0 && f.services.focus().state().focused == expected &&
                      f.services.focus().state().focus_visible && snapshot(f, owner).input == before.input &&
                      snapshot(f, owner).rows == before.rows &&
                      f.inputs.mounted_inputs()[1].editor == old_content.editor,
                  "settled internal restore closed parent or rebuilt identity");
            const auto rect = bounds(f, f.buttons.mounted_buttons()[0].component);
            check(f.services.hit_test().hit_test({rect.x + rect.width / 2, rect.y + rect.height / 2}) == trigger_hit,
                  "settled parent trigger has no actual hit");
            bool outline = false;
            for (const auto& effect : f.services.rounded_effects().packed_instances()) {
                outline |= effect.geometry.kind == graphics::RoundedEffectKind::outline && effect.material.visible &&
                           effect.material.opacity > 0;
            }
            check(outline, "settled restore has no actual focus outline");
            check(external.focus() && !snapshot(f, owner).visible && parent_closes == 1,
                  "post-restore same-frame external exit regressed to frame sampling");
        }
        clean(f);
    }
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        check(argc == 2, "case required");
        const std::string name = argv[1];
        if (name == "teardown") {
            teardown(false);
            cancel_sources();
            partial_focus_cancel();
            damaged_owner_cancel();
        } else if (name == "blur_throw") {
            teardown(true);
            keyboard_cancel();
        } else if (name == "restore") {
            restore();
            restore_cases();
        } else {
            throw std::runtime_error("unknown R3 case");
        }
        std::cout << "AutoComplete R3 " << name << " passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
