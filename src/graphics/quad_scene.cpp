#include "graphics/quad_scene.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::graphics {
namespace {

void validate_viewport(runtime::Size viewport) {
    if (viewport.width <= 0.0F || viewport.height <= 0.0F
            || !std::isfinite(viewport.width) || !std::isfinite(viewport.height)) {
        throw std::invalid_argument("Quad viewport must be finite and positive");
    }
}

} // namespace

QuadScene::QuadScene(runtime::NodeStore& nodes) noexcept : nodes_(&nodes) {}

QuadPrimitive QuadScene::add_quad(
    runtime::NodeId node,
    runtime::Size viewport,
    float corner_radius_pixels) {
    validate_viewport(viewport);
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

    const auto primitive = instances_.add(
        node,
        make_instance(node, viewport, corner_radius_pixels));
    slot = PrimitiveSlot{node.generation, primitive.instance_index, corner_radius_pixels};
    ++counters_.primitive_rebuilds;
    return primitive;
}

std::size_t QuadScene::sync_dirty(
    const runtime::DirtyQueues& dirty,
    QuadGpuBuffer& gpu_buffer,
    runtime::Size viewport) {
    validate_viewport(viewport);
    std::vector<runtime::NodeId> targets;
    targets.reserve(
        dirty.material_nodes().size()
        + dirty.transform_nodes().size()
        + dirty.geometry_nodes().size());
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
        auto next = make_instance(node, viewport, slot.corner_radius_pixels);
        if (instances_.at(index) == next) {
            continue;
        }
        instances_.at(index) = next;
        gpu_buffer.upload_range(instances_, index, 1);
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

QuadInstance QuadScene::make_instance(
    runtime::NodeId node,
    runtime::Size viewport,
    float corner_radius_pixels) const {
    const auto& source = nodes_->require(node);
    const float minimum_extent = std::min(source.bounds.width, source.bounds.height);
    const float normalized_radius = minimum_extent > 0.0F
        ? std::clamp(corner_radius_pixels / minimum_extent, 0.0F, 0.5F)
        : 0.0F;

    return {
        {
            -1.0F + 2.0F * source.bounds.x / viewport.width,
            1.0F - 2.0F * source.bounds.y / viewport.height,
            2.0F * source.bounds.width / viewport.width,
            -2.0F * source.bounds.height / viewport.height,
        },
        {source.color.red, source.color.green, source.color.blue, source.color.alpha},
        source.opacity,
        normalized_radius,
        {
            2.0F * source.translation.x / viewport.width,
            -2.0F * source.translation.y / viewport.height,
        },
    };
}

void QuadScene::append_unique(
    std::vector<runtime::NodeId>& nodes,
    runtime::NodeId node) {
    if (std::find(nodes.begin(), nodes.end(), node) == nodes.end()) {
        nodes.push_back(node);
    }
}

} // namespace ryn::graphics
