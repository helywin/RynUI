#include "support/auto_complete_fixture.hpp"
#include "component/popover_component.hpp"
#include <ryn/select.hpp>
#include <exception>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace auto_complete_test;

struct RichFailure final {
    int code{};
    std::shared_ptr<const int> marker;
};

runtime::ComponentId region(Fixture& f) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    f.layout.set_layout(node, layout::BoxLayout{});
    build.on_resource_cleanup(id, [&f, node] { f.layout.remove_layout(node); });
    return id;
}

void empty_owner_resources(Fixture& f) {
    check(f.services.auto_complete().mounted().empty() && f.inputs.mounted_inputs().empty() && f.nodes.size() == 0 &&
              f.scene.size() == 0 && f.scene.glyph_scene().instances().size() == 0 &&
              f.services.surfaces().size() == 0 && f.services.interactions().size() == 0 &&
              blocked_roots(f.services.interactions()).empty() && !f.inputs.sessions().active().valid(),
          "failed owner retained private resources");
}

void exceptions() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    InputRef fallback;
    int caught = 0;
    f.services.mount(Content{[&] {
        try {
            AutoComplete(AutoCompleteProps{}.defaultOpen().options(
                {{u8"rich", u8"rich", u8"", false,
                  AutoCompleteRichLabel{AutoCompleteOptionLabel{[] { throw RichFailure{73}; }}}},
                 {u8"two", u8"two", u8"Two"}}));
        } catch (const RichFailure& error) {
            caught = error.code;
            Input(InputProps{}.ref(fallback).defaultValue(u8"Fallback"));
        }
    }});
    // Deliberately perform the original next-frame layout before asserting retirement.
    sync(f);
    check(caught == 73 && f.services.auto_complete().mounted().empty(), "partial first mount survived local catch");
    check(f.inputs.mounted_inputs().size() == 1 && fallback.focus(), "fallback editor unusable after local catch");
    edit(f, u8"Fallback is usable");
    clean(f);

    for (int phase = 0; phase < 8; ++phase) {
        std::cout << "initial exception phase " << phase << '\n';
        auto scoped = std::make_unique<Fixture>();
        auto& current = *scoped;
        const auto marker = std::make_shared<const int>(phase + 100);
        const auto expected = std::make_exception_ptr(RichFailure{*marker, marker});
        Signal<std::vector<AutoCompleteOption>> source{options()};
        Signal<String> value{String{}};
        Signal<String> placeholder{String{u8"initial"}};
        InputRef reference;
        InputRef recovered;
        int reads = 0;
        bool delivered = false;
        current.services.mount(Content{[&] {
            const auto theme = runtime::require_component_build_context().theme_scope();
            const auto subscribers = theme->diagnostics().subscriber_count;
            try {
                AutoCompleteProps props;
                props.defaultOpen().ref(reference).value(value).options(source).placeholder(bind([&] {
                    ++reads;
                    return placeholder.get();
                }));
                if (phase == 0) {
                    props.layout(
                        LayoutStyle{}.width(bind([&]() -> LogicalLength { std::rethrow_exception(expected); })));
                }
                if (phase == 1) {
                    props.options(bind([&]() -> std::vector<AutoCompleteOption> { std::rethrow_exception(expected); }));
                }
                if (phase == 2) {
                    props.filterOption(AutoCompleteFilter{
                        [&](StringView, const AutoCompleteOption&) -> bool { std::rethrow_exception(expected); }});
                }
                if (phase >= 5) {
                    AutoCompleteRichLabel bad{AutoCompleteOptionLabel{[&] {
                        Input(InputProps{}.defaultValue(u8"Partial child"));
                        if (phase == 5) {
                            value.set(u8"mount echo");
                            source.set({{u8"changed", u8"changed", u8"Changed"}});
                        }
                        std::rethrow_exception(expected);
                    }}};
                    if (phase == 7) {
                        bad = AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                            Select(SelectProps{}.defaultOpen(true).options({{1, u8"One"}}).onOpenChange([&](bool open) {
                                if (!open) {
                                    std::rethrow_exception(expected);
                                }
                            }));
                        }}};
                        props.defaultOpen(false);
                    }
                    source.set(phase == 6 ? std::vector<AutoCompleteOption>{{u8"plain", u8"plain", u8"Plain"},
                                                                            {u8"bad", u8"bad", u8"", false, bad}}
                                          : std::vector<AutoCompleteOption>{{u8"bad", u8"bad", u8"", false, bad},
                                                                            {u8"plain", u8"plain", u8"Plain"}});
                }
                AutoComplete(props,
                             phase == 3 ? std::optional{AutoCompletePrefix{[&] { std::rethrow_exception(expected); }}}
                                        : std::nullopt,
                             phase == 4 ? std::optional{AutoCompleteSuffix{[&] { std::rethrow_exception(expected); }}}
                                        : std::nullopt);
            } catch (const RichFailure& error) {
                delivered = true;
                check(error.code == *marker && error.marker == marker, "first mount replaced the original exception");
                empty_owner_resources(current);
                check(!reference.focus() && theme->diagnostics().subscriber_count == subscribers,
                      "failed mount retained ref/theme subscriptions");
                const auto old_reads = reads;
                source.set(options());
                value.set(u8"after failure");
                placeholder.set(u8"after failure");
                check(reads == old_reads, "failed mount retained Prop observers");
                auto config = std::make_unique<ThemeConfig>();
                config->auto_complete.tokens.option_height = dp(35);
                (void)theme->update(*config);
                Input(InputProps{}.ref(recovered).defaultValue(u8"Recovered"));
            }
        }});
        sync(current);
        check(delivered && recovered.focus(), "fallback after scoped initial exception");
        edit(current, u8"Recovered input");
        clean(current);
    }

    for (bool initial : {false, true}) {
        for (bool rebind : {false, true}) {
            for (bool replace : {false, true}) {
                std::cout << "reconcile exception " << initial << rebind << replace << '\n';
                auto scoped = std::make_unique<Fixture>();
                auto& current = *scoped;
                const auto marker = std::make_shared<const int>(200);
                const auto expected = std::make_exception_ptr(RichFailure{*marker, marker});
                Signal<std::vector<AutoCompleteOption>> source{options()};
                Signal<String> value{String{}};
                InputRef reference;
                InputRef fallback_ref;
                runtime::ComponentId safe;
                runtime::ComponentId old;
                runtime::ComponentId replacement;
                std::shared_ptr<theme_runtime::ThemeScope> theme;
                std::size_t subscribers = 0;
                std::size_t replacement_subscribers = 0;
                int boundary_callbacks = 0;
                const AutoCompleteRichLabel bad{AutoCompleteOptionLabel{[&] {
                    old = current.services.auto_complete().mounted()[0];
                    if (rebind) {
                        check(current.services.destroy(old), "throwing slot retirement");
                        runtime::require_component_build_context().mount_slot(
                            safe, Content{[&] {
                                AutoComplete(AutoCompleteProps{}.ref(reference).defaultOpen().options(
                                    {{u8"new", u8"new", u8"New"}}));
                            }});
                        replacement = current.services.auto_complete().mounted()[0];
                        replacement_subscribers = theme->diagnostics().subscriber_count;
                    } else {
                        Input(InputProps{}.defaultValue(u8"Partial editor"));
                    }
                    std::rethrow_exception(expected);
                }}};
                const auto failing = replace ? std::vector<AutoCompleteOption>{{u8"a", u8"alpha", u8"", false, bad},
                                                                               {u8"b", u8"beta", u8"Beta"}}
                                             : std::vector<AutoCompleteOption>{{u8"a", u8"alpha", u8"Alpha"},
                                                                               {u8"b", u8"beta", u8"Beta"},
                                                                               {u8"bad", u8"bad", u8"", false, bad}};
                bool caught_original = false;
                current.services.mount(Content{[&] {
                    safe = region(current);
                    theme = runtime::require_component_build_context().theme_scope();
                    subscribers = theme->diagnostics().subscriber_count;
                    if (initial) {
                        source.set(failing);
                    }
                    try {
                        AutoComplete(
                            AutoCompleteProps{}.defaultOpen().options(source).value(value).ref(reference).onOpenChange(
                                [&](bool) { ++boundary_callbacks; }));
                    } catch (const RichFailure& error) {
                        caught_original = error.code == *marker && error.marker == marker;
                        Input(InputProps{}.ref(fallback_ref).defaultValue(u8"Fallback"));
                    }
                }});
                std::optional<TextInputSessionStamp> stale;
                std::function<void()> reset;
                if (!initial) {
                    sync(current);
                    old = current.services.auto_complete().mounted()[0];
                    check(reference.focus(), "reactive failure focus");
                    stale = current.inputs.sessions().active();
                    reset = detail::capture_form_input(reference).second;
                    check(bool(current.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, *stale})),
                          "reactive failure preedit");
                    try {
                        source.set(failing);
                    } catch (const RichFailure& error) {
                        caught_original = error.code == *marker && error.marker == marker;
                    }
                    current.services.append_slot(
                        safe, Content{[&] { Input(InputProps{}.ref(fallback_ref).defaultValue(u8"Fallback")); }});
                }
                sync(current);
                check(caught_original && !current.services.components().contains(old) && boundary_callbacks == 0,
                      "reconcile failure lost exception/generation or delivered unwind callbacks");
                check(!current.inputs.sessions().active().valid() &&
                          blocked_roots(current.services.interactions()).empty(),
                      "reconcile failure retained session/owned blocks");
                if (!rebind) {
                    check(current.services.auto_complete().mounted().empty() && !reference.focus(),
                          "failed reconciliation retained AutoComplete/ref");
                } else {
                    check(current.services.auto_complete().mounted().size() == 1 && replacement != old &&
                              snapshot(current, replacement).visible,
                          "throw cleanup damaged rebound generation");
                    check(reference.focus(), "replacement ref was cleared by old failure guard");
                    const auto fresh = current.inputs.sessions().active();
                    check(bool(current.inputs.dispatch(CompositionChanged{String{u8"new preedit"}, {}, fresh})),
                          "replacement preedit");
                    if (reset) {
                        reset();
                    }
                    check(current.inputs.editors().require(fresh.owner).composition().active,
                          "old reset touched replacement");
                }
                if (stale) {
                    check(!bool(current.inputs.dispatch(TextCommitted{String{u8"late"}, *stale})),
                          "failed editor session remained live");
                }
                source.set(options());
                value.set(u8"later signal");
                sync(current);
                check(fallback_ref.focus(), "fallback after reactive/local catch unusable");
                edit(current, u8"Working fallback");
                // Remove the fallback before comparing the original theme's subscribers.
                const auto fallback_input = current.inputs.mounted_inputs().back().component;
                check(current.services.destroy(fallback_input), "fallback cleanup");
                check(theme->diagnostics().subscriber_count == (rebind ? replacement_subscribers : subscribers),
                      "failed reconciliation retained theme subscriptions");
                clean(current);
            }
        }
    }
}

