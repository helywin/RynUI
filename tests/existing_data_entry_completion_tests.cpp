#include "support/rate_fixture.hpp"
#include "component/selection_component.hpp"
#include "component/slider_component.hpp"
#include <thread>

namespace {
using namespace rate_test;

void direction_and_characters() {
    Fixture f;
    Signal<RateDirection> direction{RateDirection::LeftToRight};
    Signal<RateCharacter> character{RateCharacter{u8"A"}};
    Signal<int> count{3};
    int factories{};
    f.services.mount(Content{[&] {
        Rate(RateProps{}.defaultValue(1.5F).count(count).allowHalf().direction(direction).characterFor(
            [&](std::size_t index) -> Prop<RateCharacter> {
                ++factories;
                return index == 1 ? Prop<RateCharacter>{character} : Prop<RateCharacter>{RateCharacter{u8"B"}};
            }));
    }});
    sync(f);
    const auto id = f.services.rate().mounted().front();
    const auto original = f.services.rate().snapshot(id);
    const auto mounts = f.services.components().mount_runs();
    const auto text_count = f.services.text().mounted_texts().size();
    character.set(RateCharacter{IconName::HeartFilled});
    direction.set(RateDirection::RightToLeft);
    sync(f);
    auto current = f.services.rate().snapshot(id);
    check(factories == 3 && f.services.components().mount_runs() == mounts &&
              f.services.text().mounted_texts().size() == text_count &&
              current.items[1].component == original.items[1].component,
          "reactive per-index paint remounted native contents");
    check(bounds(f, current.items[0].node).x > bounds(f, current.items[1].node).x, "RTL did not reverse item geometry");
    const auto paint = bounds(f, current.items[1].background);
    const auto fill = bounds(f, current.items[1].foreground);
    check(std::abs(fill.width * 2 - paint.width) < .01F &&
              std::abs(fill.x + fill.width - paint.x - paint.width) < .01F &&
              glyphs(f, current.items[1].foreground, fill),
          "RTL half-star did not clip the right half of actual glyphs");
    click(f, current.items[2], .75F);
    check(f.services.rate().snapshot(id).value == 2.5F, "RTL right-half pointer did not select half value");
    key(f, Key::left);
    check(f.services.rate().snapshot(id).value == 3, "RTL Left did not increase");
    key(f, Key::right);
    check(f.services.rate().snapshot(id).value == 2.5F, "RTL Right did not decrease");
    pointer(f, current.items[1], PointerAction::down);
    direction.set(RateDirection::LeftToRight);
    sync(f);
    no_capture(f);
    pointer(f, current.items[1], PointerAction::up);
    check(f.services.rate().snapshot(id).value == 2.5F, "direction change committed an obsolete press");
    count.set(1);
    character.set(RateCharacter{u8"W"});
    count.set(4);
    sync(f);
    check(factories == 4 && f.services.rate().snapshot(id).items[1].component == original.items[1].component,
          "count restoration reran a retained character factory");
    const auto state = f.services.rate().snapshot(id);
    bool rejected{};
    try {
        direction.set(static_cast<RateDirection>(99));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.services.rate().snapshot(id).items[0].component == state.items[0].component,
          "invalid direction partially published");
    direction.set(RateDirection::LeftToRight);
    f.services.dispose();
    character.set(RateCharacter{u8"retired"});
    check(!f.services.next_frame_deadline(), "retired per-index binding retained work");
}

void character_growth_rollback() {
    Fixture f;
    Signal<int> count{1};
    bool fail{true};
    f.services.mount(Content{[&] {
        Rate(RateProps{}.count(count).characterFor([&](std::size_t index) -> Prop<RateCharacter> {
            if (index == 1) {
                return bind([&]() -> RateCharacter {
                    if (fail) {
                        throw std::runtime_error("character source failed");
                    }
                    return RateCharacter{u8"W"};
                });
            }
            return RateCharacter{u8"A"};
        }));
    }});
    sync(f);
    const auto id = f.services.rate().mounted()[0];
    const auto original = f.services.rate().snapshot(id);
    bool threw{};
    try {
        count.set(3);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    const auto failed = f.services.rate().snapshot(id);
    check(threw && failed.count == 1 && failed.items.size() == 1 &&
              failed.items[0].component == original.items[0].component,
          "failed reactive character growth retained stale items or published count");
    count.set(1);
    fail = false;
    count.set(3);
    sync(f);
    check(f.services.rate().snapshot(id).items.size() == 3, "character growth could not recover after rollback");
}

void rate_references_and_events() {
    Fixture f;
    RateRef ref;
    Signal<bool> disabled{false};
    Signal<bool> readonly{false};
    Signal<int> count{3};
    int focuses{};
    int blurs{};
    std::vector<RateKeyEvent> keys;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Rate(RateProps{}
                          .ref(ref)
                          .autoFocus()
                          .disabled(disabled)
                          .readOnly(readonly)
                          .count(count)
                          .onFocus([&] { ++focuses; })
                          .onBlur([&] { ++blurs; })
                          .onKeyDown([&](RateKeyEvent event) { keys.push_back(event); }));
             }});
    }});
    sync(f);
    const auto id = f.services.rate().mounted().front();
    check(ref.bound() && focuses == 1 && ref.focus(), "Rate autoFocus/ref missing");
    key(f, Key::right);
    check(focuses == 1 && blurs == 0 && keys.size() == 1 && keys.back().key == RateKey::Right,
          "modality/pressed updates emitted duplicate focus callbacks");
    f.services.focus().dispatch({Key::up, KeyAction::down, KeyModifier::shift, true});
    check(keys.back().key == RateKey::Up && keys.back().shift && keys.back().repeat,
          "Rate key observer lost unhandled key/modifiers");
    check(ref.blur() && blurs == 1 && !ref.blur(), "Rate blur event mismatch");
    check(ref.focus(), "Rate refocus failed");
    disabled.set(true);
    sync(f);
    check(blurs == 2 && !ref.focus(), "disabled Rate ref bypassed input gate");
    disabled.set(false);
    readonly.set(true);
    check(!ref.focus(), "readOnly Rate ref bypassed edit gate");
    readonly.set(false);
    count.set(0);
    check(!ref.focus(), "empty Rate ref fabricated focus");
    count.set(3);
    bool wrong_thread{};
    std::thread other([&] {
        try {
            static_cast<void>(ref.bound());
        } catch (const std::logic_error&) {
            wrong_thread = true;
        }
    });
    other.join();
    check(wrong_thread, "RateRef accepted foreign thread");
    bool duplicate{};
    try {
        f.services.append_slot(id, Content{[&] { Rate(RateProps{}.ref(ref)); }});
    } catch (const std::invalid_argument&) {
        duplicate = true;
    }
    check(duplicate && ref.bound() && f.services.rate().mounted().size() == 1, "duplicate RateRef did not roll back");
    const auto parent = *f.services.components().parent(id);
    check(f.services.destroy(id) && !ref.bound() && !ref.focus(), "RateRef survived retirement");
    f.services.append_slot(parent, Content{[&] { Rate(RateProps{}.ref(ref)); }});
    sync(f);
    check(ref.focus(), "RateRef could not rebind");
}

