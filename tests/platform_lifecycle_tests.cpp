#include "platform/sdl/platform_state.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ryn::detail::PlatformApi;
using ryn::detail::PlatformConfig;
using ryn::detail::PlatformGpuDeviceHandle;
using ryn::detail::PlatformStage;
using ryn::detail::PlatformState;
using ryn::detail::PlatformWindowHandle;

enum class FailurePoint {
    none,
    init,
    window,
    device,
    claim,
};

class FakePlatformApi final : public PlatformApi {
public:
    explicit FakePlatformApi(FailurePoint failure) : failure_(failure) {}

    bool init_video() override {
        calls.emplace_back("init");
        return failure_ != FailurePoint::init;
    }

    void quit() noexcept override {
        calls.emplace_back("quit");
    }

    PlatformWindowHandle create_window(const char*, int, int) override {
        calls.emplace_back("create_window");
        return failure_ == FailurePoint::window ? nullptr : &window_token_;
    }

    void destroy_window(PlatformWindowHandle) noexcept override {
        calls.emplace_back("destroy_window");
    }

    PlatformGpuDeviceHandle create_gpu_device(bool) override {
        calls.emplace_back("create_device");
        return failure_ == FailurePoint::device ? nullptr : &device_token_;
    }

    void destroy_gpu_device(PlatformGpuDeviceHandle) noexcept override {
        calls.emplace_back("destroy_device");
    }

    bool claim_window(PlatformGpuDeviceHandle, PlatformWindowHandle) override {
        calls.emplace_back("claim_window");
        return failure_ != FailurePoint::claim;
    }

    void release_window(PlatformGpuDeviceHandle, PlatformWindowHandle) noexcept override {
        calls.emplace_back("release_window");
    }

    [[nodiscard]] const char* last_error() const noexcept override {
        return "injected failure";
    }

    [[nodiscard]] const char* gpu_driver(PlatformGpuDeviceHandle) const noexcept override {
        return "fake-gpu";
    }

    [[nodiscard]] float display_scale(
        PlatformWindowHandle) const noexcept override {
        return 1.25F;
    }

    void delay(std::uint32_t) noexcept override {}

    std::vector<std::string> calls;

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
        require(result.state->gpu_device() != nullptr, "GPU handle is missing");
        require(std::string(result.state->gpu_driver()) == "fake-gpu", "GPU driver differs");
        require(result.state->display_scale() == 1.25F, "display scale differs");
        require(result.state->is_owner_thread(), "lifecycle lost its owner thread");
        require_calls(
            api.calls,
            {"init", "create_window", "create_device", "claim_window"});
    }
    require_calls(
        api.calls,
        {"init",
         "create_window",
         "create_device",
         "claim_window",
         "release_window",
         "destroy_device",
         "destroy_window",
         "quit"});
}

} // namespace

int main() {
    try {
        test_failure(FailurePoint::init, PlatformStage::sdl_init, {"init"});
        test_failure(
            FailurePoint::window,
            PlatformStage::window,
            {"init", "create_window", "quit"});
        test_failure(
            FailurePoint::device,
            PlatformStage::gpu_device,
            {"init", "create_window", "create_device", "destroy_window", "quit"});
        test_failure(
            FailurePoint::claim,
            PlatformStage::window_claim,
            {"init",
             "create_window",
             "create_device",
             "claim_window",
             "destroy_device",
             "destroy_window",
             "quit"});
        test_success_cleanup();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
