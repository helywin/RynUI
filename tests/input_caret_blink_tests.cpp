#include "component/input_caret_blink.hpp"
#include "support/input_fixture.hpp"
#include "support/input_counting_gpu.hpp"
#include "support/allocation_probe.hpp"
#include <iostream>

namespace {
using namespace ryn; using namespace ryn::input; using namespace ryn::animation;
using Fixture = ryn_test::input_component::Fixture;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
AnimationTime at(std::int64_t us) { return AnimationTime::microseconds(us); }
void clock_contract() {
    for(int hz : {60, 120, 144}) {
        detail::InputCaretBlink blink;
        require(blink.configure(true, true, {}) && blink.visible() && blink.deadline() == at(500000), "initial blink differs");
        int changes{};
        for(int frame = 1; frame <= hz * 2; ++frame) {
            const auto time = std::int64_t(frame) * 1000000 / hz;
            changes += blink.tick(at(time));
            require(blink.visible() == ((time / 500000) % 2 == 0), "blink drifted with refresh cadence");
            require(blink.deadline() == at((time / 500000 + 1) * 500000), "blink deadline polled at refresh cadence");
        }
        require(changes == 4, "blink emitted per-frame changes");
        blink.configure(true, true, at(2200000), true);
        require(blink.visible() && blink.deadline() == at(2700000), "input did not reset blink");
        blink.tick(at(4200000));
        require(blink.visible() && blink.deadline() == at(4700000), "missed blink deadlines replayed/drifted");
        blink.configure(true, false, at(4300000));
        require(blink.visible() && !blink.deadline(), "reduced motion retained deadline");
        blink.stop(); require(!blink.visible() && !blink.deadline(), "stop retained caret");
    }
    detail::InputCaretBlink blink; blink.configure(true, true, {});
    ryn_test::allocation::begin();
    for(std::int64_t i = 1; i <= 20000; ++i) blink.tick(at(i * 500000));
    const auto allocations = ryn_test::allocation::end();
    require(allocations == 0, "deadline toggles allocated");
    blink.configure(true, true, at(std::numeric_limits<std::int64_t>::max()), true);
    require(blink.visible() && !blink.deadline(), "clock exhaustion did not remain static/idle");
}
class Events final : public runtime::FrameEventSource {
public:
    AnimationTime current{at(200000)}; std::uint32_t waited{};
    AnimationTime now() const noexcept override { return current; }
    bool poll_frame_event() noexcept override { return false; }
    bool wait_for_frame_event(std::uint32_t ms) noexcept override {
        waited = ms; current = current + AnimationDuration::microseconds(std::int64_t(ms) * 1000); return false;
    }
};
struct Submitter final : runtime::FrameSubmitter {
    Fixture& f;
    ryn_test::input_component::CountingGpu gpu;
    graphics::QuadGpuBuffer quads;
    detail::GlyphGpuResources glyphs;
    detail::RoundedEffectGpuResources effects;
    std::size_t calls{};
    explicit Submitter(Fixture& value) : f(value), quads(gpu, f.buttons.button_scene().instances()), glyphs(gpu), effects(gpu) {}
    runtime::FrameSubmissionResult submit_frame(AnimationTime time) override {
        ++calls; static_cast<void>(f.buttons.tick_animations(time)); f.synchronize();
        quads.synchronize(f.buttons.button_scene().instances());
        glyphs.synchronize(f.scene.atlas(), f.scene.glyph_scene().instances());
        effects.synchronize(f.buttons.rounded_effects(), {320, 240, 1});
        return runtime::FrameSubmissionResult::submitted;
    }
};
void frame_and_gpu() {
    Fixture f; f.buttons.set_motion_preference(MotionPreference::normal);
    Signal<bool> disabled{false}, read_only{false}; Signal<ThemeConfig> theme{ThemeConfig{}};
    f.inputs.mount(Content{[&] { Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
        Input(InputProps{}.defaultValue(u8"abc中文").disabled(disabled).readOnly(read_only));
    }}); }});
    const auto input = f.inputs.mounted_inputs().front(); f.synchronize();
    require(f.buttons.focus().request_focus(input.interaction, FocusModality::keyboard), "blink focus failed");
    Submitter submitter{f}; submitter.submit_frame(at(200000));
    static_cast<void>(f.frames.consume_request());
    require(!f.buttons.animations().next_deadline() && f.buttons.next_deadline() == at(500000), "host dropped caret deadline");
    const auto glyph_uploads = submitter.gpu.glyph_uploads, texture_uploads = submitter.gpu.texture_uploads;
    const auto effect_uploads = submitter.gpu.effect_uploads, quad_uploads = submitter.gpu.quad_uploads;
    const auto shapes = f.scene.text_state(f.inputs.text_scene(input.component)).counters().shape_count;
    const auto measures = f.nodes.require(input.node).measure_count;
    const auto rebuilds = f.buttons.scene_composer().diagnostics().rebuilds;
    Events events; runtime::OnDemandFrameLoop loop{f.frames, events, submitter, f.buttons, 1000};
    require(loop.step() == runtime::FrameLoopStep::submitted && events.waited == 300 && submitter.calls == 2,
        "caret did not wait directly for deadline");
    require(f.buttons.button_scene().instances().at(1).opacity == 0 && f.buttons.next_deadline() == at(1000000), "caret did not blink off");
    require(!f.frames.pending(), "caret tick scheduled a redundant frame");
    require(submitter.gpu.quad_uploads == quad_uploads + 1 && submitter.gpu.glyph_uploads == glyph_uploads
        && submitter.gpu.texture_uploads == texture_uploads && submitter.gpu.effect_uploads == effect_uploads,
        "caret blink uploaded unrelated resources");
    require(loop.step() == runtime::FrameLoopStep::submitted && events.waited == 500
        && f.buttons.button_scene().instances().at(1).opacity == 1, "caret cadence depended on frames");
    f.buttons.set_animation_time(at(1100000));
    f.buttons.focus().dispatch({Key::left, KeyAction::down});
    require(f.inputs.next_caret_deadline() == at(1600000), "navigation did not reset blink deadline");
    f.buttons.set_animation_time(at(1150000));
    require(bool(f.inputs.dispatch(TextCommitted{String{}, f.inputs.sessions().active()})), "empty text event failed");
    require(f.inputs.next_caret_deadline() == at(1650000), "valid input without value change did not reset blink");
    submitter.submit_frame(at(1150000)); static_cast<void>(f.frames.consume_request());
    f.buttons.set_motion_preference(MotionPreference::reduced); submitter.submit_frame(at(1200000));
    static_cast<void>(f.frames.consume_request());
    require(!f.buttons.next_deadline() && f.buttons.button_scene().instances().at(1).opacity == 1, "reduced motion did not settle static caret");
    events.current = at(1200000); const auto calls = submitter.calls;
    for(int i = 0; i < 4; ++i) require(loop.step() == runtime::FrameLoopStep::idle, "static caret kept submitting");
    require(submitter.calls == calls && events.waited == 1000, "static caret polled its deadline");
    f.buttons.set_animation_time(events.current); f.buttons.set_motion_preference(MotionPreference::normal);
    require(f.inputs.next_caret_deadline().has_value(), "normal motion did not restore blink");
    ThemeConfig off; off.seed.motion = false; theme.set(off); f.synchronize();
    require(!f.inputs.next_caret_deadline() && f.buttons.button_scene().instances().at(1).opacity == 1, "Theme motion=false retained blink");
    theme.set(ThemeConfig{}); read_only.set(true); f.synchronize();
    require(!f.inputs.next_caret_deadline() && f.buttons.button_scene().instances().at(1).opacity == 0, "readOnly retained caret");
    read_only.set(false); require(f.inputs.next_caret_deadline().has_value(), "editable did not restore blink");
    f.inputs.set_window_active(false); require(!f.inputs.next_caret_deadline(), "window loss retained blink");
    f.inputs.set_window_active(true); disabled.set(true); require(!f.inputs.next_caret_deadline(), "disable retained blink");
    require(f.scene.text_state(f.inputs.text_scene(input.component)).counters().shape_count == shapes
        && f.nodes.require(input.node).measure_count == measures
        && f.buttons.scene_composer().diagnostics().rebuilds == rebuilds, "blink reshaped/laid out/rebuilt scene");
    disabled.set(false); require(f.buttons.focus().request_focus(input.interaction, FocusModality::keyboard), "refocus failed");
    require(f.inputs.next_caret_deadline().has_value(), "destroy test had no deadline");
    require(f.buttons.destroy(input.component) && !f.buttons.next_deadline(), "last caret destroy retained work");
}
}
int main() {
    try { clock_contract(); frame_and_gpu(); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