void focus() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    InputRef initial_rich;
    ButtonRef outside;
    int entered = 0;
    int left = 0;
    f.services.mount(Content{[&] {
        AutoComplete(AutoCompleteProps{}.defaultOpen().options(
            {{u8"rich", u8"rich", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                  Input(InputProps{}
                            .ref(initial_rich)
                            .defaultValue(u8"Rich editor")
                            .onFocus([&] { ++entered; })
                            .onBlur([&] { ++left; }));
              }}}}}));
        Button(ButtonProps{}.ref(outside), [] { Text(u8"Outside"); });
    }});
    sync(f);
    const auto id = f.services.auto_complete().mounted()[0];
    check(!f.services.focus().state().focused, "main Input must not have focus in fixture");
    check(initial_rich.focus() && outside.focus(), "two same-frame ref focus calls");
    sync(f);
    check(entered == 1 && left == 1, "rich native focus callbacks changed");
    check(!snapshot(f, id).visible, "same-frame descendant exit retained suggestions");
    clean(f);

    for (bool nested : {false, true}) {
        for (bool button : {false, true}) {
            for (bool reject : {false, true}) {
                std::cout << "same-frame focus " << nested << button << reject << '\n';
                auto scoped = std::make_unique<Fixture>();
                auto& current = *scoped;
                Signal<bool> open{true};
                InputRef first;
                InputRef second;
                InputRef external;
                ButtonRef external_button;
                int first_focus = 0;
                int first_blur = 0;
                int second_focus = 0;
                int second_blur = 0;
                int closes = 0;
                const auto editors = [&] {
                    Input(InputProps{}.ref(first).defaultValue(u8"First").onFocus([&] { ++first_focus; }).onBlur([&] {
                        ++first_blur;
                    }));
                    Input(
                        InputProps{}.ref(second).defaultValue(u8"Second").onFocus([&] { ++second_focus; }).onBlur([&] {
                            ++second_blur;
                        }));
                };
                current.services.mount(Content{[&] {
                    AutoCompleteProps props;
                    if (reject) {
                        props.open(open);
                    } else {
                        props.defaultOpen();
                    }
                    props.onOpenChange([&](bool value) {
                        if (!value) {
                            ++closes;
                        }
                    });
                    props.options({{u8"rich", u8"rich", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                                        if (nested) {
                                            Popover(PopoverProps{}.defaultOpen(),
                                                    PopoverTrigger{[] { Text(u8"Child popup"); }},
                                                    {.content = PopoverContent{editors}});
                                        } else {
                                            editors();
                                        }
                                    }}}}});
                    AutoComplete(props);
                    Input(InputProps{}.ref(external).defaultValue(u8"External editor"));
                    Button(ButtonProps{}.ref(external_button), [] { Text(u8"External button"); });
                    AutoComplete(AutoCompleteProps{}.defaultOpen().options({{u8"other", u8"other", u8"Other owner"}}));
                }});
                sync(current, 800, 600);
                const auto owner = current.services.auto_complete().mounted()[0];
                const auto unrelated = current.services.auto_complete().mounted()[1];
                const auto before = snapshot(current, owner);
                const auto editor_a = current.inputs.mounted_inputs()[1];
                const auto editor_b = current.inputs.mounted_inputs()[2];
                const auto nodes = current.nodes.size();
                check(first.focus() && second.focus(), "same-frame logical descendant transfer");
                check(snapshot(current, owner).visible && closes == 0 && first_focus == 1 && first_blur == 1 &&
                          second_focus == 1 && second_blur == 0,
                      "internal focus transfer closed parent or replaced native callbacks");
                check(button ? external_button.focus() : external.focus(), "same-frame external focus");
                check(closes == 1 && first_focus == 1 && first_blur == 1 && second_focus == 1 && second_blur == 1 &&
                          snapshot(current, owner).visible == reject,
                      "actual descendant exit missed or duplicated close/native callbacks");
                if (reject) {
                    check(first.focus() && (button ? external_button.focus() : external.focus()),
                          "same-frame repeated rejected close");
                    check(closes == 2 && first_focus == 2 && first_blur == 2,
                          "controlled refusal lost the next actual transition");
                }
                sync(current, 800, 600);
                check(current.nodes.size() == nodes && snapshot(current, owner).input == before.input &&
                          snapshot(current, owner).rows == before.rows &&
                          current.inputs.mounted_inputs()[1].component == editor_a.component &&
                          current.inputs.mounted_inputs()[1].editor == editor_a.editor &&
                          current.inputs.mounted_inputs()[2].component == editor_b.component &&
                          current.inputs.mounted_inputs()[2].editor == editor_b.editor &&
                          snapshot(current, unrelated).visible,
                      "focus observation rebuilt identity or changed unrelated owner");
                bool popup_hit = false;
                for (const auto& hit : current.services.hit_test().records()) {
                    const auto* record = current.services.interactions().find(hit.interaction);
                    popup_hit |=
                        hit.effective_eligible && record && descendant(current, record->component, before.popup);
                }
                check(popup_hit == reject, "same-frame exit left actual popup hit eligibility");
                check(current.services.focus().state().focus_visible, "external native focus presentation changed");
                clean(current);
            }
        }
    }

    for (int retired = 0; retired < 3; ++retired) {
        std::cout << "focus callback retirement " << retired << '\n';
        auto scoped = std::make_unique<Fixture>();
        auto& current = *scoped;
        InputRef rich;
        InputRef shared;
        InputRef external;
        runtime::ComponentId safe;
        runtime::ComponentId owner;
        runtime::ComponentId replacement;
        std::vector<String> events;
        current.services.mount(Content{[&] {
            safe = region(current);
            AutoComplete(
                AutoCompleteProps{}
                    .defaultOpen()
                    .ref(shared)
                    .options({{u8"rich", u8"rich", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                                   Input(InputProps{}
                                             .ref(rich)
                                             .onFocus([&] { events.emplace_back(u8"rich focus"); })
                                             .onBlur([&] { events.emplace_back(u8"rich blur"); }));
                               }}}}})
                    .onOpenChange([&](bool value) {
                        if (value) {
                            return;
                        }
                        events.emplace_back(u8"close");
                        const auto state = snapshot(current, owner);
                        check(current.services.destroy(retired == 0   ? owner
                                                       : retired == 1 ? state.input
                                                                      : state.popup),
                              "focus close callback retirement");
                        current.services.append_slot(safe, Content{[&] {
                                                         AutoComplete(
                                                             AutoCompleteProps{}.ref(shared).defaultOpen().options(
                                                                 {{u8"new", u8"new", u8"New owner"}}));
                                                     }});
                        replacement = current.services.auto_complete().mounted().back();
                    }));
            Input(InputProps{}.ref(external).onFocus([&] { events.emplace_back(u8"external focus"); }));
        }});
        sync(current);
        owner = current.services.auto_complete().mounted()[0];
        const auto reset = detail::capture_form_input(shared).second;
        check(rich.focus(), "callback rich focus");
        const auto stale = current.inputs.sessions().active();
        check(external.focus(), "callback external focus");
        check(events == std::vector<String>{u8"rich focus", u8"rich blur", u8"external focus", u8"close"} &&
                  !current.services.components().contains(owner) && replacement != owner,
              "focus observer replaced native callback order or retained damaged owner");
        sync(current);
        check(snapshot(current, replacement).visible && !rich.focus() && shared.focus(),
              "focus callback damaged rebound ref/generation");
        const auto fresh = current.inputs.sessions().active();
        check(bool(current.inputs.dispatch(CompositionChanged{String{u8"replacement preedit"}, {}, fresh})),
              "rebound composition");
        reset();
        check(current.inputs.editors().require(fresh.owner).composition().active &&
                  !bool(current.inputs.dispatch(TextCommitted{String{u8"late"}, stale})),
              "stale focus/reset session affected replacement");
        clean(current);
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
        if (name == "exceptions") {
            exceptions();
        } else if (name == "focus") {
            focus();
        } else {
            throw std::runtime_error("unknown R2 case");
        }
        std::cout << "AutoComplete R2 " << name << " passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
