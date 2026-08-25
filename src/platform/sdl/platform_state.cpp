#include "platform/sdl/platform_state.hpp"

#include <SDL3/SDL.h>

#include <utility>

namespace ryn::detail {
namespace {

class SdlPlatformApi final : public PlatformApi {
public:
    bool init_video() override {
        return SDL_Init(SDL_INIT_VIDEO);
    }

    void quit() noexcept override {
        SDL_Quit();
    }

    PlatformWindowHandle create_window(
        const char* title,
        int width,
        int height) override {
        return SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
    }

    void destroy_window(PlatformWindowHandle window) noexcept override {
        SDL_DestroyWindow(static_cast<SDL_Window*>(window));
    }

    PlatformGpuDeviceHandle create_gpu_device(bool debug_mode) override {
        constexpr auto shader_formats = static_cast<SDL_GPUShaderFormat>(
            SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV);
        return SDL_CreateGPUDevice(shader_formats, debug_mode, nullptr);
    }

    void destroy_gpu_device(PlatformGpuDeviceHandle device) noexcept override {
        SDL_DestroyGPUDevice(static_cast<SDL_GPUDevice*>(device));
    }

    bool claim_window(
        PlatformGpuDeviceHandle device,
        PlatformWindowHandle window) override {
        return SDL_ClaimWindowForGPUDevice(
            static_cast<SDL_GPUDevice*>(device),
            static_cast<SDL_Window*>(window));
    }

    void release_window(
        PlatformGpuDeviceHandle device,
        PlatformWindowHandle window) noexcept override {
        SDL_ReleaseWindowFromGPUDevice(
            static_cast<SDL_GPUDevice*>(device),
            static_cast<SDL_Window*>(window));
    }

    [[nodiscard]] const char* last_error() const noexcept override {
        return SDL_GetError();
    }

    [[nodiscard]] const char* gpu_driver(
        PlatformGpuDeviceHandle device) const noexcept override {
        return SDL_GetGPUDeviceDriver(static_cast<SDL_GPUDevice*>(device));
    }

    [[nodiscard]] bool poll_quit_requested() noexcept override {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT
                    || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                return true;
            }
        }
        return false;
    }

    void delay(std::uint32_t milliseconds) noexcept override {
        SDL_Delay(milliseconds);
    }
};

PlatformCreateResult failed_create(
    std::unique_ptr<PlatformState> state,
    PlatformApi& api,
    PlatformStage stage) {
    const char* error = api.last_error();
    PlatformError failure{
        stage,
        error != nullptr && error[0] != '\0' ? error : "Unknown SDL error",
    };
    state.reset();
    return PlatformCreateResult{nullptr, std::move(failure)};
}

PlatformApi& real_platform_api() {
    static SdlPlatformApi api;
    return api;
}

} // namespace

PlatformState::PlatformState(PlatformApi& api) noexcept
    : api_(&api), owner_thread_(std::this_thread::get_id()) {}

PlatformState::~PlatformState() {
    if (window_claimed_) {
        api_->release_window(gpu_device_, window_);
    }
    if (gpu_device_ != nullptr) {
        api_->destroy_gpu_device(gpu_device_);
    }
    if (window_ != nullptr) {
        api_->destroy_window(window_);
    }
    if (sdl_initialized_) {
        api_->quit();
    }
}

PlatformCreateResult PlatformState::create(const PlatformConfig& config) {
    return create(real_platform_api(), config);
}

PlatformCreateResult PlatformState::create(
    PlatformApi& api,
    const PlatformConfig& config) {
    auto state = std::unique_ptr<PlatformState>(new PlatformState(api));

    if (!api.init_video()) {
        return failed_create(std::move(state), api, PlatformStage::sdl_init);
    }
    state->sdl_initialized_ = true;

    state->window_ = api.create_window(
        config.title.c_str(),
        config.width,
        config.height);
    if (state->window_ == nullptr) {
        return failed_create(std::move(state), api, PlatformStage::window);
    }

    state->gpu_device_ = api.create_gpu_device(config.gpu_debug);
    if (state->gpu_device_ == nullptr) {
        return failed_create(std::move(state), api, PlatformStage::gpu_device);
    }

    if (!api.claim_window(state->gpu_device_, state->window_)) {
        return failed_create(std::move(state), api, PlatformStage::window_claim);
    }
    state->window_claimed_ = true;

    return PlatformCreateResult{std::move(state), std::nullopt};
}

PlatformWindowHandle PlatformState::window() const noexcept {
    return window_;
}

PlatformGpuDeviceHandle PlatformState::gpu_device() const noexcept {
    return gpu_device_;
}

const char* PlatformState::gpu_driver() const noexcept {
    return api_->gpu_driver(gpu_device_);
}

bool PlatformState::is_owner_thread() const noexcept {
    return owner_thread_ == std::this_thread::get_id();
}

bool PlatformState::poll_quit_requested() noexcept {
    return api_->poll_quit_requested();
}

void PlatformState::delay(std::uint32_t milliseconds) noexcept {
    api_->delay(milliseconds);
}

} // namespace ryn::detail
