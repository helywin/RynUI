#include "font/font_runtime.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/glyph_gpu_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "renderer/sdl/text_render_controller.hpp"
#include "runtime/frame_scheduler.hpp"
#include "text/text_engine.hpp"

#include <ryn/string.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

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

class PlatformFrameEvents final : public ryn::runtime::FrameEventSource {
public:
    explicit PlatformFrameEvents(ryn::detail::PlatformState& platform) noexcept
        : platform_(&platform), started_(std::chrono::steady_clock::now()) {}

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

    [[nodiscard]] bool quit_requested() const noexcept { return quit_requested_; }

private:
    bool consume(ryn::detail::PlatformEvents events) noexcept {
        quit_requested_ = quit_requested_ || events.quit_requested;
        return events.frame_requested;
    }

    ryn::detail::PlatformState* platform_;
    std::chrono::steady_clock::time_point started_;
    bool quit_requested_{};
};

class TextSceneSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    TextSceneSubmitter(
        ryn::detail::TextRenderController& controller,
        ryn::detail::GlyphGpuResources& resources,
        ryn::detail::SdlSceneRenderer& renderer,
        ryn::graphics::GlyphPlacement& placement) noexcept
        : controller_(&controller),
          resources_(&resources),
          renderer_(&renderer),
          placement_(&placement) {}

    ryn::runtime::FrameSubmissionResult submit_frame() override {
        try {
            if (!controller_->synchronize(*placement_)) {
                last_error_ = "Text or Glyph Scene synchronization failed";
                return ryn::runtime::FrameSubmissionResult::failed;
            }
            resources_->synchronize(
                controller_->atlas(),
                controller_->glyph_scene().instances());
            renderer_->attach_scene(
                nullptr,
                *resources_,
                controller_->ordered_scene());
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

private:
    ryn::detail::TextRenderController* controller_;
    ryn::detail::GlyphGpuResources* resources_;
    ryn::detail::SdlSceneRenderer* renderer_;
    ryn::graphics::GlyphPlacement* placement_;
    std::string last_error_;
};

} // namespace

