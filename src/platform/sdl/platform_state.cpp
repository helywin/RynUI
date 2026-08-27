#include "platform/sdl/platform_state.hpp"
#include "platform/sdl/sdl_event_adapter.hpp"

#include <SDL3/SDL.h>

#include <stdexcept>
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

    [[nodiscard]] float display_scale(
        PlatformWindowHandle window) const noexcept override {
        return SDL_GetWindowDisplayScale(static_cast<SDL_Window*>(window));
    }

    void poll_events(
        PlatformWindowHandle window,
        PlatformEvents& result) override {
        auto metrics = window_metrics(window);
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            SdlEventAdapter::merge(result, event, metrics);
        }
    }

    void delay(std::uint32_t milliseconds) noexcept override {
        SDL_Delay(milliseconds);
    }

    void wait_events(
        PlatformWindowHandle window,
        std::uint32_t timeout_milliseconds,
        PlatformEvents& result) override {
        auto metrics = window_metrics(window);
        SDL_Event event{};
        if (SDL_WaitEventTimeout(&event, static_cast<Sint32>(timeout_milliseconds))) {
            SdlEventAdapter::merge(result, event, metrics);
            SDL_Event queued{};
            while (SDL_PollEvent(&queued)) {
                SdlEventAdapter::merge(result, queued, metrics);
            }
        }
    }

private:
    static SdlWindowMetrics window_metrics(PlatformWindowHandle window) noexcept {
        SdlWindowMetrics metrics;
        if (window != nullptr) {
            static_cast<void>(SDL_GetWindowSize(
                static_cast<SDL_Window*>(window),
                &metrics.width,
                &metrics.height));
        }
        return metrics;
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

PlatformState::PlatformState(PlatformApi& api)
    : api_(&api), owner_thread_(std::this_thread::get_id()) {
    events_.input.reserve(64);
}

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

float PlatformState::display_scale() const noexcept {
    return api_->display_scale(window_);
}

bool PlatformState::is_owner_thread() const noexcept {
    return owner_thread_ == std::this_thread::get_id();
}

bool PlatformState::poll_quit_requested() {
    return poll_events().quit_requested;
}

const PlatformEvents& PlatformState::poll_events() {
    require_owner_thread();
    events_.clear();
    api_->poll_events(window_, events_);
    ++event_diagnostics_.poll_calls;
    record_event_pump();
    return events_;
}

const PlatformEvents& PlatformState::wait_events(std::uint32_t timeout_milliseconds) {
    require_owner_thread();
    events_.clear();
    api_->wait_events(window_, timeout_milliseconds, events_);
    ++event_diagnostics_.wait_calls;
    record_event_pump();
    return events_;
}

PlatformEventDiagnostics PlatformState::event_diagnostics() const {
    require_owner_thread();
    return event_diagnostics_;
}

void PlatformState::delay(std::uint32_t milliseconds) noexcept {
    api_->delay(milliseconds);
}

void PlatformState::require_owner_thread() const {
    if (!is_owner_thread()) {
        throw std::logic_error("Platform event pump must run on its owner thread");
    }
}

void PlatformState::record_event_pump() {
    event_diagnostics_.normalized_input_events +=
        events_.input.size() + events_.input.coalesced_move_count();
    event_diagnostics_.coalesced_pointer_moves += events_.input.coalesced_move_count();
    event_diagnostics_.suppressed_compatibility_mouse_events +=
        events_.suppressed_compatibility_mouse_events;
    if (events_.frame_requested) {
        ++event_diagnostics_.frame_requested_pumps;
    }
    if (events_.quit_requested) {
        ++event_diagnostics_.quit_requested_pumps;
    }
}

} // namespace ryn::detail
