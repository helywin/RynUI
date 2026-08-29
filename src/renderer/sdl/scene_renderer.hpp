#pragma once

#include "graphics/glyph_scene.hpp"
#include "graphics/quad_primitive.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/glyph_gpu_resources.hpp"
#include "renderer/sdl/rounded_effect_gpu_resources.hpp"
#include "runtime/frame_scheduler.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace ryn::detail {

struct SceneRendererCounters {
    std::uint64_t upload_submissions{};
    std::uint64_t uploaded_bytes{};
    std::uint64_t command_buffers{};
    std::uint64_t render_passes{};
    std::uint64_t quad_draws{};
    std::uint64_t glyph_draws{};
    std::uint64_t effect_draws{};
    std::uint64_t effect_instances{};
    std::uint64_t atlas_page_bindings{};
    std::uint64_t frame_submissions{};
    std::uint64_t no_texture_frames{};
};

class SdlSceneRenderer final : public graphics::QuadUploadApi,
                               public GlyphGpuApi,
                               public RoundedEffectGpuApi,
                               public SceneDrawApi,
                               public runtime::FrameSubmitter {
public:
    SdlSceneRenderer(
        PlatformState& platform,
        const std::filesystem::path& shader_directory);
    SdlSceneRenderer(const SdlSceneRenderer&) = delete;
    SdlSceneRenderer& operator=(const SdlSceneRenderer&) = delete;
    ~SdlSceneRenderer() override;

    void attach_scene(
        graphics::QuadGpuBufferHandle quad_buffer,
        GlyphGpuResources& glyph_resources,
        const graphics::OrderedScene& scene,
        RoundedEffectGpuResources* effect_resources = nullptr);
    bool resize_window(int width, int height);

    graphics::QuadGpuBufferHandle create_vertex_buffer(std::size_t size) override;
    void release_buffer(graphics::QuadGpuBufferHandle buffer) noexcept override;
    bool upload(
        graphics::QuadGpuBufferHandle buffer,
        std::size_t offset,
        std::span<const std::byte> bytes) override;
    [[nodiscard]] const char* last_error() const noexcept override;

    GlyphGpuSamplerHandle create_glyph_sampler() override;
    GlyphGpuTextureHandle create_glyph_texture(
        std::uint32_t width,
        std::uint32_t height) override;
    GlyphGpuBufferHandle create_glyph_buffer(std::size_t size) override;
    bool upload_glyph_texture(
        GlyphGpuTextureHandle texture,
        const GlyphTextureUpload& upload) override;
    bool upload_glyph_buffer(
        GlyphGpuBufferHandle buffer,
        std::size_t offset,
        std::span<const std::byte> bytes) override;
    void release_glyph_buffer(GlyphGpuBufferHandle buffer) noexcept override;
    void release_glyph_texture(GlyphGpuTextureHandle texture) noexcept override;
    void release_glyph_sampler(GlyphGpuSamplerHandle sampler) noexcept override;
    [[nodiscard]] const char* glyph_gpu_error() const noexcept override;

    RoundedEffectGpuBufferHandle create_effect_buffer(std::size_t size) override;
    bool upload_effect_buffer(
        RoundedEffectGpuBufferHandle buffer,
        std::size_t offset,
        std::span<const std::byte> bytes) override;
    void release_effect_buffer(RoundedEffectGpuBufferHandle buffer) noexcept override;
    [[nodiscard]] const char* effect_gpu_error() const noexcept override;

    void draw_quad(std::uint32_t first, std::uint32_t count) override;
    void draw_glyph(
        std::uint32_t atlas_page,
        std::uint32_t first,
        std::uint32_t count) override;
    void draw_rounded_effect(
        std::uint32_t first,
        std::uint32_t count) override;
    runtime::FrameSubmissionResult submit_frame(
        animation::AnimationTime frame_time) override;

    [[nodiscard]] const char* shader_format() const noexcept;
    [[nodiscard]] const SceneRendererCounters& counters() const noexcept;

private:
    bool upload_buffer(
        void* buffer,
        std::size_t offset,
        std::span<const std::byte> bytes,
        const char* label);

    PlatformState* platform_;
    void* quad_pipeline_{nullptr};
    void* glyph_pipeline_{nullptr};
    void* effect_pipeline_{nullptr};
    void* quad_buffer_{nullptr};
    GlyphGpuResources* glyph_resources_{nullptr};
    RoundedEffectGpuResources* effect_resources_{nullptr};
    const graphics::OrderedScene* scene_{nullptr};
    void* active_render_pass_{nullptr};
    std::string shader_format_;
    std::string last_error_;
    SceneRendererCounters counters_;
};

} // namespace ryn::detail
