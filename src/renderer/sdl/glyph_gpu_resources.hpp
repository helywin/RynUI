#pragma once

#include "graphics/glyph_atlas.hpp"
#include "graphics/glyph_scene.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ryn::detail {

using GlyphGpuTextureHandle = void*;
using GlyphGpuSamplerHandle = void*;
using GlyphGpuBufferHandle = void*;

inline constexpr std::uint32_t glyph_texture_row_alignment = 256;
inline constexpr std::uint32_t glyph_texture_offset_alignment = 512;

struct GlyphTextureUpload {
    std::uint32_t page{};
    graphics::GlyphAtlasRect rectangle{};
    std::uint32_t transfer_offset{};
    std::uint32_t pixels_per_row{};
    std::uint32_t rows_per_layer{};
    std::span<const std::byte> bytes;
};

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
};

class GlyphGpuResources final {
public:
    explicit GlyphGpuResources(GlyphGpuApi& api);
    GlyphGpuResources(const GlyphGpuResources&) = delete;
    GlyphGpuResources& operator=(const GlyphGpuResources&) = delete;
    GlyphGpuResources(GlyphGpuResources&&) = delete;
    GlyphGpuResources& operator=(GlyphGpuResources&&) = delete;
    ~GlyphGpuResources();

    void synchronize(
        graphics::GlyphAtlas& atlas,
        graphics::GlyphInstanceStore& instances);

    [[nodiscard]] GlyphGpuSamplerHandle sampler() const noexcept;
    [[nodiscard]] GlyphGpuTextureHandle texture(std::uint32_t page) const;
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
