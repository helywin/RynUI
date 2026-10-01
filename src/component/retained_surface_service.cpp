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

    remap_after_replace(removed, 0, &record->range);

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
            "retained surface update changed its visual layer count; use the "
            "content range API for a variable count");
    }

    // Surface updates run on interaction frames and are covered by allocation
    // count assertions, so this path stays on the stack; the fixed layer limit
    // keeps those buffers bounded. Variable counts belong to the content range
    // API below, which is not on the per-interaction path.
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

std::uint32_t RetainedSurfaceService::acquire_content_slot() {
    if (!free_content_slots_.empty()) {
        const auto index = free_content_slots_.back();
        free_content_slots_.pop_back();
        return index;
    }
    if (content_slots_.size() >= RetainedSurfaceId::invalid_index) {
        throw std::length_error(
            "RetainedSurfaceService exhausted content range indices");
    }
    content_slots_.emplace_back();
    return static_cast<std::uint32_t>(content_slots_.size() - 1);
}

RetainedSurfaceService::ContentRecord* RetainedSurfaceService::find_content(
    RetainedSurfaceId id) noexcept {
    if (!id.valid() || !id.content_range || id.index >= content_slots_.size()) {
        return nullptr;
    }
    auto& slot = content_slots_[id.index];
    if (!slot.record.has_value() || slot.record->id.generation != id.generation
            || !components_->contains(slot.record->fragment)) {
        return nullptr;
    }
    return &*slot.record;
}

RetainedSurfaceService::ContentRecord& RetainedSurfaceService::require_content(
    RetainedSurfaceId id) {
    auto* record = find_content(id);
    if (record == nullptr) {
        throw std::out_of_range("content range id is stale or unknown");
    }
    return *record;
}

void RetainedSurfaceService::publish_content(ContentRecord& record) {
    const graphics::SceneDrawCommand fill{
        graphics::SceneDrawKind::quad,
        record.range.first,
        record.range.count,
        graphics::invalid_glyph_atlas_page,
    };
    // The fragment belongs to the caller, so the command list is replaced rather
    // than merged; the caller keeps sole ownership of that fragment's contents.
    std::vector<graphics::SceneDrawCommand> commands;
    if (record.range.count) commands.push_back(fill);
    for (const auto effect : record.effects) {
        if (const auto packed = effect_scene_.store().packed_index(effect))
            commands.push_back({graphics::SceneDrawKind::rounded_effect, *packed, 1,
                graphics::invalid_glyph_atlas_page});
    }
    composer_->set_fragment(record.fragment, commands);
}

std::size_t RetainedSurfaceService::republish_range(
    graphics::QuadInstanceRange& range,
    std::span<const graphics::QuadInstance> visuals) {
    if (range.count == visuals.size()) {
        std::size_t updates = 0;
        for (std::uint32_t index = 0; index < range.count; ++index) {
            const auto& visual = visuals[index];
            const graphics::QuadMaterial material{visual.color, visual.opacity};
            const graphics::QuadGeometry geometry{visual.clip_rect,
                visual.corner_radius, visual.translation};
            const graphics::QuadInstanceRange single{range.first + index, 1};
            const auto material_updates = instances_.update_material(single, {&material, 1});
            const auto geometry_updates = instances_.update_geometry(single, {&geometry, 1});
            diagnostics_.material_updates += material_updates;
            diagnostics_.geometry_updates += geometry_updates;
            updates += material_updates + geometry_updates;
        }
        return updates;
    }
    // Empty ranges have no storage position. Insert them at the tail to avoid
    // ambiguous ownership when several empty layers share the same index.
    if (range.count == 0) range.first = static_cast<std::uint32_t>(instances_.size());
    const auto old = range;
    const auto replaced = instances_.replace(range, visuals);
    remap_after_replace(old, replaced.count, &range);
    ++diagnostics_.fragment_remaps;
    range = replaced;
    return range.count;
}

RetainedSurfaceId RetainedSurfaceService::create_content_range(
    runtime::SceneFragmentId fragment,
    std::span<const graphics::QuadInstance> visuals) {
    ensure_owner_thread();
    validate_content_visuals(visuals);
    if (!components_->contains(fragment)) throw std::invalid_argument("content fragment is stale");
    const auto slot_index = acquire_content_slot();
    auto& slot = content_slots_[slot_index];
    const RetainedSurfaceId id{slot_index, slot.generation, true};
    try {
        slot.record.emplace(ContentRecord{
            id, fragment, instances_.append(visuals)});
        publish_content(*slot.record);
    } catch (...) {
        if (slot.record) {
            const auto removed = slot.record->range;
            static_cast<void>(instances_.replace(removed, {}));
            remap_after_replace(removed, 0, &slot.record->range);
        }
        slot.record.reset();
        try {
            free_content_slots_.push_back(slot_index);
        } catch (...) {
        }
        throw;
    }
    ++live_records_;
    ++diagnostics_.creates;
    return id;
}

