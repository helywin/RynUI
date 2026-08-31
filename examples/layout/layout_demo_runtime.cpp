#include "layout_demo_runtime.hpp"

#include "component/button_component.hpp"
#include "component/flex_component.hpp"
#include "component/space_component.hpp"
#include "font/font_runtime.hpp"
#include "graphics/quad_primitive.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/glyph_gpu_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "runtime/animation_frame_deadline.hpp"
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
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

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

std::filesystem::path executable_directory(char* executable) {
    return std::filesystem::absolute(executable).parent_path();
}

class LayoutPlatformEvents final : public ryn::runtime::FrameEventSource {
public:
    LayoutPlatformEvents(
        ryn::detail::PlatformState& platform,
        ryn::detail::ButtonComponentHost& application,
        ryn::runtime::FrameRequestState& frame_requests,
        ryn::runtime::Size& viewport) noexcept
        : platform_(&platform),
          application_(&application),
          frame_requests_(&frame_requests),
          viewport_(&viewport),
          started_(std::chrono::steady_clock::now()) {}

    ryn::animation::AnimationTime now() const noexcept override {
        return ryn::animation::AnimationTime::microseconds(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started_).count());
    }

    bool poll_frame_event() noexcept override {
        return consume(platform_->poll_events());
    }

    bool wait_for_frame_event(std::uint32_t timeout_milliseconds) noexcept override {
        return consume(platform_->wait_events(timeout_milliseconds));
    }

    [[nodiscard]] bool quit_requested() const noexcept { return quit_requested_; }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

private:
    bool consume(const ryn::detail::PlatformEvents& events) noexcept {
        application_->set_animation_time(now());
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
            || events.redraw_requested
            || (events.frame_requested && events.input.empty());
    }

    void dispatch(const ryn::input::PointerInputEvent& event) {
        application_->pointer().dispatch(event);
    }

    void dispatch(const ryn::input::ScrollInputEvent&) {}

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
                *viewport_ = {
                    static_cast<float>(event.width),
                    static_cast<float>(event.height),
                };
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
    std::chrono::steady_clock::time_point started_;
    bool quit_requested_{};
    std::string last_error_;
};

class LayoutComponentSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    LayoutComponentSubmitter(
        ryn::detail::PlatformState& platform,
        ryn::detail::ButtonComponentHost& application,
        ryn::detail::TextSceneService& text_scene,
        ryn::detail::GlyphGpuResources& glyph_resources,
        ryn::detail::SdlSceneRenderer& renderer,
        ryn::runtime::Size& viewport) noexcept
        : platform_(&platform),
          application_(&application),
          text_scene_(&text_scene),
          glyph_resources_(&glyph_resources),
          renderer_(&renderer),
          effect_resources_(renderer),
          viewport_(&viewport) {}

    ryn::runtime::FrameSubmissionResult submit_frame(
        ryn::animation::AnimationTime frame_time) override {
        try {
            static_cast<void>(application_->tick_animations(frame_time));
            const ryn::runtime::Rect clip{
                24.0F,
                20.0F,
                std::max(0.0F, viewport_->width - 48.0F),
                std::max(0.0F, viewport_->height - 40.0F),
            };
            if (!application_->layout_and_synchronize(
                    *viewport_, clip, {24.0F, 28.0F}, 0.0F)) {
                last_error_ = "Layout demo layout or scene sync failed";
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
                    metrics.display_scale,
                });
            renderer_->attach_scene(
                quad_buffer_->handle(),
                *glyph_resources_,
                application_->scene_composer().ordered_scene(),
                &effect_resources_);
            const auto result = renderer_->submit_frame(frame_time);
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

private:
    ryn::detail::PlatformState* platform_;
    ryn::detail::ButtonComponentHost* application_;
    ryn::detail::TextSceneService* text_scene_;
    ryn::detail::GlyphGpuResources* glyph_resources_;
    ryn::detail::SdlSceneRenderer* renderer_;
    ryn::detail::RoundedEffectGpuResources effect_resources_;
    ryn::runtime::Size* viewport_;
    std::unique_ptr<ryn::graphics::QuadGpuBuffer> quad_buffer_;
    std::string last_error_;
};

struct LayoutDiagnostics final {
    std::uint64_t layout_passes{};
    std::size_t line_count{};
};

void collect_layout_diagnostics(
    const ryn::runtime::ComponentHost& components,
    const ryn::runtime::NodeStore& nodes,
    const ryn::layout::LayoutEngine& layout,
    ryn::runtime::ComponentId component,
    LayoutDiagnostics& result) {
    const auto node = components.root(component);
    result.layout_passes += nodes.require(node).place_count;
    if (components.state<ryn::detail::FlexComponentState>(component) != nullptr
            || components.state<ryn::detail::SpaceComponentState>(component) != nullptr) {
        result.line_count += layout.flex_layout_diagnostics(node).line_count;
    }
    for (const auto child : components.children(component)) {
        collect_layout_diagnostics(components, nodes, layout, child, result);
    }
}

} // namespace

