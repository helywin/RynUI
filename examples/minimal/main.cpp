#include "graphics/quad_scene.hpp"
#include "layout/layout_engine.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/quad_renderer.hpp"
#include "runtime/component_mount.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/invalidation.hpp"

#include <ryn/rynui.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

bool has_argument(int argc, char** argv, std::string_view expected) {
    for (int index = 1; index < argc; ++index) {
        if (argv[index] == expected) {
            return true;
        }
    }
    return false;
}

const char* stage_name(ryn::detail::PlatformStage stage) {
    using ryn::detail::PlatformStage;
    switch (stage) {
    case PlatformStage::sdl_init:
        return "sdl_init";
    case PlatformStage::window:
        return "window";
    case PlatformStage::gpu_device:
        return "gpu_device";
    case PlatformStage::window_claim:
        return "window_claim";
    }
    return "unknown";
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

    [[nodiscard]] bool quit_requested() const noexcept {
        return quit_requested_;
    }

private:
    bool consume(const ryn::detail::PlatformEvents& events) noexcept {
        quit_requested_ = quit_requested_ || events.quit_requested;
        return events.frame_requested;
    }

    ryn::detail::PlatformState* platform_;
    std::chrono::steady_clock::time_point started_;
    bool quit_requested_{false};
};

class SceneSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    SceneSubmitter(
        ryn::runtime::DirtyQueues& dirty,
        ryn::layout::LayoutEngine& layout,
        ryn::graphics::QuadScene& scene,
        ryn::graphics::QuadGpuBuffer& gpu_buffer,
        ryn::detail::SdlQuadRenderer& renderer,
        ryn::runtime::Size viewport) noexcept
        : dirty_(&dirty),
          layout_(&layout),
          scene_(&scene),
          gpu_buffer_(&gpu_buffer),
          renderer_(&renderer),
          viewport_(viewport) {}

    ryn::runtime::FrameSubmissionResult submit_frame() override {
        for (const auto root : dirty_->layout_roots()) {
            static_cast<void>(layout_->layout(
                root,
                ryn::layout::Constraints::fixed(viewport_.width, viewport_.height)));
        }
        static_cast<void>(scene_->sync_dirty(*dirty_, *gpu_buffer_, viewport_));
        dirty_->clear();
        return renderer_->submit_frame();
    }

private:
    ryn::runtime::DirtyQueues* dirty_;
    ryn::layout::LayoutEngine* layout_;
    ryn::graphics::QuadScene* scene_;
    ryn::graphics::QuadGpuBuffer* gpu_buffer_;
    ryn::detail::SdlQuadRenderer* renderer_;
    ryn::runtime::Size viewport_;
};

struct ExampleCounters {
    std::uint64_t signal_writes{0};
    std::uint64_t observer_executions{0};
};

} // namespace

