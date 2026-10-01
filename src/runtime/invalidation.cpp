#include "runtime/invalidation.hpp"

#include "runtime/frame_scheduler.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::runtime {

DirtyQueues::DirtyQueues(NodeStore& nodes, FrameRequestState* frames) noexcept
    : nodes_(&nodes), frames_(frames) {}

void DirtyQueues::invalidate(NodeId id, DirtyFlags flags) {
    invalidate_impl(id, flags, true);
}
void DirtyQueues::invalidate_in_frame(NodeId id, DirtyFlags flags) {
    invalidate_impl(id, flags, false);
}
void DirtyQueues::invalidate_impl(NodeId id, DirtyFlags flags, bool request_frame) {
    static_cast<void>(nodes_->require(id));
    if (request_frame && flags != DirtyFlags::None && frames_ != nullptr) {
        frames_->request_invalidation_frame();
    }
    if (has_any(flags, DirtyFlags::Measure | DirtyFlags::Layout)) {
        enqueue_unique(layout_roots_, layout_root_for(id), Domain::layout);
    }
    if (has_any(flags, DirtyFlags::Placement)) {
        enqueue_unique(placement_roots_, layout_root_for(id), Domain::placement);
    }
    if (has_any(flags, DirtyFlags::Material)) {
        enqueue_unique(material_nodes_, id, Domain::material);
    }
    if (has_any(flags, DirtyFlags::Transform)) {
        enqueue_unique(transform_nodes_, id, Domain::transform);
    }
    if (has_any(flags, DirtyFlags::Geometry)) {
        enqueue_unique(geometry_nodes_, id, Domain::geometry);
    }
    if (has_any(flags, DirtyFlags::Text)) {
        enqueue_unique(text_nodes_, id, Domain::text);
    }
    if (has_any(flags, DirtyFlags::Animation)) {
        enqueue_unique(animation_nodes_, id, Domain::animation);
    }
    if (has_any(
            flags,
            DirtyFlags::HitTest
                | DirtyFlags::Structure
                | DirtyFlags::Measure
                | DirtyFlags::Layout
                | DirtyFlags::Placement)) {
        const auto hit_test_root = has_any(
            flags,
            DirtyFlags::Structure
                | DirtyFlags::Measure
                | DirtyFlags::Layout
                | DirtyFlags::Placement)
            ? layout_root_for(id)
            : id;
        enqueue_unique(hit_test_nodes_, hit_test_root, Domain::hit_test);
    }
}

void DirtyQueues::invalidate_subtree(NodeId root, DirtyFlags flags) {
    static_cast<void>(nodes_->require(root));
    if (flags != DirtyFlags::None && frames_ != nullptr) {
        frames_->request_invalidation_frame();
    }
    if (has_any(flags, DirtyFlags::Measure | DirtyFlags::Layout)) {
        enqueue_unique(layout_roots_, root, Domain::layout);
    }
    if (has_any(flags, DirtyFlags::Placement)) {
        enqueue_unique(placement_roots_, root, Domain::placement);
    }
    if (has_any(flags, DirtyFlags::Material)) {
        enqueue_unique(material_nodes_, root, Domain::material);
    }
    if (has_any(flags, DirtyFlags::Transform)) {
        enqueue_unique(transform_nodes_, root, Domain::transform);
    }
    if (has_any(flags, DirtyFlags::Geometry)) {
        enqueue_unique(geometry_nodes_, root, Domain::geometry);
    }
    if (has_any(flags, DirtyFlags::Text)) {
        enqueue_unique(text_nodes_, root, Domain::text);
    }
    if (has_any(flags, DirtyFlags::Animation)) {
        enqueue_unique(animation_nodes_, root, Domain::animation);
    }
    if (has_any(flags, DirtyFlags::HitTest | DirtyFlags::Structure | DirtyFlags::Measure |
                           DirtyFlags::Layout | DirtyFlags::Placement)) {
        enqueue_unique(hit_test_nodes_, root, Domain::hit_test);
    }
}