void RetainedSurfaceService::remap_after_replace(
    graphics::QuadInstanceRange old_range, std::uint32_t new_count,
    const graphics::QuadInstanceRange* owner) {
    const auto end = old_range.first + old_range.count;
    const auto shift = static_cast<std::int64_t>(new_count) - old_range.count;
    if (shift == 0) return;
    for (auto& slot : slots_) {
        if (!slot.record || &slot.record->range == owner
                || slot.record->range.first < end) continue;
        slot.record->range.first = static_cast<std::uint32_t>(slot.record->range.first + shift);
        if (components_->contains(slot.record->fragment)) bind_fragment(*slot.record);
        ++diagnostics_.fragment_remaps;
    }
    for (auto& slot : content_slots_) {
        if (!slot.record || &slot.record->range == owner
                || slot.record->range.first < end) continue;
        slot.record->range.first = static_cast<std::uint32_t>(slot.record->range.first + shift);
        if (components_->contains(slot.record->fragment)) publish_content(*slot.record);
        ++diagnostics_.fragment_remaps;
    }
}

bool RetainedSurfaceService::destroy_content_range(RetainedSurfaceId id) {
    ensure_owner_thread();
    // Cleanup is also valid after ComponentHost has removed the fragment.
    if (!id.valid() || !id.content_range || id.index >= content_slots_.size()) return false;
    auto& slot = content_slots_[id.index];
    if (!slot.record || slot.generation != id.generation) return false;
    const auto removed = slot.record->range;
    static_cast<void>(composer_->remove_fragment(slot.record->fragment));
    for (const auto effect : slot.record->effects) static_cast<void>(effect_scene_.store().remove(effect));
    static_cast<void>(instances_.replace(removed, {}));
    remap_after_replace(removed, 0, &slot.record->range);
    slot.record.reset();
    if (++slot.generation == 0) slot.generation = 1;
    free_content_slots_.push_back(id.index);
    --live_records_;
    ++diagnostics_.destroys;
    return true;
}

std::size_t RetainedSurfaceService::set_content_range(
    RetainedSurfaceId id,
    runtime::SceneFragmentId fragment,
    std::span<const graphics::QuadInstance> visuals) {
    ensure_owner_thread();
    auto& record = require_content(id);
    validate_content_visuals(visuals);
    if (!components_->contains(fragment)) throw std::invalid_argument("content fragment is stale");
    if (record.fragment != fragment) static_cast<void>(composer_->remove_fragment(record.fragment));
    record.fragment = fragment;
    const auto updates = republish_range(record.range, visuals);
    publish_content(record);
    return updates;
}

std::size_t RetainedSurfaceService::update_content_range(
    RetainedSurfaceId id,
    std::span<const graphics::QuadInstance> visuals) {
    ensure_owner_thread();
    auto& record = require_content(id);
    validate_content_visuals(visuals);
    const auto updates = republish_range(record.range, visuals);
    publish_content(record);
    return updates;
}

std::size_t RetainedSurfaceService::update_content_effects(RetainedSurfaceId id,
    std::span<const graphics::RoundedEffectInstance> effects) {
    ensure_owner_thread();
    auto& record = require_content(id);
    for (const auto& effect : effects) graphics::validate_rounded_effect(effect);
    auto& store = effect_scene_.store();
    std::size_t updates = 0;
    while (record.effects.size() > effects.size()) {
        static_cast<void>(store.remove(record.effects.back()));
        record.effects.pop_back(); ++updates;
    }
    for (std::size_t index = 0; index < effects.size(); ++index) {
        if (index == record.effects.size()) {
            record.effects.push_back(store.add(effects[index])); ++updates;
        } else {
            updates += store.update_geometry(record.effects[index], effects[index].geometry);
            updates += store.update_material(record.effects[index], effects[index].material);
        }
    }
    publish_content(record);
    return updates;
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
    for (auto& slot : content_slots_) {
        if (slot.record && components_->contains(slot.record->fragment)) publish_content(*slot.record);
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
    if (id.content_range) {
        if (!id.valid() || id.index >= content_slots_.size())
            throw std::out_of_range("content range id is stale or unknown");
        const auto& slot = content_slots_[id.index];
        if (!slot.record || slot.generation != id.generation)
            throw std::out_of_range("content range id is stale or unknown");
        return slot.record->range;
    }
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
    if (!id.valid() || id.content_range || id.index >= slots_.size()) {
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
    if (!id.valid() || id.content_range || id.index >= slots_.size()) {
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
    validate_finite_visuals(visuals);
}

void RetainedSurfaceService::validate_content_visuals(
    std::span<const graphics::QuadInstance> visuals) {
    // A content range may legitimately be empty (a text run with no decoration
    // after a reflow), and its count is not bounded by the surface layer limit.
    if (visuals.size() > retained_content_visual_capacity) {
        throw std::invalid_argument(
            "retained content visual count is invalid");
    }
    validate_finite_visuals(visuals);
}

void RetainedSurfaceService::validate_finite_visuals(
    std::span<const graphics::QuadInstance> visuals) {
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
