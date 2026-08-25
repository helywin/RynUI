#include "renderer/sdl/frame_renderer.hpp"

#include <SDL3/SDL.h>

#include <utility>

namespace ryn::detail {
namespace {

class SdlGpuFrameApi final : public GpuFrameApi {
public:
    GpuCommandBufferHandle acquire_command_buffer(
        PlatformGpuDeviceHandle device) override {
        return SDL_AcquireGPUCommandBuffer(static_cast<SDL_GPUDevice*>(device));
    }

    bool wait_and_acquire_swapchain(
        GpuCommandBufferHandle command_buffer,
        PlatformWindowHandle window,
        GpuTextureHandle& texture,
        std::uint32_t& width,
        std::uint32_t& height) override {
        SDL_GPUTexture* sdl_texture = nullptr;
        const bool acquired = SDL_WaitAndAcquireGPUSwapchainTexture(
            static_cast<SDL_GPUCommandBuffer*>(command_buffer),
            static_cast<SDL_Window*>(window),
            &sdl_texture,
            &width,
            &height);
        texture = sdl_texture;
        return acquired;
    }

    GpuRenderPassHandle begin_clear_pass(
        GpuCommandBufferHandle command_buffer,
        GpuTextureHandle texture,
        ClearColor color) override {
        SDL_GPUColorTargetInfo target{};
        target.texture = static_cast<SDL_GPUTexture*>(texture);
        target.clear_color = SDL_FColor{color.red, color.green, color.blue, color.alpha};
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        return SDL_BeginGPURenderPass(
            static_cast<SDL_GPUCommandBuffer*>(command_buffer),
            &target,
            1,
            nullptr);
    }

    void end_render_pass(GpuRenderPassHandle render_pass) noexcept override {
        SDL_EndGPURenderPass(static_cast<SDL_GPURenderPass*>(render_pass));
    }

    bool submit(GpuCommandBufferHandle command_buffer) override {
        return SDL_SubmitGPUCommandBuffer(
            static_cast<SDL_GPUCommandBuffer*>(command_buffer));
    }

    bool cancel(GpuCommandBufferHandle command_buffer) noexcept override {
        return SDL_CancelGPUCommandBuffer(
            static_cast<SDL_GPUCommandBuffer*>(command_buffer));
    }

    [[nodiscard]] const char* last_error() const noexcept override {
        return SDL_GetError();
    }
};

GpuFrameApi& real_gpu_frame_api() {
    static SdlGpuFrameApi api;
    return api;
}

std::string copy_error(GpuFrameApi& api, const char* fallback) {
    const char* error = api.last_error();
    return error != nullptr && error[0] != '\0' ? error : fallback;
}

} // namespace

FrameRenderer::FrameRenderer(PlatformState& platform)
    : FrameRenderer(platform, real_gpu_frame_api()) {}

FrameRenderer::FrameRenderer(PlatformState& platform, GpuFrameApi& api) noexcept
    : platform_(&platform), api_(&api) {}

FrameResult FrameRenderer::clear_and_present(ClearColor color) {
    if (!platform_->is_owner_thread()) {
        return FrameResult{
            FrameStatus::failed,
            "GPU frame work must run on the Window owner thread",
        };
    }

    const auto command_buffer = api_->acquire_command_buffer(platform_->gpu_device());
    if (command_buffer == nullptr) {
        return FrameResult{
            FrameStatus::failed,
            copy_error(*api_, "Failed to acquire GPU command buffer"),
        };
    }
    ++counters_.command_buffers;

    GpuTextureHandle swapchain_texture = nullptr;
    std::uint32_t swapchain_width = 0;
    std::uint32_t swapchain_height = 0;
    if (!api_->wait_and_acquire_swapchain(
            command_buffer,
            platform_->window(),
            swapchain_texture,
            swapchain_width,
            swapchain_height)) {
        auto message = copy_error(*api_, "Failed to acquire GPU swapchain texture");
        api_->cancel(command_buffer);
        return FrameResult{FrameStatus::failed, std::move(message)};
    }
    ++counters_.swapchain_acquisitions;

    if (swapchain_texture == nullptr) {
        if (!api_->submit(command_buffer)) {
            return FrameResult{
                FrameStatus::failed,
                copy_error(*api_, "Failed to submit empty GPU command buffer"),
            };
        }
        ++counters_.submissions;
        ++counters_.no_texture_frames;
        return FrameResult{FrameStatus::no_swapchain_texture, {}};
    }

    const auto render_pass = api_->begin_clear_pass(
        command_buffer,
        swapchain_texture,
        color);
    if (render_pass == nullptr) {
        auto message = copy_error(*api_, "Failed to begin GPU render pass");
        api_->submit(command_buffer);
        return FrameResult{FrameStatus::failed, std::move(message)};
    }
    ++counters_.render_passes;
    api_->end_render_pass(render_pass);

    if (!api_->submit(command_buffer)) {
        return FrameResult{
            FrameStatus::failed,
            copy_error(*api_, "Failed to submit GPU command buffer"),
        };
    }
    ++counters_.submissions;
    return FrameResult{FrameStatus::submitted, {}};
}

const FrameCounters& FrameRenderer::counters() const noexcept {
    return counters_;
}

} // namespace ryn::detail
