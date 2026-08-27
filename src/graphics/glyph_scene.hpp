#pragma once

#include "graphics/glyph_atlas.hpp"
#include "runtime/geometry.hpp"
#include "text/text_engine.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ryn::graphics {

struct alignas(16) GlyphInstance {
    std::array<float, 4> position_size{};
    std::array<float, 4> uv_rect{};
    std::array<float, 4> clip_bounds{};
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 4> translation_opacity{0.0F, 0.0F, 1.0F, 0.0F};

    friend bool operator==(const GlyphInstance&, const GlyphInstance&) = default;
};

static_assert(sizeof(GlyphInstance) == 80);
static_assert(offsetof(GlyphInstance, position_size) == 0);
static_assert(offsetof(GlyphInstance, uv_rect) == 16);
static_assert(offsetof(GlyphInstance, clip_bounds) == 32);
static_assert(offsetof(GlyphInstance, color) == 48);
static_assert(offsetof(GlyphInstance, translation_opacity) == 64);

enum class GlyphAttributeFormat : std::uint8_t {
    float4,
};

struct GlyphAttributeBinding {
    std::uint32_t location{};
    GlyphAttributeFormat format{GlyphAttributeFormat::float4};
    std::uint32_t offset{};

    friend bool operator==(GlyphAttributeBinding, GlyphAttributeBinding) = default;
};

inline constexpr std::array<GlyphAttributeBinding, 5> glyph_attribute_bindings{{
    {0, GlyphAttributeFormat::float4, 0},
    {1, GlyphAttributeFormat::float4, 16},
    {2, GlyphAttributeFormat::float4, 32},
    {3, GlyphAttributeFormat::float4, 48},
    {4, GlyphAttributeFormat::float4, 64},
}};

inline constexpr std::uint32_t glyph_vertex_count = 6;

struct GlyphInstanceRange {
    std::uint32_t first{};
    std::uint32_t count{};

    friend bool operator==(GlyphInstanceRange, GlyphInstanceRange) = default;
};

struct GlyphDrawRange {
    std::uint32_t atlas_page{};
    GlyphInstanceRange instances{};

    friend bool operator==(GlyphDrawRange, GlyphDrawRange) = default;
};

class GlyphInstanceStore final {
public:
    [[nodiscard]] GlyphInstanceRange append(std::span<const GlyphInstance> instances);
    [[nodiscard]] GlyphInstanceRange replace(
        GlyphInstanceRange range,
        std::span<const GlyphInstance> instances);
    [[nodiscard]] const GlyphInstance& at(std::uint32_t index) const;
    [[nodiscard]] GlyphInstance& at(std::uint32_t index);
    [[nodiscard]] std::span<const GlyphInstance> instances() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::span<const std::byte> bytes(GlyphInstanceRange range) const;

    [[nodiscard]] std::size_t update_material(
        GlyphInstanceRange range,
        std::array<float, 4> color,
        float opacity);
    [[nodiscard]] std::size_t update_geometry(
        GlyphInstanceRange range,
        std::array<float, 4> clip_bounds,
        std::array<float, 2> translation);

    [[nodiscard]] std::span<const GlyphInstanceRange> material_dirty_ranges() const noexcept;
    [[nodiscard]] std::span<const GlyphInstanceRange> geometry_dirty_ranges() const noexcept;
    void clear_dirty_ranges() noexcept;

private:
    static void mark_dirty(
        std::vector<GlyphInstanceRange>& ranges,
        GlyphInstanceRange range);
    void require_range(GlyphInstanceRange range) const;

    std::vector<GlyphInstance> instances_;
    std::vector<GlyphInstanceRange> material_dirty_ranges_;
    std::vector<GlyphInstanceRange> geometry_dirty_ranges_;
};

struct GlyphPlacement {
    runtime::Point origin_pixels{};
    runtime::Size viewport_pixels{};
    runtime::Rect clip_pixels{};
    runtime::Point translation_pixels{};
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    float opacity{1.0F};

    friend bool operator==(const GlyphPlacement&, const GlyphPlacement&) = default;
};

struct GlyphPrimitive {
    GlyphInstanceRange instances{};
    std::vector<GlyphDrawRange> draw_ranges;
};

struct GlyphSceneResult {
    GlyphPrimitive primitive;
    GlyphAtlasError error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return !error;
    }
};

class GlyphScene final {
public:
    [[nodiscard]] GlyphSceneResult append_text(
        font::FontRuntime& fonts,
        GlyphAtlas& atlas,
        const text::ShapedText& shaped,
        const text::TextMeasurement& measurement,
        GlyphPlacement placement);
    [[nodiscard]] GlyphSceneResult replace_text(
        GlyphInstanceRange range,
        font::FontRuntime& fonts,
        GlyphAtlas& atlas,
        const text::ShapedText& shaped,
        const text::TextMeasurement& measurement,
        GlyphPlacement placement);
    [[nodiscard]] std::size_t update_geometry(
        GlyphInstanceRange range,
        GlyphPlacement placement);

    [[nodiscard]] GlyphInstanceStore& instances() noexcept;
    [[nodiscard]] const GlyphInstanceStore& instances() const noexcept;

private:
    GlyphInstanceStore instances_;
};

enum class SceneDrawKind : std::uint8_t {
    quad,
    glyph,
    rounded_effect,
};

struct SceneDrawCommand {
    SceneDrawKind kind{SceneDrawKind::quad};
    std::uint32_t first_instance{};
    std::uint32_t instance_count{};
    std::uint32_t atlas_page{invalid_glyph_atlas_page};

    friend bool operator==(SceneDrawCommand, SceneDrawCommand) = default;
};

class OrderedScene final {
public:
    void reserve(std::size_t command_capacity);
    void append_quad(std::uint32_t first_instance, std::uint32_t instance_count);
    void append_glyph(GlyphDrawRange range);
    void append_glyph(const GlyphPrimitive& primitive);
    void append_command(SceneDrawCommand command);

    [[nodiscard]] std::span<const SceneDrawCommand> commands() const noexcept;
    void clear() noexcept;

private:
    void append(SceneDrawCommand command);

    std::vector<SceneDrawCommand> commands_;
};

} // namespace ryn::graphics
