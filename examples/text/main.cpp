#include "component/text_component.hpp"
#include "font/font_runtime.hpp"
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
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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
    bool consume(const ryn::detail::PlatformEvents& events) noexcept {
        quit_requested_ = quit_requested_ || events.quit_requested;
        return events.frame_requested;
    }

    ryn::detail::PlatformState* platform_;
    std::chrono::steady_clock::time_point started_;
    bool quit_requested_{};
};

class TextComponentSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    TextComponentSubmitter(
        ryn::detail::TextComponentHost& host,
        ryn::detail::TextSceneService& scene,
        ryn::detail::GlyphGpuResources& resources,
        ryn::detail::SdlSceneRenderer& renderer,
        ryn::runtime::Size& viewport) noexcept
        : host_(&host),
          scene_(&scene),
          resources_(&resources),
          renderer_(&renderer),
          viewport_(&viewport) {}

    ryn::runtime::FrameSubmissionResult submit_frame() override {
        try {
            const ryn::runtime::Rect clip{
                48.0F,
                48.0F,
                std::max(0.0F, viewport_->width - 96.0F),
                std::max(0.0F, viewport_->height - 96.0F),
            };
            if (!host_->layout_and_synchronize(
                    *viewport_, clip, {64.0F, 72.0F}, 12.0F)) {
                last_error_ = "Text component layout or scene synchronization failed";
                return ryn::runtime::FrameSubmissionResult::failed;
            }
            resources_->synchronize(
                scene_->atlas(),
                scene_->glyph_scene().instances());
            renderer_->attach_scene(nullptr, *resources_, scene_->ordered_scene());
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

private:
    ryn::detail::TextComponentHost* host_;
    ryn::detail::TextSceneService* scene_;
    ryn::detail::GlyphGpuResources* resources_;
    ryn::detail::SdlSceneRenderer* renderer_;
    ryn::runtime::Size* viewport_;
    std::string last_error_;
};

} // namespace

