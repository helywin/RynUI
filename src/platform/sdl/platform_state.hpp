#pragma once

#include "input/platform_input.hpp"

#include <cmath>
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
    bool high_pixel_density{true};
};

struct PlatformWindowMetrics {
    int coordinate_width{0};
    int coordinate_height{0};
    int pixel_width{0};
    int pixel_height{0};
    float pixel_density{1.0F};
    float display_scale{1.0F};

    [[nodiscard]] float logical_width() const noexcept {
        return logical_extent(pixel_width, coordinate_width);
    }

    [[nodiscard]] float logical_height() const noexcept {
        return logical_extent(pixel_height, coordinate_height);
    }

    [[nodiscard]] float coordinate_to_logical_scale() const noexcept {
        return valid_scale(pixel_density) / valid_scale(display_scale);
    }

    friend bool operator==(const PlatformWindowMetrics&, const PlatformWindowMetrics&) =
        default;

private:
    [[nodiscard]] static float valid_scale(float value) noexcept {
        return std::isfinite(value) && value > 0.0F ? value : 1.0F;
    }

    [[nodiscard]] float logical_extent(int pixels, int coordinates) const noexcept {
        if (pixels > 0) {
            return static_cast<float>(pixels) / valid_scale(display_scale);
        }
        return coordinates > 0
            ? static_cast<float>(coordinates) * coordinate_to_logical_scale()
            : 0.0F;
    }
};

struct PlatformEvents {
    bool quit_requested{false};
    bool frame_requested{false};
    std::uint64_t suppressed_compatibility_mouse_events{0};
    input::PlatformInputBatch input;

    void clear() noexcept {
        quit_requested = false;
        frame_requested = false;
        suppressed_compatibility_mouse_events = 0;
        input.clear();
    }
};

struct PlatformEventDiagnostics {
    std::uint64_t poll_calls{0};
    std::uint64_t wait_calls{0};
    std::uint64_t normalized_input_events{0};
    std::uint64_t coalesced_pointer_moves{0};
    std::uint64_t suppressed_compatibility_mouse_events{0};
    std::uint64_t frame_requested_pumps{0};
    std::uint64_t quit_requested_pumps{0};

    friend bool operator==(
        const PlatformEventDiagnostics&,
        const PlatformEventDiagnostics&) = default;
};

class PlatformApi {
public:
    virtual ~PlatformApi() = default;

    virtual bool init_video() = 0;
    virtual void quit() noexcept = 0;
    virtual PlatformWindowHandle create_window(
        const char* title,
        int width,
        int height,
        bool high_pixel_density) = 0;
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
    [[nodiscard]] virtual PlatformWindowMetrics window_metrics(
        PlatformWindowHandle window) const noexcept = 0;
    virtual void delay(std::uint32_t milliseconds) noexcept = 0;
    virtual void poll_events(
        PlatformWindowHandle,
        PlatformEvents&) {
    }
    virtual void wait_events(
        PlatformWindowHandle window,
        std::uint32_t timeout_milliseconds,
        PlatformEvents& result) {
        delay(timeout_milliseconds);
        poll_events(window, result);
    }
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
    [[nodiscard]] PlatformWindowMetrics window_metrics() const noexcept;
    [[nodiscard]] float display_scale() const noexcept;
    [[nodiscard]] bool is_owner_thread() const noexcept;
    [[nodiscard]] bool poll_quit_requested();
    [[nodiscard]] const PlatformEvents& poll_events();
    [[nodiscard]] const PlatformEvents& wait_events(std::uint32_t timeout_milliseconds);
    [[nodiscard]] PlatformEventDiagnostics event_diagnostics() const;
    void delay(std::uint32_t milliseconds) noexcept;

private:
    explicit PlatformState(PlatformApi& api);

    void require_owner_thread() const;
    void record_event_pump();

    PlatformApi* api_;
    PlatformWindowHandle window_{nullptr};
    PlatformGpuDeviceHandle gpu_device_{nullptr};
    bool sdl_initialized_{false};
    bool window_claimed_{false};
    std::thread::id owner_thread_;
    PlatformEvents events_;
    PlatformEventDiagnostics event_diagnostics_;
};

struct PlatformCreateResult {
    std::unique_ptr<PlatformState> state;
    std::optional<PlatformError> error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return state != nullptr;
    }
};

} // namespace ryn::detail
