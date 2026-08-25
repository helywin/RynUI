#include "runtime/node_store.hpp"

#include <algorithm>
#include <stdexcept>

namespace ryn::runtime {

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

    slot->node = {};
    slot->occupied = false;
    advance_generation(*slot);
    free_slots_.push_back(id.index);
    --live_nodes_;
    return true;
}

std::size_t NodeStore::size() const noexcept {
    return live_nodes_;
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
