#include "support/popover_fixture.hpp"
#include "component/date_picker_component.hpp"
#include "component/time_picker_component.hpp"
#include "component/select_component.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace ryn_test::popover;
using namespace ryn::theme_runtime;

void fonts() {
    auto root = ThemeScope::create_default();
    auto local = std::make_unique<ThemeConfig>();
    auto child = ThemeScope::create(root, *local);
    int notifications{};
    auto subscription = root->capture([&](DirtyPhase) { ++notifications; }, [&] { (void)root->popover_typography(); });
    auto child_subscription =
        child->capture([&](DirtyPhase) { ++notifications; }, [&] { (void)child->popover_typography(); });
    for (int mode = 0; mode < 4; ++mode) {
        auto config = std::make_unique<ThemeConfig>();
        if (mode == 0) {
            config->popover.tokens.font_size = dp(.25F);
        }
        if (mode == 1) {
            config->popover.algorithm = true;
            config->popover.seed.font_size = dp(.25F);
        }
        if (mode == 2) {
            config->popover.tokens.font_size = dp(static_cast<float>(std::numeric_limits<long>::max()));
        }
        if (mode == 3) {
            config->popover.tokens.font_size = dp(static_cast<float>(std::numeric_limits<std::uint32_t>::max()));
        }
        const auto identity = root->snapshot().identity();
        const auto child_identity = child->snapshot().identity();
        const auto generation = root->generation();
        const auto child_generation = child->generation();
        const auto count = notifications;
        bool rejected{};
        try {
            root->update(*config);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "R1.1: unrenderable final Popover font was published");
        check(root->snapshot().identity() == identity && child->snapshot().identity() == child_identity &&
                  root->generation() == generation && child->generation() == child_generation && notifications == count,
              "R1.1: failed font update was not atomic");
        *config = ThemeConfig{};
        config->popover.tokens.font_size = dp(.5F);
        root->update(*config);
        check(child->snapshot().popover().font_size == .5F, "R1.1: inherited legal font did not recover");
    }
    auto parent = ThemeScope::create_default();
    local->popover.algorithm = true;
    local->popover.seed.font_size = dp(.75F);
    auto independent = ThemeScope::create(parent, *local);
    auto descendant = ThemeScope::create(independent, detail::default_theme_config());
    const auto p = parent->snapshot().identity();
    const auto c = independent->snapshot().identity();
    const auto d = descendant->snapshot().identity();
    const auto pg = parent->generation();
    const auto cg = independent->generation();
    const auto dg = descendant->generation();
    int changes{};
    auto a = parent->capture([&](DirtyPhase) { ++changes; }, [&] { (void)parent->popover_typography(); });
    auto b = independent->capture([&](DirtyPhase) { ++changes; }, [&] { (void)independent->popover_typography(); });
    auto third = descendant->capture([&](DirtyPhase) { ++changes; }, [&] { (void)descendant->popover_typography(); });
    auto config = std::make_unique<ThemeConfig>();
    config->algorithms = {ThemeAlgorithm::Compact};
    bool rejected{};
    try {
        parent->update(*config);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && parent->snapshot().identity() == p && independent->snapshot().identity() == c &&
              descendant->snapshot().identity() == d && parent->generation() == pg && independent->generation() == cg &&
              descendant->generation() == dg && changes == 0,
          "R1.1: descendant preflight published partial font state");
    local->popover.seed.font_size = dp(14);
    independent->update(*local);
    check(parent->update(*config) && descendant->snapshot().popover().font_size == 12,
          "R1.1: descendant font recovery failed");
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<ThemeConfig> theme{detail::default_theme_config()};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[] {
                  Popover(PopoverProps{}.defaultOpen(), PopoverTrigger{[] { Text(u8"Entry"); }},
                          {.content = PopoverContent{[] {
                               Icon(IconProps{}.name(IconName::InfoCircleOutlined));
                               Text(u8"Body");
                           }}});
              }});
    }});
    sync(f);
    config->algorithms.clear();
    config->popover.tokens.font_size = dp(.5F);
    theme.set(*config);
    sync(f);
}