void callback_retirement() {
    for (int mode = 0; mode < 3; ++mode) {
        Fixture f;
        RateRef ref;
        runtime::ComponentId id;
        f.services.mount(Content{[&] {
            Rate(RateProps{}
                     .ref(ref)
                     .onFocus([&] {
                         if (mode == 0) {
                             static_cast<void>(f.services.destroy(id));
                         }
                     })
                     .onBlur([&] {
                         if (mode == 1) {
                             static_cast<void>(f.services.destroy(id));
                         }
                     })
                     .onKeyDown([&](RateKeyEvent) {
                         if (mode == 2) {
                             static_cast<void>(f.services.destroy(id));
                         }
                     }));
        }});
        sync(f);
        id = f.services.rate().mounted().front();
        static_cast<void>(ref.focus());
        if (mode == 1) {
            static_cast<void>(ref.blur());
        }
        if (mode == 2) {
            key(f, Key::right);
        }
        check(!ref.bound() && !f.services.components().contains(id), "Rate callback retirement retained owner/ref");
        no_capture(f);
    }
    Fixture f;
    RateRef ref;
    f.services.mount(
        Content{[&] { Rate(RateProps{}.ref(ref).onKeyDown([](RateKeyEvent) { throw std::runtime_error("key"); })); }});
    sync(f);
    static_cast<void>(ref.focus());
    bool threw{};
    try {
        key(f, Key::right);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && f.services.rate().snapshot(f.services.rate().mounted()[0]).value == 0,
          "throwing key callback committed input");
    f.services.dispose();
    check(!ref.bound(), "throwing callback prevented ref cleanup");
}

