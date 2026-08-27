#include "token_gallery_definition.hpp"

#include "component/button_component.hpp"
#include "font/font_runtime.hpp"
#include "graphics/quad_primitive.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/glyph_gpu_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/invalidation.hpp"
#include "text/text_engine.hpp"
#include "text/text_scene_service.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace rynui::example {
namespace {

bool has_argument(int argc, char** argv, std::string_view expected) {
    for (int index = 1; index < argc; ++index) {
        if (argv[index] == expected) {
            return true;
        }
    }
    return false;
}

std::optional<float> acceptance_scale_argument(int argc, char** argv) {
    constexpr std::string_view prefix = "--acceptance-scale=";
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (!argument.starts_with(prefix)) {
            continue;
        }
        const auto value = argument.substr(prefix.size());
        if (value == "1" || value == "1.0") {
            return 1.0F;
        }
        if (value == "1.5") {
            return 1.5F;
        }
        if (value == "2" || value == "2.0") {
            return 2.0F;
        }
        throw std::invalid_argument(
            "--acceptance-scale must be 1.0, 1.5, or 2.0");
    }
    return std::nullopt;
}

std::filesystem::path executable_directory(char* executable) {
    return std::filesystem::absolute(executable).parent_path();
}

class GalleryEvents final : public ryn::runtime::FrameEventSource {
public:
    GalleryEvents(
        ryn::detail::PlatformState& platform,
        ryn::detail::ButtonComponentHost& application,
        ryn::runtime::FrameRequestState& frame_requests,
        ryn::runtime::Size& viewport,
        float render_scale,
        const std::function<void(float)>& set_viewport_width) noexcept
        : platform_(&platform),
          application_(&application),
          frame_requests_(&frame_requests),
          viewport_(&viewport),
          render_scale_(render_scale),
          set_viewport_width_(&set_viewport_width),
          started_(std::chrono::steady_clock::now()) {}

    std::uint64_t now_milliseconds() const noexcept override {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started_).count());
    }

    bool poll_frame_event() noexcept override { return consume(platform_->poll_events()); }
    bool wait_for_frame_event(std::uint32_t timeout_milliseconds) noexcept override {
        return consume(platform_->wait_events(timeout_milliseconds));
    }

    [[nodiscard]] bool quit_requested() const noexcept { return quit_requested_; }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

private:
    bool consume(const ryn::detail::PlatformEvents& events) noexcept {
        quit_requested_ = quit_requested_ || events.quit_requested;
        try {
            for (const auto& event : events.input.events()) {
                std::visit([this](const auto& value) { dispatch(value); }, event);
            }
        } catch (const std::exception& error) {
            last_error_ = error.what();
            quit_requested_ = true;
            return true;
        }
        return frame_requests_->pending()
            || (events.frame_requested && events.input.empty());
    }

    void dispatch(const ryn::input::PointerInputEvent& event) {
        application_->pointer().dispatch(event);
    }

    void dispatch(const ryn::input::KeyboardInputEvent& event) {
        application_->focus().dispatch(event);
    }

    void dispatch(const ryn::input::WindowInputEvent& event) {
        switch (event.action) {
        case ryn::input::WindowInputAction::focus_gained:
            application_->set_window_active(true);
            return;
        case ryn::input::WindowInputAction::focus_lost:
            application_->set_window_active(false);
            return;
        case ryn::input::WindowInputAction::resized:
            if (event.width > 0 && event.height > 0) {
                const auto metrics = platform_->window_metrics();
                const auto logical = token_gallery_logical_viewport(
                    metrics.pixel_width,
                    metrics.pixel_height,
                    render_scale_);
                *viewport_ = {logical.width, logical.height};
                (*set_viewport_width_)(viewport_->width);
                frame_requests_->request_frame();
            }
            return;
        case ryn::input::WindowInputAction::invalid:
            throw std::invalid_argument("Invalid normalized Window input event");
        }
    }

    ryn::detail::PlatformState* platform_;
    ryn::detail::ButtonComponentHost* application_;
    ryn::runtime::FrameRequestState* frame_requests_;
    ryn::runtime::Size* viewport_;
    float render_scale_;
    const std::function<void(float)>* set_viewport_width_;
    std::chrono::steady_clock::time_point started_;
    bool quit_requested_{};
    std::string last_error_;
};