void press(Fixture& f, runtime::ComponentId component) {
    const auto b = bounds(f, component);
    pointer(f, PointerAction::down, b.x + b.width / 2, b.y + b.height / 2);
    pointer(f, PointerAction::up, b.x + b.width / 2, b.y + b.height / 2);
}

void positioning() {
    for (bool time : {false, true}) {
        auto fixture = std::make_unique<Fixture>();
        auto& f = *fixture;
        Signal<LogicalLength> margin{dp(20)};
        Signal<bool> inner_open{false};
        const auto body = [&] {
            Popover(PopoverProps{}.trigger(PopoverTriggerMode::Click).placement(PopoverPlacement::BottomLeft),
                    PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Child popover"); }); }},
                    {.content = PopoverContent{[&] {
                         if (time) {
                             DatePicker(DatePickerProps{}.open(inner_open));
                         } else {
                             TimePicker(TimePickerProps{}.open(inner_open));
                         }
                     }}});
        };
        f.services.mount(Content{[&] {
            if (time) {
                TimePicker(TimePickerProps{}.defaultOpen().layout(LayoutStyle{}.margin_left(margin).margin_top(dp(40))),
                           TimePickerFooter{body});
            } else {
                DatePicker(DatePickerProps{}.defaultOpen().layout(LayoutStyle{}.margin_left(margin).margin_top(dp(40))),
                           DatePickerFooter{body});
            }
        }});
        sync(f, 1000, 1000);
        const auto popover = f.services.popover().mounted()[0];
        press(f, trigger(f, popover));
        sync(f, 1000, 1000);
        auto s = f.services.popover().snapshot(popover);
        check(s.geometry.visible && s.geometry.anchor == bounds(f, popover),
              "R1.2: initial footer Popover anchor is stale");
        inner_open.set(true);
        sync(f, 1000, 1000);
        for (int phase = 0; phase < 3; ++phase) {
            margin.set(dp(phase == 0 ? 150.0F : phase == 1 ? 300.0F : 50.0F));
            const float width = phase == 1 ? 700.0F : 1000.0F;
            const float height = phase == 1 ? 700.0F : 1000.0F;
            sync(f, width, height);
            s = f.services.popover().snapshot(popover);
            check(s.geometry.anchor == bounds(f, popover),
                  "R1.2: moved picker footer left child Popover on previous anchor");
            const auto panel = time ? f.services.date_picker().snapshot(f.services.date_picker().mounted()[0]).bounds
                                    : f.services.time_picker().snapshot(f.services.time_picker().mounted()[0]).bounds;
            const auto selector =
                time ? f.services.date_picker().snapshot(f.services.date_picker().mounted()[0]).input
                     : f.services.time_picker().snapshot(f.services.time_picker().mounted()[0]).selector;
            const auto input = bounds(f, selector);
            check(std::abs(panel.x - std::clamp(input.x, 0.0F, width - panel.width)) < .01F,
                  "R1.2: Popover child picker did not follow final content coordinates");
        }
    }
    // Opposite direction with another Popover below the picker, exercising
    // alternating hosts without extra sync calls or a recursion cycle.
    for (bool time : {false, true}) {
        auto fixture = std::make_unique<Fixture>();
        auto& f = *fixture;
        Signal<LogicalLength> margin{dp(20)};
        Signal<bool> picker_open{false};
        Signal<bool> leaf_open{false};
        f.services.mount(Content{[&] {
            Popover(PopoverProps{}.defaultOpen().layout(LayoutStyle{}.margin_left(margin)),
                    PopoverTrigger{[] { Text(u8"Root"); }},
                    {.content = PopoverContent{[&] {
                         const auto footer = [&] {
                             Popover(PopoverProps{}.open(leaf_open).content(u8"Leaf"),
                                     PopoverTrigger{[] { Text(u8"Leaf anchor"); }});
                         };
                         if (time) {
                             TimePicker(TimePickerProps{}.open(picker_open), TimePickerFooter{footer});
                         } else {
                             DatePicker(DatePickerProps{}.open(picker_open), DatePickerFooter{footer});
                         }
                     }}});
        }});
        sync(f, 1000, 1000);
        picker_open.set(true);
        sync(f, 1000, 1000);
        const auto leaf = f.services.popover().mounted()[0];
        leaf_open.set(true);
        sync(f, 1000, 1000);
        for (float x : {100.0F, 200.0F}) {
            margin.set(dp(x));
            sync(f, 900, 800);
            const auto s = f.services.popover().snapshot(leaf);
            check(s.geometry.visible && s.geometry.anchor == bounds(f, leaf), "R1.2: reverse alternating anchor stale");
        }
    }
}

