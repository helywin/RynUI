#include "component/feedback_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void delay_percent_identity() {
    Fixture f;
    Signal<bool> loading{true};
    Signal<SpinPercent> percent{SpinPercent{SpinAutomaticPercent{}}};
    int mounts{};
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(loading).delay(std::chrono::milliseconds{200}).percent(percent), SpinContent{[&] {
                 ++mounts;
                 Input(InputProps{}.defaultValue(u8"draft").layout(LayoutStyle{}.width(dp(240)).height(dp(80))));
             }});
    }});
    f.services.set_motion_preference(animation::MotionPreference::normal);
    f.synchronize();
    const auto id = f.services.feedback().spins()[0];
    const auto initial = f.services.feedback().spin_snapshot(id);
    check(initial.requested && !initial.spinning && f.services.next_frame_deadline().has_value(),
          "Spin delay was not armed");
    loading.set(false);
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(500000)));
    f.synchronize();
    check(!f.services.feedback().spin_snapshot(id).spinning && !f.services.next_frame_deadline(),
          "cancelled delay displayed a stale Spin");
    loading.set(true);
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(700000)));
    f.synchronize();
    check(f.services.feedback().spin_snapshot(id).spinning, "Spin missed its delayed reveal");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(900000)));
    f.synchronize();
    check(f.services.feedback().spin_snapshot(id).percent.value_or(0) == 5,
          "Spin auto first pinned 200ms step is not 5%");
    percent.set(SpinPercent{35.0F});
    f.synchronize();
    check(f.services.feedback().spin_snapshot(id).percent == 35 && !f.services.next_frame_deadline(),
          "numeric percent kept an animation timer");
    check(f.services.feedback().spin_snapshot(id).content == initial.content && mounts == 1,
          "Spin remounted retained content");
    percent.set(SpinPercent{SpinAutomaticPercent{}});
    f.synchronize();
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    f.synchronize();
    check(!f.services.next_frame_deadline(), "reduced motion kept auto percent deadline");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    f.synchronize(320, {300, 200, 10, 10});
    check(!f.services.next_frame_deadline(), "offscreen Spin kept auto deadline");
    f.services.destroy(id);
    check(!f.services.next_frame_deadline(), "Spin destruction kept a timer");
}

void fullscreen_gate_cleanup() {
    Fixture f;
    Signal<bool> spinning{false};
    Signal<bool> fullscreen{true};
    int clicks{};
    f.services.mount(Content{[&] {
        Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Outside"); });
        Spin(SpinProps{}.spinning(spinning).fullscreen(fullscreen),
             SpinSlots{.indicator = SpinIndicator{[] { Text(u8"Working"); }}});
    }});
    f.synchronize();
    const auto outside = f.buttons.mounted_buttons()[0];
    const auto id = f.services.feedback().spins()[0];
    check(f.services.focus().request_focus(outside.interaction, input::FocusModality::keyboard),
          "outside focus failed");
    spinning.set(true);
    f.synchronize();
    const auto view = f.services.feedback().spin_snapshot(id);
    check(view.overlay_bounds == runtime::Rect{0, 0, 320, 240} &&
              f.services.components().is_window_layer_root(view.overlay),
          "fullscreen Spin is not a viewport layer");
    check(!f.services.focus().state().focused && !f.services.interactions().component_active(outside.component),
          "fullscreen did not release focus and gate outside input");
    check(f.services.hit_test().hit_test({319, 239}) == view.shield, "fullscreen shield does not cover viewport");
    fullscreen.set(false);
    f.synchronize();
    check(f.services.interactions().component_active(outside.component), "switching fullscreen off kept global gate");
    fullscreen.set(true);
    f.synchronize();
    f.services.destroy(id);
    f.synchronize();
    check(f.services.interactions().component_active(outside.component) && f.services.interactions().size() == 1,
          "fullscreen owner destruction leaked gate/shield");
}

void local_failure() {
    Fixture f;
    f.services.mount(Content{[&] {
        try {
            Spin({}, SpinContent{[] { throw std::runtime_error("content failed"); }});
        } catch (const std::runtime_error&) {
        }
        Spin(SpinProps{}.spinning(false));
    }});
    f.synchronize();
    check(f.services.feedback().spins().size() == 1 && f.services.interactions().size() == 1,
          "locally caught Spin failure leaked owner/shield");
}

void overlapping_fullscreen_and_blur_retirement() {
    Fixture f;
    Signal<bool> first{false};
    Signal<bool> second{false};
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.fullscreen(true).spinning(first),
             SpinSlots{.indicator = SpinIndicator{[] { Button({}, [] { Text(u8"First overlay control"); }); }}});
        Spin(SpinProps{}.fullscreen(true).spinning(second),
             SpinSlots{.indicator = SpinIndicator{[] { Button({}, [] { Text(u8"Second overlay control"); }); }}});
    }});
    first.set(true);
    second.set(true);
    f.synchronize();
    const auto lower = f.buttons.mounted_buttons()[0].component;
    const auto upper = f.buttons.mounted_buttons()[1].component;
    check(!f.services.interactions().component_active(lower) && f.services.interactions().component_active(upper),
          "overlapping fullscreen leases intersected into an empty input domain");
    f.services.destroy(f.services.feedback().spins().back());
    f.synchronize();
    check(f.services.interactions().component_active(lower),
          "destroying top fullscreen did not restore previous domain");
    f.services.dispose();

    auto recovery = std::make_unique<Fixture>();
    Signal<bool> loading{false};
    runtime::ComponentId owner;
    InputRef input;
    recovery->services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(loading),
             SpinContent{[&] { Input(InputProps{}.ref(input).onBlur([&] { recovery->services.destroy(owner); })); }});
    }});
    owner = recovery->services.feedback().spins().front();
    recovery->synchronize();
    check(input.focus(), "Spin blur retirement input did not focus");
    loading.set(true);
    recovery->synchronize();
    check(!recovery->services.components().contains(owner) && recovery->services.feedback().spins().empty() &&
              recovery->services.interactions().size() == 0 && !recovery->services.next_frame_deadline(),
          "Spin used stale state or leaked gate after blur destroyed its owner");
}
} // namespace

int main() {
    try {
        delay_percent_identity();
        fullscreen_gate_cleanup();
        local_failure();
        overlapping_fullscreen_and_blur_retirement();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
