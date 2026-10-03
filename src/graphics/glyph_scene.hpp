#pragma once

#include "graphics/dirty_ranges.hpp"
#include "graphics/glyph_atlas.hpp"
#include "runtime/geometry.hpp"
#include "text/text_engine.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ryn::graphics {

// Logical x/y/width/height and left/top/right/bottom clip; UV stays normalized.
// The first two translation_opacity entries are logical lengths, not NDC.
struct GlyphTransform final {
    runtime::Point pivot;
    float angle_degrees{};

    friend bool operator==(const GlyphTransform&, const GlyphTransform&) = default;
};

struct GlyphInstance {
    std::array<float, 4> position_size{};
    std::array<float, 4> uv_rect{};
    std::array<float, 4> clip_bounds{};
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 4> translation_opacity{0.0F, 0.0F, 1.0F, 0.0F};
    GlyphTransform transform;

    friend bool operator==(const GlyphInstance&, const GlyphInstance&) = default;
};

[[nodiscard]] runtime::Point glyph_vertex(const GlyphInstance& instance, runtime::Point corner);

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
    [[nodiscard]] GlyphInstanceRange replace(GlyphInstanceRange range, std::span<const GlyphInstance> instances);
    [[nodiscard]] const GlyphInstance& at(std::uint32_t index) const;
    [[nodiscard]] GlyphInstance& at(std::uint32_t index);
    [[nodiscard]] std::span<const GlyphInstance> instances() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::span<const std::byte> bytes(GlyphInstanceRange range) const;

    [[nodiscard]] std::size_t update_material(GlyphInstanceRange range, std::array<float, 4> color, float opacity);
    [[nodiscard]] std::size_t update_geometry(GlyphInstanceRange range, std::array<float, 4> clip_bounds,
                                              std::array<float, 2> translation);
    [[nodiscard]] std::size_t update_transform(GlyphInstanceRange range, GlyphTransform transform);

    [[nodiscard]] std::span<const GlyphInstanceRange> material_dirty_ranges() const noexcept;
    [[nodiscard]] std::span<const GlyphInstanceRange> geometry_dirty_ranges() const noexcept;
    void clear_dirty_ranges() noexcept;
    void mark_all_dirty();

private:
    void require_range(GlyphInstanceRange range) const;

    std::vector<GlyphInstance> instances_;
    DirtyRangeAccumulator<GlyphInstanceRange> material_dirty_ranges_;
    DirtyRangeAccumulator<GlyphInstanceRange> geometry_dirty_ranges_;
};

struct GlyphPlacement {
    // Legacy *_pixels names denote logical units; density belongs to font rasterization.
    runtime::Point origin_pixels{};
    runtime::Size viewport_pixels{};
    runtime::Rect clip_pixels{};
    runtime::Point translation_pixels{};
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    float opacity{1.0F};

    friend bool operator==(const GlyphPlacement&, const GlyphPlacement&) = default;
};

struct GlyphCoverage {
    std::size_t byte_begin{};
    std::size_t byte_end{};
    std::size_t line{};
    float x{};
    float width{};
};

struct GlyphPrimitive {
    GlyphInstanceRange instances{};
    std::vector<GlyphDrawRange> draw_ranges;
    // CPU metadata for each measured line; empty glyphs do not occupy instances.
    std::vector<GlyphInstanceRange> line_ranges;
    // CPU-only logical ownership, aligned with visible instances.
    std::vector<GlyphCoverage> coverage;
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
    [[nodiscard]] GlyphSceneResult append_text(font::FontRuntime& fonts, GlyphAtlas& atlas,
                                               const text::ShapedText& shaped, const text::TextMeasurement& measurement,
                                               GlyphPlacement placement);
    [[nodiscard]] GlyphSceneResult replace_text(GlyphInstanceRange range, font::FontRuntime& fonts, GlyphAtlas& atlas,
                                                const text::ShapedText& shaped,
                                                const text::TextMeasurement& measurement, GlyphPlacement placement);
    [[nodiscard]] std::size_t update_geometry(GlyphInstanceRange range, GlyphPlacement placement);

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
