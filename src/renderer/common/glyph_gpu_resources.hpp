#pragma once

#include "graphics/glyph_atlas.hpp"
#include "graphics/glyph_scene.hpp"
#include "renderer/common/scene_packing.hpp"
#include "renderer/common/glyph_texture_upload.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <optional>
#include <vector>

namespace ryn::detail {

using GlyphGpuTextureHandle = void*;
using GlyphGpuSamplerHandle = void*;
using GlyphGpuBufferHandle = void*;

class GlyphGpuApi {
public:
    virtual ~GlyphGpuApi() = default;

    virtual GlyphGpuSamplerHandle create_glyph_sampler() = 0;
    virtual GlyphGpuTextureHandle create_glyph_texture(
        std::uint32_t width,
        std::uint32_t height) = 0;
    virtual GlyphGpuBufferHandle create_glyph_buffer(std::size_t size) = 0;
    virtual bool upload_glyph_texture(
        GlyphGpuTextureHandle texture,
        const GlyphTextureUpload& upload) = 0;
    virtual bool upload_glyph_buffer(
        GlyphGpuBufferHandle buffer,
        std::size_t offset,
        std::span<const std::byte> bytes) = 0;
    virtual void release_glyph_buffer(GlyphGpuBufferHandle buffer) noexcept = 0;
    virtual void release_glyph_texture(GlyphGpuTextureHandle texture) noexcept = 0;
    virtual void release_glyph_sampler(GlyphGpuSamplerHandle sampler) noexcept = 0;
    [[nodiscard]] virtual const char* glyph_gpu_error() const noexcept = 0;
};

struct GlyphGpuResourceCounters {
    std::uint64_t textures_created{};
    std::uint64_t texture_uploads{};
    std::uint64_t texture_uploaded_bytes{};
    std::uint64_t buffer_reallocations{};
    std::uint64_t buffer_uploads{};
    std::uint64_t buffer_uploaded_bytes{};
    std::uint64_t buffer_upload_coalesces{};
};

class GlyphGpuResources final {
public:
    explicit GlyphGpuResources(GlyphGpuApi& api);
    GlyphGpuResources(const GlyphGpuResources&) = delete;
    GlyphGpuResources& operator=(const GlyphGpuResources&) = delete;
    GlyphGpuResources(GlyphGpuResources&&) = delete;
    GlyphGpuResources& operator=(GlyphGpuResources&&) = delete;
    ~GlyphGpuResources();
    void abandon_device() noexcept { sampler_ = nullptr; textures_.clear(); instance_buffer_ = nullptr; instance_capacity_ = 0; metrics_.reset(); }

    void synchronize(
        graphics::GlyphAtlas& atlas,
        graphics::GlyphInstanceStore& instances, SceneDeviceMetrics metrics);
    void set_sparse_upload_coalescing_limit(std::size_t max_span_bytes) noexcept;

    [[nodiscard]] GlyphGpuSamplerHandle sampler() const noexcept;
    [[nodiscard]] GlyphGpuTextureHandle texture(std::uint32_t page) const;
    [[nodiscard]] std::size_t texture_count() const noexcept { return textures_.size(); }
    [[nodiscard]] GlyphGpuBufferHandle instance_buffer() const noexcept;
    [[nodiscard]] std::uint32_t instance_capacity() const noexcept;
    [[nodiscard]] const GlyphGpuResourceCounters& counters() const noexcept;

private:
    void ensure_textures(const graphics::GlyphAtlas& atlas);
    [[nodiscard]] bool ensure_instance_buffer(
        const graphics::GlyphInstanceStore& instances);
    void upload_atlas(graphics::GlyphAtlas& atlas);
    void upload_instance_ranges(graphics::GlyphInstanceStore& instances);

    GlyphGpuApi* api_;
    GlyphGpuSamplerHandle sampler_{nullptr};
    std::vector<GlyphGpuTextureHandle> textures_;
    GlyphGpuBufferHandle instance_buffer_{nullptr};
    std::uint32_t instance_capacity_{};
    std::vector<graphics::GlyphInstanceRange> dirty_scratch_;
    std::vector<GlyphGpuInstance> packed_;
    std::optional<SceneDeviceMetrics> metrics_;
    void convert_range(const graphics::GlyphInstanceStore& instances,
                       graphics::GlyphInstanceRange range, SceneDeviceMetrics metrics);
    std::size_t max_coalesced_upload_bytes_{};
    GlyphGpuResourceCounters counters_;
};

class SceneDrawApi {
public:
    virtual ~SceneDrawApi() = default;
    virtual void draw_quad(std::uint32_t first, std::uint32_t count) = 0;
    virtual void draw_glyph(
        std::uint32_t atlas_page,
        std::uint32_t first,
        std::uint32_t count) = 0;
    virtual void draw_rounded_effect(
        std::uint32_t first,
        std::uint32_t count) = 0;
};

void draw_ordered_scene(const graphics::OrderedScene& scene, SceneDrawApi& api);

} // namespace ryn::detail
