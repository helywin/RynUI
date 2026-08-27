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

#include <ryn/rynui.hpp>

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

ryn::String click_label(std::uint64_t clicks) {
    auto parsed = ryn::String::from_utf8(
        "Clicks / 点击次数: " + std::to_string(clicks));
    if (!parsed) {
        throw std::logic_error("Button click label is not valid UTF-8");
    }
    return std::move(parsed).value();
}

class ButtonPlatformEvents final : public ryn::runtime::FrameEventSource {
public:
    ButtonPlatformEvents(
        ryn::detail::PlatformState& platform,
        ryn::detail::ButtonComponentHost& application,
        ryn::runtime::FrameRequestState& frame_requests,
        ryn::runtime::Size& viewport) noexcept
        : platform_(&platform),
          application_(&application),
          frame_requests_(&frame_requests),
          viewport_(&viewport),
          started_(std::chrono::steady_clock::now()) {}

    std::uint64_t now_milliseconds() const noexcept override {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started_).count());
    }

    bool poll_frame_event() noexcept override {
        return consume(platform_->poll_events());
    }

    bool wait_for_frame_event(std::uint32_t timeout_milliseconds) noexcept override {
        return consume(platform_->wait_events(timeout_milliseconds));
    }

    [[nodiscard]] bool quit_requested() const noexcept {
        return quit_requested_;
    }

    [[nodiscard]] const std::string& last_error() const noexcept {
        return last_error_;
    }

