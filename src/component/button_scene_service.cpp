#include "component/button_scene_service.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <stdexcept>

namespace ryn::component {

ButtonSceneService::ButtonSceneService(
    runtime::ComponentHost& components,
    runtime::NodeStore& nodes,
    ComponentSceneComposer& composer) noexcept
    : components_(&components), nodes_(&nodes), composer_(&composer) {}

void ButtonSceneService::reserve(std::size_t button_capacity) {
    ensure_owner_thread();
    slots_.reserve(button_capacity);
    free_slots_.reserve(button_capacity);
}

ButtonSceneId ButtonSceneService::create(
    runtime::ComponentId component,
    runtime::NodeId node,
    runtime::SceneFragmentId fragment,
    std::optional<input::InteractionId> interaction,
    const ButtonVisualData& visuals) {
    ensure_owner_thread();
    if (!components_->contains(component)
            || components_->root(component) != node
            || nodes_->find(node) == nullptr
            || !components_->contains(fragment)) {
        throw std::invalid_argument(
            "Button scene requires live Component, root Node, and fragment identities");
    }
    validate_visuals(visuals);
    const auto slot_index = acquire_slot();
    auto& slot = slots_[slot_index];
    const ButtonSceneId id{slot_index, slot.generation};
    graphics::QuadInstanceRange range;
    try {
        range = instances_.append(visuals);
        slot.record.emplace(Record{
            id,
            component,
            node,
            fragment,
            interaction,
            range,
        });
        bind_fragment(*slot.record);
    } catch (...) {
        if (range.count != 0) {
            static_cast<void>(instances_.replace(range, {}));
        }
        slot.record.reset();
        try {
            free_slots_.push_back(slot_index);
        } catch (...) {
        }
        throw;
    }
    ++live_records_;
    ++diagnostics_.creates;
    return id;
}

bool ButtonSceneService::destroy(ButtonSceneId id) {
    ensure_owner_thread();
    auto* record = find(id);
    if (record == nullptr) {
        ++diagnostics_.stale_rejections;
        return false;
    }
    const auto removed = record->range;
    static_cast<void>(composer_->remove_fragment(record->fragment));
    static_cast<void>(instances_.replace(removed, {}));

    for (auto& candidate_slot : slots_) {
        if (!candidate_slot.record.has_value()
                || candidate_slot.record->id == id
                || candidate_slot.record->range.first <= removed.first) {
            continue;
        }
        candidate_slot.record->range.first -= removed.count;
        bind_fragment(*candidate_slot.record);
        ++diagnostics_.fragment_remaps;
    }

    auto& slot = slots_[id.index];
    slot.record.reset();
    advance_generation(slot);
    free_slots_.push_back(id.index);
    --live_records_;
    ++diagnostics_.destroys;
    if (removed.count != 0) {
        ++diagnostics_.range_compactions;
    }
    return true;
}

std::size_t ButtonSceneService::update(
    ButtonSceneId id,
    const ButtonVisualData& visuals) {
    ensure_owner_thread();
    auto& record = require(id);
    validate_visuals(visuals);

    std::array<graphics::QuadMaterial, button_visual_layer_count> materials;
    std::array<graphics::QuadGeometry, button_visual_layer_count> geometry;
    for (std::size_t index = 0; index < visuals.size(); ++index) {
        materials[index] = {visuals[index].color, visuals[index].opacity};
        geometry[index] = {
            visuals[index].clip_rect,
            visuals[index].corner_radius,
            visuals[index].translation,
        };
    }
    const auto material_updates = instances_.update_material(record.range, materials);
    const auto geometry_updates = instances_.update_geometry(record.range, geometry);
    diagnostics_.material_updates += material_updates;
    diagnostics_.geometry_updates += geometry_updates;
    return material_updates + geometry_updates;
}

void ButtonSceneService::synchronize_gpu(
    graphics::QuadGpuBuffer& gpu_buffer) {
    ensure_owner_thread();
    gpu_buffer.synchronize(instances_);
}

graphics::QuadInstanceRange ButtonSceneService::visual_range(
    ButtonSceneId id) const {
    ensure_owner_thread();
    return require(id).range;
}

graphics::QuadInstanceStore& ButtonSceneService::instances() noexcept {
    return instances_;
}

const graphics::QuadInstanceStore&
ButtonSceneService::instances() const noexcept {
    return instances_;
}

std::size_t ButtonSceneService::size() const noexcept {
    return live_records_;
}

const ButtonSceneDiagnostics&
ButtonSceneService::diagnostics() const noexcept {
    return diagnostics_;
}

ButtonSceneService::Record* ButtonSceneService::find(
    ButtonSceneId id) noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    auto& slot = slots_[id.index];
    if (slot.generation != id.generation || !slot.record.has_value()) {
        return nullptr;
    }
    auto& record = *slot.record;
    return components_->contains(record.component)
            && nodes_->find(record.node) != nullptr
            && components_->contains(record.fragment)
        ? &record
        : nullptr;
}