void escape_owner() {
    for (bool time : {false, true}) {
        for (bool controlled : {false, true}) {
            auto fixture = std::make_unique<Fixture>();
            auto& f = *fixture;
            Signal<bool> parent_open{true};
            Signal<bool> open{false};
            Signal<bool> select_open{false};
            InputRef input;
            SelectRef selection;
            int clicks{};
            bool reject = controlled;
            const auto footer = [&] {
                auto props = PopoverProps{}.trigger(PopoverTriggerMode::Click).onOpenChange([&](bool value) {
                    if (value || !reject) {
                        open.set(value);
                    }
                });
                if (controlled) {
                    props.open(open);
                }
                Popover(props, PopoverTrigger{[&] {
                            Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Child"); });
                        }},
                        {.content = PopoverContent{[&] {
                             Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                      Input(InputProps{}.ref(input));
                                      Select(SelectProps{}
                                                 .ref(selection)
                                                 .open(select_open)
                                                 .options({{1, u8"One"}})
                                                 .onOpenChange([&](bool v) { select_open.set(v); }));
                                  }});
                         }}});
            };
            f.services.mount(Content{[&] {
                if (time) {
                    TimePicker(TimePickerProps{}.open(parent_open).onOpenChange([&](bool v) { parent_open.set(v); }),
                               TimePickerFooter{footer});
                } else {
                    DatePicker(DatePickerProps{}.open(parent_open).onOpenChange([&](bool v) { parent_open.set(v); }),
                               DatePickerFooter{footer});
                }
            }});
            sync(f, 1000, 1000);
            const auto owner = f.services.popover().mounted()[0];
            press(f, trigger(f, owner));
            sync(f, 1000, 1000);
            check(clicks == 1 && f.services.popover().snapshot(owner).geometry.visible,
                  "R1.3: pointer child opening/native click failed");
            check(f.services.focus().state().focused == interaction(f, trigger(f, owner)),
                  "R1.3: pointer open moved focus off native trigger");
            f.services.focus().dispatch({Key::escape, KeyAction::down});
            sync(f, 1000, 1000);
            check(parent_open.get() && f.services.popover().snapshot(owner).geometry.visible == controlled,
                  "R1.3: Escape closed parent picker before its open child Popover");
            if (!controlled) {
                press(f, trigger(f, owner));
                sync(f, 1000, 1000);
            } else {
                // Begin a new authority transaction after rejecting the first
                // close; repeated identical requests intentionally deduplicate.
                open.set(false);
                open.set(true);
                sync(f, 1000, 1000);
            }
            reject = false;
            check(input.focus(), "R1.3: editor focus failed");
            check(bool(f.inputs.dispatch(CompositionChanged{String{u8"zhong"}, {}, f.inputs.sessions().active()})),
                  "R1.3: IME fixture failed");
            f.services.focus().dispatch({Key::escape, KeyAction::down});
            sync(f, 1000, 1000);
            check(parent_open.get() && f.services.popover().snapshot(owner).geometry.visible,
                  "R1.3: IME Escape closed popup");
            check(selection.focus(), "R1.3: child Select focus failed");
            select_open.set(true);
            sync(f, 1000, 1000);
            f.services.focus().dispatch({Key::escape, KeyAction::down});
            sync(f, 1000, 1000);
            check(!select_open.get() && parent_open.get() && f.services.popover().snapshot(owner).geometry.visible,
                  "R1.3: deeper child priority lost");
            f.services.focus().request_focus(interaction(f, trigger(f, owner)), FocusModality::keyboard);
            f.services.focus().dispatch({Key::escape, KeyAction::down});
            sync(f, 1000, 1000);
            check(parent_open.get() && !f.services.popover().snapshot(owner).geometry.visible,
                  "R1.3: accepted child close failed");
        }
    }
}

