#pragma once

#include "platform/sdl/platform_state.hpp"

#include <cstdint>

namespace ryn::detail {

using GpuDeviceHandle = void*;
#if defined(NDEBUG)
inline constexpr bool default_sdl_gpu_debug = false;
#else
inline constexpr bool default_sdl_gpu_debug = true;
#endif

class GpuBindingApi {
public:
    virtual ~GpuBindingApi() = default;
    virtual GpuDeviceHandle create_gpu_device(bool debug_mode) = 0;
    virtual void destroy_gpu_device(GpuDeviceHandle device) noexcept = 0;
    virtual bool claim_window(GpuDeviceHandle device, PlatformWindowHandle window) = 0;
    virtual void release_window(GpuDeviceHandle device, PlatformWindowHandle window) noexcept = 0;
    [[nodiscard]] virtual const char* gpu_driver(GpuDeviceHandle device) const noexcept = 0;
    [[nodiscard]] virtual const char* last_error() const noexcept = 0;
};

// Host outlives this binding; every GPU resource/pipeline must be retired before
// the binding is destroyed. Rebuild means constructing a fresh binding/renderer.
class SdlGpuBinding final {
public:
    explicit SdlGpuBinding(PlatformState& host, bool debug_mode = default_sdl_gpu_debug);
    SdlGpuBinding(PlatformState& host, GpuBindingApi& api, bool debug_mode = false);
    SdlGpuBinding(const SdlGpuBinding&) = delete;
    SdlGpuBinding& operator=(const SdlGpuBinding&) = delete;
    ~SdlGpuBinding();

    [[nodiscard]] GpuDeviceHandle device() const noexcept {
        return device_;
    }

    [[nodiscard]] const char* driver() const noexcept;

    [[nodiscard]] std::uint64_t epoch() const noexcept {
        return epoch_;
    }

    [[nodiscard]] PlatformState& host() const noexcept {
        return *host_;
    }

private:
    PlatformState* host_;
    GpuBindingApi* api_;
    GpuDeviceHandle device_{};
    std::uint64_t epoch_{};
};

} // namespace ryn::detail
