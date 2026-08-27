#include "input/interaction_registry.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ryn::input {
namespace {

bool valid_rect(runtime::Rect rect) noexcept {
    return std::isfinite(rect.x)
        && std::isfinite(rect.y)
        && std::isfinite(rect.width)
        && std::isfinite(rect.height)
        && rect.width >= 0.0F
        && rect.height >= 0.0F;
}

runtime::Rect intersect(runtime::Rect left, runtime::Rect right) noexcept {
    const float x = std::max(left.x, right.x);
    const float y = std::max(left.y, right.y);
    const float right_edge = std::min(
        left.x + left.width,
        right.x + right.width);
    const float bottom_edge = std::min(
        left.y + left.height,
        right.y + right.height);
    return {
        x,
        y,
        std::max(0.0F, right_edge - x),
        std::max(0.0F, bottom_edge - y),
    };
}

bool contains(runtime::Rect rect, runtime::Point point) noexcept {
    return rect.width > 0.0F
        && rect.height > 0.0F
        && point.x >= rect.x
        && point.y >= rect.y
        && point.x < rect.x + rect.width
        && point.y < rect.y + rect.height;
}

} // namespace

InteractionRegistry::InteractionRegistry(
    runtime::ComponentHost& components,
    runtime::NodeStore& nodes) noexcept
    : components_(&components),
      nodes_(&nodes),
      owner_thread_(std::this_thread::get_id()) {}

void InteractionRegistry::reserve(std::size_t capacity) {
    ensure_owner_thread();
    slots_.reserve(capacity);
    free_slots_.reserve(capacity);
    declaration_order_.reserve(capacity);
}

InteractionId InteractionRegistry::create(InteractionRegistration registration) {
    ensure_owner_thread();
    validate_registration(registration);
    const auto slot_index = acquire_slot();
    auto& slot = slots_[slot_index];
    const InteractionId id{slot_index, slot.generation};
    try {
        slot.record.emplace(InteractionRecord{
            id,
            registration.component,
            registration.node,
            registration.parent,
            registration.eligible,
            registration.focusable,
            std::move(registration.handlers),
            next_declaration_order_,
        });
        declaration_order_.push_back(id);
    } catch (...) {
        slot.record.reset();
        try {
            free_slots_.push_back(slot_index);
        } catch (...) {
        }
        throw;
    }
    ++next_declaration_order_;
    ++live_records_;
    return id;
}

bool InteractionRegistry::remove(InteractionId id) {
    ensure_owner_thread();
    auto* record = find_slot_record(id);
    if (record == nullptr) {
        return false;
    }
    std::erase(declaration_order_, id);
    auto& slot = slots_[id.index];
    slot.record.reset();
    advance_generation(slot);
    free_slots_.push_back(id.index);
    --live_records_;
    return true;
}

bool InteractionRegistry::set_eligible(InteractionId id, bool eligible) {
    auto& record = require(id);
    if (record.eligible == eligible) {
        return false;
    }
    record.eligible = eligible;
    return true;
}

bool InteractionRegistry::set_focusable(InteractionId id, bool focusable) {
    auto& record = require(id);
    if (record.focusable == focusable) {
        return false;
    }
    record.focusable = focusable;
    return true;
}

bool InteractionRegistry::set_handlers(
    InteractionId id,
    InteractionHandlers handlers) {
    auto& record = require(id);
    record.handlers = std::move(handlers);
    return true;
}

InteractionRecord* InteractionRegistry::find(InteractionId id) {
    ensure_owner_thread();
    auto* record = find_slot_record(id);
    return record != nullptr && associations_are_live(*record) ? record : nullptr;
}

const InteractionRecord* InteractionRegistry::find(InteractionId id) const {
    ensure_owner_thread();
    const auto* record = find_slot_record(id);
    return record != nullptr && associations_are_live(*record) ? record : nullptr;
}