private:
    bool consume(const ryn::detail::PlatformEvents& events) noexcept {
        quit_requested_ = quit_requested_ || events.quit_requested;
        try {
            for (const auto& event : events.input.events()) {
                std::visit([this](const auto& value) {
                    dispatch(value);
                }, event);
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

class ButtonComponentSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    ButtonComponentSubmitter(
        ryn::detail::ButtonComponentHost& application,
        ryn::detail::TextSceneService& text_scene,
        ryn::detail::GlyphGpuResources& glyph_resources,
        ryn::detail::SdlSceneRenderer& renderer,
        ryn::runtime::Size& viewport) noexcept
        : application_(&application),
          text_scene_(&text_scene),
          glyph_resources_(&glyph_resources),
          renderer_(&renderer),
          viewport_(&viewport) {}

    ryn::runtime::FrameSubmissionResult submit_frame() override {
        try {
            const ryn::runtime::Rect clip{
                32.0F,
                24.0F,
                std::max(0.0F, viewport_->width - 64.0F),
                std::max(0.0F, viewport_->height - 48.0F),
            };
            if (!application_->layout_and_synchronize(
                    *viewport_, clip, {48.0F, 36.0F}, 10.0F)) {
                last_error_ = "Button application layout or scene sync failed";
                return ryn::runtime::FrameSubmissionResult::failed;
            }
            if (quad_buffer_ == nullptr) {
                quad_buffer_ = std::make_unique<ryn::graphics::QuadGpuBuffer>(
                    *renderer_, application_->button_scene().instances());
            } else {
                application_->button_scene().synchronize_gpu(*quad_buffer_);
            }
            glyph_resources_->synchronize(
                text_scene_->atlas(),
                text_scene_->glyph_scene().instances());
            renderer_->attach_scene(
                quad_buffer_->handle(),
                *glyph_resources_,
                application_->scene_composer().ordered_scene());
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

    [[nodiscard]] const std::string& last_error() const noexcept {
        return last_error_;
    }

    [[nodiscard]] const ryn::graphics::QuadUploadCounters&
    quad_upload_counters() const {
        if (quad_buffer_ == nullptr) {
            throw std::logic_error("Button Quad GPU buffer was not created");
        }
        return quad_buffer_->counters();
    }

private:
    ryn::detail::ButtonComponentHost* application_;
    ryn::detail::TextSceneService* text_scene_;
    ryn::detail::GlyphGpuResources* glyph_resources_;
    ryn::detail::SdlSceneRenderer* renderer_;
    ryn::runtime::Size* viewport_;
    std::unique_ptr<ryn::graphics::QuadGpuBuffer> quad_buffer_;
    std::string last_error_;
};

} // namespace

int main(int argc, char** argv) {
    try {
        const bool smoke_mode = has_argument(argc, argv, "--smoke");
        const auto executable = executable_directory(argv[0]);
        constexpr ryn::runtime::Size requested_window{960.0F, 720.0F};
        ryn::runtime::Size viewport = requested_window;
        constexpr std::uint32_t pixel_size = 14;

        ryn::detail::PlatformConfig platform_config;
        platform_config.title = "RynUI Public Button DSL";
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

        ryn::runtime::NodeStore nodes;
        ryn::layout::LayoutEngine layout(nodes);
        ryn::runtime::FrameRequestState frame_requests;
        ryn::runtime::DirtyQueues dirty(nodes, &frame_requests);
        ryn::text::TextEngine text_engine(*fonts);
        ryn::detail::TextSceneService text_scene(
            *fonts, text_engine, frame_requests);
        ryn::detail::ButtonComponentHost application(
            nodes,
            layout,
            dirty,
            text_scene,
            font_chain.identities(),
            frame_requests);

        ryn::Signal<ryn::ButtonType> reactive_type{ryn::ButtonType::Default};
        ryn::Signal<ryn::ControlSize> reactive_size{ryn::ControlSize::Middle};
        ryn::Signal<bool> reactive_disabled{false};
        ryn::Signal<bool> reactive_loading{false};
        ryn::Signal<ryn::String> observed_clicks{click_label(0)};
        std::uint64_t click_count = 0;
        std::uint64_t prop_updates = 0;
        auto record_click = [&] {
            ++click_count;
            observed_clicks.set(click_label(click_count));
        };
        application.mount(ryn::Content{[&] {
            ryn::Text(
                ryn::TextProps{}
                    .content(u8"RynUI Button / Latin + 中文按钮")
                    .tone(ryn::TextTone::Primary));
            ryn::Button(
                ryn::ButtonProps{}
                    .type(ryn::ButtonType::Default)
                    .size(ryn::ControlSize::Middle)
                    .onClick(record_click),
                [] { ryn::Text(u8"Default / 默认按钮"); });
            ryn::Button(
                ryn::ButtonProps{}
                    .type(ryn::ButtonType::Primary)
                    .size(ryn::ControlSize::Middle)
                    .onClick(record_click),
                [] { ryn::Text(u8"Primary / 主要按钮"); });
            ryn::Button(
                ryn::ButtonProps{}
                    .size(ryn::ControlSize::Small)
                    .onClick(record_click),
                [] { ryn::Text(u8"Small / 小按钮"); });
            ryn::Button(
                ryn::ButtonProps{}
                    .size(ryn::ControlSize::Large)
                    .onClick(record_click),
                [] { ryn::Text(u8"Large / 大按钮"); });
            ryn::Button(
                ryn::ButtonProps{}.disabled(true),
                [] { ryn::Text(u8"Disabled / 禁用"); });
            ryn::Button(
                ryn::ButtonProps{}.loading(true),
                [] { ryn::Text(u8"Loading / 加载中"); });
            ryn::Button(
                ryn::ButtonProps{}
                    .type(reactive_type)
                    .size(reactive_size)
                    .disabled(reactive_disabled)
                    .loading(reactive_loading)
                    .onClick([&] {
                        record_click();
                        reactive_type.set(
                            reactive_type.get() == ryn::ButtonType::Default
                                ? ryn::ButtonType::Primary
                                : ryn::ButtonType::Default);
                        reactive_size.set(
                            reactive_size.get() == ryn::ControlSize::Large
                                ? ryn::ControlSize::Small
                                : ryn::ControlSize::Large);
                        ++prop_updates;
                    }),
                [] { ryn::Text(u8"Reactive / 响应式按钮"); });
            ryn::Text(
                ryn::TextProps{}
                    .content(observed_clicks)
                    .tone(ryn::TextTone::Secondary));
        }});

        ryn::detail::SdlSceneRenderer renderer(platform, executable / "shaders");
        ryn::detail::GlyphGpuResources glyph_resources(renderer);
        ButtonComponentSubmitter submitter(
            application, text_scene, glyph_resources, renderer, viewport);
        ButtonPlatformEvents events(
            platform, application, frame_requests, viewport);
        ryn::runtime::OnDemandFrameLoop loop(
            frame_requests, events, submitter, 10);

        int smoke_stage = 0;
        while (!events.quit_requested()) {
            const auto elapsed = events.now_milliseconds();
            if (smoke_mode && smoke_stage == 0 && elapsed >= 250) {
                reactive_type.set(ryn::ButtonType::Primary);
                ++prop_updates;
                ++smoke_stage;
            } else if (smoke_mode && smoke_stage == 1 && elapsed >= 500) {
                reactive_size.set(ryn::ControlSize::Large);
                ++prop_updates;
                ++smoke_stage;
            } else if (smoke_mode && smoke_stage == 2 && elapsed >= 750) {
                reactive_disabled.set(true);
                ++prop_updates;
                ++smoke_stage;
            } else if (smoke_mode && smoke_stage == 3 && elapsed >= 1'000) {
                reactive_disabled.set(false);
                reactive_loading.set(true);
                prop_updates += 2;
                ++smoke_stage;
            } else if (smoke_mode && smoke_stage == 4 && elapsed >= 1'250) {
                reactive_loading.set(false);
                ++prop_updates;
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
            if (smoke_mode && smoke_stage == 5 && elapsed >= 1'800
                    && loop.counters().idle_waits >= 20) {
                break;
            }
        }

        std::uint64_t layout_passes = 0;
        for (const auto& mounted : application.mounted_buttons()) {
            layout_passes += nodes.require(mounted.node).place_count;
        }
        for (const auto& mounted : application.text().mounted_texts()) {
            layout_passes +=
                nodes.require(text_scene.node(mounted.scene)).place_count;
        }
        const auto platform_diagnostics = platform.event_diagnostics();
        const auto& hit_test_diagnostics = application.hit_test().diagnostics();
        const auto& pointer_diagnostics = application.pointer().diagnostics();
        const auto& focus_diagnostics = application.focus().diagnostics();
        const auto& scene_diagnostics =
            application.scene_composer().diagnostics();
        const auto& button_diagnostics = application.button_scene().diagnostics();
        const auto& quad_uploads = submitter.quad_upload_counters();
        const auto& glyph_uploads = glyph_resources.counters();
        const auto& renderer_counters = renderer.counters();
        const auto& loop_counters = loop.counters();
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
            << " input_events=" << platform_diagnostics.normalized_input_events
            << " hit_test_queries=" << hit_test_diagnostics.queries
            << " routes_dispatched=" << pointer_diagnostics.routes_dispatched
            << " captures_started=" << pointer_diagnostics.captures_started
            << " captures_released=" << pointer_diagnostics.captures_released
            << " focus_changes=" << focus_diagnostics.focus_changes
            << " clicks=" << click_count
            << " prop_updates=" << prop_updates
            << " layout_passes=" << layout_passes
            << " scene_rebuilds=" << scene_diagnostics.rebuilds
            << " button_material_updates=" << button_diagnostics.material_updates
            << " button_geometry_updates=" << button_diagnostics.geometry_updates
            << " quad_uploads="
            << quad_uploads.initial_uploads + quad_uploads.range_uploads
            << " quad_uploaded_bytes=" << quad_uploads.uploaded_bytes
            << " glyph_uploads="
            << glyph_uploads.texture_uploads + glyph_uploads.buffer_uploads
            << " glyph_uploaded_bytes="
            << glyph_uploads.texture_uploaded_bytes
                + glyph_uploads.buffer_uploaded_bytes
            << " quad_draws=" << renderer_counters.quad_draws
            << " glyph_draws=" << renderer_counters.glyph_draws
            << " submits=" << renderer_counters.frame_submissions
            << " idle_waits=" << loop_counters.idle_waits
            << " exit_code=0\n";
        return smoke_mode && (smoke_stage != 5 || prop_updates != 6)
            ? 6
            : 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal_error=" << error.what() << '\n';
        return 7;
    }
}
