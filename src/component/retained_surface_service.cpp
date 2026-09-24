#include "component/retained_surface_service.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <stdexcept>

namespace ryn::component {

RetainedSurfaceService::RetainedSurfaceService(
    runtime::ComponentHost& components,
    runtime::NodeStore& nodes,
    ComponentSceneComposer& composer) noexcept
    : components_(&components), nodes_(&nodes), composer_(&composer) {}

void RetainedSurfaceService::reserve(
    std::size_t surface_capacity, std::size_t visual_capacity) {
    ensure_owner_thread();
    if (visual_capacity > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("retained visual capacity exceeds uint32_t");
    }
    slots_.reserve(surface_capacity);
    free_slots_.reserve(surface_capacity);
    instances_.reserve(visual_capacity, surface_capacity);
}

RetainedSurfaceId RetainedSurfaceService::create(
    runtime::ComponentId component,
    runtime::NodeId node,
    runtime::SceneFragmentId fragment,
    std::optional<input::InteractionId> interaction,
    std::span<const graphics::QuadInstance> visuals,
    const RetainedSurfaceEffects& effects) {
    return create_record(
        component, node, fragment, interaction, visuals, effects);
}

RetainedSurfaceId RetainedSurfaceService::create_surface(
    runtime::ComponentId component,
    runtime::NodeId node,
    runtime::SceneFragmentId fragment,
    std::span<const graphics::QuadInstance> visuals,
    const RetainedSurfaceEffects& effects,
    std::optional<input::InteractionId> interaction) {
    return create_record(
        component, node, fragment, interaction, visuals, effects);
}

RetainedSurfaceId RetainedSurfaceService::create_record(
    runtime::ComponentId component,
    runtime::NodeId node,
    runtime::SceneFragmentId fragment,
    std::optional<input::InteractionId> interaction,
    std::span<const graphics::QuadInstance> visuals,
    const RetainedSurfaceEffects& effects) {
    ensure_owner_thread();
    if (!components_->contains(component)
            || components_->root(component) != node
            || nodes_->find(node) == nullptr
            || !components_->contains(fragment)) {
        throw std::invalid_argument(
            "retained surface requires live Component, root Node, and fragment identities");
    }
    validate_visuals(visuals);
    const auto slot_index = acquire_slot();
    auto& slot = slots_[slot_index];
    const RetainedSurfaceId id{slot_index, slot.generation};
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
            effects,
            {},
            {},
            {},
        });
        create_effects(*slot.record);
        bind_fragment(*slot.record);
    } catch (...) {
        if (range.count != 0) {
            static_cast<void>(instances_.replace(range, {}));
        }
        if (slot.record.has_value()) {
            remove_effects(*slot.record);
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

bool RetainedSurfaceService::destroy(RetainedSurfaceId id) {
    ensure_owner_thread();
    auto* record = find(id);
    if (record == nullptr) {
        ++diagnostics_.stale_rejections;
        return false;
    }
    const auto removed = record->range;
    static_cast<void>(composer_->remove_fragment(record->fragment));
    static_cast<void>(instances_.replace(removed, {}));
    remove_effects(*record);

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

std::size_t RetainedSurfaceService::update(
    RetainedSurfaceId id,
    std::span<const graphics::QuadInstance> visuals) {
    return update_surface(id, visuals);
}

std::size_t RetainedSurfaceService::update_surface(
    RetainedSurfaceId id,
    std::span<const graphics::QuadInstance> visuals) {
    ensure_owner_thread();
    auto& record = require(id);
    validate_visuals(visuals);
    if (visuals.size() != record.range.count) {
        throw std::invalid_argument(
            "retained surface update changed its visual layer count");
    }

    std::array<graphics::QuadMaterial, retained_surface_visual_capacity> materials;
    std::array<graphics::QuadGeometry, retained_surface_visual_capacity> geometry;
    for (std::size_t index = 0; index < visuals.size(); ++index) {
        materials[index] = {visuals[index].color, visuals[index].opacity};
        geometry[index] = {
            visuals[index].clip_rect,
            visuals[index].corner_radius,
            visuals[index].translation,
        };
    }
    const auto material_updates = instances_.update_material(
        record.range, std::span{materials}.first(visuals.size()));
    const auto geometry_updates = instances_.update_geometry(
        record.range, std::span{geometry}.first(visuals.size()));
    diagnostics_.material_updates += material_updates;
    diagnostics_.geometry_updates += geometry_updates;
    return material_updates + geometry_updates;
}

std::size_t RetainedSurfaceService::update_effects(
    RetainedSurfaceId id,
    const RetainedSurfaceEffects& effects) {
    ensure_owner_thread();
    auto& record = require(id);
    if (record.effects == effects) {
        return 0;
    }

    bool topology_changed = record.shadow_ids.size() != effects.shadows.size()
        || record.focus_id.valid() != effects.focus_enabled;
    if (!topology_changed) {
        for (std::size_t index = 0; index < record.shadow_ids.size(); ++index) {
            const auto expected = effects.shadows[index].kind == ShadowKind::outer
                ? graphics::RoundedEffectKind::outer_shadow
                : graphics::RoundedEffectKind::inset_shadow;
            if (effect_scene_.store().at(record.shadow_ids[index]).geometry.kind
                    != expected) {
                topology_changed = true;
                break;
            }
        }
    }
    if (topology_changed) {
        remove_effects(record);
        record.effects = effects;
        create_effects(record);
        ++diagnostics_.effect_topology_updates;
        return effects.shadows.size() + (effects.focus_enabled ? 1U : 0U);
    }

    std::size_t updates = 0;
    for (std::size_t index = 0; index < record.shadow_ids.size(); ++index) {
        auto candidate = graphics::make_shadow_effect(
            effects.shape,
            effects.shadows[index],
            effects.translation,
            effects.ancestor_clip);
        candidate.material.opacity = effects.shadow_opacity;
        const auto effect = record.shadow_ids[index];
        if (effect_scene_.store().update_geometry(effect, candidate.geometry)) {
            ++diagnostics_.effect_geometry_updates;
            ++updates;
        }
        if (effect_scene_.store().update_material(effect, candidate.material)) {
            ++diagnostics_.effect_material_updates;
            ++updates;
        }
    }
    if (effects.focus_enabled) {
        auto focus = graphics::make_outline_effect(
            effects.shape,
            effects.focus_width,
            effects.focus_offset,
            effects.focus_color,
            effects.focus_opacity,
            effects.translation,
            effects.ancestor_clip);
        if (effect_scene_.store().update_geometry(record.focus_id, focus.geometry)) {
            ++diagnostics_.effect_geometry_updates;
            ++updates;
        }
        if (effect_scene_.store().update_material(record.focus_id, focus.material)) {
            ++diagnostics_.effect_material_updates;
            ++updates;
        }
    }
    record.effects = effects;
    return updates;
}

bool RetainedSurfaceService::compact_effects(runtime::Rect window_clip) {
    ensure_owner_thread();
    if (!effect_scene_.store().compact(window_clip)) {
        return false;
    }
    for (const auto& slot : slots_) {
        if (slot.record.has_value()) {
            bind_fragment(*slot.record);
        }
    }
    return true;
}

void RetainedSurfaceService::synchronize_gpu(
    graphics::QuadGpuBuffer& gpu_buffer) {
    ensure_owner_thread();
    gpu_buffer.synchronize(instances_);
}

graphics::QuadInstanceRange RetainedSurfaceService::visual_range(
    RetainedSurfaceId id) const {
    ensure_owner_thread();
    return require(id).range;
}

const graphics::RoundedEffectInstance& RetainedSurfaceService::focus_effect(
    RetainedSurfaceId id) const {
    ensure_owner_thread();
    const auto& record = require(id);
    return effect_scene_.store().at(record.focus_id);
}

std::span<const graphics::RoundedEffectId> RetainedSurfaceService::shadow_effects(
    RetainedSurfaceId id) const {
    ensure_owner_thread();
    return require(id).shadow_ids;
}

graphics::QuadInstanceStore& RetainedSurfaceService::instances() noexcept {
    return instances_;
}

const graphics::QuadInstanceStore&
RetainedSurfaceService::instances() const noexcept {
    return instances_;
}

graphics::RoundedEffectStore& RetainedSurfaceService::effects() noexcept {
    return effect_scene_.store();
}

const graphics::RoundedEffectStore& RetainedSurfaceService::effects() const noexcept {
    return effect_scene_.store();
}

std::size_t RetainedSurfaceService::size() const noexcept {
    return live_records_;
}

const RetainedSurfaceDiagnostics&
RetainedSurfaceService::diagnostics() const noexcept {
    return diagnostics_;
}

RetainedSurfaceService::Record* RetainedSurfaceService::find(
    RetainedSurfaceId id) noexcept {
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

const RetainedSurfaceService::Record* RetainedSurfaceService::find(
    RetainedSurfaceId id) const noexcept {
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

RetainedSurfaceService::Record& RetainedSurfaceService::require(RetainedSurfaceId id) {
    if (auto* record = find(id)) {
        return *record;
    }
    ++diagnostics_.stale_rejections;
    throw std::out_of_range("RetainedSurfaceId is stale or has stale associations");
}

const RetainedSurfaceService::Record& RetainedSurfaceService::require(
    RetainedSurfaceId id) const {
    if (const auto* record = find(id)) {
        return *record;
    }
    throw std::out_of_range("RetainedSurfaceId is stale or has stale associations");
}

std::uint32_t RetainedSurfaceService::acquire_slot() {
    if (!free_slots_.empty()) {
        const auto index = free_slots_.back();
        free_slots_.pop_back();
        return index;
    }
    if (slots_.size() >= RetainedSurfaceId::invalid_index) {
        throw std::length_error("RetainedSurfaceService exhausted RetainedSurfaceId indices");
    }
    slots_.emplace_back();
    return static_cast<std::uint32_t>(slots_.size() - 1);
}

void RetainedSurfaceService::bind_fragment(const Record& record) {
    const graphics::SceneDrawCommand fill{
        graphics::SceneDrawKind::quad,
        record.range.first,
        record.range.count,
        graphics::invalid_glyph_atlas_page,
    };
    std::vector<graphics::SceneDrawCommand> commands;
    commands.reserve(record.shadow_ids.size() + 2);
    effect_scene_.compose_surface(record.effect_primitive, fill, commands);
    composer_->set_fragment(
        record.fragment,
        commands,
        record.interaction);
}

void RetainedSurfaceService::create_effects(Record& record) {
    record.shadow_ids.clear();
    record.effect_primitive = {};
    record.shadow_ids.reserve(record.effects.shadows.size());
    record.effect_primitive.before_fill.reserve(record.effects.shadows.size() + 1);
    record.effect_primitive.after_fill.reserve(record.effects.shadows.size());
    try {
        for (const auto& layer : record.effects.shadows.layers()) {
            auto instance = graphics::make_shadow_effect(
                record.effects.shape,
                layer,
                record.effects.translation,
                record.effects.ancestor_clip);
            instance.material.opacity = record.effects.shadow_opacity;
            const auto id = effect_scene_.store().add(std::move(instance));
            record.shadow_ids.push_back(id);
            if (layer.kind == ShadowKind::outer) {
                record.effect_primitive.before_fill.push_back(id);
            } else {
                record.effect_primitive.after_fill.push_back(id);
            }
        }
        if (record.effects.focus_enabled) {
            auto outline = graphics::make_outline_effect(
                record.effects.shape,
                record.effects.focus_width,
                record.effects.focus_offset,
                record.effects.focus_color,
                record.effects.focus_opacity,
                record.effects.translation,
                record.effects.ancestor_clip);
            record.focus_id = effect_scene_.store().add(std::move(outline));
            record.effect_primitive.before_fill.push_back(record.focus_id);
        }
    } catch (...) {
        remove_effects(record);
        throw;
    }
}

void RetainedSurfaceService::remove_effects(Record& record) noexcept {
    try {
        static_cast<void>(effect_scene_.remove(record.effect_primitive));
    } catch (...) {
    }
    record.shadow_ids.clear();
    record.focus_id = {};
    record.effect_primitive = {};
}

void RetainedSurfaceService::ensure_owner_thread() const {
    if (!components_->is_owner_thread()) {
        throw std::logic_error(
            "RetainedSurfaceService can only be used on its owner thread");
    }
}

void RetainedSurfaceService::validate_visuals(
    std::span<const graphics::QuadInstance> visuals) {
    if (visuals.empty()
            || visuals.size() > retained_surface_visual_capacity) {
        throw std::invalid_argument(
            "retained surface visual layer count is invalid");
    }
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
            throw std::invalid_argument("retained surface visual data is invalid");
        }
    }
}

void RetainedSurfaceService::advance_generation(Slot& slot) noexcept {
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
}

} // namespace ryn::component
