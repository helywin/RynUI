#pragma once

#include "graphics/dirty_ranges.hpp"
#include "runtime/node_store.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ryn::graphics {

inline constexpr std::uint32_t logical_scene_version = 3;

// Logical x/y/width/height, radius and translation. Not a shader byte layout.
struct QuadInstance {
    std::array<float, 4> bounds{};
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    float opacity{1.0F};
    float corner_radius{0.0F};
    std::array<float, 2> translation{};

    friend constexpr bool operator==(const QuadInstance&, const QuadInstance&) = default;
};

struct QuadPrimitive {
    runtime::NodeId node;
    std::uint32_t instance_index{0};
};

struct QuadInstanceRange final {
    std::uint32_t first{0};
    std::uint32_t count{0};

    friend constexpr bool operator==(QuadInstanceRange, QuadInstanceRange) = default;
};

struct QuadMaterial final {
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    float opacity{1.0F};

    friend constexpr bool operator==(const QuadMaterial&, const QuadMaterial&) = default;
};

struct QuadGeometry final {
    std::array<float, 4> bounds{};
    float corner_radius{0.0F};
    std::array<float, 2> translation{};

    friend constexpr bool operator==(const QuadGeometry&, const QuadGeometry&) = default;
};

class QuadInstanceStore final {
public:
    void reserve(std::size_t instance_capacity, std::size_t dirty_range_capacity = 0);
    [[nodiscard]] QuadPrimitive add(runtime::NodeId node, QuadInstance instance);
    [[nodiscard]] QuadInstanceRange append(std::span<const QuadInstance> instances);
    [[nodiscard]] QuadInstanceRange replace(QuadInstanceRange range, std::span<const QuadInstance> instances);
    [[nodiscard]] const QuadInstance& at(std::uint32_t index) const;
    [[nodiscard]] QuadInstance& at(std::uint32_t index);
    [[nodiscard]] std::span<const QuadInstance> instances() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;
    [[nodiscard]] std::span<const std::byte> bytes(std::uint32_t first, std::uint32_t count) const;
    [[nodiscard]] std::span<const std::byte> bytes(QuadInstanceRange range) const;
    [[nodiscard]] std::size_t update_material(QuadInstanceRange range, std::span<const QuadMaterial> materials);
    [[nodiscard]] std::size_t update_geometry(QuadInstanceRange range, std::span<const QuadGeometry> geometry);
    [[nodiscard]] std::span<const QuadInstanceRange> material_dirty_ranges() const noexcept;
    [[nodiscard]] std::span<const QuadInstanceRange> geometry_dirty_ranges() const noexcept;
    void clear_dirty_ranges() noexcept;
    void mark_all_dirty();

private:
    void require_range(QuadInstanceRange range) const;

    std::vector<QuadInstance> instances_;
    DirtyRangeAccumulator<QuadInstanceRange> material_dirty_ranges_;
    DirtyRangeAccumulator<QuadInstanceRange> geometry_dirty_ranges_;
};

} // namespace ryn::graphics