InteractionRecord& InteractionRegistry::require(InteractionId id) {
    if (auto* record = find(id)) {
        return *record;
    }
    throw std::out_of_range("InteractionId is stale or has stale associations");
}

const InteractionRecord& InteractionRegistry::require(InteractionId id) const {
    if (const auto* record = find(id)) {
        return *record;
    }
    throw std::out_of_range("InteractionId is stale or has stale associations");
}

bool InteractionRegistry::contains(InteractionId id) const {
    return find(id) != nullptr;
}

bool InteractionRegistry::is_owner_thread() const noexcept {
    return owner_thread_ == std::this_thread::get_id();
}

std::size_t InteractionRegistry::size() const {
    ensure_owner_thread();
    return live_records_;
}

std::span<const InteractionId> InteractionRegistry::declaration_order() const {
    ensure_owner_thread();
    return declaration_order_;
}

void InteractionRegistry::ensure_owner_thread() const {
    if (!is_owner_thread()) {
        throw std::logic_error("InteractionRegistry can only be used on its owner thread");
    }
}

void InteractionRegistry::validate_registration(
    const InteractionRegistration& registration) const {
    if (!registration.component.valid()
            || !components_->contains(registration.component)) {
        throw std::invalid_argument("Interaction registration requires a live ComponentId");
    }
    if (!registration.node.valid()
            || nodes_->find(registration.node) == nullptr
            || !node_belongs_to_component(registration.node, registration.component)) {
        throw std::invalid_argument(
            "Interaction registration requires a live NodeId owned by its component");
    }
    if (registration.parent.has_value()) {
        const auto* parent = find_slot_record(*registration.parent);
        if (parent == nullptr || !associations_are_live(*parent)) {
            throw std::invalid_argument("Interaction parent is stale or invalid");
        }
        if (!component_is_same_or_ancestor(
                parent->component,
                registration.component)) {
            throw std::invalid_argument(
                "Interaction parent component is not an ancestor");
        }
    }
}

bool InteractionRegistry::node_belongs_to_component(
    runtime::NodeId node,
    runtime::ComponentId component) const {
    const auto component_root = components_->root(component);
    runtime::NodeId current = node;
    while (const auto* record = nodes_->find(current)) {
        if (current == component_root) {
            return true;
        }
        if (!record->parent.has_value()) {
            return false;
        }
        current = *record->parent;
    }
    return false;
}

bool InteractionRegistry::component_is_same_or_ancestor(
    runtime::ComponentId ancestor,
    runtime::ComponentId component) const {
    auto current = std::optional<runtime::ComponentId>{component};
    while (current.has_value()) {
        if (*current == ancestor) {
            return true;
        }
        if (!components_->contains(*current)) {
            return false;
        }
        current = components_->parent(*current);
    }
    return false;
}

InteractionRecord* InteractionRegistry::find_slot_record(
    InteractionId id) noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    auto& slot = slots_[id.index];
    if (slot.generation != id.generation || !slot.record.has_value()) {
        return nullptr;
    }
    return &*slot.record;
}

const InteractionRecord* InteractionRegistry::find_slot_record(
    InteractionId id) const noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    const auto& slot = slots_[id.index];
    if (slot.generation != id.generation || !slot.record.has_value()) {
        return nullptr;
    }
    return &*slot.record;
}

bool InteractionRegistry::associations_are_live(
    const InteractionRecord& record) const {
    if (!components_->contains(record.component)
            || nodes_->find(record.node) == nullptr
            || !node_belongs_to_component(record.node, record.component)) {
        return false;
    }
    if (record.parent.has_value()) {
        const auto* parent = find_slot_record(*record.parent);
        if (parent == nullptr
                || !components_->contains(parent->component)
                || nodes_->find(parent->node) == nullptr) {
            return false;
        }
    }
    return true;
}

