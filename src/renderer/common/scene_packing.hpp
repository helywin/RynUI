#pragma once

#include "graphics/glyph_scene.hpp"
#include "graphics/quad_primitive.hpp"
#include "renderer/common/scene_metrics.hpp"

namespace ryn::detail {

// CPU logical scene v3 and packed GPU ABI v2 are separate contracts.
inline constexpr std::uint32_t packed_scene_abi_version = 2;

struct alignas(16) QuadGpuInstance {
    std::array<float, 4> clip_rect{};
    std::array<float, 4> color{1, 1, 1, 1};
    float opacity{1};
    float corner_radius{};
    std::array<float, 2> translation{};
    friend bool operator==(const QuadGpuInstance&, const QuadGpuInstance&) = default;
};

static_assert(sizeof(QuadGpuInstance) == 48);
static_assert(offsetof(QuadGpuInstance, clip_rect) == 0);
static_assert(offsetof(QuadGpuInstance, color) == 16);
static_assert(offsetof(QuadGpuInstance, opacity) == 32);
static_assert(offsetof(QuadGpuInstance, corner_radius) == 36);
static_assert(offsetof(QuadGpuInstance, translation) == 40);

enum class QuadAttributeFormat { float1, float2, float4 };

struct QuadAttributeBinding {
    std::uint32_t location;
    QuadAttributeFormat format;
    std::uint32_t offset;
    friend constexpr bool operator==(QuadAttributeBinding, QuadAttributeBinding) = default;
};

inline constexpr std::array<QuadAttributeBinding, 5> quad_attribute_bindings{{
    {0, QuadAttributeFormat::float4, 0},
    {1, QuadAttributeFormat::float4, 16},
    {2, QuadAttributeFormat::float1, 32},
    {3, QuadAttributeFormat::float1, 36},
    {4, QuadAttributeFormat::float2, 40},
}};
inline constexpr std::uint32_t quad_vertex_count = 6;

struct alignas(16) GlyphGpuInstance {
    std::array<float, 4> position_size{};
    std::array<float, 4> uv_rect{};
    std::array<float, 4> clip_bounds{};
    std::array<float, 4> color{1, 1, 1, 1};
    std::array<float, 4> translation_opacity{0, 0, 1, 0};
    std::array<float, 4> rotation_basis{1, 0, 0, 1};
    friend bool operator==(const GlyphGpuInstance&, const GlyphGpuInstance&) = default;
};

static_assert(sizeof(GlyphGpuInstance) == 96);
static_assert(offsetof(GlyphGpuInstance, position_size) == 0);
static_assert(offsetof(GlyphGpuInstance, uv_rect) == 16);
static_assert(offsetof(GlyphGpuInstance, clip_bounds) == 32);
static_assert(offsetof(GlyphGpuInstance, color) == 48);
static_assert(offsetof(GlyphGpuInstance, translation_opacity) == 64);
static_assert(offsetof(GlyphGpuInstance, rotation_basis) == 80);

enum class GlyphAttributeFormat : std::uint8_t { float4 };

struct GlyphAttributeBinding {
    std::uint32_t location{};
    GlyphAttributeFormat format{GlyphAttributeFormat::float4};
    std::uint32_t offset{};
    friend constexpr bool operator==(GlyphAttributeBinding, GlyphAttributeBinding) = default;
};

inline constexpr std::array<GlyphAttributeBinding, 6> glyph_attribute_bindings{{
    {0, GlyphAttributeFormat::float4, 0},
    {1, GlyphAttributeFormat::float4, 16},
    {2, GlyphAttributeFormat::float4, 32},
    {3, GlyphAttributeFormat::float4, 48},
    {4, GlyphAttributeFormat::float4, 64},
    {5, GlyphAttributeFormat::float4, 80},
}};
inline constexpr std::uint32_t glyph_vertex_count = 6;

[[nodiscard]] QuadGpuInstance pack_quad_instance(const graphics::QuadInstance& instance, SceneDeviceMetrics metrics);
[[nodiscard]] GlyphGpuInstance pack_glyph_instance(const graphics::GlyphInstance& instance, SceneDeviceMetrics metrics);
[[nodiscard]] runtime::Point packed_glyph_vertex(const GlyphGpuInstance& instance, runtime::Point corner);
} // namespace ryn::detail
