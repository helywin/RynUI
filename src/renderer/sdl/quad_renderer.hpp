#pragma once

#include "renderer/common/quad_gpu_resources.hpp"

#include "graphics/quad_primitive.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/gpu_binding.hpp"
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

class SdlQuadRenderer final : public detail::QuadUploadApi, public runtime::FrameSubmitter {
public:
    SdlQuadRenderer(PlatformState& platform, const std::filesystem::path& shader_directory,
                    bool debug_mode = default_sdl_gpu_debug);
    SdlQuadRenderer(const SdlQuadRenderer&) = delete;
    SdlQuadRenderer& operator=(const SdlQuadRenderer&) = delete;
    SdlQuadRenderer(SdlQuadRenderer&&) = delete;
    SdlQuadRenderer& operator=(SdlQuadRenderer&&) = delete;
    ~SdlQuadRenderer() override;

    void attach_scene(detail::QuadGpuBuffer& buffer, std::uint32_t instance_count);

    detail::QuadGpuBufferHandle create_vertex_buffer(std::size_t size) override;
    void release_buffer(detail::QuadGpuBufferHandle buffer) noexcept override;
    bool upload(detail::QuadGpuBufferHandle buffer, std::size_t offset, std::span<const std::byte> bytes) override;
    [[nodiscard]] const char* last_error() const noexcept override;

    runtime::FrameSubmissionResult submit_frame(animation::AnimationTime frame_time) override;

    [[nodiscard]] const char* shader_format() const noexcept;

    [[nodiscard]] const char* gpu_driver() const noexcept {
        return binding_.driver();
    }

    [[nodiscard]] const QuadRendererCounters& counters() const noexcept;

private:
    PlatformState* platform_;
    SdlGpuBinding binding_;
    void* pipeline_{nullptr};
    void* scene_buffer_{nullptr};
    std::uint32_t instance_count_{0};
    std::string shader_format_;
    std::string last_error_;
    QuadRendererCounters counters_;
};

} // namespace ryn::detail
