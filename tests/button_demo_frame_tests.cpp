#include "component/button_component.hpp"
#include "renderer/sdl/glyph_gpu_resources.hpp"
#include "renderer/sdl/rounded_effect_gpu_resources.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/invalidation.hpp"

#include <ryn/rynui.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ryn::String click_label(std::uint64_t clicks) {
    auto parsed = ryn::String::from_utf8(
        "Clicks / 点击次数: " + std::to_string(clicks));
    require(static_cast<bool>(parsed), "click label is not valid UTF-8");
    return std::move(parsed).value();
}

class RecordingGpuApi final : public ryn::graphics::QuadUploadApi,
                              public ryn::detail::GlyphGpuApi,
                              public ryn::detail::RoundedEffectGpuApi {
public:
    void* create_vertex_buffer(std::size_t) override {
        return handle(next_++);
    }

    void release_buffer(void*) noexcept override {}

    bool upload(
        void*,
        std::size_t,
        std::span<const std::byte> bytes) override {
        ++quad_uploads;
        quad_uploaded_bytes += bytes.size();
        return true;
    }

    const char* last_error() const noexcept override { return ""; }

    void* create_glyph_sampler() override { return handle(next_++); }

    void* create_glyph_texture(std::uint32_t, std::uint32_t) override {
        return handle(next_++);
    }

    void* create_glyph_buffer(std::size_t) override {
        return handle(next_++);
    }

    bool upload_glyph_texture(
        void*,
        const ryn::detail::GlyphTextureUpload& upload) override {
        ++glyph_texture_uploads;
        glyph_uploaded_bytes += upload.bytes.size();
        return true;
    }

    bool upload_glyph_buffer(
        void*,
        std::size_t,
        std::span<const std::byte> bytes) override {
        ++glyph_buffer_uploads;
        glyph_uploaded_bytes += bytes.size();
        return true;
    }

    void release_glyph_buffer(void*) noexcept override {}
    void release_glyph_texture(void*) noexcept override {}
    void release_glyph_sampler(void*) noexcept override {}
    const char* glyph_gpu_error() const noexcept override { return ""; }

    void* create_effect_buffer(std::size_t) override {
        return handle(next_++);
    }

    bool upload_effect_buffer(
        void*,
        std::size_t,
        std::span<const std::byte> bytes) override {
        ++effect_buffer_uploads;
        effect_uploaded_bytes += bytes.size();
        return true;
    }

    void release_effect_buffer(void*) noexcept override {}
    const char* effect_gpu_error() const noexcept override { return ""; }

    static void* handle(std::uintptr_t value) {
        return reinterpret_cast<void*>(value);
    }

    std::uintptr_t next_{1};
    std::uint64_t quad_uploads{};
    std::uint64_t quad_uploaded_bytes{};
    std::uint64_t glyph_texture_uploads{};
    std::uint64_t glyph_buffer_uploads{};
    std::uint64_t glyph_uploaded_bytes{};
    std::uint64_t effect_buffer_uploads{};
    std::uint64_t effect_uploaded_bytes{};
};

class RecordingDrawApi final : public ryn::detail::SceneDrawApi {
public:
    void draw_quad(std::uint32_t, std::uint32_t count) override {
        ++quad_draws;
        quad_instances += count;
    }

    void draw_glyph(
        std::uint32_t,
        std::uint32_t,
        std::uint32_t count) override {
        ++glyph_draws;
        glyph_instances += count;
    }

    void draw_rounded_effect(std::uint32_t, std::uint32_t count) override {
        ++effect_draws;
        effect_instances += count;
    }

    std::uint64_t quad_draws{};
    std::uint64_t quad_instances{};
    std::uint64_t glyph_draws{};
    std::uint64_t glyph_instances{};
    std::uint64_t effect_draws{};
    std::uint64_t effect_instances{};
};

struct Fixture final {
    Fixture()
        : fonts(create_runtime()),
          engine(*fonts),
          layout(nodes),
          dirty(nodes, &frames),
          text_scene(*fonts, engine, frames) {
        const auto latin = fonts->load_font_file(
            RYNUI_VALIDATION_LATIN_FONT, 0, 14);
        const auto cjk = fonts->load_font_file(
            RYNUI_VALIDATION_CJK_FONT, 0, 14);
        require(latin && cjk, "Button demo frame fonts failed to load");
        host = std::make_unique<ryn::detail::ButtonComponentHost>(
            nodes,
            layout,
            dirty,
            text_scene,
            std::vector<ryn::font::FontIdentity>{latin.font, cjk.font},
            frames);
    }