int main(int argc, char** argv) {
    try {
        const bool smoke_mode = has_argument(argc, argv, "--smoke");
        const auto executable = executable_directory(argv[0]);
        constexpr ryn::runtime::Size initial_viewport{960.0F, 540.0F};
        constexpr std::uint32_t pixel_size = 14;

        ryn::detail::PlatformConfig platform_config;
        platform_config.title = "RynUI Latin and CJK Text";
        platform_config.width = static_cast<int>(initial_viewport.width);
        platform_config.height = static_cast<int>(initial_viewport.height);
#if !defined(NDEBUG)
        platform_config.gpu_debug = true;
#endif
        auto platform_result = ryn::detail::PlatformState::create(platform_config);
        if (!platform_result) {
            std::cerr << "platform_error=" << platform_result.error->message << '\n';
            return 1;
        }
        auto& platform = *platform_result.state;

        auto font_result = ryn::font::FontRuntime::create();
        if (!font_result) {
            std::cerr << "font_error=" << font_result.error.diagnostic << '\n';
            return 2;
        }
        auto fonts = std::move(font_result.runtime);
        const auto latin = fonts->load_font_file(
            executable / "fonts/latin.ttf", 0, pixel_size);
        const auto cjk = fonts->load_font_file(
            executable / "fonts/cjk.otf", 0, pixel_size);
        if (!latin || !cjk) {
            std::cerr << "font_error=locked validation fonts could not be loaded\n";
            return 3;
        }

        ryn::String content = u8"RynUI text foundation\nLatin body text and 中文回退字体";
        ryn::text::TextEngine text_engine(*fonts);
        ryn::runtime::FrameRequestState frame_requests;
        ryn::detail::TextRenderController controller(
            *fonts,
            text_engine,
            frame_requests,
            std::move(content),
            {latin.font, cjk.font},
            pixel_size,
            {20.0F, 720.0F});
        // Ant Design 6.5 dark-mode secondary text: white at 65% semantic alpha.
        static_cast<void>(controller.set_color({1.0F, 1.0F, 1.0F, 0.65F}));

        ryn::detail::SdlSceneRenderer renderer(platform, executable / "shaders");
        ryn::detail::GlyphGpuResources resources(renderer);
        ryn::graphics::GlyphPlacement placement{
            {64.0F, 72.0F},
            initial_viewport,
            {48.0F, 48.0F, 864.0F, 444.0F},
            {0.0F, 0.0F},
            {},
            1.0F,
        };
        TextSceneSubmitter submitter(controller, resources, renderer, placement);
        PlatformFrameEvents events(platform);
        ryn::runtime::OnDemandFrameLoop loop(
            frame_requests, events, submitter, 10);

        int update_stage = 0;
        while (!events.quit_requested()) {
            const auto elapsed = events.now_milliseconds();
            if (update_stage == 0 && elapsed >= 300) {
                static_cast<void>(controller.set_content(
                    ryn::String{u8"RynUI text foundation\nContent update: Latin + 中文字形缓存"}));
                ++update_stage;
            } else if (update_stage == 1 && elapsed >= 600) {
                static_cast<void>(controller.set_color({0.36F, 0.72F, 1.0F, 0.65F}));
                ++update_stage;
            } else if (update_stage == 2 && elapsed >= 900) {
                static_cast<void>(controller.set_width_constraint(340.0F));
                ++update_stage;
            } else if (update_stage == 3 && elapsed >= 1'200) {
                if (!renderer.resize_window(840, 500)) {
                    std::cerr << "resize_error=" << renderer.last_error() << '\n';
                    return 4;
                }
                placement.viewport_pixels = {840.0F, 500.0F};
                placement.clip_pixels = {48.0F, 48.0F, 744.0F, 404.0F};
                static_cast<void>(controller.set_width_constraint(420.0F));
                static_cast<void>(controller.set_color({1.0F, 1.0F, 1.0F, 0.65F}));
                ++update_stage;
            }

            const auto step = loop.step();
            if (step == ryn::runtime::FrameLoopStep::failed) {
                std::cerr << "frame_error=" << submitter.last_error() << '\n';
                return 5;
            }
            if (smoke_mode && update_stage == 4 && elapsed >= 1'800
                    && loop.counters().idle_waits >= 20) {
                break;
            }
        }

        const auto& shaped = controller.text_state().shaped();
        const auto& text_counters = controller.text_state().counters();
        const auto& font_counters = fonts->counters();
        const auto& resource_counters = resources.counters();
        const auto& renderer_counters = renderer.counters();
        const auto& loop_counters = loop.counters();
        std::cout
            << "gpu_driver=" << platform.gpu_driver()
            << " shader_format=" << renderer.shader_format()
            << " font_rasterizations=" << font_counters.rasterizations
            << " font_cache_hits=" << font_counters.cache_hits
            << " replacement_count=" << shaped.replacement_count
            << " fallback_runs=" << shaped.runs.size()
            << " shape_count=" << text_counters.shape_count
            << " measure_count=" << text_counters.measure_count
            << " atlas_pages=" << controller.atlas().page_count()
            << " atlas_entries=" << controller.atlas().entry_count()
            << " atlas_uploads=" << resource_counters.texture_uploads
            << " atlas_uploaded_bytes=" << resource_counters.texture_uploaded_bytes
            << " instance_count=" << controller.glyph_scene().instances().size()
            << " instance_rebuilds=" << controller.counters().instance_rebuilds
            << " material_updates=" << controller.counters().material_updates
            << " buffer_uploads=" << resource_counters.buffer_uploads
            << " glyph_draws=" << renderer_counters.glyph_draws
            << " submits=" << renderer_counters.frame_submissions
            << " idle_waits=" << loop_counters.idle_waits
            << " exit_code=0\n";
        return renderer_counters.frame_submissions >= 5 ? 0 : 6;
    } catch (const std::exception& error) {
        std::cerr << "fatal_error=" << error.what() << '\n';
        return 7;
    }
}
