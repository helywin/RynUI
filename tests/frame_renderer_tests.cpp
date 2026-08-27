#include "renderer/sdl/frame_renderer.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace ryn::detail;

class FakePlatformApi final : public PlatformApi {
public:
    bool init_video() override { return true; }
    void quit() noexcept override {}
    PlatformWindowHandle create_window(const char*, int, int) override { return &window_; }
    void destroy_window(PlatformWindowHandle) noexcept override {}
    PlatformGpuDeviceHandle create_gpu_device(bool) override { return &device_; }
    void destroy_gpu_device(PlatformGpuDeviceHandle) noexcept override {}
    bool claim_window(PlatformGpuDeviceHandle, PlatformWindowHandle) override { return true; }
    void release_window(PlatformGpuDeviceHandle, PlatformWindowHandle) noexcept override {}
    [[nodiscard]] const char* last_error() const noexcept override { return "platform error"; }
    [[nodiscard]] const char* gpu_driver(PlatformGpuDeviceHandle) const noexcept override {
        return "fake-gpu";
    }
    [[nodiscard]] float display_scale(
        PlatformWindowHandle) const noexcept override {
        return 1.0F;
    }
    void delay(std::uint32_t) noexcept override {}

private:
    int window_{0};
    int device_{0};
};

class FakeGpuFrameApi final : public GpuFrameApi {
public:
    enum class Mode {
        submitted,
        minimized,
        command_failure,
        swapchain_failure,
    };

    explicit FakeGpuFrameApi(Mode mode) : mode_(mode) {}

    GpuCommandBufferHandle acquire_command_buffer(PlatformGpuDeviceHandle) override {
        calls.emplace_back("acquire_command_buffer");
        return mode_ == Mode::command_failure ? nullptr : &command_buffer_;
    }

    bool wait_and_acquire_swapchain(
        GpuCommandBufferHandle,
        PlatformWindowHandle,
        GpuTextureHandle& texture,
        std::uint32_t& width,
        std::uint32_t& height) override {
        calls.emplace_back("acquire_swapchain");
        if (mode_ == Mode::swapchain_failure) {
            return false;
        }
        texture = mode_ == Mode::minimized ? nullptr : &texture_;
        width = 960;
        height = 640;
        return true;
    }

    GpuRenderPassHandle begin_clear_pass(
        GpuCommandBufferHandle,
        GpuTextureHandle,
        ClearColor) override {
        calls.emplace_back("begin_render_pass");
        return &render_pass_;
    }

    void end_render_pass(GpuRenderPassHandle) noexcept override {
        calls.emplace_back("end_render_pass");
    }

    bool submit(GpuCommandBufferHandle) override {
        calls.emplace_back("submit");
        return true;
    }

    bool cancel(GpuCommandBufferHandle) noexcept override {
        calls.emplace_back("cancel");
        return true;
    }

    [[nodiscard]] const char* last_error() const noexcept override {
        return "injected frame failure";
    }

    std::vector<std::string> calls;

private:
    Mode mode_;
    int command_buffer_{0};
    int texture_{0};
    int render_pass_{0};
};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_calls(
    const std::vector<std::string>& actual,
    const std::vector<std::string>& expected) {
    require(actual == expected, "frame call order differs");
}

std::unique_ptr<PlatformState> create_platform(FakePlatformApi& api) {
    auto result = PlatformState::create(api, PlatformConfig{});
    require(static_cast<bool>(result), "fake platform creation failed");
    return std::move(result.state);
}

void test_submitted_frame() {
    FakePlatformApi platform_api;
    auto platform = create_platform(platform_api);
    FakeGpuFrameApi frame_api(FakeGpuFrameApi::Mode::submitted);
    FrameRenderer renderer(*platform, frame_api);

    const auto result = renderer.clear_and_present();
    require(result.status == FrameStatus::submitted, "frame was not submitted");
    require_calls(
        frame_api.calls,
        {"acquire_command_buffer",
         "acquire_swapchain",
         "begin_render_pass",
         "end_render_pass",
         "submit"});
    const auto& counters = renderer.counters();
    require(counters.command_buffers == 1, "command buffer counter differs");
    require(counters.swapchain_acquisitions == 1, "swapchain counter differs");
    require(counters.render_passes == 1, "render pass counter differs");
    require(counters.submissions == 1, "submission counter differs");
    require(counters.no_texture_frames == 0, "no-texture counter differs");
}

void test_minimized_frame_submits_without_render_pass() {
    FakePlatformApi platform_api;
    auto platform = create_platform(platform_api);
    FakeGpuFrameApi frame_api(FakeGpuFrameApi::Mode::minimized);
    FrameRenderer renderer(*platform, frame_api);

    const auto result = renderer.clear_and_present();
    require(
        result.status == FrameStatus::no_swapchain_texture,
        "minimized frame was treated as failure");
    require_calls(
        frame_api.calls,
        {"acquire_command_buffer", "acquire_swapchain", "submit"});
    require(renderer.counters().render_passes == 0, "minimized frame began a render pass");
    require(renderer.counters().submissions == 1, "minimized frame was not submitted");
    require(renderer.counters().no_texture_frames == 1, "minimized frame was not counted");
}

void test_pre_swapchain_failure_cancels_command_buffer() {
    FakePlatformApi platform_api;
    auto platform = create_platform(platform_api);
    FakeGpuFrameApi frame_api(FakeGpuFrameApi::Mode::swapchain_failure);
    FrameRenderer renderer(*platform, frame_api);

    const auto result = renderer.clear_and_present();
    require(result.status == FrameStatus::failed, "swapchain failure unexpectedly succeeded");
    require(result.message == "injected frame failure", "frame error was not preserved");
    require_calls(
        frame_api.calls,
        {"acquire_command_buffer", "acquire_swapchain", "cancel"});
    require(renderer.counters().submissions == 0, "failed frame was counted as submitted");
}

void test_owner_thread_guard() {
    FakePlatformApi platform_api;
    auto platform = create_platform(platform_api);
    FakeGpuFrameApi frame_api(FakeGpuFrameApi::Mode::submitted);
    FrameRenderer renderer(*platform, frame_api);
    FrameResult result;

    std::thread worker([&] { result = renderer.clear_and_present(); });
    worker.join();

    require(result.status == FrameStatus::failed, "worker thread frame unexpectedly succeeded");
    require(frame_api.calls.empty(), "worker thread reached the GPU API");
}

} // namespace

int main() {
    try {
        test_submitted_frame();
        test_minimized_frame_submits_without_render_pass();
        test_pre_swapchain_failure_cancels_command_buffer();
        test_owner_thread_guard();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
