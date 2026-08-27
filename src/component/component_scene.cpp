#include "component/component_scene.hpp"

#include <algorithm>
#include <stdexcept>

namespace ryn::component {

ComponentSceneComposer::ComponentSceneComposer(
    runtime::ComponentHost& components,
    input::InteractionRegistry& interactions,
    input::HitTestSnapshot& hit_test) noexcept
    : components_(&components),
      interactions_(&interactions),
      hit_test_(&hit_test) {}

void ComponentSceneComposer::reserve(
    std::size_t fragment_capacity,
    std::size_t command_capacity,
    std::size_t interaction_capacity) {
    ensure_owner_thread();
    bindings_.reserve(fragment_capacity);
    ordered_scene_.reserve(command_capacity);
    interaction_order_.reserve(interaction_capacity);
}

void ComponentSceneComposer::set_fragment(
    runtime::SceneFragmentId fragment,
    std::span<const graphics::SceneDrawCommand> commands,
    std::optional<input::InteractionId> interaction,
    std::optional<runtime::Rect> interaction_clip) {
    ensure_owner_thread();
    if (!components_->contains(fragment)) {
        throw std::invalid_argument("Scene fragment binding requires a live fragment");
    }
    if (interaction.has_value()) {
        const auto* record = interactions_->find(*interaction);
        const auto traversal = components_->paint_traversal();
        const auto owner = std::find_if(
            traversal.begin(),
            traversal.end(),
            [&](const auto& entry) { return entry.fragment == fragment; });
        if (record == nullptr || owner == traversal.end()
                || record->component != owner->component) {
            throw std::invalid_argument(
                "Scene fragment interaction must belong to the same component");
        }
    }
    if (bindings_.size() <= fragment.index) {
        bindings_.resize(static_cast<std::size_t>(fragment.index) + 1);
    }
    FragmentBinding replacement;
    replacement.generation = fragment.generation;
    replacement.commands.assign(commands.begin(), commands.end());
    replacement.interaction = interaction;
    replacement.interaction_clip = interaction_clip;
    bindings_[fragment.index] = std::move(replacement);
}

bool ComponentSceneComposer::remove_fragment(
    runtime::SceneFragmentId fragment) {
    ensure_owner_thread();
    auto* binding = find_binding(fragment);
    if (binding == nullptr) {
        return false;
    }
    bindings_[fragment.index].reset();
    return true;
}

void ComponentSceneComposer::rebuild(runtime::Rect window_clip) {
    ensure_owner_thread();
    ordered_scene_.clear();
    interaction_order_.clear();
    for (const auto& entry : components_->paint_traversal()) {
        const auto* binding = find_binding(entry.fragment);
        if (binding == nullptr) {
            continue;
        }
        if (!components_->contains(entry.fragment)) {
            ++diagnostics_.stale_bindings_skipped;
            continue;
        }
        ++diagnostics_.fragments_emitted;
        for (const auto command : binding->commands) {
            ordered_scene_.append_command(command);
            if (command.instance_count != 0) {
                ++diagnostics_.commands_emitted;
            }
        }
        if (!binding->interaction.has_value()) {
            continue;
        }
        const auto* interaction = interactions_->find(*binding->interaction);
        if (interaction == nullptr || interaction->component != entry.component) {
            ++diagnostics_.stale_bindings_skipped;
            continue;
        }
        if (std::find_if(
                interaction_order_.begin(),
                interaction_order_.end(),
                [&](const auto& paint) {
                    return paint.interaction == *binding->interaction;
                }) != interaction_order_.end()) {
            throw std::logic_error(
                "Component paint traversal emitted an interaction more than once");
        }
        interaction_order_.push_back({
            *binding->interaction,
            binding->interaction_clip,
        });
        ++diagnostics_.interactions_emitted;
    }
    hit_test_->rebuild(interaction_order_, window_clip);
    ++diagnostics_.rebuilds;
}

const graphics::OrderedScene&
ComponentSceneComposer::ordered_scene() const noexcept {
    return ordered_scene_;
}

std::span<const input::HitTestPaintEntry>
ComponentSceneComposer::interaction_order() const noexcept {
    return interaction_order_;
}

const ComponentSceneDiagnostics&
ComponentSceneComposer::diagnostics() const noexcept {
    return diagnostics_;
}

ComponentSceneComposer::FragmentBinding*
ComponentSceneComposer::find_binding(
    runtime::SceneFragmentId fragment) noexcept {
    if (!fragment.valid() || fragment.index >= bindings_.size()) {
        return nullptr;
    }
    auto& binding = bindings_[fragment.index];
    return binding.has_value() && binding->generation == fragment.generation
        ? &*binding
        : nullptr;
}

const ComponentSceneComposer::FragmentBinding*
ComponentSceneComposer::find_binding(
    runtime::SceneFragmentId fragment) const noexcept {
    if (!fragment.valid() || fragment.index >= bindings_.size()) {
        return nullptr;
    }
    const auto& binding = bindings_[fragment.index];
    return binding.has_value() && binding->generation == fragment.generation
        ? &*binding
        : nullptr;
}

void ComponentSceneComposer::ensure_owner_thread() const {
    if (!components_->is_owner_thread()) {
        throw std::logic_error(
            "ComponentSceneComposer can only be used on its owner thread");
    }
}

} // namespace ryn::component