class GallerySubmitter final : public ryn::runtime::FrameSubmitter {
public:
    GallerySubmitter(
        ryn::detail::PlatformState& platform,
        ryn::detail::ButtonComponentHost& application,
        ryn::detail::TextSceneService& text_scene,
        ryn::detail::GlyphGpuResources& glyph_resources,
        ryn::detail::SdlSceneRenderer& renderer,
        ryn::runtime::Size& viewport,
        float render_scale) noexcept
        : platform_(&platform),
          application_(&application),
          text_scene_(&text_scene),
          glyph_resources_(&glyph_resources),
          renderer_(&renderer),
          effect_resources_(renderer),
          viewport_(&viewport),
          render_scale_(render_scale) {}

    ryn::runtime::FrameSubmissionResult submit_frame() override {
        try {
            const ryn::runtime::Rect clip{
                16.0F,
                12.0F,
                std::max(0.0F, viewport_->width - 32.0F),
                std::max(0.0F, viewport_->height - 24.0F),
            };
            if (!application_->layout_and_synchronize(
                    *viewport_, clip, {24.0F, 20.0F}, 0.0F)) {
                last_error_ = "Token Gallery layout or scene sync failed";
                return ryn::runtime::FrameSubmissionResult::failed;
            }
            if (quad_buffer_ == nullptr) {
                quad_buffer_ = std::make_unique<ryn::graphics::QuadGpuBuffer>(
                    *renderer_, application_->button_scene().instances());
            } else {
                application_->button_scene().synchronize_gpu(*quad_buffer_);
            }
            glyph_resources_->synchronize(
                text_scene_->atlas(), text_scene_->glyph_scene().instances());
            const auto metrics = platform_->window_metrics();
            effect_resources_.synchronize(
                application_->rounded_effects(),
                {
                    static_cast<std::uint32_t>(metrics.pixel_width),
                    static_cast<std::uint32_t>(metrics.pixel_height),
                    render_scale_,
                });
            renderer_->attach_scene(
                quad_buffer_->handle(),
                *glyph_resources_,
                application_->scene_composer().ordered_scene(),
                &effect_resources_);
            const auto result = renderer_->submit_frame();
            if (result == ryn::runtime::FrameSubmissionResult::failed) {
                last_error_ = renderer_->last_error();
            }
            return result;
        } catch (const std::exception& error) {
            last_error_ = error.what();
            return ryn::runtime::FrameSubmissionResult::failed;
        }
    }

    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }
    [[nodiscard]] const ryn::graphics::QuadUploadCounters& quad_uploads() const {
        if (quad_buffer_ == nullptr) {
            throw std::logic_error("Token Gallery Quad buffer was not created");
        }
        return quad_buffer_->counters();
    }
    [[nodiscard]] const ryn::detail::RoundedEffectGpuResourceCounters&
        effect_uploads() const noexcept {
        return effect_resources_.counters();
    }

private:
    ryn::detail::PlatformState* platform_;
    ryn::detail::ButtonComponentHost* application_;
    ryn::detail::TextSceneService* text_scene_;
    ryn::detail::GlyphGpuResources* glyph_resources_;
    ryn::detail::SdlSceneRenderer* renderer_;
    ryn::detail::RoundedEffectGpuResources effect_resources_;
    ryn::runtime::Size* viewport_;
    float render_scale_;
    std::unique_ptr<ryn::graphics::QuadGpuBuffer> quad_buffer_;
    std::string last_error_;
};

} // namespace