void DirtyQueues::clear() noexcept {
    layout_roots_.clear();
    placement_roots_.clear();
    material_nodes_.clear();
    transform_nodes_.clear();
    geometry_nodes_.clear();
    hit_test_nodes_.clear();
    text_nodes_.clear();
    animation_nodes_.clear();
    if (epoch_ == std::numeric_limits<std::uint64_t>::max()) {
        for (auto& domain_stamps : stamps_) {
            domain_stamps.clear();
        }
        epoch_ = 1;
    } else {
        ++epoch_;
    }
    checked_topology_revisions_.fill(nodes_->topology_revision());
}

const std::vector<NodeId>& DirtyQueues::layout_roots() const noexcept {
    return live_queue(layout_roots_, Domain::layout);
}

const std::vector<NodeId>& DirtyQueues::placement_roots() const noexcept {
    return live_queue(placement_roots_, Domain::placement);
}

const std::vector<NodeId>& DirtyQueues::material_nodes() const noexcept {
    return live_queue(material_nodes_, Domain::material);
}

const std::vector<NodeId>& DirtyQueues::transform_nodes() const noexcept {
    return live_queue(transform_nodes_, Domain::transform);
}

const std::vector<NodeId>& DirtyQueues::geometry_nodes() const noexcept {
    return live_queue(geometry_nodes_, Domain::geometry);
}

const std::vector<NodeId>& DirtyQueues::hit_test_nodes() const noexcept {
    return live_queue(hit_test_nodes_, Domain::hit_test);
}

const std::vector<NodeId>& DirtyQueues::text_nodes() const noexcept {
    return live_queue(text_nodes_, Domain::text);
}

const std::vector<NodeId>& DirtyQueues::animation_nodes() const noexcept {
    return live_queue(animation_nodes_, Domain::animation);
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

void DirtyQueues::enqueue_unique(
    std::vector<NodeId>& queue, NodeId id, Domain domain) {
    const auto domain_index = static_cast<std::size_t>(domain);
    auto& stamps = stamps_[domain_index];
    constexpr std::size_t small_queue_limit = 256;
    if (stamps.empty() && queue.size() < small_queue_limit) {
        // The first few entries are cheaper to scan than initializing a slot
        // table for a large, mostly clean tree. This scan has a fixed bound.
        if (std::find(queue.begin(), queue.end(), id) != queue.end()) {
            return;
        }
        if (queue.empty()) {
            checked_topology_revisions_[domain_index] = nodes_->topology_revision();
        }
        queue.push_back(id);
        return;
    }
    if (stamps.empty()) {
        stamps.resize(std::max(
            static_cast<std::size_t>(id.index) + 1, nodes_->slot_capacity()));
        for (const NodeId queued : queue) {
            if (nodes_->find(queued) != nullptr) {
                stamps[queued.index] = {queued.generation, epoch_};
            }
        }
    }
    if (id.index >= stamps.size()) {
        stamps.resize(std::max({
            static_cast<std::size_t>(id.index) + 1,
            nodes_->slot_capacity(),
            stamps.size() * 2,
        }));
    }
    auto& stamp = stamps[id.index];
    if (stamp.epoch == epoch_ && stamp.generation == id.generation) {
        return;
    }
    if (queue.empty()) {
        checked_topology_revisions_[domain_index] = nodes_->topology_revision();
    }
    queue.push_back(id);
    stamp = {id.generation, epoch_};
}

const std::vector<NodeId>& DirtyQueues::live_queue(
    std::vector<NodeId>& queue, Domain domain) const noexcept {
    const auto domain_index = static_cast<std::size_t>(domain);
    const auto revision = nodes_->topology_revision();
    if (checked_topology_revisions_[domain_index] != revision) {
        std::erase_if(queue, [this](NodeId id) { return nodes_->find(id) == nullptr; });
        checked_topology_revisions_[domain_index] = revision;
    }
    return queue;
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