int main(int argc, char** argv) {
    try {
        const bool smoke_mode = has_argument(argc, argv, "--smoke");
        constexpr ryn::runtime::Size requested_window{960.0F, 640.0F};
        const auto current = ryn::version();
        std::cout << "RynUI " << current.major << '.' << current.minor << '.'
                  << current.patch << '\n';

        ryn::detail::PlatformConfig config;
        config.title = "RynUI Reactive Quad";
        config.width = static_cast<int>(requested_window.width);
        config.height = static_cast<int>(requested_window.height);
#if !defined(NDEBUG)
        config.gpu_debug = true;
#endif

        auto created = ryn::detail::PlatformState::create(config);
        if (!created) {
            std::cerr << "platform_stage=" << stage_name(created.error->stage)
                      << " error=" << created.error->message << '\n';
            return 1;
        }
        auto& platform = *created.state;
        const auto initial_window_metrics = platform.window_metrics();
        const ryn::runtime::Size viewport{
            initial_window_metrics.logical_width(),
            initial_window_metrics.logical_height(),
        };
        if (viewport.width <= 0.0F || viewport.height <= 0.0F) {
            std::cerr << "platform_error=window metrics did not provide a logical viewport\n";
            return 1;
        }

        ryn::Signal<ryn::runtime::Color> color{{0.12F, 0.48F, 0.95F, 1.0F}};
        ryn::Signal<float> opacity{0.95F};
        ryn::Signal<ryn::runtime::Point> translation{{0.0F, 0.0F}};
        ryn::Signal<ryn::runtime::Size> size{{360.0F, 220.0F}};
        ExampleCounters example_counters;
        ryn::runtime::NodeStore nodes;
        ryn::runtime::FrameRequestState frame_requests;
        ryn::runtime::DirtyQueues dirty(nodes, &frame_requests);
        ryn::runtime::NodePropertyWriter properties(nodes, dirty);
        ryn::runtime::NodeId root;
        ryn::runtime::NodeId quad_node;

        ryn::runtime::ComponentInstance component(
            nodes,
            [&](ryn::runtime::MountContext& context) {
                root = context.create_root();
                quad_node = context.create_child(root);
                static_cast<void>(ryn::connect_binding(
                    context.scope(),
                    ryn::bind([&] { return color.get(); }),
                    [&](ryn::runtime::Color value) {
                        ++example_counters.observer_executions;
                        static_cast<void>(properties.set_color(quad_node, value));
                    }));
                static_cast<void>(ryn::connect_binding(
                    context.scope(),
                    ryn::bind([&] { return opacity.get(); }),
                    [&](float value) {
                        ++example_counters.observer_executions;
                        static_cast<void>(properties.set_opacity(quad_node, value));
                    }));
                static_cast<void>(ryn::connect_binding(
                    context.scope(),
                    ryn::bind([&] { return translation.get(); }),
                    [&](ryn::runtime::Point value) {
                        ++example_counters.observer_executions;
                        static_cast<void>(properties.set_translation(quad_node, value));
                    }));
                static_cast<void>(ryn::connect_binding(
                    context.scope(),
                    ryn::bind([&] { return size.get(); }),
                    [&](ryn::runtime::Size value) {
                        ++example_counters.observer_executions;
                        static_cast<void>(properties.set_size(quad_node, value));
                    }));
            });

        ryn::layout::LayoutEngine layout(nodes);
        layout.set_layout(root, ryn::layout::BoxLayout{
            {260.0F, 190.0F, 0.0F, 0.0F},
            true,
            true,
        });
        layout.set_layout(quad_node, ryn::layout::LeafLayout{size.get()});
        static_cast<void>(layout.layout(
            root,
            ryn::layout::Constraints::fixed(viewport.width, viewport.height)));

        ryn::graphics::QuadScene scene(nodes);
        static_cast<void>(scene.add_quad(quad_node, viewport, 28.0F));
        ryn::detail::SdlQuadRenderer renderer(
            platform,
            executable_directory(argv[0]) / "shaders");
        ryn::graphics::QuadGpuBuffer gpu_buffer(renderer, scene.instances());
        renderer.attach_scene(gpu_buffer, static_cast<std::uint32_t>(scene.instances().size()));
        dirty.clear();
        if (!frame_requests.pending()) {
            frame_requests.request_frame();
        }

        SceneSubmitter submitter(
            dirty,
            layout,
            scene,
            gpu_buffer,
            renderer,
            viewport);
        PlatformFrameEvents events(platform);
        ryn::runtime::OnDemandFrameLoop frame_loop(
            frame_requests,
            events,
            submitter,
            10);

        int update_stage = 0;
        while (!events.quit_requested()) {
            const auto elapsed = events.now_milliseconds();
            if (update_stage == 0 && elapsed >= 250) {
                ryn::batch([&] {
                    color.set({0.42F, 0.20F, 0.92F, 1.0F});
                    opacity.set(0.78F);
                });
                example_counters.signal_writes += 2;
                ++update_stage;
            } else if (update_stage == 1 && elapsed >= 550) {
                translation.set({70.0F, 25.0F});
                ++example_counters.signal_writes;
                ++update_stage;
            } else if (update_stage == 2 && elapsed >= 850) {
                size.set({470.0F, 270.0F});
                ++example_counters.signal_writes;
                ++update_stage;
            }

            const auto step = frame_loop.step();
            if (step == ryn::runtime::FrameLoopStep::failed) {
                std::cerr << "frame_error=" << renderer.last_error() << '\n';
                return 2;
            }
            if (smoke_mode
                    && update_stage == 3
                    && elapsed >= 1'300
                    && frame_loop.counters().idle_waits >= 20) {
                break;
            }
        }

        const auto& root_node = nodes.require(root);
        const auto& quad = nodes.require(quad_node);
        const auto& scene_counters = scene.counters();
        const auto& upload_counters = gpu_buffer.counters();
        const auto& renderer_counters = renderer.counters();
        const auto& loop_counters = frame_loop.counters();
        const auto window_metrics = platform.window_metrics();
        std::cout
            << "gpu_driver=" << platform.gpu_driver()
            << " shader_format=" << renderer.shader_format()
            << " display_scale=" << window_metrics.display_scale
            << " pixel_density=" << window_metrics.pixel_density
            << " window_size=" << window_metrics.coordinate_width << 'x'
            << window_metrics.coordinate_height
            << " pixel_size=" << window_metrics.pixel_width << 'x'
            << window_metrics.pixel_height
            << " viewport=" << viewport.width << 'x' << viewport.height
            << " component_runs=" << component.mount_runs()
            << " signal_writes=" << example_counters.signal_writes
            << " observer_executions=" << example_counters.observer_executions
            << " measure=" << root_node.measure_count + quad.measure_count
            << " layout=" << root_node.place_count + quad.place_count
            << " primitive_rebuilds=" << scene_counters.primitive_rebuilds
            << " instance_updates=" << scene_counters.instance_updates
            << " gpu_uploads="
            << upload_counters.initial_uploads + upload_counters.range_uploads
            << " gpu_uploaded_bytes=" << upload_counters.uploaded_bytes
            << " submits=" << renderer_counters.frame_submissions
            << " idle_wakes=" << loop_counters.event_wakes
            << " idle_waits=" << loop_counters.idle_waits
            << '\n';
        return renderer_counters.frame_submissions >= 4 ? 0 : 3;
    } catch (const std::exception& error) {
        std::cerr << "fatal_error=" << error.what() << '\n';
        return 4;
    }
}