    static std::unique_ptr<ryn::font::FontRuntime> create_runtime() {
        auto created = ryn::font::FontRuntime::create();
        require(static_cast<bool>(created), "Font Runtime initialization failed");
        return std::move(created.runtime);
    }

    std::unique_ptr<ryn::font::FontRuntime> fonts;
    ryn::text::TextEngine engine;
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::NodeStore nodes;
    ryn::layout::LayoutEngine layout;
    ryn::runtime::DirtyQueues dirty;
    ryn::detail::TextSceneService text_scene;
    std::unique_ptr<ryn::detail::ButtonComponentHost> host;
};

class ControlledInputEvents final : public ryn::runtime::FrameEventSource {
public:
    ControlledInputEvents(
        ryn::detail::ButtonComponentHost& host,
        ryn::runtime::FrameRequestState& frames) noexcept
        : host_(&host), frames_(&frames) {}

    std::uint64_t now_milliseconds() const noexcept override { return now_; }

    bool poll_frame_event() noexcept override {
        return dispatch_next();
    }

    bool wait_for_frame_event(std::uint32_t timeout) noexcept override {
        now_ += timeout;
        return dispatch_next();
    }

    void push(ryn::input::PlatformInputEvent event) {
        events_.push_back(std::move(event));
    }

    [[nodiscard]] std::uint64_t input_events() const noexcept {
        return dispatched_;
    }

    [[nodiscard]] bool failed() const noexcept { return failed_; }

private:
    bool dispatch_next() noexcept {
        if (next_ >= events_.size()) {
            return false;
        }
        const auto& event = events_[next_++];
        try {
            std::visit([this](const auto& value) {
                dispatch(value);
            }, event);
            ++dispatched_;
            return frames_->pending();
        } catch (...) {
            failed_ = true;
            return true;
        }
    }

    void dispatch(const ryn::input::PointerInputEvent& event) {
        host_->pointer().dispatch(event);
    }

    void dispatch(const ryn::input::KeyboardInputEvent& event) {
        host_->focus().dispatch(event);
    }

    void dispatch(const ryn::input::WindowInputEvent& event) {
        switch (event.action) {
        case ryn::input::WindowInputAction::focus_gained:
            host_->set_window_active(true);
            return;
        case ryn::input::WindowInputAction::focus_lost:
            host_->set_window_active(false);
            return;
        case ryn::input::WindowInputAction::resized:
        case ryn::input::WindowInputAction::invalid:
            throw std::invalid_argument("unsupported controlled Window event");
        }
    }

    ryn::detail::ButtonComponentHost* host_;
    ryn::runtime::FrameRequestState* frames_;
    std::vector<ryn::input::PlatformInputEvent> events_;
    std::size_t next_{};
    std::uint64_t now_{};
    std::uint64_t dispatched_{};
    bool failed_{};
};

class HeadlessButtonSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    HeadlessButtonSubmitter(
        ryn::detail::ButtonComponentHost& host,
        ryn::detail::TextSceneService& text_scene,
        RecordingGpuApi& gpu,
        RecordingDrawApi& draw) noexcept
        : host_(&host),
          text_scene_(&text_scene),
          gpu_(&gpu),
          glyph_resources_(gpu),
          effect_resources_(gpu),
          draw_(&draw) {}

    ryn::runtime::FrameSubmissionResult submit_frame() override {
        if (!host_->layout_and_synchronize(
                viewport_,
                {0.0F, 0.0F, viewport_.width, viewport_.height},
                {24.0F, 20.0F},
                8.0F)) {
            return ryn::runtime::FrameSubmissionResult::failed;
        }
        if (quad_buffer_ == nullptr) {
            quad_buffer_ = std::make_unique<ryn::graphics::QuadGpuBuffer>(
                *gpu_, host_->button_scene().instances());
        } else {
            host_->button_scene().synchronize_gpu(*quad_buffer_);
        }
        glyph_resources_.synchronize(
            text_scene_->atlas(),
            text_scene_->glyph_scene().instances());
        effect_resources_.synchronize(
            host_->rounded_effects(),
            {640, 360, 1.0F});
        ryn::detail::draw_ordered_scene(
            host_->scene_composer().ordered_scene(), *draw_);
        return ryn::runtime::FrameSubmissionResult::submitted;
    }

    [[nodiscard]] const ryn::graphics::QuadUploadCounters&
    quad_counters() const {
        require(quad_buffer_ != nullptr, "Quad GPU buffer was not created");
        return quad_buffer_->counters();
    }

