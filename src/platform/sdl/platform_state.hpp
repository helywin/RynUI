#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace ryn::detail {

using PlatformWindowHandle = void*;
using PlatformGpuDeviceHandle = void*;

enum class PlatformStage {
    sdl_init,
    window,
    gpu_device,
    window_claim,
};

struct PlatformError {
    PlatformStage stage;
    std::string message;
};

struct PlatformConfig {
    std::string title{"RynUI"};
    int width{960};
    int height{640};
    bool gpu_debug{false};
};

class PlatformApi {
public:
    virtual ~PlatformApi() = default;

    virtual bool init_video() = 0;
    virtual void quit() noexcept = 0;
    virtual PlatformWindowHandle create_window(
        const char* title,
        int width,
        int height) = 0;
    virtual void destroy_window(PlatformWindowHandle window) noexcept = 0;
    virtual PlatformGpuDeviceHandle create_gpu_device(bool debug_mode) = 0;
    virtual void destroy_gpu_device(PlatformGpuDeviceHandle device) noexcept = 0;
    virtual bool claim_window(
        PlatformGpuDeviceHandle device,
        PlatformWindowHandle window) = 0;
    virtual void release_window(
        PlatformGpuDeviceHandle device,
        PlatformWindowHandle window) noexcept = 0;
    [[nodiscard]] virtual const char* last_error() const noexcept = 0;
    [[nodiscard]] virtual const char* gpu_driver(
        PlatformGpuDeviceHandle device) const noexcept = 0;
    [[nodiscard]] virtual bool poll_quit_requested() noexcept = 0;
    virtual void delay(std::uint32_t milliseconds) noexcept = 0;
};

struct PlatformCreateResult;

class PlatformState final {
public:
    PlatformState(const PlatformState&) = delete;
    PlatformState& operator=(const PlatformState&) = delete;
    PlatformState(PlatformState&&) = delete;
    PlatformState& operator=(PlatformState&&) = delete;
    ~PlatformState();

    [[nodiscard]] static PlatformCreateResult create(const PlatformConfig& config);
    [[nodiscard]] static PlatformCreateResult create(
        PlatformApi& api,
        const PlatformConfig& config);

    [[nodiscard]] PlatformWindowHandle window() const noexcept;
    [[nodiscard]] PlatformGpuDeviceHandle gpu_device() const noexcept;
    [[nodiscard]] const char* gpu_driver() const noexcept;
    [[nodiscard]] bool is_owner_thread() const noexcept;
    [[nodiscard]] bool poll_quit_requested() noexcept;
    void delay(std::uint32_t milliseconds) noexcept;

private:
    explicit PlatformState(PlatformApi& api) noexcept;

    PlatformApi* api_;
    PlatformWindowHandle window_{nullptr};
    PlatformGpuDeviceHandle gpu_device_{nullptr};
    bool sdl_initialized_{false};
    bool window_claimed_{false};
    std::thread::id owner_thread_;
};

struct PlatformCreateResult {
    std::unique_ptr<PlatformState> state;
    std::optional<PlatformError> error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return state != nullptr;
    }
};

} // namespace ryn::detail