void slider_focus_events() {
    Fixture f;
    SliderRef ref;
    Signal<SliderDisabledHandles> disabled{SliderDisabledHandles{false, false}};
    int focuses{};
    int blurs{};
    f.services.mount(Content{[&] {
        RangeSlider(RangeSliderProps{}
                        .ref(ref)
                        .defaultValue({20, 80})
                        .handleDisabled(disabled)
                        .onFocus([&] { ++focuses; })
                        .onBlur([&] { ++blurs; }));
    }});
    sync(f);
    const auto row = f.services.slider().mounted().front();
    check(ref.focus(), "Slider ref focus failed");
    key(f, Key::right);
    check(focuses == 1 && blurs == 0, "Slider key gesture duplicated focus event");
    check(f.services.focus().request_focus(row.thumbs[1], FocusModality::keyboard),
          "Slider second handle focus failed");
    check(focuses == 2 && blurs == 1, "Slider handle handoff missed focus callbacks");
    disabled.set({false, true});
    check(blurs == 2, "Slider handle disabled missed blur");
    check(ref.focus(), "Slider ref did not skip disabled handle");
    check(ref.blur() && blurs == 3, "Slider ref blur failed");
    f.services.dispose();
    check(!ref.bound(), "Slider event callbacks blocked retirement");
    Fixture retired;
    runtime::ComponentId id;
    retired.services.mount(
        Content{[&] { Slider(SliderProps{}.onFocus([&] { static_cast<void>(retired.services.destroy(id)); })); }});
    sync(retired);
    const auto target = retired.services.slider().mounted().front();
    id = target.component;
    static_cast<void>(retired.services.focus().request_focus(target.thumbs[0], FocusModality::keyboard));
    check(retired.services.slider().mounted().empty(), "Slider onFocus destruction retained owner");
}

