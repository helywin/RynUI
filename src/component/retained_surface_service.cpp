#include "component/retained_surface_service.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <stdexcept>

namespace ryn::component {

RetainedSurfaceService::RetainedSurfaceService(runtime::ComponentHost& components, runtime::NodeStore& nodes,
                                               ComponentSceneComposer& composer) noexcept
    : components_(&components), nodes_(&nodes), composer_(&composer) {}

void RetainedSurfaceService::reserve(std::size_t surface_capacity, std::size_t visual_capacity) {
    ensure_owner_thread();
    if (visual_capacity > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("retained visual capacity exceeds uint32_t");
    }
    slots_.reserve(surface_capacity);
    free_slots_.reserve(surface_capacity);
    instances_.reserve(visual_capacity, surface_capacity);
}

RetainedSurfaceId RetainedSurfaceService::create(runtime::ComponentId component, runtime::NodeId node,
                                                 runtime::SceneFragmentId fragment,
                                                 std::optional<input::InteractionId> interaction,
                                                 std::span<const graphics::QuadInstance> visuals,
                                                 const RetainedSurfaceEffects& effects) {
    return create_record(component, node, fragment, interaction, visuals, effects);
}

RetainedSurfaceId RetainedSurfaceService::create_surface(runtime::ComponentId component, runtime::NodeId node,
                                                         runtime::SceneFragmentId fragment,
                                                         std::span<const graphics::QuadInstance> visuals,
                                                         const RetainedSurfaceEffects& effects,
                                                         std::optional<input::InteractionId> interaction) {
    return create_record(component, node, fragment, interaction, visuals, effects);
}

RetainedSurfaceId RetainedSurfaceService::create_record(runtime::ComponentId component, runtime::NodeId node,
                                                        runtime::SceneFragmentId fragment,
                                                        std::optional<input::InteractionId> interaction,
                                                        std::span<const graphics::QuadInstance> visuals,
                                                        const RetainedSurfaceEffects& effects) {
    ensure_owner_thread();
    if (!components_->contains(component) || components_->root(component) != node || nodes_->find(node) == nullptr ||
        !components_->contains(fragment)) {
        throw std::invalid_argument("retained surface requires live Component, root Node, and fragment identities");
    }
    validate_visuals(visuals);
    if (effects.shadow_fill_offset > visuals.size()) {
        throw std::invalid_argument("shadow fill offset exceeds surface visuals");
    }
    const auto slot_index = acquire_slot();
    auto& slot = slots_[slot_index];
    const RetainedSurfaceId id{slot_index, slot.generation};
    graphics::QuadInstanceRange range;
    try {
        const auto factor = nodes_->paint_opacity(node);
        had_paint_opacity_ = had_paint_opacity_ || factor != 1;
        std::array<graphics::QuadInstance, retained_surface_visual_capacity> painted;
        std::copy(visuals.begin(), visuals.end(), painted.begin());
        for (auto& visual : std::span{painted}.first(visuals.size())) {
            visual.opacity *= factor;
        }
        auto painted_effects = effects;
        painted_effects.shadow_opacity *= factor;
        painted_effects.focus_opacity *= factor;
        range = instances_.append(std::span{painted}.first(visuals.size()));
        slot.record.emplace(Record{
            id,
            component,
            node,
            fragment,
            interaction,
            range,
            painted_effects,
            {},
            {},
            {},
        });
        std::copy(visuals.begin(), visuals.end(), slot.record->source_visuals.begin());
        slot.record->source_effects = effects;
        slot.record->paint_factor = factor;
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
    for (auto effect : record->clipped_fill_ids) {
        static_cast<void>(effect_scene_.store().remove(effect));
    }

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

std::size_t RetainedSurfaceService::update(RetainedSurfaceId id, std::span<const graphics::QuadInstance> visuals) {
    return update_surface(id, visuals);
}

std::size_t RetainedSurfaceService::update_surface(RetainedSurfaceId id,
                                                   std::span<const graphics::QuadInstance> visuals) {
    ensure_owner_thread();
    auto& record = require(id);
    validate_visuals(visuals);
    if (visuals.size() != record.range.count) {
        throw std::invalid_argument("retained surface update changed its visual layer count; use the "
                                    "content range API for a variable count");
    }

    // Surface updates run on interaction frames and are covered by allocation
    // count assertions, so this path stays on the stack; the fixed layer limit
    // keeps those buffers bounded. Variable counts belong to the content range
    // API below, which is not on the per-interaction path.
    std::array<graphics::QuadMaterial, retained_surface_visual_capacity> materials;
    std::array<graphics::QuadGeometry, retained_surface_visual_capacity> geometry;
    const auto factor = nodes_->paint_opacity(record.node);
    had_paint_opacity_ = had_paint_opacity_ || factor != 1;
    record.paint_pending = record.paint_pending || factor != record.paint_factor;
    for (std::size_t index = 0; index < visuals.size(); ++index) {
        record.source_visuals[index] = visuals[index];
        materials[index] = {visuals[index].color, visuals[index].opacity * factor};
        geometry[index] = {
            visuals[index].bounds,
            visuals[index].corner_radius,
            visuals[index].translation,
        };
    }
    const auto material_updates = instances_.update_material(record.range, std::span{materials}.first(visuals.size()));
    const auto geometry_updates = instances_.update_geometry(record.range, std::span{geometry}.first(visuals.size()));
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
        throw std::length_error("RetainedSurfaceService exhausted content range indices");
    }
    content_slots_.emplace_back();
    return static_cast<std::uint32_t>(content_slots_.size() - 1);
}

RetainedSurfaceService::ContentRecord* RetainedSurfaceService::find_content(RetainedSurfaceId id) noexcept {
    if (!id.valid() || !id.content_range || id.index >= content_slots_.size()) {
        return nullptr;
    }
    auto& slot = content_slots_[id.index];
    if (!slot.record.has_value() || slot.record->id.generation != id.generation ||
        !components_->contains(slot.record->fragment)) {
        return nullptr;
    }
    return &*slot.record;
}

RetainedSurfaceService::ContentRecord& RetainedSurfaceService::require_content(RetainedSurfaceId id) {
    auto* record = find_content(id);
    if (record == nullptr) {
        throw std::out_of_range("content range id is stale or unknown");
    }
    return *record;
}

std::span<const graphics::QuadInstance>
RetainedSurfaceService::paint_content(ContentRecord& record, std::span<const graphics::QuadInstance> visuals) {
    if (visuals.empty()) {
        record.source_visuals.clear();
    } else if (visuals.data() != record.source_visuals.data()) {
        record.source_visuals.assign(visuals.begin(), visuals.end());
    }
    record.painted_visuals = record.source_visuals;
    const auto factor = nodes_->paint_opacity(components_->root(components_->fragment_component(record.fragment)));
    had_paint_opacity_ = had_paint_opacity_ || factor != 1;
    record.paint_pending = record.paint_pending || factor != record.paint_factor;
    for (auto& visual : record.painted_visuals) {
        visual.opacity *= factor;
    }
    return record.painted_visuals;
}

std::size_t RetainedSurfaceService::synchronize_paint_opacity() {
    ensure_owner_thread();
    const bool active = nodes_->has_paint_opacity();
    if (!active && !had_paint_opacity_) {
        return 0;
    }
    had_paint_opacity_ = active;
    std::size_t updates{};
    for (auto& slot : slots_) {
        if (!slot.record || !components_->contains(slot.record->component)) {
            continue;
        }
        auto& record = *slot.record;
        const auto factor = nodes_->paint_opacity(record.node);
        if (factor == record.paint_factor && !record.paint_pending) {
            continue;
        }
        updates += update_surface(
            record.id, std::span<const graphics::QuadInstance>{record.source_visuals}.first(record.range.count));
        const auto source = record.source_effects;
        updates += update_effects(record.id, source);
        record.paint_factor = factor;
        record.paint_pending = false;
    }
    for (auto& slot : content_slots_) {
        if (!slot.record || !components_->contains(slot.record->fragment)) {
            continue;
        }
        auto& record = *slot.record;
        const auto factor = nodes_->paint_opacity(components_->root(components_->fragment_component(record.fragment)));
        if (factor == record.paint_factor && !record.paint_pending) {
            continue;
        }
        updates += update_content_range(record.id, record.source_visuals);
        updates += update_content_effects(record.id, record.source_effects);
        record.paint_factor = factor;
        record.paint_pending = false;
    }
    return updates;
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
    if (record.range.count && record.clipped_fill_ids.empty()) {
        commands.push_back(fill);
    }
    for (const auto effect : record.clipped_fill_ids) {
        if (const auto packed = effect_scene_.store().packed_index(effect)) {
            commands.push_back(
                {graphics::SceneDrawKind::rounded_effect, *packed, 1, graphics::invalid_glyph_atlas_page});
        }
    }
    for (const auto effect : record.effects) {
        if (const auto packed = effect_scene_.store().packed_index(effect)) {
            commands.push_back(
                {graphics::SceneDrawKind::rounded_effect, *packed, 1, graphics::invalid_glyph_atlas_page});
        }
    }
    composer_->set_fragment(record.fragment, commands);
}

std::size_t RetainedSurfaceService::republish_range(graphics::QuadInstanceRange& range,
                                                    std::span<const graphics::QuadInstance> visuals) {
    if (range.count == visuals.size()) {
        std::size_t updates = 0;
        for (std::uint32_t index = 0; index < range.count; ++index) {
            const auto& visual = visuals[index];
            const graphics::QuadMaterial material{visual.color, visual.opacity};
            const graphics::QuadGeometry geometry{visual.bounds, visual.corner_radius, visual.translation};
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
    if (range.count == 0) {
        range.first = static_cast<std::uint32_t>(instances_.size());
    }
    const auto old = range;
    const auto replaced = instances_.replace(range, visuals);
    remap_after_replace(old, replaced.count, &range);
    ++diagnostics_.fragment_remaps;
    range = replaced;
    return range.count;
}

RetainedSurfaceId RetainedSurfaceService::create_content_range(runtime::SceneFragmentId fragment,
                                                               std::span<const graphics::QuadInstance> visuals) {
    ensure_owner_thread();
    validate_content_visuals(visuals);
    if (!components_->contains(fragment)) {
        throw std::invalid_argument("content fragment is stale");
    }
    const auto slot_index = acquire_content_slot();
    auto& slot = content_slots_[slot_index];
    const RetainedSurfaceId id{slot_index, slot.generation, true};
    try {
        slot.record.emplace(ContentRecord{id, fragment, {}});
        slot.record->range = instances_.append(paint_content(*slot.record, visuals));
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

void RetainedSurfaceService::remap_after_replace(graphics::QuadInstanceRange old_range, std::uint32_t new_count,
                                                 const graphics::QuadInstanceRange* owner) {
    const auto end = old_range.first + old_range.count;
    const auto shift = static_cast<std::int64_t>(new_count) - old_range.count;
    if (shift == 0) {
        return;
    }
    for (auto& slot : slots_) {
        if (!slot.record || &slot.record->range == owner || slot.record->range.first < end) {
            continue;
        }
        slot.record->range.first = static_cast<std::uint32_t>(slot.record->range.first + shift);
        if (components_->contains(slot.record->fragment)) {
            bind_fragment(*slot.record);
        }
        ++diagnostics_.fragment_remaps;
    }
    for (auto& slot : content_slots_) {
        if (!slot.record || &slot.record->range == owner || slot.record->range.first < end) {
            continue;
        }
        slot.record->range.first = static_cast<std::uint32_t>(slot.record->range.first + shift);
        if (components_->contains(slot.record->fragment)) {
            publish_content(*slot.record);
        }
        ++diagnostics_.fragment_remaps;
    }
}

bool RetainedSurfaceService::destroy_content_range(RetainedSurfaceId id) {
    ensure_owner_thread();
    // Cleanup is also valid after ComponentHost has removed the fragment.
    if (!id.valid() || !id.content_range || id.index >= content_slots_.size()) {
        return false;
    }
    auto& slot = content_slots_[id.index];
    if (!slot.record || slot.generation != id.generation) {
        return false;
    }
    const auto removed = slot.record->range;
    static_cast<void>(composer_->remove_fragment(slot.record->fragment));
    for (const auto effect : slot.record->effects) {
        static_cast<void>(effect_scene_.store().remove(effect));
    }
    for (const auto effect : slot.record->clipped_fill_ids) {
        static_cast<void>(effect_scene_.store().remove(effect));
    }
    static_cast<void>(instances_.replace(removed, {}));
    remap_after_replace(removed, 0, &slot.record->range);
    slot.record.reset();
    if (++slot.generation == 0) {
        slot.generation = 1;
    }
    free_content_slots_.push_back(id.index);
    --live_records_;
    ++diagnostics_.destroys;
    return true;
}

std::size_t RetainedSurfaceService::set_content_range(RetainedSurfaceId id, runtime::SceneFragmentId fragment,
                                                      std::span<const graphics::QuadInstance> visuals) {
    ensure_owner_thread();
    auto& record = require_content(id);
    validate_content_visuals(visuals);
    if (!components_->contains(fragment)) {
        throw std::invalid_argument("content fragment is stale");
    }
    if (record.fragment != fragment) {
        static_cast<void>(composer_->remove_fragment(record.fragment));
    }
    record.fragment = fragment;
    const auto updates = republish_range(record.range, paint_content(record, visuals));
    publish_content(record);
    return updates;
}

std::size_t RetainedSurfaceService::update_content_range(RetainedSurfaceId id,
                                                         std::span<const graphics::QuadInstance> visuals) {
    ensure_owner_thread();
    auto& record = require_content(id);
    validate_content_visuals(visuals);
    const auto old_range = record.range;
    const auto updates = republish_range(record.range, paint_content(record, visuals));
    if (old_range != record.range) {
        publish_content(record);
    }
    return updates;
}

std::size_t RetainedSurfaceService::update_content_effects(RetainedSurfaceId id,
                                                           std::span<const graphics::RoundedEffectInstance> effects) {
    ensure_owner_thread();
    auto& record = require_content(id);
    for (const auto& effect : effects) {
        graphics::validate_rounded_effect(effect);
    }
    if (effects.empty()) {
        record.source_effects.clear();
    } else if (effects.data() != record.source_effects.data()) {
        record.source_effects.assign(effects.begin(), effects.end());
    }
    record.painted_effects = record.source_effects;
    const auto factor = nodes_->paint_opacity(components_->root(components_->fragment_component(record.fragment)));
    had_paint_opacity_ = had_paint_opacity_ || factor != 1;
    record.paint_pending = record.paint_pending || factor != record.paint_factor;
    for (auto& effect : record.painted_effects) {
        effect.material.opacity *= factor;
    }
    effects = std::span<const graphics::RoundedEffectInstance>{record.painted_effects};
    for (const auto& effect : effects) {
        graphics::validate_rounded_effect(effect);
    }
    auto& store = effect_scene_.store();
    std::size_t updates = 0;
    while (record.effects.size() > effects.size()) {
        static_cast<void>(store.remove(record.effects.back()));
        record.effects.pop_back();
        ++updates;
    }
    for (std::size_t index = 0; index < effects.size(); ++index) {
        if (index == record.effects.size()) {
            record.effects.push_back(store.add(effects[index]));
            ++updates;
        } else {
            updates += store.update_geometry(record.effects[index], effects[index].geometry);
            updates += store.update_material(record.effects[index], effects[index].material);
        }
    }
    publish_content(record);
    return updates;
}

std::size_t RetainedSurfaceService::update_effects(RetainedSurfaceId id, const RetainedSurfaceEffects& source) {
    ensure_owner_thread();
    auto& record = require(id);
    if (source.shadow_fill_offset > record.range.count) {
        throw std::invalid_argument("shadow fill offset exceeds surface visuals");
    }
    auto effects = source;
    record.source_effects = effects;
    const auto factor = nodes_->paint_opacity(record.node);
    had_paint_opacity_ = had_paint_opacity_ || factor != 1;
    record.paint_pending = record.paint_pending || factor != record.paint_factor;
    effects.shadow_opacity *= factor;
    effects.focus_opacity *= factor;
    if (effects.shadow_fill_offset > record.range.count) {
        throw std::invalid_argument("shadow fill offset exceeds surface visuals");
    }
    if (record.effects == effects) {
        return 0;
    }

    const std::size_t shadow_stride = effects.rounded_corners ? 4 : 1;
    bool topology_changed = record.shadow_ids.size() != effects.shadows.size() * shadow_stride ||
                            record.focus_id.valid() != effects.focus_enabled ||
                            record.effects.rounded_corners.has_value() != effects.rounded_corners.has_value() ||
                            record.effects.shadow_fill_offset != effects.shadow_fill_offset;
    if (!topology_changed) {
        for (std::size_t index = 0; index < record.shadow_ids.size(); ++index) {
            const auto expected = effects.shadows[index / shadow_stride].kind == ShadowKind::outer
                                      ? graphics::RoundedEffectKind::outer_shadow
                                      : graphics::RoundedEffectKind::inset_shadow;
            if (effect_scene_.store().at(record.shadow_ids[index]).geometry.kind != expected) {
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
        return effects.shadows.size() * shadow_stride +
               (effects.focus_enabled ? effects.rounded_corners ? 4U : 1U : 0U);
    }

    const auto updates = synchronize_effects(record, effects);
    record.effects = effects;
    return updates;
}

std::size_t RetainedSurfaceService::synchronize_effects(Record& record, const RetainedSurfaceEffects& effects) {
    const std::size_t shadow_stride = effects.rounded_corners ? 4 : 1;
    std::size_t updates = 0;
    for (std::size_t index = 0; index < record.shadow_ids.size(); ++index) {
        const auto shape = effects.shadow_shape.value_or(effects.shape);
        const auto& layer = effects.shadows[index / shadow_stride];
        auto candidate =
            effects.rounded_corners
                ? graphics::make_corner_shadow_effects(shape, *effects.rounded_corners, layer, effects.translation,
                                                       effects.ancestor_clip)[index % shadow_stride]
                : graphics::make_shadow_effect(shape, layer, effects.translation, effects.ancestor_clip);
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
        const auto publish = [&](graphics::RoundedEffectId id, const graphics::RoundedEffectInstance& focus) {
            if (effect_scene_.store().update_geometry(id, focus.geometry)) {
                ++diagnostics_.effect_geometry_updates;
                ++updates;
            }
            if (effect_scene_.store().update_material(id, focus.material)) {
                ++diagnostics_.effect_material_updates;
                ++updates;
            }
        };
        if (effects.rounded_corners) {
            const auto corners = graphics::make_corner_outline_effects(
                effects.shape, *effects.rounded_corners, effects.focus_width, effects.focus_offset, effects.focus_color,
                effects.focus_opacity, effects.translation, effects.ancestor_clip);
            for (std::size_t i = 0; i < corners.size(); ++i) {
                publish(record.corner_focus_ids[i], corners[i]);
            }
        } else {
            publish(record.focus_id, graphics::make_outline_effect(
                                         effects.shape, effects.focus_width, effects.focus_offset, effects.focus_color,
                                         effects.focus_opacity, effects.translation, effects.ancestor_clip));
        }
    }
    return updates;
}

bool RetainedSurfaceService::synchronize_clipped_fills(runtime::Rect window_clip, runtime::Rect document_clip,
                                                       std::optional<float> logical_antialias_guard) {
    bool topology_changed = false;
    const auto ancestor_clip = [&](runtime::ComponentId component, const runtime::Node& node,
                                   runtime::ClipPurpose purpose) {
        const auto base = components_->in_window_layer(component) ? window_clip : document_clip;
        return node.clip_boundary || !node.parent ? base : nodes_->content_clip(*node.parent, base, purpose);
    };
    const auto needs_clip = [&](graphics::QuadInstanceRange range, runtime::Rect clip, bool window_layer) {
        // The GPU viewport already clips the complete window. Keep fully
        // contained fills on the compact Quad path; only crossing a stricter
        // content clip needs the clip-capable effect representation.
        if (clip == window_clip) {
            return false;
        }
        for (std::uint32_t i = 0; i < range.count; ++i) {
            const auto& q = instances_.at(range.first + i);
            if (q.bounds[2] <= 0 || q.bounds[3] <= 0) {
                continue;
            }
            // Keep a window layer's existing clip representation stable during
            // scrolls and opacity transitions, including fully contained dots.
            // Material-only changes must not tear down the retained draw range.
            if (window_layer || !logical_antialias_guard) {
                // Without device density, containment of the AA support domain
                // cannot be proved; conversion preserves the hard clip.
                return true;
            }
            const auto guard = *logical_antialias_guard;
            const auto left = q.bounds[0] + q.translation[0] - guard;
            const auto top = q.bounds[1] + q.translation[1] - guard;
            if (left < clip.x || top < clip.y || left + q.bounds[2] + 2 * guard > clip.x + clip.width ||
                top + q.bounds[3] + 2 * guard > clip.y + clip.height) {
                return true;
            }
        }
        return false;
    };
    const auto clear_fills = [&](auto& record) {
        if (record.clipped_fill_ids.empty()) {
            return false;
        }
        for (auto effect : record.clipped_fill_ids) {
            static_cast<void>(effect_scene_.store().remove(effect));
        }
        record.clipped_fill_ids.clear();
        topology_changed = true;
        return true;
    };
    const auto synchronize_fills = [&](auto& record, runtime::Rect clip) {
        const auto count = static_cast<std::size_t>(record.range.count);
        while (record.clipped_fill_ids.size() > count) {
            static_cast<void>(effect_scene_.store().remove(record.clipped_fill_ids.back()));
            record.clipped_fill_ids.pop_back();
            topology_changed = true;
        }
        record.clipped_fill_ids.reserve(count);
        for (std::uint32_t i = 0; i < record.range.count; ++i) {
            const auto& q = instances_.at(record.range.first + i);
            const auto radius = std::min(q.corner_radius, std::min(q.bounds[2], q.bounds[3]) / 2);
            auto fill = graphics::make_shadow_effect(
                {{q.bounds[0], q.bounds[1], q.bounds[2], q.bounds[3]}, radius},
                ShadowLayer{ShadowKind::outer, {}, 0, 0, Color{q.color[0], q.color[1], q.color[2], q.color[3]}},
                {q.translation[0], q.translation[1]}, graphics::EffectClip{1, clip});
            fill.material.opacity = q.opacity;
            if (i == record.clipped_fill_ids.size()) {
                record.clipped_fill_ids.push_back(effect_scene_.store().add(fill));
                topology_changed = true;
            } else {
                const auto id = record.clipped_fill_ids[i];
                diagnostics_.effect_geometry_updates += effect_scene_.store().update_geometry(id, fill.geometry);
                diagnostics_.effect_material_updates += effect_scene_.store().update_material(id, fill.material);
            }
        }
    };
    for (auto& slot : slots_) {
        if (!slot.record) {
            continue;
        }
        auto& record = *slot.record;
        if (!components_->branch_active(record.component)) {
            static_cast<void>(clear_fills(record));
            if (record.inherited_effect_clip) {
                static_cast<void>(synchronize_effects(record, record.effects));
                record.inherited_effect_clip = false;
            }
            continue;
        }
        const auto& node = nodes_->require(record.node);
        const auto clip = ancestor_clip(record.component, node, runtime::ClipPurpose::content);
        auto effects = record.effects;
        const auto decoration_clip = ancestor_clip(record.component, node, runtime::ClipPurpose::decoration);
        if (decoration_clip != window_clip) {
            if (effects.ancestor_clip) {
                effects.ancestor_clip->bounds =
                    graphics::intersect_effect_bounds(decoration_clip, effects.ancestor_clip->bounds);
            } else {
                effects.ancestor_clip = graphics::EffectClip{1, decoration_clip};
            }
        }
        const bool inherited_clip = effects.ancestor_clip != record.effects.ancestor_clip;
        if (inherited_clip || record.inherited_effect_clip) {
            static_cast<void>(synchronize_effects(record, effects));
        }
        record.inherited_effect_clip = inherited_clip;
        if (!needs_clip(record.range, clip, components_->in_window_layer(record.component))) {
            static_cast<void>(clear_fills(record));
            continue;
        }
        synchronize_fills(record, clip);
    }
    for (auto& slot : content_slots_) {
        if (!slot.record || !components_->contains(slot.record->fragment)) {
            continue;
        }
        auto& record = *slot.record;
        const auto component = components_->fragment_component(record.fragment);
        if (!components_->branch_active(component)) {
            static_cast<void>(clear_fills(record));
            continue;
        }
        const auto& node = nodes_->require(components_->root(component));
        const auto clip = ancestor_clip(component, node, runtime::ClipPurpose::content);
        if (!needs_clip(record.range, clip, components_->in_window_layer(component))) {
            static_cast<void>(clear_fills(record));
            continue;
        }
        synchronize_fills(record, clip);
    }
    return topology_changed;
}

bool RetainedSurfaceService::compact_effects(runtime::Rect window_clip, std::optional<runtime::Rect> document_clip,
                                             std::optional<float> logical_antialias_guard) {
    ensure_owner_thread();
    if (logical_antialias_guard && (!std::isfinite(*logical_antialias_guard) || *logical_antialias_guard <= 0)) {
        throw std::invalid_argument("logical antialias guard must be finite and positive");
    }
    // Visit previous conversions after clipping or layer ownership changes too.
    const bool fills_changed =
        synchronize_clipped_fills(window_clip, document_clip.value_or(window_clip), logical_antialias_guard);
    const bool packed_changed =
        effect_scene_.store().compact(window_clip, std::max(1.0, double(logical_antialias_guard.value_or(1.0F))));
    if (!fills_changed && !packed_changed) {
        return false;
    }
    for (const auto& slot : slots_) {
        if (slot.record.has_value()) {
            bind_fragment(*slot.record);
        }
    }
    for (auto& slot : content_slots_) {
        if (slot.record && components_->contains(slot.record->fragment)) {
            publish_content(*slot.record);
        }
    }
    return true;
}

std::optional<RetainedSurfacePaintSnapshot> RetainedSurfaceService::paint_snapshot(RetainedSurfaceId id) const {
    ensure_owner_thread();
    RetainedSurfacePaintSnapshot result;
    result.id = id;
    const auto append_effects = [&](auto ids, auto& destination) {
        for (const auto effect : ids) {
            if (effect_scene_.store().contains(effect)) {
                destination.push_back(
                    {effect, effect_scene_.store().packed_index(effect), effect_scene_.store().at(effect)});
            }
        }
    };
    if (id.content_range) {
        if (!id.valid() || id.index >= content_slots_.size()) {
            return std::nullopt;
        }
        const auto& slot = content_slots_[id.index];
        if (!slot.record || slot.record->id != id || !components_->contains(slot.record->fragment)) {
            return std::nullopt;
        }
        const auto& record = *slot.record;
        result.fragment = record.fragment;
        result.owner = components_->fragment_component(record.fragment);
        result.node = components_->root(result.owner);
        result.range = record.range;
        result.paint_pending = record.paint_pending;
        append_effects(record.effects, result.content_effects);
        append_effects(record.clipped_fill_ids, result.clipped_fills);
    } else {
        const auto* record = find(id);
        if (!record) {
            return std::nullopt;
        }
        result.owner = record->component;
        result.node = record->node;
        result.fragment = record->fragment;
        result.range = record->range;
        result.paint_pending = record->paint_pending;
        append_effects(record->shadow_ids, result.shadows);
        if (record->focus_id.valid()) {
            append_effects(std::span{&record->focus_id, 1}, result.focus);
        }
        append_effects(record->corner_focus_ids, result.focus);
        append_effects(record->clipped_fill_ids, result.clipped_fills);
    }
    for (std::uint32_t offset = 0; offset < result.range.count; ++offset) {
        result.quads.push_back(instances_.at(result.range.first + offset));
    }
    result.binding = composer_->paint_snapshot(result.fragment);
    return result;
}

graphics::QuadInstanceRange RetainedSurfaceService::visual_range(RetainedSurfaceId id) const {
    ensure_owner_thread();
    if (id.content_range) {
        if (!id.valid() || id.index >= content_slots_.size()) {
            throw std::out_of_range("content range id is stale or unknown");
        }
        const auto& slot = content_slots_[id.index];
        if (!slot.record || slot.generation != id.generation) {
            throw std::out_of_range("content range id is stale or unknown");
        }
        return slot.record->range;
    }
    return require(id).range;
}

const graphics::RoundedEffectInstance& RetainedSurfaceService::focus_effect(RetainedSurfaceId id) const {
    ensure_owner_thread();
    const auto& record = require(id);
    return effect_scene_.store().at(record.focus_id);
}

std::span<const graphics::RoundedEffectId> RetainedSurfaceService::shadow_effects(RetainedSurfaceId id) const {
    ensure_owner_thread();
    return require(id).shadow_ids;
}

graphics::QuadInstanceStore& RetainedSurfaceService::instances() noexcept {
    return instances_;
}

const graphics::QuadInstanceStore& RetainedSurfaceService::instances() const noexcept {
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

const RetainedSurfaceDiagnostics& RetainedSurfaceService::diagnostics() const noexcept {
    return diagnostics_;
}

RetainedSurfaceService::Record* RetainedSurfaceService::find(RetainedSurfaceId id) noexcept {
    if (!id.valid() || id.content_range || id.index >= slots_.size()) {
        return nullptr;
    }
    auto& slot = slots_[id.index];
    if (slot.generation != id.generation || !slot.record.has_value()) {
        return nullptr;
    }
    auto& record = *slot.record;
    return components_->contains(record.component) && nodes_->find(record.node) != nullptr &&
                   components_->contains(record.fragment)
               ? &record
               : nullptr;
}

const RetainedSurfaceService::Record* RetainedSurfaceService::find(RetainedSurfaceId id) const noexcept {
    if (!id.valid() || id.content_range || id.index >= slots_.size()) {
        return nullptr;
    }
    const auto& slot = slots_[id.index];
    if (slot.generation != id.generation || !slot.record.has_value()) {
        return nullptr;
    }
    const auto& record = *slot.record;
    return components_->contains(record.component) && nodes_->find(record.node) != nullptr &&
                   components_->contains(record.fragment)
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

const RetainedSurfaceService::Record& RetainedSurfaceService::require(RetainedSurfaceId id) const {
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
    const auto offset = record.effects.shadow_fill_offset;
    if (!record.clipped_fill_ids.empty()) {
        std::vector<graphics::SceneDrawCommand> commands;
        commands.reserve(record.clipped_fill_ids.size() + record.shadow_ids.size() + 4);
        const auto append = [&](std::span<const graphics::RoundedEffectId> ids) {
            for (auto id : ids) {
                if (const auto index = effect_scene_.store().packed_index(id)) {
                    commands.push_back(
                        {graphics::SceneDrawKind::rounded_effect, *index, 1, graphics::invalid_glyph_atlas_page});
                }
            }
        };
        const auto fills = std::span{record.clipped_fill_ids};
        append(fills.first(offset));
        append(record.effect_primitive.before_fill);
        append(fills.subspan(offset));
        append(record.effect_primitive.after_fill);
        composer_->set_fragment(record.fragment, commands, record.interaction);
        return;
    }
    const graphics::SceneDrawCommand fill{
        graphics::SceneDrawKind::quad,
        record.range.first + offset,
        record.range.count - offset,
        graphics::invalid_glyph_atlas_page,
    };
    std::vector<graphics::SceneDrawCommand> commands;
    commands.reserve(record.shadow_ids.size() + 2);
    if (offset > 0) {
        commands.push_back(
            {graphics::SceneDrawKind::quad, record.range.first, offset, graphics::invalid_glyph_atlas_page});
    }
    effect_scene_.compose_surface(record.effect_primitive, fill, commands);
    composer_->set_fragment(record.fragment, commands, record.interaction);
}

void RetainedSurfaceService::create_effects(Record& record) {
    record.shadow_ids.clear();
    record.effect_primitive = {};
    const std::size_t shadow_stride = record.effects.rounded_corners ? 4 : 1;
    record.shadow_ids.reserve(record.effects.shadows.size() * shadow_stride);
    record.effect_primitive.before_fill.reserve(record.effects.shadows.size() * shadow_stride +
                                                (record.effects.rounded_corners ? 4 : 1));
    record.effect_primitive.after_fill.reserve(record.effects.shadows.size() * shadow_stride);
    try {
        for (const auto& layer : record.effects.shadows.layers()) {
            const auto shape = record.effects.shadow_shape.value_or(record.effects.shape);
            const auto append = [&](graphics::RoundedEffectInstance instance) {
                instance.material.opacity = record.effects.shadow_opacity;
                const auto id = effect_scene_.store().add(std::move(instance));
                record.shadow_ids.push_back(id);
                if (layer.kind == ShadowKind::outer) {
                    record.effect_primitive.before_fill.push_back(id);
                } else {
                    record.effect_primitive.after_fill.push_back(id);
                }
            };
            if (record.effects.rounded_corners) {
                for (const auto& instance :
                     graphics::make_corner_shadow_effects(shape, *record.effects.rounded_corners, layer,
                                                          record.effects.translation, record.effects.ancestor_clip)) {
                    append(instance);
                }
            } else {
                append(graphics::make_shadow_effect(shape, layer, record.effects.translation,
                                                    record.effects.ancestor_clip));
            }
        }
        if (record.effects.focus_enabled) {
            if (record.effects.rounded_corners) {
                const auto corners = graphics::make_corner_outline_effects(
                    record.effects.shape, *record.effects.rounded_corners, record.effects.focus_width,
                    record.effects.focus_offset, record.effects.focus_color, record.effects.focus_opacity,
                    record.effects.translation, record.effects.ancestor_clip);
                record.corner_focus_ids.reserve(4);
                for (const auto& corner : corners) {
                    const auto id = effect_scene_.store().add(corner);
                    record.corner_focus_ids.push_back(id);
                    record.effect_primitive.before_fill.push_back(id);
                }
                record.focus_id = record.corner_focus_ids.front();
            } else {
                auto outline = graphics::make_outline_effect(record.effects.shape, record.effects.focus_width,
                                                             record.effects.focus_offset, record.effects.focus_color,
                                                             record.effects.focus_opacity, record.effects.translation,
                                                             record.effects.ancestor_clip);
                record.focus_id = effect_scene_.store().add(std::move(outline));
                record.effect_primitive.before_fill.push_back(record.focus_id);
            }
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
    record.corner_focus_ids.clear();
    record.effect_primitive = {};
}

void RetainedSurfaceService::ensure_owner_thread() const {
    if (!components_->is_owner_thread()) {
        throw std::logic_error("RetainedSurfaceService can only be used on its owner thread");
    }
}

void RetainedSurfaceService::validate_visuals(std::span<const graphics::QuadInstance> visuals) {
    if (visuals.empty() || visuals.size() > retained_surface_visual_capacity) {
        throw std::invalid_argument("retained surface visual layer count is invalid");
    }
    validate_finite_visuals(visuals);
}

void RetainedSurfaceService::validate_content_visuals(std::span<const graphics::QuadInstance> visuals) {
    // A content range may legitimately be empty (a text run with no decoration
    // after a reflow), and its count is not bounded by the surface layer limit.
    if (visuals.size() > retained_content_visual_capacity) {
        throw std::invalid_argument("retained content visual count is invalid");
    }
    validate_finite_visuals(visuals);
}

void RetainedSurfaceService::validate_finite_visuals(std::span<const graphics::QuadInstance> visuals) {
    for (const auto& visual : visuals) {
        const bool finite_clip = std::ranges::all_of(visual.bounds, [](float value) { return std::isfinite(value); });
        const bool finite_color = std::ranges::all_of(visual.color, [](float value) { return std::isfinite(value); });
        const bool finite_translation =
            std::ranges::all_of(visual.translation, [](float value) { return std::isfinite(value); });
        if (!finite_clip || !finite_color || !finite_translation || !std::isfinite(visual.opacity) ||
            !std::isfinite(visual.corner_radius) || visual.opacity < 0.0F || visual.opacity > 1.0F ||
            visual.corner_radius < 0.0F || visual.bounds[2] < 0.0F || visual.bounds[3] < 0.0F) {
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
