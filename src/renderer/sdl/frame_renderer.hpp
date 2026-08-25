#pragma once

#include "platform/sdl/platform_state.hpp"

#include <cstdint>
#include <string>

namespace ryn::detail {

using GpuCommandBufferHandle = void*;
using GpuTextureHandle = void*;
using GpuRenderPassHandle = void*;

struct ClearColor {
    float red{0.10F};
    float green{0.20F};
    float blue{0.42F};
    float alpha{1.0F};
};

enum class FrameStatus {
    submitted,
    no_swapchain_texture,
    failed,
};

struct FrameResult {
    FrameStatus status{FrameStatus::failed};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return status != FrameStatus::failed;
    }
};

struct FrameCounters {
    std::uint64_t command_buffers{0};
    std::uint64_t swapchain_acquisitions{0};
    std::uint64_t render_passes{0};
    std::uint64_t submissions{0};
    std::uint64_t no_texture_frames{0};
};

class GpuFrameApi {
public:
    virtual ~GpuFrameApi() = default;

    virtual GpuCommandBufferHandle acquire_command_buffer(
        PlatformGpuDeviceHandle device) = 0;
    virtual bool wait_and_acquire_swapchain(
        GpuCommandBufferHandle command_buffer,
        PlatformWindowHandle window,
        GpuTextureHandle& texture,
        std::uint32_t& width,
        std::uint32_t& height) = 0;
    virtual GpuRenderPassHandle begin_clear_pass(
        GpuCommandBufferHandle command_buffer,
        GpuTextureHandle texture,
        ClearColor color) = 0;
    virtual void end_render_pass(GpuRenderPassHandle render_pass) noexcept = 0;
    virtual bool submit(GpuCommandBufferHandle command_buffer) = 0;
    virtual bool cancel(GpuCommandBufferHandle command_buffer) noexcept = 0;
    [[nodiscard]] virtual const char* last_error() const noexcept = 0;
};

class FrameRenderer final {
public:
    explicit FrameRenderer(PlatformState& platform);
    FrameRenderer(PlatformState& platform, GpuFrameApi& api) noexcept;

    [[nodiscard]] FrameResult clear_and_present(ClearColor color = {});
    [[nodiscard]] const FrameCounters& counters() const noexcept;

private:
    PlatformState* platform_;
    GpuFrameApi* api_;
    FrameCounters counters_;
};

} // namespace ryn::detail