private:
    ryn::detail::ButtonComponentHost* host_;
    ryn::detail::TextSceneService* text_scene_;
    RecordingGpuApi* gpu_;
    ryn::detail::GlyphGpuResources glyph_resources_;
    ryn::detail::RoundedEffectGpuResources effect_resources_;
    RecordingDrawApi* draw_;
    ryn::runtime::Size viewport_{640.0F, 360.0F};
    std::unique_ptr<ryn::graphics::QuadGpuBuffer> quad_buffer_;
};

ryn::input::PointerInputEvent pointer_event(
    ryn::input::PointerAction action,
    ryn::runtime::Point point,
    ryn::input::PointerButton button = ryn::input::PointerButton::none) {
    return {
        ryn::input::PointerIdentity::mouse(),
        action,
        button,
        point.x,
        point.y,
    };
}

ryn::input::KeyboardInputEvent key_event(
    ryn::input::Key key,
    ryn::input::KeyAction action,
    bool repeat = false) {
    return {key, action, ryn::input::KeyModifier::none, repeat};
}

void require_step(
    ControlledInputEvents& events,
    ryn::runtime::OnDemandFrameLoop& loop,
    ryn::input::PlatformInputEvent event,
    ryn::runtime::FrameLoopStep expected,
    const char* message) {
    events.push(std::move(event));
    require(loop.step() == expected, message);
}

