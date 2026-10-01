#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/gpu_binding.hpp"
#include "graphics/quad_primitive.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ryn::detail::PlatformApi;
using ryn::detail::PlatformConfig;
using ryn::detail::GpuDeviceHandle;
using ryn::detail::GpuBindingApi;
using ryn::detail::SdlGpuBinding;
using ryn::detail::PlatformStage;
using ryn::detail::PlatformState;
using ryn::detail::PlatformWindowHandle;
using ryn::detail::PlatformWindowMetrics;

enum class FailurePoint {
    none,
    init,
    window,
    device,
    claim,
};

class FakePlatformApi final : public PlatformApi, public GpuBindingApi {
public:
    explicit FakePlatformApi(FailurePoint failure) : failure_(failure) {}

    bool init_video() override {
        calls.emplace_back("init");
        return failure_ != FailurePoint::init;
    }

    void quit() noexcept override {
        calls.emplace_back("quit");
    }

    PlatformWindowHandle create_window(const char*, int, int, bool high_pixel_density) override {
        calls.emplace_back("create_window");
        high_pixel_density_requested = high_pixel_density;
        return failure_ == FailurePoint::window ? nullptr : &window_token_;
    }

    void destroy_window(PlatformWindowHandle) noexcept override {
        calls.emplace_back("destroy_window");
    }

    GpuDeviceHandle create_gpu_device(bool) override {
        calls.emplace_back("create_device");
        device_live = failure_ != FailurePoint::device;
        return failure_ == FailurePoint::device ? nullptr : &device_token_;
    }

    void destroy_gpu_device(GpuDeviceHandle) noexcept override {
        calls.emplace_back("destroy_device");
        device_live = false;
    }

    bool claim_window(GpuDeviceHandle, PlatformWindowHandle) override {
        calls.emplace_back("claim_window");
        return failure_ != FailurePoint::claim;
    }

    void release_window(GpuDeviceHandle, PlatformWindowHandle) noexcept override {
        calls.emplace_back("release_window");
    }

    [[nodiscard]] const char* last_error() const noexcept override {
        return "injected failure";
    }

    [[nodiscard]] const char* gpu_driver(GpuDeviceHandle) const noexcept override {
        return "fake-gpu";
    }

    [[nodiscard]] PlatformWindowMetrics window_metrics(
        PlatformWindowHandle) const noexcept override {
        return {960, 720, 1200, 900, 1.25F, 1.25F};
    }

    void delay(std::uint32_t) noexcept override {}

    std::vector<std::string> calls;
    bool high_pixel_density_requested{false};
    bool device_live{};

private:
    FailurePoint failure_;
    int window_token_{0};
    int device_token_{0};
};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_calls(
    const std::vector<std::string>& actual,
    const std::vector<std::string>& expected) {
    if (actual != expected) {
        std::cerr << "Expected:";
        for (const auto& call : expected) {
            std::cerr << ' ' << call;
        }
        std::cerr << "\nActual:";
        for (const auto& call : actual) {
            std::cerr << ' ' << call;
        }
        std::cerr << '\n';
        throw std::runtime_error("lifecycle call order differs");
    }
}

void test_failure(
    FailurePoint point,
    PlatformStage expected_stage,
    const std::vector<std::string>& expected_calls) {
    FakePlatformApi api(point);
    const auto result = PlatformState::create(api, PlatformConfig{});
    require(!result, "injected failure unexpectedly succeeded");
    require(result.error.has_value(), "failure did not include an error");
    require(result.error->stage == expected_stage, "failure stage differs");
    require(result.error->message == "injected failure", "SDL error was not preserved");
    require_calls(api.calls, expected_calls);
}