void throwing_retirement() {
    {
        Fixture f;
        RateRef ref;
        f.services.mount(Content{[&] { Rate(RateProps{}.ref(ref).onBlur([] { throw std::runtime_error("blur"); })); }});
        sync(f);
        const auto id = f.services.rate().mounted()[0];
        check(ref.focus(), "throwing Rate blur fixture focus failed");
        check(f.services.destroy(id) && !ref.bound() && f.services.interactions().size() == 0 &&
                  f.services.surfaces().size() == 0,
              "throwing Rate retirement leaked input/scene/ref");
    }
    for (int kind = 0; kind < 3; ++kind) {
        Fixture f;
        detail::SelectionComponentHost host{f.services};
        f.services.mount(Content{[&] {
            const auto fail = [] {
                throw std::runtime_error("blur");
            };
            if (kind == 0) {
                Checkbox(CheckboxProps{}.onBlur(fail));
            }
            if (kind == 1) {
                Radio(RadioProps{}.onBlur(fail));
            }
            if (kind == 2) {
                Switch(SwitchProps{}.onBlur(fail));
            }
        }});
        sync(f);
        const auto row = host.mounted()[0];
        check(f.services.focus().request_focus(row.interaction, FocusModality::keyboard),
              "throwing selection fixture focus failed");
        check(f.services.destroy(row.component) && host.mounted().empty() && f.services.interactions().size() == 0 &&
                  f.services.surfaces().size() == 0 && !f.services.next_frame_deadline(),
              "throwing selection retirement leaked input/scene/deadline");
    }
    {
        Fixture f;
        SliderRef ref;
        f.services.mount(
            Content{[&] { Slider(SliderProps{}.ref(ref).onBlur([] { throw std::runtime_error("Slider blur"); })); }});
        sync(f);
        const auto row = f.services.slider().mounted().front();
        check(ref.focus(), "throwing Slider blur fixture focus failed");
        const auto area = f.nodes.require(f.services.interactions().require(row.thumbs.front()).node).bounds;
        f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                       area.x + area.width / 2, area.y + area.height / 2});
        const auto captured = f.services.pointer().state(PointerIdentity::mouse());
        check(captured && captured->capture, "throwing Slider retirement fixture lacks capture");
        check(f.services.destroy(row.component) && !ref.bound() && f.services.interactions().size() == 0 &&
                  f.services.surfaces().size() == 0 && !f.services.next_frame_deadline(),
              "throwing Slider retirement leaked input/scene/deadline");
        no_capture(f);
    }
}

void selection_focus_events() {
    for (int kind = 0; kind < 3; ++kind) {
        Fixture f;
        detail::SelectionComponentHost host{f.services};
        int focuses{};
        int blurs{};
        Signal<bool> disabled{false};
        f.services.mount(Content{[&] {
            if (kind == 0) {
                Checkbox(CheckboxProps{}.disabled(disabled).onFocus([&] { ++focuses; }).onBlur([&] { ++blurs; }));
            }
            if (kind == 1) {
                Radio(RadioProps{}.disabled(disabled).onFocus([&] { ++focuses; }).onBlur([&] { ++blurs; }));
            }
            if (kind == 2) {
                Switch(SwitchProps{}.disabled(disabled).onFocus([&] { ++focuses; }).onBlur([&] { ++blurs; }));
            }
        }});
        sync(f);
        const auto row = host.mounted().front();
        check(f.services.focus().request_focus(row.interaction, FocusModality::keyboard), "selection focus failed");
        key(f, Key::space);
        check(focuses == 1 && blurs == 0, "selection pressed changes duplicated focus event");
        disabled.set(true);
        check(blurs == 1, "selection disabled did not report blur");
        disabled.set(false);
        check(f.services.focus().request_focus(row.interaction, FocusModality::keyboard), "selection refocus failed");
        static_cast<void>(f.services.destroy(row.component));
        check(focuses == 2 && blurs == 2, "selection retirement lost actual blur");
    }
    Fixture f;
    detail::SelectionComponentHost host{f.services};
    runtime::ComponentId id;
    f.services.mount(
        Content{[&] { Checkbox(CheckboxProps{}.onFocus([&] { static_cast<void>(f.services.destroy(id)); })); }});
    sync(f);
    const auto row = host.mounted().front();
    id = row.component;
    static_cast<void>(f.services.focus().request_focus(row.interaction, FocusModality::keyboard));
    check(host.mounted().empty() && !f.services.focus().state().focused, "selection focus callback retirement failed");
}
} // namespace

int main() {
    try {
        direction_and_characters();
        character_growth_rollback();
        rate_references_and_events();
        callback_retirement();
        selection_focus_events();
        slider_focus_events();
        throwing_retirement();
        std::cout << "Existing DataEntry completion contracts passed: 7 groups\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
