#pragma once

#include "runtime/geometry.hpp"
#include "runtime/node_store.hpp"
#include "theme/theme_runtime_types.hpp"

#include <cstdint>
#include <vector>

namespace ryn::runtime {

class FrameRequestState;

enum class DirtyFlags : std::uint32_t {
    None = 0,
    Structure = 1U << 0U,
    Measure = 1U << 1U,
    Layout = 1U << 2U,
    Geometry = 1U << 3U,
    Material = 1U << 4U,
    Transform = 1U << 5U,
    HitTest = 1U << 6U,
    Placement = 1U << 7U,
    Text = 1U << 8U,
    Animation = 1U << 9U,
};

constexpr DirtyFlags operator|(DirtyFlags left, DirtyFlags right) noexcept {
    return static_cast<DirtyFlags>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr DirtyFlags operator&(DirtyFlags left, DirtyFlags right) noexcept {
    return static_cast<DirtyFlags>(
        static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

constexpr bool has_any(DirtyFlags value, DirtyFlags mask) noexcept {
    return (value & mask) != DirtyFlags::None;
}

enum class NodeProperty {
    color,
    opacity,
    translation,
    size,
    padding,
};

[[nodiscard]] constexpr DirtyFlags dirty_flags_for(NodeProperty property) noexcept {
    switch (property) {
    case NodeProperty::color:
    case NodeProperty::opacity:
        return DirtyFlags::Material;
    case NodeProperty::translation:
        return DirtyFlags::Transform | DirtyFlags::HitTest;
    case NodeProperty::size:
    case NodeProperty::padding:
        return DirtyFlags::Measure
            | DirtyFlags::Layout
            | DirtyFlags::Geometry
            | DirtyFlags::HitTest;
    }
    return DirtyFlags::None;
}

[[nodiscard]] constexpr DirtyFlags dirty_flags_for_theme(
    theme_runtime::DirtyPhase phase) noexcept {
    DirtyFlags flags = DirtyFlags::None;
    if (theme_runtime::has_any(
            phase,
            theme_runtime::DirtyPhase::paint_material)) {
        flags = flags | DirtyFlags::Material;
    }
    if (theme_runtime::has_any(phase, theme_runtime::DirtyPhase::geometry)) {
        flags = flags | DirtyFlags::Geometry;
    }
    if (theme_runtime::has_any(phase, theme_runtime::DirtyPhase::text)) {
        flags = flags | DirtyFlags::Text;
    }
    if (theme_runtime::has_any(
            phase,
            theme_runtime::DirtyPhase::measure_layout)) {
        flags = flags | DirtyFlags::Measure | DirtyFlags::Layout;
    }
    if (theme_runtime::has_any(phase, theme_runtime::DirtyPhase::hit_test)) {
        flags = flags | DirtyFlags::HitTest;
    }
    if (theme_runtime::has_any(phase, theme_runtime::DirtyPhase::animation)) {
        flags = flags | DirtyFlags::Animation;
    }
    return flags;
}

class DirtyQueues final {
public:
    explicit DirtyQueues(NodeStore& nodes, FrameRequestState* frames = nullptr) noexcept;

    void invalidate(NodeId id, DirtyFlags flags);
    void invalidate_subtree(NodeId root, DirtyFlags flags);
    void clear() noexcept;

    [[nodiscard]] const std::vector<NodeId>& layout_roots() const noexcept;
    [[nodiscard]] const std::vector<NodeId>& placement_roots() const noexcept;
    [[nodiscard]] const std::vector<NodeId>& material_nodes() const noexcept;
    [[nodiscard]] const std::vector<NodeId>& transform_nodes() const noexcept;
    [[nodiscard]] const std::vector<NodeId>& geometry_nodes() const noexcept;
    [[nodiscard]] const std::vector<NodeId>& hit_test_nodes() const noexcept;
    [[nodiscard]] const std::vector<NodeId>& text_nodes() const noexcept;
    [[nodiscard]] const std::vector<NodeId>& animation_nodes() const noexcept;

private:
    [[nodiscard]] NodeId layout_root_for(NodeId id) const;
    static void enqueue_unique(std::vector<NodeId>& queue, NodeId id);

    NodeStore* nodes_;
    FrameRequestState* frames_;
    std::vector<NodeId> layout_roots_;
    std::vector<NodeId> placement_roots_;
    std::vector<NodeId> material_nodes_;
    std::vector<NodeId> transform_nodes_;
    std::vector<NodeId> geometry_nodes_;
    std::vector<NodeId> hit_test_nodes_;
    std::vector<NodeId> text_nodes_;
    std::vector<NodeId> animation_nodes_;
};

class NodePropertyWriter final {
public:
    NodePropertyWriter(NodeStore& nodes, DirtyQueues& dirty) noexcept;

    bool set_color(NodeId id, Color color);
    bool set_opacity(NodeId id, float opacity);
    bool set_translation(NodeId id, Point translation);
    bool set_size(NodeId id, Size size);

private:
    NodeStore* nodes_;
    DirtyQueues* dirty_;
};

} // namespace ryn::runtime