const ButtonSceneService::Record* ButtonSceneService::find(
    ButtonSceneId id) const noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    const auto& slot = slots_[id.index];
    if (slot.generation != id.generation || !slot.record.has_value()) {
        return nullptr;
    }
    const auto& record = *slot.record;
    return components_->contains(record.component)
            && nodes_->find(record.node) != nullptr
            && components_->contains(record.fragment)
        ? &record
        : nullptr;
}

ButtonSceneService::Record& ButtonSceneService::require(ButtonSceneId id) {
    if (auto* record = find(id)) {
        return *record;
    }
    ++diagnostics_.stale_rejections;
    throw std::out_of_range("ButtonSceneId is stale or has stale associations");
}

const ButtonSceneService::Record& ButtonSceneService::require(
    ButtonSceneId id) const {
    if (const auto* record = find(id)) {
        return *record;
    }
    throw std::out_of_range("ButtonSceneId is stale or has stale associations");
}

std::uint32_t ButtonSceneService::acquire_slot() {
    if (!free_slots_.empty()) {
        const auto index = free_slots_.back();
        free_slots_.pop_back();
        return index;
    }
    if (slots_.size() >= ButtonSceneId::invalid_index) {
        throw std::length_error("ButtonSceneService exhausted ButtonSceneId indices");
    }
    slots_.emplace_back();
    return static_cast<std::uint32_t>(slots_.size() - 1);
}

void ButtonSceneService::bind_fragment(const Record& record) {
    const graphics::SceneDrawCommand command{
        graphics::SceneDrawKind::quad,
        record.range.first,
        record.range.count,
        graphics::invalid_glyph_atlas_page,
    };
    composer_->set_fragment(
        record.fragment,
        std::span<const graphics::SceneDrawCommand>{&command, 1},
        record.interaction);
}

void ButtonSceneService::ensure_owner_thread() const {
    if (!components_->is_owner_thread()) {
        throw std::logic_error(
            "ButtonSceneService can only be used on its owner thread");
    }
}

void ButtonSceneService::validate_visuals(
    const ButtonVisualData& visuals) {
    for (const auto& visual : visuals) {
        const bool finite_clip = std::ranges::all_of(
            visual.clip_rect, [](float value) { return std::isfinite(value); });
        const bool finite_color = std::ranges::all_of(
            visual.color, [](float value) { return std::isfinite(value); });
        const bool finite_translation = std::ranges::all_of(
            visual.translation, [](float value) { return std::isfinite(value); });
        if (!finite_clip || !finite_color || !finite_translation
                || !std::isfinite(visual.opacity)
                || !std::isfinite(visual.corner_radius)
                || visual.opacity < 0.0F || visual.opacity > 1.0F
                || visual.corner_radius < 0.0F
                || visual.corner_radius > 0.5F) {
            throw std::invalid_argument("Button visual data is invalid");
        }
    }
}

void ButtonSceneService::advance_generation(Slot& slot) noexcept {
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
}

} // namespace ryn::component