void first_focus() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    ButtonRef parent_ref;
    ButtonRef child_ref;
    int clicks{};
    f.services.mount(Content{[&] {
        Popover(PopoverProps{}.trigger(PopoverTriggerMode::Click),
                PopoverTrigger{[&] { Button(ButtonProps{}.ref(parent_ref), [] { Text(u8"Parent"); }); }},
                {.content = PopoverContent{[&] {
                     Popover(PopoverProps{}.trigger(PopoverTriggerMode::Click).content(u8"Child body"),
                             PopoverTrigger{[&] {
                                 Button(ButtonProps{}.ref(child_ref).onClick([&] { ++clicks; }),
                                        [] { Text(u8"Native child entry"); });
                             }});
                 }}});
    }});
    sync(f);
    check(parent_ref.focus(), "R1.4: parent focus failed");
    key(f, Key::enter);
    const auto child = f.services.popover().mounted()[0];
    check(f.services.focus().state().focused == interaction(f, trigger(f, child)),
          "R1.4: initial focus selected proxy wrapper instead of native entry");
    key(f, Key::enter);
    check(clicks == 1 && f.services.popover().snapshot(child).geometry.visible, "R1.4: native Enter swallowed");
    key(f, Key::escape);
    key(f, Key::space);
    check(clicks == 2 && f.services.popover().snapshot(child).geometry.visible, "R1.4: native Space swallowed");
}

void delays() {
    for (bool entering : {false, true}) {
        for (float replacement : {2000.0F, 50.0F, 0.0F}) {
            auto fixture = std::make_unique<Fixture>();
            auto& f = *fixture;
            Signal<Duration> enter{Duration::milliseconds(1000)};
            Signal<Duration> leave{Duration::milliseconds(1000)};
            f.services.mount(Content{[&] {
                Popover(PopoverProps{}.content(u8"Body").mouseEnterDelay(enter).mouseLeaveDelay(leave),
                        PopoverTrigger{[] { Text(u8"Entry"); }});
            }});
            sync(f);
            const auto owner = f.services.popover().mounted()[0];
            const auto b = bounds(f, trigger(f, owner));
            pointer(f, PointerAction::move, b.x + 2, b.y + 2);
            if (!entering) {
                tick(f, 1000000);
                pointer(f, PointerAction::move, 690, 590);
            }
            const auto base = entering ? 100000 : 1100000;
            tick(f, base);
            (entering ? enter : leave).set(Duration::milliseconds(replacement));
            sync(f);
            const auto deadline = f.services.popover().next_auxiliary_deadline();
            check(replacement == 0 || deadline.has_value(),
                  "R1.5: reactive leave delay discarded pending close forever");
            if (replacement > 0) {
                check(deadline ==
                          animation::AnimationTime::microseconds(base + static_cast<std::int64_t>(replacement * 1000)),
                      "R1.5: pending direction/deadline not recomputed");
                tick(f, deadline->count_microseconds() - 1);
                check(f.services.popover().snapshot(owner).geometry.visible != entering,
                      "R1.5: pending transition ran early");
            }
            tick(f, base + static_cast<std::int64_t>(replacement * 1000));
            check(f.services.popover().snapshot(owner).geometry.visible == entering &&
                      !f.services.popover().next_auxiliary_deadline(),
                  "R1.5: retimed transition did not finish");
            if (entering) {
                const auto phase = base + static_cast<std::int64_t>(replacement * 1000);
                pointer(f, PointerAction::move, 690, 590);
                leave.set(Duration::milliseconds(2000));
                const auto popup = f.services.popover().snapshot(owner).geometry.bounds;
                pointer(f, PointerAction::move, popup.x + 2, popup.y + 2);
                tick(f, phase + 3000000);
                check(f.services.popover().snapshot(owner).geometry.visible,
                      "R1.5: cancelled retimed leave closed popup");
                pointer(f, PointerAction::move, 690, 590);
                f.services.destroy(owner);
                tick(f, phase + 6000000);
                check(!f.services.popover().next_auxiliary_deadline(), "R1.5: destroyed leaving owner retained timer");
            }
        }
    }
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<Duration> delay{Duration::milliseconds(1000)};
    f.services.mount(Content{[&] {
        Popover(PopoverProps{}.content(u8"Body").mouseEnterDelay(delay), PopoverTrigger{[] { Text(u8"Entry"); }});
    }});
    sync(f);
    const auto owner = f.services.popover().mounted()[0];
    const auto b = bounds(f, trigger(f, owner));
    pointer(f, PointerAction::move, b.x + 2, b.y + 2);
    delay.set(Duration::milliseconds(2000));
    pointer(f, PointerAction::move, 690, 590);
    tick(f, 3000000);
    check(!f.services.popover().snapshot(owner).geometry.visible, "R1.5: cancelled retimed enter reopened");
    pointer(f, PointerAction::move, b.x + 2, b.y + 2);
    f.services.destroy(owner);
    tick(f, 6000000);
    check(!f.services.popover().next_auxiliary_deadline(), "R1.5: destroyed owner retained deadline");
}

