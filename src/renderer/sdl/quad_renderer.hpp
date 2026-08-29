#pragma once

#include "graphics/quad_primitive.hpp"
#include "platform/sdl/platform_state.hpp"
#include "runtime/frame_scheduler.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace ryn::detail {

struct QuadRendererCounters {
    std::uint64_t upload_submissions{0};
    std::uint64_t uploaded_bytes{0};
    std::uint64_t command_buffers{0};
    std::uint64_t render_passes{0};
    std::uint64_t draw_calls{0};
    std::uint64_t frame_submissions{0};
    std::uint64_t no_texture_frames{0};
};

class SdlQuadRenderer final : public graphics::QuadUploadApi,
                              public runtime::FrameSubmitter {
public:
    SdlQuadRenderer(
        PlatformState& platform,
        const std::filesystem::path& shader_directory);
    SdlQuadRenderer(const SdlQuadRenderer&) = delete;
    SdlQuadRenderer& operator=(const SdlQuadRenderer&) = delete;
    SdlQuadRenderer(SdlQuadRenderer&&) = delete;
    SdlQuadRenderer& operator=(SdlQuadRenderer&&) = delete;
    ~SdlQuadRenderer() override;

    void attach_scene(graphics::QuadGpuBuffer& buffer, std::uint32_t instance_count);

    graphics::QuadGpuBufferHandle create_vertex_buffer(std::size_t size) override;
    void release_buffer(graphics::QuadGpuBufferHandle buffer) noexcept override;
    bool upload(
        graphics::QuadGpuBufferHandle buffer,
        std::size_t offset,
        std::span<const std::byte> bytes) override;
    [[nodiscard]] const char* last_error() const noexcept override;

    runtime::FrameSubmissionResult submit_frame(
        animation::AnimationTime frame_time) override;

    [[nodiscard]] const char* shader_format() const noexcept;
    [[nodiscard]] const QuadRendererCounters& counters() const noexcept;

private:
    PlatformState* platform_;
    void* pipeline_{nullptr};
    void* scene_buffer_{nullptr};
    std::uint32_t instance_count_{0};
    std::string shader_format_;
    std::string last_error_;
    QuadRendererCounters counters_;
};

} // namespace ryn::detail
