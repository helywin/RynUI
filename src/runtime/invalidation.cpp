#include "runtime/invalidation.hpp"

#include "runtime/frame_scheduler.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::runtime {

DirtyQueues::DirtyQueues(NodeStore& nodes, FrameRequestState* frames) noexcept
    : nodes_(&nodes), frames_(frames) {}

void DirtyQueues::invalidate(NodeId id, DirtyFlags flags) {
    static_cast<void>(nodes_->require(id));
    if (flags != DirtyFlags::None && frames_ != nullptr) {
        frames_->request_frame();
    }
    if (has_any(flags, DirtyFlags::Measure | DirtyFlags::Layout)) {
        enqueue_unique(layout_roots_, layout_root_for(id));
    }
    if (has_any(flags, DirtyFlags::Material)) {
        enqueue_unique(material_nodes_, id);
    }
    if (has_any(flags, DirtyFlags::Transform)) {
        enqueue_unique(transform_nodes_, id);
    }
    if (has_any(flags, DirtyFlags::Geometry)) {
        enqueue_unique(geometry_nodes_, id);
    }
}

void DirtyQueues::clear() noexcept {
    layout_roots_.clear();
    material_nodes_.clear();
    transform_nodes_.clear();
    geometry_nodes_.clear();
}

const std::vector<NodeId>& DirtyQueues::layout_roots() const noexcept {
    return layout_roots_;
}

const std::vector<NodeId>& DirtyQueues::material_nodes() const noexcept {
    return material_nodes_;
}

const std::vector<NodeId>& DirtyQueues::transform_nodes() const noexcept {
    return transform_nodes_;
}

const std::vector<NodeId>& DirtyQueues::geometry_nodes() const noexcept {
    return geometry_nodes_;
}

NodeId DirtyQueues::layout_root_for(NodeId id) const {
    NodeId root = id;
    const Node* node = &nodes_->require(root);
    while (node->parent.has_value()) {
        root = *node->parent;
        node = &nodes_->require(root);
    }
    return root;
}

void DirtyQueues::enqueue_unique(std::vector<NodeId>& queue, NodeId id) {
    if (std::find(queue.begin(), queue.end(), id) == queue.end()) {
        queue.push_back(id);
    }
}

NodePropertyWriter::NodePropertyWriter(NodeStore& nodes, DirtyQueues& dirty) noexcept
    : nodes_(&nodes), dirty_(&dirty) {}

bool NodePropertyWriter::set_color(NodeId id, Color color) {
    auto& node = nodes_->require(id);
    if (node.color == color) {
        return false;
    }
    node.color = color;
    dirty_->invalidate(id, dirty_flags_for(NodeProperty::color));
    return true;
}

bool NodePropertyWriter::set_opacity(NodeId id, float opacity) {
    if (std::isnan(opacity) || opacity < 0.0F || opacity > 1.0F) {
        throw std::invalid_argument("Node opacity must be between zero and one");
    }
    auto& node = nodes_->require(id);
    if (node.opacity == opacity) {
        return false;
    }
    node.opacity = opacity;
    dirty_->invalidate(id, dirty_flags_for(NodeProperty::opacity));
    return true;
}

bool NodePropertyWriter::set_translation(NodeId id, Point translation) {
    if (std::isnan(translation.x) || std::isnan(translation.y)) {
        throw std::invalid_argument("Node translation cannot contain NaN");
    }
    auto& node = nodes_->require(id);
    if (node.translation == translation) {
        return false;
    }
    node.translation = translation;
    dirty_->invalidate(id, dirty_flags_for(NodeProperty::translation));
    return true;
}

bool NodePropertyWriter::set_size(NodeId id, Size size) {
    if (size.width < 0.0F || size.height < 0.0F
            || std::isnan(size.width) || std::isnan(size.height)) {
        throw std::invalid_argument("Node size must be non-negative");
    }
    auto& node = nodes_->require(id);
    if (node.requested_size == size) {
        return false;
    }
    node.requested_size = size;
    dirty_->invalidate(id, dirty_flags_for(NodeProperty::size));
    return true;
}

} // namespace ryn::runtime