void immediate_focus() {
    for (bool tab : {false, true}) {
        auto fixture = std::make_unique<Fixture>();
        auto& f = *fixture;
        ButtonRef ref;
        f.services.mount(Content{[&] {
            Popover(PopoverProps{}
                        .trigger(PopoverTriggerMode::HoverFocus)
                        .content(u8"Body")
                        .mouseEnterDelay(Duration::seconds(10)),
                    PopoverTrigger{[&] { Button(ButtonProps{}.ref(ref), [] { Text(u8"Entry"); }); }});
            Button(ButtonProps{}, [] { Text(u8"Outside"); });
        }});
        sync(f);
        const auto owner = f.services.popover().mounted()[0];
        const auto b = bounds(f, trigger(f, owner));
        pointer(f, PointerAction::move, b.x + 5, b.y + 5);
        tick(f, 100000);
        check(f.services.popover().next_auxiliary_deadline().has_value(), "R1.6: missing pending hover fixture");
        if (tab) {
            key(f, Key::tab);
        } else {
            check(ref.focus(), "R1.6: ref focus failed");
            sync(f);
        }
        check(f.services.popover().snapshot(owner).geometry.visible && !f.services.popover().next_auxiliary_deadline(),
              "R1.6: pending hover delayed immediate focus");
        pointer(f, PointerAction::move, 690, 590);
        f.services.focus().request_focus(f.buttons.mounted_buttons().back().interaction, FocusModality::keyboard);
        sync(f);
        tick(f, 200000);
        check(!f.services.popover().snapshot(owner).geometry.visible && !f.services.popover().next_auxiliary_deadline(),
              "R1.6: focus leave did not close normally");
    }
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        const std::string name = argc > 1 ? argv[1] : "fonts";
        if (name == "fonts") {
            fonts();
        } else if (name == "positioning") {
            positioning();
        } else if (name == "escape") {
            escape_owner();
        } else if (name == "focus") {
            first_focus();
        } else if (name == "delays") {
            delays();
        } else if (name == "immediate") {
            immediate_focus();
        } else {
            throw std::runtime_error("unknown R1 case");
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
