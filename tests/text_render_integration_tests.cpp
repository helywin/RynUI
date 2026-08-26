#include "renderer/sdl/glyph_gpu_resources.hpp"
#include "renderer/sdl/text_render_controller.hpp"
#include "runtime/frame_scheduler.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class RecordingGpuApi final : public ryn::detail::GlyphGpuApi {
public:
    void* create_glyph_sampler() override { return handle(next_++); }
    void* create_glyph_texture(std::uint32_t, std::uint32_t) override {
        return handle(next_++);
    }
    void* create_glyph_buffer(std::size_t) override { return handle(next_++); }
    bool upload_glyph_texture(
        void*,
        const ryn::detail::GlyphTextureUpload&) override {
        ++texture_uploads;
        return true;
    }
    bool upload_glyph_buffer(
        void*,
        std::size_t,
        std::span<const std::byte>) override {
        ++buffer_uploads;
        return true;
    }
    void release_glyph_buffer(void*) noexcept override {}
    void release_glyph_texture(void*) noexcept override {}
    void release_glyph_sampler(void*) noexcept override {}
    const char* glyph_gpu_error() const noexcept override { return ""; }

    static void* handle(std::uintptr_t value) {
        return reinterpret_cast<void*>(value);
    }

    std::uintptr_t next_{1};
    std::size_t texture_uploads{};
    std::size_t buffer_uploads{};
};

class ControlledEvents final : public ryn::runtime::FrameEventSource {
public:
    std::uint64_t now_milliseconds() const noexcept override { return now_; }
    bool poll_frame_event() noexcept override { return false; }
    bool wait_for_frame_event(std::uint32_t timeout) noexcept override {
        now_ += timeout;
        return false;
    }

private:
    std::uint64_t now_{};
};

class TextSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    TextSubmitter(
        ryn::detail::TextRenderController& controller,
        ryn::detail::GlyphGpuResources& resources) noexcept
        : controller_(&controller), resources_(&resources) {}

    ryn::runtime::FrameSubmissionResult submit_frame() override {
        if (!controller_->synchronize(placement_)) {
            return ryn::runtime::FrameSubmissionResult::failed;
        }
        resources_->synchronize(
            controller_->atlas(),
            controller_->glyph_scene().instances());
        return ryn::runtime::FrameSubmissionResult::submitted;
    }

    ryn::graphics::GlyphPlacement placement_{
        {24.0F, 24.0F},
        {640.0F, 360.0F},
        {0.0F, 0.0F, 640.0F, 360.0F},
        {0.0F, 0.0F},
        {},
        1.0F,
    };

private:
    ryn::detail::TextRenderController* controller_;
    ryn::detail::GlyphGpuResources* resources_;
};

void test_text_dirty_layers_drive_on_demand_frames() {
    auto created = ryn::font::FontRuntime::create();
    require(static_cast<bool>(created), "Font Runtime initialization failed");
    auto fonts = std::move(created.runtime);
    const auto latin = fonts->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 14);
    const auto cjk = fonts->load_font_file(RYNUI_VALIDATION_CJK_FONT, 0, 14);
    require(latin && cjk, "integration fonts failed to load");

    ryn::text::TextEngine engine(*fonts);
    ryn::runtime::FrameRequestState requests;
    ryn::detail::TextRenderController controller(
        *fonts,
        engine,
        requests,
        ryn::String{u8"RynUI Hello 中"},
        {latin.font, cjk.font},
        14,
        {20.0F, 420.0F});
    static_cast<void>(controller.set_color({1.0F, 1.0F, 1.0F, 0.65F}));
    RecordingGpuApi api;
    ryn::detail::GlyphGpuResources resources(api);
    TextSubmitter submitter(controller, resources);
    ControlledEvents events;
    ryn::runtime::OnDemandFrameLoop loop(requests, events, submitter, 5);

    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "initial Text frame was not submitted");
    const auto initial_shape = controller.text_state().counters().shape_count;
    const auto initial_measure = controller.text_state().counters().measure_count;
    const auto initial_texture_uploads = api.texture_uploads;
    const auto initial_buffer_uploads = api.buffer_uploads;
    require(initial_shape == 1 && initial_measure == 1
                && initial_texture_uploads > 0 && initial_buffer_uploads == 1,
            "initial Text frame did not shape, atlas, and upload exactly once");

    require(loop.step() == ryn::runtime::FrameLoopStep::idle
                && loop.counters().submissions == 1,
            "stable Text state continued submitting frames");

    static_cast<void>(controller.set_color({0.36F, 0.72F, 1.0F, 0.65F}));
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Material update did not wake the frame loop");
    require(controller.text_state().counters().shape_count == initial_shape
                && controller.text_state().counters().measure_count == initial_measure
                && api.texture_uploads == initial_texture_uploads
                && api.buffer_uploads == initial_buffer_uploads + 1
                && controller.counters().material_updates == 1,
            "Material update escaped its Glyph instance dirty layer");

    require(loop.step() == ryn::runtime::FrameLoopStep::idle,
            "post-Material stable state did not return to idle");
    static_cast<void>(controller.set_content(ryn::String{u8"RynUI Hello 中文界"}));
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "content update did not wake the frame loop");
    require(controller.text_state().counters().shape_count == initial_shape + 1
                && api.texture_uploads > initial_texture_uploads
                && resources.counters().buffer_reallocations == 2,
            "content update did not shape, add glyphs, and replace its instance buffer");

    const auto shape_before_constraint = controller.text_state().counters().shape_count;
    const auto atlas_before_constraint = api.texture_uploads;
    static_cast<void>(controller.set_width_constraint(160.0F));
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "constraint update did not wake the frame loop");
    require(controller.text_state().counters().shape_count == shape_before_constraint
                && controller.text_state().counters().measure_count == initial_measure + 2
                && api.texture_uploads == atlas_before_constraint,
            "constraint update reshaped Text or dirtied the atlas");
}

} // namespace

int main() {
    try {
        test_text_dirty_layers_drive_on_demand_frames();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