void test_success_cleanup() {
    FakePlatformApi api(FailurePoint::none);
    {
        auto result = PlatformState::create(api, PlatformConfig{});
        require(static_cast<bool>(result), "valid lifecycle failed");
        require(!result.error.has_value(), "successful lifecycle included an error");
        require(result.state->window() != nullptr, "window handle is missing");
        require(result.state->display_scale() == 1.25F, "display scale differs");
        require(result.state->window_metrics().logical_width() == 960.0F,
                "logical viewport width differs");
        require(api.high_pixel_density_requested,
                "platform did not request a high-pixel-density window");
        require(result.state->is_owner_thread(), "lifecycle lost its owner thread");
        require_calls(
            api.calls,
            {"init", "create_window"});
    }
    require_calls(
        api.calls,
        {"init",
         "create_window",
         "destroy_window",
         "quit"});
}

void test_gpu_failure_preserves_host(FailurePoint point) {
    FakePlatformApi api(point);
    auto host = PlatformState::create(api, {});
    require(bool(host), "GPU failure prevented host creation");
    try {
        SdlGpuBinding binding(*host.state, api);
        throw std::logic_error("GPU binding unexpectedly succeeded");
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()).find("injected failure") != std::string::npos,
                "binding lost GPU error");
    }
    static_cast<void>(host.state->poll_events());
    require(host.state->window_metrics().pixel_width == 1200, "host service lost after GPU failure");
    require(!api.device_live, "failed binding leaked its GPU device");
    if (point == FailurePoint::device)
        require_calls(api.calls, {"init", "create_window", "create_device"});
    else
        require_calls(api.calls, {"init", "create_window", "create_device", "claim_window", "destroy_device"});
    host.state.reset();
    require(api.calls[api.calls.size() - 2] == "destroy_window" && api.calls.back() == "quit",
            "host did not independently clean up");
}

class ResourceApi final : public ryn::graphics::QuadUploadApi {
public:
    explicit ResourceApi(FakePlatformApi& api) : api_(&api) {}
    void* create_vertex_buffer(std::size_t) override { return this; }
    void release_buffer(void*) noexcept override {
        released_on_live_device = api_->device_live;
        api_->calls.emplace_back("release_resource");
    }
    bool upload(void*, std::size_t, std::span<const std::byte>) override { return api_->device_live; }
    const char* last_error() const noexcept override { return "resource device absent"; }
    bool released_on_live_device{};
private:
    FakePlatformApi* api_;
};

void test_resources_retired_before_binding_rebuild() {
    FakePlatformApi api(FailurePoint::none);
    auto host = PlatformState::create(api, {});
    require(bool(host), "host create failed");
    ResourceApi resources(api);
    ryn::graphics::QuadInstanceStore store;
    const std::array<ryn::graphics::QuadInstance, 1> instances{};
    static_cast<void>(store.append(instances));
    std::uint64_t previous_epoch{};
    for (int index = 0; index != 2; ++index) {
        {
            SdlGpuBinding binding(*host.state, api);
            require(binding.device() && std::string(binding.driver()) == "fake-gpu", "binding invalid");
            require(binding.epoch() > previous_epoch, "rebuild reused device epoch");
            previous_epoch = binding.epoch();
            { ryn::graphics::QuadGpuBuffer buffer(resources, store); }
            require(resources.released_on_live_device, "old resource released through destroyed device");
        }
        require(!api.device_live, "binding did not destroy device");
    }
    require_calls(api.calls, {"init", "create_window", "create_device", "claim_window",
        "release_resource", "release_window", "destroy_device", "create_device", "claim_window",
        "release_resource", "release_window", "destroy_device"});
}

} // namespace

int main() {
    try {
        test_failure(FailurePoint::init, PlatformStage::sdl_init, {"init"});
        test_failure(
            FailurePoint::window,
            PlatformStage::window,
            {"init", "create_window", "quit"});
        test_gpu_failure_preserves_host(FailurePoint::device);
        test_gpu_failure_preserves_host(FailurePoint::claim);
        test_resources_retired_before_binding_rebuild();
        test_success_cleanup();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