void test_public_button_demo_input_frame_and_idle_contract() {
    Fixture fixture;
    ryn::Signal<ryn::ButtonType> type{ryn::ButtonType::Default};
    ryn::Signal<ryn::ControlSize> size{ryn::ControlSize::Middle};
    ryn::Signal<bool> disabled{false};
    ryn::Signal<bool> loading{false};
    ryn::Signal<ryn::String> observed_clicks{click_label(0)};
    std::uint64_t clicks = 0;
    fixture.host->mount(ryn::Content{[&] {
        ryn::Button(
            ryn::ButtonProps{}
                .type(type)
                .size(size)
                .disabled(disabled)
                .loading(loading)
                .onClick([&] {
                    ++clicks;
                    observed_clicks.set(click_label(clicks));
                }),
            [] { ryn::Text(u8"Submit / 提交"); });
        ryn::Text(
            ryn::TextProps{}
                .content(observed_clicks)
                .tone(ryn::TextTone::Secondary));
    }});

    RecordingGpuApi gpu;
    RecordingDrawApi draw;
    ControlledInputEvents events(*fixture.host, fixture.frames);
    HeadlessButtonSubmitter submitter(
        *fixture.host, fixture.text_scene, gpu, draw);
    ryn::runtime::OnDemandFrameLoop loop(
        fixture.frames, events, submitter, 5);

    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "initial Button demo frame was not submitted");
    const auto target = fixture.host->mounted_buttons()[0];
    const auto& initial_bounds = fixture.nodes.require(target.node).bounds;
    const ryn::runtime::Point inside{
        initial_bounds.x + initial_bounds.width * 0.5F,
        initial_bounds.y + initial_bounds.height * 0.5F,
    };
    const ryn::runtime::Point outside{500.0F, 300.0F};
    require(draw.quad_draws > 0
                && draw.glyph_draws > 0
                && gpu.quad_uploads == 1
                && gpu.glyph_texture_uploads > 0
                && gpu.glyph_buffer_uploads == 1
                && gpu.effect_buffer_uploads == 1
                && fixture.host->scene_composer().diagnostics().rebuilds == 1,
            "initial Button frame missed retained upload/draw work");

    type.set(ryn::ButtonType::Primary);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "reactive Button type did not submit a frame");
    size.set(ryn::ControlSize::Large);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "reactive Button size did not submit a frame");
    disabled.set(true);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "reactive Button disabled state did not submit a frame");
    require_step(
        events,
        loop,
        pointer_event(
            ryn::input::PointerAction::down,
            inside,
            ryn::input::PointerButton::primary),
        ryn::runtime::FrameLoopStep::idle,
        "disabled Button input submitted or activated");
    disabled.set(false);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "reactive Button re-enable did not submit a frame");

    require_step(
        events,
        loop,
        pointer_event(
            ryn::input::PointerAction::down,
            inside,
            ryn::input::PointerButton::primary),
        ryn::runtime::FrameLoopStep::submitted,
        "pointer down did not submit pressed state");
    require_step(
        events,
        loop,
        pointer_event(
            ryn::input::PointerAction::up,
            inside,
            ryn::input::PointerButton::primary),
        ryn::runtime::FrameLoopStep::submitted,
        "pointer up did not submit click state");
    require(clicks == 1, "pointer click did not invoke exactly once");

    require_step(
        events,
        loop,
        pointer_event(
            ryn::input::PointerAction::down,
            inside,
            ryn::input::PointerButton::primary),
        ryn::runtime::FrameLoopStep::submitted,
        "drag-out pointer down did not submit");
    require_step(
        events,
        loop,
        pointer_event(ryn::input::PointerAction::move, outside),
        ryn::runtime::FrameLoopStep::submitted,
        "captured drag-out did not submit hover transition");
    require_step(
        events,
        loop,
        pointer_event(
            ryn::input::PointerAction::up,
            outside,
            ryn::input::PointerButton::primary),
        ryn::runtime::FrameLoopStep::submitted,
        "drag-out release did not settle pressed state");
    require(clicks == 1, "drag-out produced a click");

    require_step(
        events,
        loop,
        key_event(ryn::input::Key::tab, ryn::input::KeyAction::down),
        ryn::runtime::FrameLoopStep::submitted,
        "Tab did not submit focus-visible state");
    require_step(
        events,
        loop,
        key_event(ryn::input::Key::enter, ryn::input::KeyAction::down),
        ryn::runtime::FrameLoopStep::submitted,
        "Enter did not activate the focused Button");
    require_step(
        events,
        loop,
        key_event(ryn::input::Key::enter, ryn::input::KeyAction::down, true),
        ryn::runtime::FrameLoopStep::idle,
        "repeated Enter submitted or activated");
    require_step(
        events,
        loop,
        key_event(ryn::input::Key::enter, ryn::input::KeyAction::up),
        ryn::runtime::FrameLoopStep::idle,
        "Enter key-up submitted an unchanged frame");
    require_step(
        events,
        loop,
        key_event(ryn::input::Key::space, ryn::input::KeyAction::down),
        ryn::runtime::FrameLoopStep::submitted,
        "Space down did not submit pressed state");
    require_step(
        events,
        loop,
        key_event(ryn::input::Key::space, ryn::input::KeyAction::up),
        ryn::runtime::FrameLoopStep::submitted,
        "Space up did not activate and settle state");
    require(clicks == 3, "Enter and Space did not each click exactly once");

    loading.set(true);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "reactive loading state did not submit a frame");
    require_step(
        events,
        loop,
        key_event(ryn::input::Key::enter, ryn::input::KeyAction::down),
        ryn::runtime::FrameLoopStep::idle,
        "loading Button accepted keyboard activation");
    require_step(
        events,
        loop,
        pointer_event(
            ryn::input::PointerAction::down,
            inside,
            ryn::input::PointerButton::primary),
        ryn::runtime::FrameLoopStep::submitted,
        "loading Button did not submit pointer hover transition");
    const auto loading_pointer = fixture.host->pointer().state(
        ryn::input::PointerIdentity::mouse());
    require(clicks == 3
                && !fixture.host->snapshot(target.component).pointer_pressed
                && loading_pointer.has_value()
                && !loading_pointer->capture.has_value(),
            "loading Button accepted a press or produced a duplicate click");

    require_step(
        events,
        loop,
        ryn::input::WindowInputEvent{
            ryn::input::WindowInputAction::focus_lost, 0, 0},
        ryn::runtime::FrameLoopStep::submitted,
        "window focus loss did not submit cleared focus state");
    loading.set(false);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "loading completion did not submit a frame");

    const auto submissions = loop.counters().submissions;
    for (int index = 0; index < 60; ++index) {
        require(loop.step() == ryn::runtime::FrameLoopStep::idle,
                "stable Button demo continued submitting frames");
    }
    const auto& pointer_diagnostics = fixture.host->pointer().diagnostics();
    const auto& focus_diagnostics = fixture.host->focus().diagnostics();
    const auto& hit_test_diagnostics = fixture.host->hit_test().diagnostics();
    const auto& quad_counters = submitter.quad_counters();
    require(loop.counters().submissions == submissions
                && loop.counters().idle_waits >= 60
                && !events.failed()
                && events.input_events() >= 14
                && hit_test_diagnostics.queries > 0
                && pointer_diagnostics.routes_dispatched > 0
                && pointer_diagnostics.captures_started == 2
                && pointer_diagnostics.captures_released == 2
                && focus_diagnostics.focus_changes > 0
                && fixture.host->scene_composer().diagnostics().rebuilds == 1
                && quad_counters.range_uploads > 0
                && gpu.glyph_buffer_uploads > 1
                && gpu.effect_buffer_uploads > 1
                && draw.quad_draws > 1
                && draw.glyph_draws > 1
                && draw.effect_draws > 1,
            "Button demo diagnostics missed input/scene/upload/draw/idle evidence");
}

} // namespace

int main() {
    try {
        test_public_button_demo_input_frame_and_idle_contract();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