std::uint32_t InteractionRegistry::acquire_slot() {
    if (!free_slots_.empty()) {
        const auto index = free_slots_.back();
        free_slots_.pop_back();
        return index;
    }
    if (slots_.size() >= InteractionId::invalid_index) {
        throw std::length_error("InteractionRegistry exhausted InteractionId indices");
    }
    free_slots_.reserve(slots_.size() + 1);
    slots_.emplace_back();
    return static_cast<std::uint32_t>(slots_.size() - 1);
}

void InteractionRegistry::advance_generation(Slot& slot) noexcept {
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
}

HitTestSnapshot::HitTestSnapshot(
    InteractionRegistry& registry,
    runtime::NodeStore& nodes) noexcept
    : registry_(&registry), nodes_(&nodes) {}

void HitTestSnapshot::reserve(std::size_t capacity) {
    records_.reserve(capacity);
}

void HitTestSnapshot::rebuild(
    std::span<const HitTestPaintEntry> paint_entries,
    runtime::Rect window_clip) {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("HitTestSnapshot can only be used on its owner thread");
    }
    if (!valid_rect(window_clip)) {
        throw std::invalid_argument("HitTest window clip must be finite and non-negative");
    }

    std::vector<HitTestRecord> replacement;
    replacement.reserve(std::max(records_.capacity(), paint_entries.size()));
    for (std::size_t index = 0; index < paint_entries.size(); ++index) {
        if (std::find_if(
                replacement.begin(),
                replacement.end(),
                [&](const auto& record) {
                    return record.interaction == paint_entries[index].interaction;
                }) != replacement.end()) {
            throw std::invalid_argument("HitTest paint traversal contains a duplicate interaction");
        }
        replacement.push_back(make_record(
            paint_entries[index],
            index,
            replacement,
            window_clip));
    }
    records_ = std::move(replacement);
    window_clip_ = window_clip;
    ++diagnostics_.snapshot_rebuilds;
    diagnostics_.records_refreshed += records_.size();
}

std::size_t HitTestSnapshot::refresh(
    std::span<const runtime::NodeId> dirty_nodes) {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("HitTestSnapshot can only be used on its owner thread");
    }
    std::size_t refreshed = 0;
    for (std::size_t index = 0; index < records_.size(); ++index) {
        const bool affected = std::any_of(
            dirty_nodes.begin(),
            dirty_nodes.end(),
            [&](runtime::NodeId dirty) {
                return node_descends_from(records_[index].node, dirty);
            });
        if (affected && refresh_record(index)) {
            ++refreshed;
        }
    }
    diagnostics_.records_refreshed += refreshed;
    return refreshed;
}

std::size_t HitTestSnapshot::refresh_interaction(InteractionId interaction) {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("HitTestSnapshot can only be used on its owner thread");
    }
    std::size_t refreshed = 0;
    for (std::size_t index = 0; index < records_.size(); ++index) {
        if ((records_[index].interaction == interaction
                || snapshot_descends_from(records_[index], interaction))
                && refresh_record(index)) {
            ++refreshed;
        }
    }
    diagnostics_.records_refreshed += refreshed;
    return refreshed;
}

std::optional<InteractionId> HitTestSnapshot::hit_test(runtime::Point point) {
    if (!registry_->is_owner_thread()) {
        throw std::logic_error("HitTestSnapshot can only be used on its owner thread");
    }
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        throw std::invalid_argument("HitTest point must be finite");
    }
    ++diagnostics_.queries;
    if (!contains(window_clip_, point)) {
        return std::nullopt;
    }
    for (auto iterator = records_.rbegin(); iterator != records_.rend(); ++iterator) {
        ++diagnostics_.records_examined;
        const auto* interaction = registry_->find(iterator->interaction);
        if (interaction == nullptr) {
            ++diagnostics_.stale_skips;
            continue;
        }
        if (!iterator->has_committed_layout
                || !iterator->effective_eligible
                || !interaction->eligible
                || !contains(iterator->effective_clip, point)
                || !contains(iterator->translated_bounds, point)) {
            continue;
        }
        ++diagnostics_.hits;
        return iterator->interaction;
    }
    return std::nullopt;
}

