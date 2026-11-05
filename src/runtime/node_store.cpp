#include "runtime/node_store.hpp"

#include <algorithm>
#include <stdexcept>
#include <cmath>

namespace ryn::runtime {

Rect NodeStore::content_clip(NodeId id, Rect clip, ClipPurpose purpose) const {
    for (auto current = std::optional{id}; current; current = require(*current).parent) {
        const auto& node = require(*current);
        if (node.clip_content && (purpose == ClipPurpose::content || node.clip_decorations)) {
            const auto& bounds = node.bounds;
            const float left = std::max(clip.x, bounds.x + node.translation.x);
            const float top = std::max(clip.y, bounds.y + node.translation.y);
            const float right = std::min(clip.x + clip.width, bounds.x + node.translation.x + bounds.width);
            const float bottom = std::min(clip.y + clip.height, bounds.y + node.translation.y + bounds.height);
            clip = {left, top, std::max(0.0F, right - left), std::max(0.0F, bottom - top)};
        }
        if (node.clip_boundary) {
            break;
        }
    }
    return clip;
}

float NodeStore::content_opacity(NodeId id) const {
    float opacity = 1;
    for (auto current = std::optional{id}; current; current = require(*current).parent) {
        opacity *= require(*current).content_opacity;
    }
    return opacity;
}

float NodeStore::paint_opacity(NodeId id) const {
    if (paint_opacity_count_ == 0) {
        static_cast<void>(require(id));
        return 1;
    }
    float result = 1;
    for (auto current = std::optional{id}; current; current = require(*current).parent) {
        result *= require(*current).paint_opacity;
    }
    return result;
}

bool NodeStore::set_paint_opacity(NodeId id, float value) {
    if (!std::isfinite(value) || value < 0 || value > 1) {
        throw std::invalid_argument("Node paint opacity must be finite in 0..1");
    }
    auto& node = require(id);
    if (node.paint_opacity == value) {
        return false;
    }
    if (node.paint_opacity == 1 && value != 1) {
        ++paint_opacity_count_;
    }
    if (node.paint_opacity != 1 && value == 1) {
        --paint_opacity_count_;
    }
    node.paint_opacity = value;
    return true;
}

NodeId NodeStore::create_root() {
    return create(std::nullopt);
}

NodeId NodeStore::create_child(NodeId parent) {
    if (find(parent) == nullptr) {
        throw std::invalid_argument("Cannot attach a node to an invalid parent");
    }
    return create(parent);
}

Node* NodeStore::find(NodeId id) noexcept {
    if (auto* slot = find_slot(id)) {
        return &slot->node;
    }
    return nullptr;
}

const Node* NodeStore::find(NodeId id) const noexcept {
    if (const auto* slot = find_slot(id)) {
        return &slot->node;
    }
    return nullptr;
}

Node& NodeStore::require(NodeId id) {
    if (auto* node = find(id)) {
        return *node;
    }
    throw std::out_of_range("NodeId is stale or invalid");
}

const Node& NodeStore::require(NodeId id) const {
    if (const auto* node = find(id)) {
        return *node;
    }
    throw std::out_of_range("NodeId is stale or invalid");
}

bool NodeStore::destroy(NodeId id) noexcept {
    auto* slot = find_slot(id);
    if (slot == nullptr) {
        return false;
    }

    while (!slot->node.children.empty()) {
        const NodeId child = slot->node.children.back();
        static_cast<void>(destroy(child));
    }

    if (slot->node.parent.has_value()) {
        if (auto* parent = find(*slot->node.parent)) {
            std::erase(parent->children, id);
        }
    }

    if (slot->node.paint_opacity != 1) {
        --paint_opacity_count_;
    }
    slot->node = {};
    slot->occupied = false;
    advance_generation(*slot);
    free_slots_.push_back(id.index);
    --live_nodes_;
    ++topology_revision_;
    return true;
}

std::size_t NodeStore::size() const noexcept {
    return live_nodes_;
}

std::size_t NodeStore::slot_capacity() const noexcept {
    return slots_.size();
}

std::uint64_t NodeStore::topology_revision() const noexcept {
    return topology_revision_;
}

NodeId NodeStore::create(std::optional<NodeId> parent) {
    std::uint32_t index = 0;
    if (free_slots_.empty()) {
        if (slots_.size() >= NodeId::invalid_index) {
            throw std::length_error("NodeStore exhausted NodeId indices");
        }
        free_slots_.reserve(slots_.size() + 1);
        index = static_cast<std::uint32_t>(slots_.size());
        slots_.emplace_back();
    } else {
        index = free_slots_.back();
        free_slots_.pop_back();
    }

    auto& slot = slots_[index];
    const NodeId id{index, slot.generation};
    slot.node = {};
    slot.node.id = id;
    slot.node.parent = parent;
    slot.occupied = true;
    ++live_nodes_;

    if (parent.has_value()) {
        require(*parent).children.push_back(id);
    }
    ++topology_revision_;
    return id;
}

NodeStore::Slot* NodeStore::find_slot(NodeId id) noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    auto& slot = slots_[id.index];
    if (!slot.occupied || slot.generation != id.generation) {
        return nullptr;
    }
    return &slot;
}

const NodeStore::Slot* NodeStore::find_slot(NodeId id) const noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    const auto& slot = slots_[id.index];
    if (!slot.occupied || slot.generation != id.generation) {
        return nullptr;
    }
    return &slot;
}

void NodeStore::advance_generation(Slot& slot) noexcept {
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
}

} // namespace ryn::runtime
