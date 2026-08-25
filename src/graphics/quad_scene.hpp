#pragma once

#include "graphics/quad_primitive.hpp"
#include "runtime/invalidation.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ryn::graphics {

struct QuadSceneCounters {
    std::uint64_t primitive_rebuilds{0};
    std::uint64_t instance_updates{0};
};

class QuadScene final {
public:
    explicit QuadScene(runtime::NodeStore& nodes) noexcept;

    [[nodiscard]] QuadPrimitive add_quad(
        runtime::NodeId node,
        runtime::Size viewport,
        float corner_radius_pixels = 0.0F);
    [[nodiscard]] std::size_t sync_dirty(
        const runtime::DirtyQueues& dirty,
        QuadGpuBuffer& gpu_buffer,
        runtime::Size viewport);

    [[nodiscard]] QuadInstanceStore& instances() noexcept;
    [[nodiscard]] const QuadInstanceStore& instances() const noexcept;
    [[nodiscard]] const QuadSceneCounters& counters() const noexcept;

private:
    struct PrimitiveSlot {
        std::uint32_t generation{0};
        std::optional<std::uint32_t> instance_index;
        float corner_radius_pixels{0.0F};
    };

    [[nodiscard]] PrimitiveSlot& require_slot(runtime::NodeId node);
    [[nodiscard]] QuadInstance make_instance(
        runtime::NodeId node,
        runtime::Size viewport,
        float corner_radius_pixels) const;
    static void append_unique(std::vector<runtime::NodeId>& nodes, runtime::NodeId node);

    runtime::NodeStore* nodes_;
    QuadInstanceStore instances_;
    std::vector<PrimitiveSlot> primitives_;
    QuadSceneCounters counters_;
};

} // namespace ryn::graphics