int run_token_gallery(int argc, char** argv, TokenGalleryDefinition definition) {
    try {
        const bool smoke_mode = has_argument(argc, argv, "--smoke");
        const auto acceptance_scale = acceptance_scale_argument(argc, argv);
        const auto executable = executable_directory(argv[0]);
        constexpr ryn::runtime::Size requested_window{1280.0F, 900.0F};
        ryn::runtime::Size viewport = requested_window;

        ryn::detail::PlatformConfig platform_config;
        platform_config.title = "RynUI Ant Design Token Gallery";
        platform_config.width = static_cast<int>(requested_window.width);
        platform_config.height = static_cast<int>(requested_window.height);
#if !defined(NDEBUG)
        platform_config.gpu_debug = true;
#endif
        auto platform_result = ryn::detail::PlatformState::create(platform_config);
        if (!platform_result) {
            std::cerr << "platform_error=" << platform_result.error->message << '\n';
            return 1;
        }
        auto& platform = *platform_result.state;
        const auto initial_metrics = platform.window_metrics();
        const float render_scale = acceptance_scale.value_or(initial_metrics.display_scale);
        const auto logical_viewport = token_gallery_logical_viewport(
            initial_metrics.pixel_width,
            initial_metrics.pixel_height,
            render_scale);
        viewport = {logical_viewport.width, logical_viewport.height};
        if (viewport.width <= 0.0F || viewport.height <= 0.0F) {
            std::cerr << "platform_error=window metrics did not provide a logical viewport\n";
            return 1;
        }
        definition.set_viewport_width(viewport.width);

        auto font_result = ryn::font::FontRuntime::create();
        if (!font_result) {
            std::cerr << "font_error=" << font_result.error.diagnostic << '\n';
            return 2;
        }
        auto fonts = std::move(font_result.runtime);
        ryn::detail::DefaultFontChainRequest font_request;
        font_request.raster = {14, render_scale};
        font_request.fallback_latin = executable / "fonts/latin.ttf";
        font_request.fallback_cjk = executable / "fonts/cjk.otf";
        const auto font_chain = ryn::detail::load_default_ui_font_chain(*fonts, font_request);
        if (!font_chain) {
            std::cerr << "font_error=" << font_chain.diagnostic << '\n';
            return 3;
        }
        auto font_resolver = ryn::detail::make_default_ui_font_resolver(
            *fonts, font_chain, render_scale);

        ryn::runtime::NodeStore nodes;
        ryn::layout::LayoutEngine layout(nodes);
        ryn::runtime::FrameRequestState frame_requests;
        ryn::runtime::DirtyQueues dirty(nodes, &frame_requests);
        ryn::text::TextEngine text_engine(*fonts);
        ryn::detail::TextSceneService text_scene(*fonts, text_engine, frame_requests);
        ryn::detail::ButtonComponentHost application(
            nodes, layout, dirty, text_scene, std::move(font_resolver), frame_requests);
        application.mount(definition.content);

        ryn::detail::SdlSceneRenderer renderer(platform, executable / "shaders");
        ryn::detail::GlyphGpuResources glyph_resources(renderer);
        GallerySubmitter submitter(
            platform,
            application,
            text_scene,
            glyph_resources,
            renderer,
            viewport,
            render_scale);
        GalleryEvents events(
            platform,
            application,
            frame_requests,
            viewport,
            render_scale,
            definition.set_viewport_width);
        ryn::runtime::OnDemandFrameLoop loop(frame_requests, events, submitter, 10);

        std::size_t smoke_stage = 0;
        while (!events.quit_requested()) {
            const auto elapsed = events.now_milliseconds();
            if (smoke_mode && smoke_stage < 5
                    && elapsed >= 250 * (smoke_stage + 1)) {
                definition.smoke_step(smoke_stage++);
            }
            const auto step = loop.step();
            if (!events.last_error().empty()) {
                std::cerr << "input_error=" << events.last_error() << '\n';
                return 4;
            }
            if (step == ryn::runtime::FrameLoopStep::failed) {
                std::cerr << "frame_error=" << submitter.last_error() << '\n';
                return 5;
            }
            if (smoke_mode && smoke_stage == 5 && elapsed >= 1'700
                    && loop.counters().idle_waits >= 20) {
                break;
            }
        }

        const auto telemetry = definition.telemetry();
        const auto platform_diagnostics = platform.event_diagnostics();
        const auto scene = application.scene_composer().diagnostics();
        const auto button_scene = application.button_scene().diagnostics();
        const auto quad = submitter.quad_uploads();
        const auto glyph = glyph_resources.counters();
        const auto effect = submitter.effect_uploads();
        const auto render = renderer.counters();
        const auto frames = loop.counters();
        const auto metrics = platform.window_metrics();
        std::uint64_t outer_layers = 0;
        std::uint64_t inset_layers = 0;
        std::uint64_t focus_layers = 0;
        for (const auto& instance : application.rounded_effects().packed_instances()) {
            switch (instance.geometry.kind) {
            case ryn::graphics::RoundedEffectKind::outer_shadow:
                ++outer_layers;
                break;
            case ryn::graphics::RoundedEffectKind::inset_shadow:
                ++inset_layers;
                break;
            case ryn::graphics::RoundedEffectKind::outline:
                ++focus_layers;
                break;
            }
        }
        std::uint64_t layout_passes = 0;
        for (const auto& mounted : application.mounted_buttons()) {
            layout_passes += nodes.require(mounted.node).place_count;
        }
        for (const auto& mounted : application.text().mounted_texts()) {
            layout_passes += nodes.require(text_scene.node(mounted.scene)).place_count;
        }

        std::cout
            << "catalog_hash=" << RYNUI_TOKEN_CATALOG_HASH
            << " gpu_driver=" << platform.gpu_driver()
            << " shader_format=" << renderer.shader_format()
            << " display_scale=" << render_scale
            << " host_display_scale=" << platform.display_scale()
            << " scale_source=" << (acceptance_scale.has_value() ? "acceptance" : "window")
            << " pixel_density=" << metrics.pixel_density
            << " window_system=" << RYNUI_WINDOW_SYSTEM
            << " window_size=" << metrics.coordinate_width << 'x' << metrics.coordinate_height
            << " pixel_size=" << metrics.pixel_width << 'x' << metrics.pixel_height
            << " viewport=" << viewport.width << 'x' << viewport.height
            << " font_source=" << font_chain.telemetry_source()
            << " font_families=" << font_chain.telemetry_families()
            << " stable_test_ids=" << definition.stable_test_ids.size()
            << " snapshot_identity=" << telemetry.snapshot_identity
            << " snapshot=" << telemetry.snapshot_diagnostic
            << " content_runs=" << telemetry.content_runs
            << " theme_content_runs=" << telemetry.theme_content_runs
            << " theme_updates=" << telemetry.theme_updates
            << " brand_updates=" << telemetry.brand_updates
            << " viewport_updates=" << telemetry.viewport_updates
            << " state_updates=" << telemetry.state_updates
            << " activations=" << telemetry.activations
            << " input_events=" << platform_diagnostics.normalized_input_events
            << " component_count=" << application.components().component_count()
            << " layout_passes=" << layout_passes
            << " scene_rebuilds=" << scene.rebuilds
            << " effect_layers=" << application.rounded_effects().live_count()
            << " outer_layers=" << outer_layers
            << " inset_layers=" << inset_layers
            << " focus_layers=" << focus_layers
            << " button_material_updates=" << button_scene.material_updates
            << " button_geometry_updates=" << button_scene.geometry_updates
            << " quad_uploads=" << quad.initial_uploads + quad.range_uploads
            << " quad_uploaded_bytes=" << quad.uploaded_bytes
            << " glyph_uploads=" << glyph.texture_uploads + glyph.buffer_uploads
            << " glyph_uploaded_bytes="
            << glyph.texture_uploaded_bytes + glyph.buffer_uploaded_bytes
            << " effect_uploads=" << effect.buffer_uploads
            << " effect_uploaded_bytes=" << effect.uploaded_bytes
            << " quad_draws=" << render.quad_draws
            << " glyph_draws=" << render.glyph_draws
            << " effect_draws=" << render.effect_draws
            << " submits=" << render.frame_submissions
            << " idle_waits=" << frames.idle_waits
            << " exit_code=0\n";

        return smoke_mode
                && (smoke_stage != 5 || telemetry.content_runs != 1
                    || telemetry.theme_updates != 4 || telemetry.brand_updates != 1
                    || telemetry.state_updates != 2)
            ? 6
            : 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal_error=" << error.what() << '\n';
        return 7;
    }
}

} // namespace rynui::example