int run_layout_demo(int argc, char** argv, LayoutDemoDefinition definition) {
    try {
        const bool smoke_mode = has_argument(argc, argv, "--smoke");
        const auto executable = executable_directory(argv[0]);
        constexpr ryn::runtime::Size requested_window{960.0F, 720.0F};
        ryn::runtime::Size viewport = requested_window;
        constexpr std::uint32_t pixel_size = 14;

        ryn::detail::PlatformConfig platform_config;
        platform_config.title = "RynUI Public Flex and Space DSL";
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
        const auto initial_window_metrics = platform.window_metrics();
        viewport = {
            initial_window_metrics.logical_width(),
            initial_window_metrics.logical_height(),
        };
        if (viewport.width <= 0.0F || viewport.height <= 0.0F) {
            std::cerr << "platform_error=window metrics did not provide a logical viewport\n";
            return 1;
        }

        auto font_result = ryn::font::FontRuntime::create();
        if (!font_result) {
            std::cerr << "font_error=" << font_result.error.diagnostic << '\n';
            return 2;
        }
        auto fonts = std::move(font_result.runtime);
        const ryn::font::FontRasterConfig font_raster{
            pixel_size,
            initial_window_metrics.display_scale,
        };
        ryn::detail::DefaultFontChainRequest font_request;
        font_request.raster = font_raster;
        font_request.fallback_latin = executable / "fonts/latin.ttf";
        font_request.fallback_cjk = executable / "fonts/cjk.otf";
        const auto font_chain =
            ryn::detail::load_default_ui_font_chain(*fonts, font_request);
        if (!font_chain) {
            std::cerr << "font_error=" << font_chain.diagnostic << '\n';
            return 3;
        }
        const auto font_metrics = fonts->metrics(font_chain.faces.front().identity);
        if (!font_metrics) {
            std::cerr << "font_error=font metrics could not be queried\n";
            return 3;
        }
        auto font_resolver = ryn::detail::make_default_ui_font_resolver(
            *fonts,
            font_chain,
            initial_window_metrics.display_scale);

        ryn::runtime::NodeStore nodes;
        ryn::layout::LayoutEngine layout(nodes);
        ryn::runtime::FrameRequestState frame_requests;
        ryn::runtime::DirtyQueues dirty(nodes, &frame_requests);
        ryn::text::TextEngine text_engine(*fonts);
        ryn::detail::TextSceneService text_scene(*fonts, text_engine, frame_requests);
        ryn::detail::ButtonComponentHost application(
            nodes,
            layout,
            dirty,
            text_scene,
            std::move(font_resolver),
            frame_requests);
        application.mount(definition.content);

        ryn::detail::SdlSceneRenderer renderer(platform, executable / "shaders");
        ryn::detail::GlyphGpuResources glyph_resources(renderer);
        LayoutComponentSubmitter submitter(
            platform, application, text_scene, glyph_resources, renderer, viewport);
        LayoutPlatformEvents events(platform, application, frame_requests, viewport);
        ryn::runtime::AnimationFrameDeadlineSource animation_deadlines(
            application.animations());
        ryn::runtime::OnDemandFrameLoop loop(
            frame_requests, events, submitter, animation_deadlines, 10);

        std::size_t smoke_stage = 0;
        while (!events.quit_requested()) {
            application.set_animation_time(events.now());
            const auto elapsed = events.now_milliseconds();
            if (smoke_mode && smoke_stage < 3
                    && elapsed >= 250 * (smoke_stage + 1)) {
                definition.smoke_step(smoke_stage);
                ++smoke_stage;
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
            if (smoke_mode && smoke_stage == 3 && elapsed >= 1'400
                    && loop.counters().idle_waits >= 20) {
                break;
            }
        }

        LayoutDiagnostics layout_diagnostics;
        for (const auto root : application.components().root_components()) {
            collect_layout_diagnostics(
                application.components(), nodes, layout, root, layout_diagnostics);
        }
        const auto telemetry = definition.telemetry();
        const auto scene = application.scene_composer().diagnostics();
        const auto renderer_counters = renderer.counters();
        const auto loop_counters = loop.counters();
        const auto window_metrics = platform.window_metrics();
        std::cout
            << "gpu_driver=" << platform.gpu_driver()
            << " shader_format=" << renderer.shader_format()
            << " display_scale=" << platform.display_scale()
            << " pixel_density=" << window_metrics.pixel_density
            << " window_size=" << window_metrics.coordinate_width << 'x'
            << window_metrics.coordinate_height
            << " pixel_size=" << window_metrics.pixel_width << 'x'
            << window_metrics.pixel_height
            << " viewport=" << viewport.width << 'x' << viewport.height
            << " font_logical_pixel_size=" << font_metrics.metrics.logical_pixel_size
            << " font_raster_pixel_size=" << font_metrics.metrics.raster_pixel_size
            << " font_raster_scale=" << font_metrics.metrics.raster_scale
            << " font_source=" << font_chain.telemetry_source()
            << " font_families=" << font_chain.telemetry_families()
            << " line_count=" << layout_diagnostics.line_count
            << " content_runs=" << telemetry.content_runs
            << " component_count=" << application.components().component_count()
            << " prop_updates=" << telemetry.prop_updates
            << " activations=" << telemetry.activations
            << " layout_passes=" << layout_diagnostics.layout_passes
            << " scene_rebuilds=" << scene.rebuilds
            << " submits=" << renderer_counters.frame_submissions
            << " idle_waits=" << loop_counters.idle_waits
            << " exit_code=0\n";
        return smoke_mode
                && (smoke_stage != 3 || telemetry.content_runs != 1
                    || telemetry.prop_updates != 21 || telemetry.activations != 3)
            ? 6
            : 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal_error=" << error.what() << '\n';
        return 7;
    }
}

} // namespace rynui::example
