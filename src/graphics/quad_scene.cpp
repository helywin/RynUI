#include "graphics/quad_scene.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::graphics {

QuadScene::QuadScene(runtime::NodeStore& nodes) noexcept : nodes_(&nodes) {}

QuadPrimitive QuadScene::add_quad(runtime::NodeId node, float corner_radius_pixels) {
    if (corner_radius_pixels < 0.0F || !std::isfinite(corner_radius_pixels)) {
        throw std::invalid_argument("Quad corner radius must be finite and non-negative");
    }
    static_cast<void>(nodes_->require(node));
    if (primitives_.size() <= node.index) {
        primitives_.resize(static_cast<std::size_t>(node.index) + 1);
    }
    auto& slot = primitives_[node.index];
    if (slot.generation == node.generation && slot.instance_index.has_value()) {
        throw std::logic_error("Node already has a QuadPrimitive");
    }

    const auto primitive = instances_.add(node, make_instance(node, corner_radius_pixels));
    slot = PrimitiveSlot{node.generation, primitive.instance_index, corner_radius_pixels};
    ++counters_.primitive_rebuilds;
    return primitive;
}

std::size_t QuadScene::sync_dirty(const runtime::DirtyQueues& dirty) {
    std::vector<runtime::NodeId> targets;
    targets.reserve(dirty.material_nodes().size() + dirty.transform_nodes().size() + dirty.geometry_nodes().size());
    for (const auto node : dirty.material_nodes()) {
        append_unique(targets, node);
    }
    for (const auto node : dirty.transform_nodes()) {
        append_unique(targets, node);
    }
    for (const auto node : dirty.geometry_nodes()) {
        append_unique(targets, node);
    }

    std::size_t updated = 0;
    for (const auto node : targets) {
        auto& slot = require_slot(node);
        const auto index = *slot.instance_index;
        auto next = make_instance(node, slot.corner_radius_pixels);
        if (instances_.at(index) == next) {
            continue;
        }
        const QuadMaterial material{next.color, next.opacity};
        const QuadGeometry geometry{next.bounds, next.corner_radius, next.translation};
        static_cast<void>(instances_.update_material({index, 1}, {&material, 1}));
        static_cast<void>(instances_.update_geometry({index, 1}, {&geometry, 1}));
        ++counters_.instance_updates;
        ++updated;
    }
    return updated;
}

QuadInstanceStore& QuadScene::instances() noexcept {
    return instances_;
}

const QuadInstanceStore& QuadScene::instances() const noexcept {
    return instances_;
}

const QuadSceneCounters& QuadScene::counters() const noexcept {
    return counters_;
}

QuadScene::PrimitiveSlot& QuadScene::require_slot(runtime::NodeId node) {
    static_cast<void>(nodes_->require(node));
    if (node.index >= primitives_.size()) {
        throw std::logic_error("Node has no QuadPrimitive");
    }
    auto& slot = primitives_[node.index];
    if (slot.generation != node.generation || !slot.instance_index.has_value()) {
        throw std::logic_error("Node has no QuadPrimitive for its current generation");
    }
    return slot;
}

QuadInstance QuadScene::make_instance(runtime::NodeId node, float corner_radius_pixels) const {
    const auto& source = nodes_->require(node);
    return {
        {
            source.bounds.x,
            source.bounds.y,
            source.bounds.width,
            source.bounds.height,
        },
        {source.color.red, source.color.green, source.color.blue, source.color.alpha},
        source.opacity,
        corner_radius_pixels,
        {
            source.translation.x,
            source.translation.y,
        },
    };
}

void QuadScene::append_unique(std::vector<runtime::NodeId>& nodes, runtime::NodeId node) {
    if (std::find(nodes.begin(), nodes.end(), node) == nodes.end()) {
        nodes.push_back(node);
    }
}

} // namespace ryn::graphics
