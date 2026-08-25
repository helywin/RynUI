#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/frame_renderer.hpp"

#include <ryn/rynui.hpp>

#include <chrono>
#include <iostream>
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

} // namespace

int main(int argc, char** argv) {
    const bool smoke_mode = has_argument(argc, argv, "--smoke");
    const auto current = ryn::version();
    std::cout << "RynUI " << current.major << '.' << current.minor << '.'
              << current.patch << '\n';

    ryn::detail::PlatformConfig config;
    config.title = "RynUI GPU Lifecycle";
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
    ryn::detail::FrameRenderer renderer(platform);
    bool running = true;
    bool needs_frame = true;
    const auto started_at = std::chrono::steady_clock::now();

    while (running) {
        if (platform.poll_quit_requested()) {
            running = false;
            continue;
        }

        if (needs_frame) {
            const auto frame = renderer.clear_and_present();
            if (!frame) {
                std::cerr << "frame_error=" << frame.message << '\n';
                return 2;
            }
            needs_frame = frame.status == ryn::detail::FrameStatus::no_swapchain_texture;
        }

        const auto elapsed = std::chrono::steady_clock::now() - started_at;
        if (smoke_mode
                && renderer.counters().submissions > 0
                && elapsed >= std::chrono::milliseconds(500)) {
            running = false;
            continue;
        }
        platform.delay(10);
    }

    const auto& counters = renderer.counters();
    std::cout << "gpu_driver=" << platform.gpu_driver()
              << " command_buffers=" << counters.command_buffers
              << " render_passes=" << counters.render_passes
              << " submissions=" << counters.submissions
              << " no_texture_frames=" << counters.no_texture_frames << '\n';
    return counters.submissions > 0 ? 0 : 3;
}