std::span<const HitTestRecord> HitTestSnapshot::records() const noexcept {
    return records_;
}

std::size_t HitTestSnapshot::capacity() const noexcept {
    return records_.capacity();
}

const HitTestDiagnostics& HitTestSnapshot::diagnostics() const noexcept {
    return diagnostics_;
}

HitTestRecord HitTestSnapshot::make_record(
    const HitTestPaintEntry& entry,
    std::size_t paint_order,
    std::span<const HitTestRecord> preceding,
    runtime::Rect window_clip) const {
    const auto& interaction = registry_->require(entry.interaction);
    if (entry.clip.has_value() && !valid_rect(*entry.clip)) {
        throw std::invalid_argument("HitTest entry clip must be finite and non-negative");
    }

    const HitTestRecord* parent = nullptr;
    if (interaction.parent.has_value()) {
        parent = find_preceding(*interaction.parent, preceding);
        if (parent == nullptr) {
            throw std::invalid_argument(
                "HitTest traversal must place an interaction parent before its child");
        }
    }

    const auto* node = nodes_->find(interaction.node);
    const bool committed = node != nullptr
        && node->place_generation != 0
        && valid_rect(node->bounds)
        && std::isfinite(node->translation.x)
        && std::isfinite(node->translation.y);
    runtime::Rect bounds{};
    if (committed) {
        bounds = {
            node->bounds.x + node->translation.x,
            node->bounds.y + node->translation.y,
            node->bounds.width,
            node->bounds.height,
        };
    }

    auto clip = parent == nullptr ? window_clip : parent->effective_clip;
    if (entry.clip.has_value()) {
        clip = intersect(clip, *entry.clip);
    }
    return {
        entry.interaction,
        interaction.node,
        interaction.parent,
        bounds,
        clip,
        entry.clip,
        parent == nullptr ? 0 : parent->depth + 1,
        paint_order,
        interaction.eligible && (parent == nullptr || parent->effective_eligible),
        committed,
    };
}

bool HitTestSnapshot::refresh_record(std::size_t index) {
    if (index >= records_.size()) {
        return false;
    }
    auto& target = records_[index];
    const auto* interaction = registry_->find(target.interaction);
    if (interaction == nullptr) {
        target.has_committed_layout = false;
        target.effective_eligible = false;
        return true;
    }
    const HitTestPaintEntry entry{target.interaction, target.source_clip};
    target = make_record(
        entry,
        target.paint_order,
        std::span<const HitTestRecord>{records_.data(), index},
        window_clip_);
    return true;
}

const HitTestRecord* HitTestSnapshot::find_preceding(
    InteractionId id,
    std::span<const HitTestRecord> records) const noexcept {
    const auto found = std::find_if(
        records.begin(),
        records.end(),
        [&](const auto& record) { return record.interaction == id; });
    return found == records.end() ? nullptr : &*found;
}

bool HitTestSnapshot::snapshot_descends_from(
    const HitTestRecord& record,
    InteractionId ancestor) const noexcept {
    auto parent = record.parent;
    while (parent.has_value()) {
        if (*parent == ancestor) {
            return true;
        }
        const auto found = std::find_if(
            records_.begin(),
            records_.end(),
            [&](const auto& current) { return current.interaction == *parent; });
        if (found == records_.end()) {
            return false;
        }
        parent = found->parent;
    }
    return false;
}

bool HitTestSnapshot::node_descends_from(
    runtime::NodeId node,
    runtime::NodeId ancestor) const noexcept {
    auto current = std::optional<runtime::NodeId>{node};
    while (current.has_value()) {
        if (*current == ancestor) {
            return true;
        }
        const auto* record = nodes_->find(*current);
        if (record == nullptr) {
            return false;
        }
        current = record->parent;
    }
    return false;
}

} // namespace ryn::input