int main(int argc, char** argv) {
    try {
        const bool smoke_mode = has_argument(argc, argv, "--smoke");
        const auto executable = executable_directory(argv[0]);
        constexpr ryn::runtime::Size requested_window{960.0F, 540.0F};
        ryn::runtime::Size viewport = requested_window;
        constexpr std::uint32_t pixel_size = 14;

        ryn::detail::PlatformConfig platform_config;
        platform_config.title = "RynUI Public Text Components";
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
        ryn::detail::TextSceneService scene(
            *fonts, text_engine, frame_requests);
        ryn::detail::TextComponentHost application(
            nodes,
            layout,
            dirty,
            scene,
            font_chain.identities());

        ryn::Signal<ryn::String> content{
            ryn::String{u8"RynUI Device Monitor / Latin + 设备监控"}};
        ryn::Signal<ryn::TextTone> tone{ryn::TextTone::Secondary};
        ryn::Signal<ryn::LogicalLength> width{ryn::dp(520.0F)};
        ryn::Signal<ryn::LogicalLength> margin{ryn::dp(8.0F)};
        application.mount(ryn::Content{[&] {
            ryn::Text(
                ryn::TextProps{}
                    .content(content)
                    .layout(
                        ryn::LayoutStyle{}
                            .max_width(width)
                            .margin_bottom(margin)));
            ryn::Text(
                ryn::TextProps{}
                    .content(u8"Secondary: shared RynUI 中文 glyph cache")
                    .tone(tone)
                    .layout(ryn::LayoutStyle{}.max_width(ryn::dp(620.0F))));
            ryn::Text(
                ryn::TextProps{}
                    .content(u8"Disabled: 设备离线 / unavailable")
                    .tone(ryn::TextTone::Disabled));
            ryn::Text(
                ryn::TextProps{}
                    .content(u8"Shared glyph proof: RynUI 中文")
                    .tone(ryn::TextTone::Secondary));
        }});

        ryn::detail::SdlSceneRenderer renderer(platform, executable / "shaders");
        ryn::detail::GlyphGpuResources resources(renderer);
        TextComponentSubmitter submitter(
            application, scene, resources, renderer, viewport);
        PlatformFrameEvents events(platform);
        ryn::runtime::OnDemandFrameLoop loop(
            frame_requests, events, submitter, 10);

        std::uint64_t prop_updates = 0;
        std::uint64_t resize_updates = 0;
        int update_stage = 0;
        while (!events.quit_requested()) {
            const auto elapsed = events.now_milliseconds();
            if (update_stage == 0 && elapsed >= 300) {
                content.set(ryn::String{
                    u8"RynUI live update / 内容更新：温度正常"});
                ++prop_updates;
                ++update_stage;
            } else if (update_stage == 1 && elapsed >= 600) {
                tone.set(ryn::TextTone::Primary);
                ++prop_updates;
                ++update_stage;
            } else if (update_stage == 2 && elapsed >= 900) {
                width.set(ryn::dp(340.0F));
                ++prop_updates;
                ++update_stage;
            } else if (update_stage == 3 && elapsed >= 1'200) {
                margin.set(ryn::dp(24.0F));
                ++prop_updates;
                ++update_stage;
            } else if (update_stage == 4 && elapsed >= 1'500) {
                if (!renderer.resize_window(840, 500)) {
                    std::cerr << "resize_error=" << renderer.last_error() << '\n';
                    return 4;
                }
                const auto resized_metrics = platform.window_metrics();
                viewport = {
                    resized_metrics.logical_width(),
                    resized_metrics.logical_height(),
                };
                frame_requests.request_frame();
                ++resize_updates;
                ++update_stage;
            }

            const auto step = loop.step();
            if (step == ryn::runtime::FrameLoopStep::failed) {
                std::cerr << "frame_error=" << submitter.last_error() << '\n';
                return 5;
            }
            if (smoke_mode && update_stage == 5 && elapsed >= 2'100
                    && loop.counters().idle_waits >= 20) {
                break;
            }
        }

        std::uint64_t shape_count = 0;
        std::uint64_t measure_count = 0;
        std::uint64_t layout_count = 0;
        std::uint64_t instance_rebuilds = 0;
        std::uint64_t material_updates = 0;
        std::uint64_t geometry_updates = 0;
        std::uint64_t replacement_count = 0;
        std::uint64_t fallback_runs = 0;
        for (const auto& mounted : application.mounted_texts()) {
            const auto& state = scene.text_state(mounted.scene);
            const auto& record = scene.record_counters(mounted.scene);
            shape_count += state.counters().shape_count;
            measure_count += state.counters().measure_count;
            replacement_count += state.shaped().replacement_count;
            fallback_runs += state.shaped().runs.size();
            instance_rebuilds += record.instance_rebuilds;
            material_updates += record.material_updates;
            geometry_updates += record.geometry_updates;
            layout_count += nodes.require(scene.node(mounted.scene)).place_count;
        }

        const auto& font_counters = fonts->counters();
        const auto& resource_counters = resources.counters();
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
            << " mount_runs=" << application.components().mount_runs()
            << " prop_updates=" << prop_updates
            << " resize_updates=" << resize_updates
            << " font_rasterizations=" << font_counters.rasterizations
            << " font_cache_hits=" << font_counters.cache_hits
            << " replacement_count=" << replacement_count
            << " fallback_runs=" << fallback_runs
            << " shape_count=" << shape_count
            << " measure_count=" << measure_count
            << " layout_count=" << layout_count
            << " atlas_pages=" << scene.atlas().page_count()
            << " atlas_entries=" << scene.atlas().entry_count()
            << " atlas_uploads=" << resource_counters.texture_uploads
            << " atlas_uploaded_bytes=" << resource_counters.texture_uploaded_bytes
            << " instance_count=" << scene.glyph_scene().instances().size()
            << " instance_rebuilds=" << instance_rebuilds
            << " material_updates=" << material_updates
            << " geometry_updates=" << geometry_updates
            << " buffer_uploads=" << resource_counters.buffer_uploads
            << " glyph_draws=" << renderer_counters.glyph_draws
            << " submits=" << renderer_counters.frame_submissions
            << " idle_waits=" << loop_counters.idle_waits
            << " exit_code=0\n";
        return renderer_counters.frame_submissions >= 6
                && prop_updates == 4
                && resize_updates == 1
            ? 0
            : 6;
    } catch (const std::exception& error) {
        std::cerr << "fatal_error=" << error.what() << '\n';
        return 7;
    }
}
