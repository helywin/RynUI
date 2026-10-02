#include "renderer/sdl/gpu_binding.hpp"

#include <SDL3/SDL.h>

#include <atomic>
#include <stdexcept>
#include <string>

namespace ryn::detail {
namespace {
class SdlGpuBindingApi final : public GpuBindingApi {
public:
    GpuDeviceHandle create_gpu_device(bool debug_mode) override {
        return SDL_CreateGPUDevice(
            static_cast<SDL_GPUShaderFormat>(SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV), debug_mode,
            nullptr);
    }

    void destroy_gpu_device(GpuDeviceHandle device) noexcept override {
        SDL_DestroyGPUDevice(static_cast<SDL_GPUDevice*>(device));
    }

    bool claim_window(GpuDeviceHandle device, PlatformWindowHandle window) override {
        return SDL_ClaimWindowForGPUDevice(static_cast<SDL_GPUDevice*>(device), static_cast<SDL_Window*>(window));
    }

    void release_window(GpuDeviceHandle device, PlatformWindowHandle window) noexcept override {
        SDL_ReleaseWindowFromGPUDevice(static_cast<SDL_GPUDevice*>(device), static_cast<SDL_Window*>(window));
    }

    const char* gpu_driver(GpuDeviceHandle device) const noexcept override {
        return SDL_GetGPUDeviceDriver(static_cast<SDL_GPUDevice*>(device));
    }

    const char* last_error() const noexcept override {
        return SDL_GetError();
    }
};

GpuBindingApi& real_api() {
    static SdlGpuBindingApi api;
    return api;
}

std::atomic<std::uint64_t> next_epoch{1};

std::runtime_error failure(const GpuBindingApi& api, const char* stage) {
    const char* error = api.last_error();
    return std::runtime_error(std::string(stage) + ": " + (error && *error ? error : "Unknown GPU error"));
}
} // namespace

SdlGpuBinding::SdlGpuBinding(PlatformState& host, bool debug_mode) : SdlGpuBinding(host, real_api(), debug_mode) {}

SdlGpuBinding::SdlGpuBinding(PlatformState& host, GpuBindingApi& api, bool debug_mode) : host_(&host), api_(&api) {
    if (!host.is_owner_thread()) {
        throw std::logic_error("GPU binding requires the host owner thread");
    }
    device_ = api.create_gpu_device(debug_mode);
    if (!device_) {
        throw failure(api, "GPU device creation failed");
    }
    try {
        if (!api.claim_window(device_, host.window())) {
            throw failure(api, "GPU window claim failed");
        }
    } catch (...) {
        api.destroy_gpu_device(device_);
        device_ = nullptr;
        throw;
    }
    epoch_ = next_epoch.fetch_add(1, std::memory_order_relaxed);
}

SdlGpuBinding::~SdlGpuBinding() {
    api_->release_window(device_, host_->window());
    api_->destroy_gpu_device(device_);
}

const char* SdlGpuBinding::driver() const noexcept {
    return api_->gpu_driver(device_);
}
} // namespace ryn::detail
