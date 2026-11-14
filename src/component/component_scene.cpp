#include "component/component_scene.hpp"

#include <algorithm>
#include <stdexcept>

namespace ryn::component {

ComponentSceneComposer::ComponentSceneComposer(runtime::ComponentHost& components,
                                               input::InteractionRegistry& interactions,
                                               input::HitTestSnapshot& hit_test) noexcept
    : components_(&components), interactions_(&interactions), hit_test_(&hit_test) {}

void ComponentSceneComposer::reserve(std::size_t fragment_capacity, std::size_t command_capacity,
                                     std::size_t interaction_capacity) {
    ensure_owner_thread();
    bindings_.reserve(fragment_capacity);
    ordered_scene_.reserve(command_capacity);
    interaction_order_.reserve(interaction_capacity);
}

void ComponentSceneComposer::set_fragment(runtime::SceneFragmentId fragment,
                                          std::span<const graphics::SceneDrawCommand> commands,
                                          std::optional<input::InteractionId> interaction,
                                          std::optional<runtime::Rect> interaction_clip) {
    ensure_owner_thread();
    if (!components_->contains(fragment)) {
        throw std::invalid_argument("Scene fragment binding requires a live fragment");
    }
    if (interaction.has_value()) {
        const auto* record = interactions_->find(*interaction);
        if (record == nullptr || record->component != components_->fragment_component(fragment)) {
            throw std::invalid_argument("Scene fragment interaction must belong to the same component");
        }
    }
    if (bindings_.size() <= fragment.index) {
        bindings_.resize(static_cast<std::size_t>(fragment.index) + 1);
    }
    const auto* previous = find_binding(fragment);
    if (previous && previous->interaction == interaction && previous->interaction_clip == interaction_clip &&
        std::ranges::equal(previous->commands, commands)) {
        return;
    }
    FragmentBinding replacement;
    replacement.generation = fragment.generation;
    replacement.commands.assign(commands.begin(), commands.end());
    replacement.interaction = interaction;
    replacement.interaction_clip = interaction_clip;
    replacement.publication_revision = ++publication_revision_;
    if (previous) {
        replacement.outside_own_mask = previous->outside_own_mask;
        replacement.coverage = previous->coverage;
        replacement.backdrop = previous->backdrop;
    }
    bindings_[fragment.index] = std::move(replacement);
    bindings_dirty_ = true;
}

bool ComponentSceneComposer::remove_fragment(runtime::SceneFragmentId fragment) {
    ensure_owner_thread();
    auto* binding = find_binding(fragment);
    if (binding == nullptr) {
        return false;
    }
    bindings_[fragment.index].reset();
    bindings_dirty_ = true;
    return true;
}

void ComponentSceneComposer::set_fragment_composition(runtime::SceneFragmentId fragment, bool outside,
                                                      graphics::DrawCoverageRole coverage,
                                                      std::optional<graphics::SceneCompositionRequest> backdrop) {
    ensure_owner_thread();
    auto* binding = find_binding(fragment);
    if (!binding || !components_->contains(fragment)) {
        throw std::invalid_argument("Composition requires a bound live fragment");
    }
    if (binding->outside_own_mask == outside && binding->coverage == coverage && binding->backdrop == backdrop) {
        return;
    }
    binding->outside_own_mask = outside;
    binding->coverage = coverage;
    binding->backdrop = std::move(backdrop);
    binding->publication_revision = ++publication_revision_;
    bindings_dirty_ = true;
}

std::optional<FragmentPaintSnapshot> ComponentSceneComposer::paint_snapshot(runtime::SceneFragmentId fragment) const {
    ensure_owner_thread();
    const auto* binding = find_binding(fragment);
    if (!binding || !components_->contains(fragment)) {
        return std::nullopt;
    }
    return FragmentPaintSnapshot{fragment,
                                 components_->fragment_component(fragment),
                                 binding->publication_revision,
                                 binding->commands,
                                 binding->interaction_clip,
                                 binding->outside_own_mask,
                                 binding->coverage,
                                 binding->backdrop};
}

void ComponentSceneComposer::rebuild(runtime::Rect window_clip, std::optional<runtime::Rect> document_clip) {
    ensure_owner_thread();
    graphics::OrderedScene next_scene;
    next_scene.reserve(ordered_scene_.commands().size());
    std::vector<input::HitTestPaintEntry> next_interactions;
    next_interactions.reserve(interaction_order_.capacity());
    const auto& nodes = components_->nodes();
    graphics::SceneCompositionProgram storage;
    auto* program = &storage;
    const auto owners = nodes.live_rounded_content_owners();
    for (const auto owner : owners) {
        auto hard = nodes.content_clip(owner, window_clip);
        bool window_boundary = false;
        for (auto current = std::optional{owner}; current; current = nodes.require(*current).parent) {
            if (nodes.require(*current).clip_boundary) {
                window_boundary = true;
                break;
            }
        }
        if (document_clip && !window_boundary) {
            const float left = std::max(hard.x, document_clip->x);
            const float top = std::max(hard.y, document_clip->y);
            const float right = std::min(hard.x + hard.width, document_clip->x + document_clip->width);
            const float bottom = std::min(hard.y + hard.height, document_clip->y + document_clip->height);
            hard = {left, top, std::max(0.0F, right - left), std::max(0.0F, bottom - top)};
        }
        program->requests.push_back({owner, nodes.rounded_content_clip(owner), hard});
    }
    // A hidden request is still live and must be validated by the renderer.
    for (std::size_t index = 0; index < bindings_.size(); ++index) {
        const auto& binding = bindings_[index];
        if (binding && binding->backdrop &&
            components_->contains(runtime::SceneFragmentId{static_cast<std::uint32_t>(index), binding->generation})) {
            program->requests.push_back(*binding->backdrop);
        }
    }
    std::vector<std::uint32_t> open;
    const auto transition = [&](const std::vector<std::uint32_t>& next) {
        std::size_t common = 0;
        while (common < open.size() && common < next.size() && open[common] == next[common]) {
            ++common;
        }
        while (open.size() > common) {
            program->operations.push_back({graphics::SceneCompositionOpKind::EndMaskedGroup, 0, 0, open.back()});
            open.pop_back();
        }
        for (std::size_t i = common; i < next.size(); ++i) {
            program->operations.push_back({graphics::SceneCompositionOpKind::BeginMaskedGroup, 0, 0, next[i]});
            open.push_back(next[i]);
        }
        if (!program->requests.empty()) {
            next_scene.barrier();
        }
    };
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
        const auto root = components_->root(entry.component);
        auto chain = nodes.rounded_content_owners(root);
        if (binding->outside_own_mask && !chain.empty() && chain.back() == root) {
            chain.pop_back();
        }
        std::vector<std::uint32_t> next;
        for (const auto owner : chain) {
            const auto found = std::find(owners.begin(), owners.end(), owner);
            next.push_back(static_cast<std::uint32_t>(found - owners.begin()));
        }
        transition(next);
        if (binding->backdrop) {
            if (!open.empty()) {
                throw std::invalid_argument("Backdrop must be in the window root target");
            }
            const auto found = std::find(program->requests.begin(), program->requests.end(), *binding->backdrop);
            program->operations.push_back({graphics::SceneCompositionOpKind::Backdrop, 0, 0,
                                           static_cast<std::uint32_t>(found - program->requests.begin())});
            next_scene.barrier();
        }
        for (const auto command : binding->commands) {
            const auto first = static_cast<std::uint32_t>(next_scene.commands().size());
            if (!program->requests.empty()) {
                next_scene.barrier();
            }
            next_scene.append_command(command);
            if (!program->requests.empty() && command.instance_count) {
                program->operations.push_back(
                    {graphics::SceneCompositionOpKind::DrawRange, first, 1, 0, binding->coverage});
            }
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
        if (std::find_if(next_interactions.begin(), next_interactions.end(), [&](const auto& paint) {
                return paint.interaction == *binding->interaction;
            }) != next_interactions.end()) {
            throw std::logic_error("Component paint traversal emitted an interaction more than once");
        }
        auto interaction_clip = binding->interaction_clip;
        if (document_clip && !components_->in_window_layer(entry.component)) {
            if (interaction_clip) {
                const float left = std::max(interaction_clip->x, document_clip->x);
                const float top = std::max(interaction_clip->y, document_clip->y);
                const float right =
                    std::min(interaction_clip->x + interaction_clip->width, document_clip->x + document_clip->width);
                const float bottom =
                    std::min(interaction_clip->y + interaction_clip->height, document_clip->y + document_clip->height);
                interaction_clip = runtime::Rect{left, top, std::max(0.0F, right - left), std::max(0.0F, bottom - top)};
            } else {
                interaction_clip = document_clip;
            }
        }
        next_interactions.push_back({
            *binding->interaction,
            interaction_clip,
        });
        ++diagnostics_.interactions_emitted;
    }
    transition({});
    if (!program->requests.empty()) {
        program->scene_revision = next_scene.revision();
        next_scene.set_composition(std::make_shared<const graphics::SceneCompositionProgram>(std::move(storage)));
    }
    hit_test_->rebuild(next_interactions, window_clip);
    ordered_scene_ = std::move(next_scene);
    interaction_order_ = std::move(next_interactions);
    ++diagnostics_.rebuilds;
    bindings_dirty_ = false;
}

VisibleSceneStats ComponentSceneComposer::build_visible_scene(const runtime::NodeStore& nodes,
                                                              runtime::Rect window_clip,
                                                              graphics::OrderedScene& destination) const {
    ensure_owner_thread();
    if (ordered_scene_.composition()) {
        // Preserve the complete balanced program and actual shadow support.
        // EffectStore already performs its own geometry-aware compaction.
        destination = ordered_scene_;
        return {components_->paint_traversal().size(), components_->paint_traversal().size()};
    }
    constexpr float visual_overflow = 32.0F;
    const float clip_left = window_clip.x - visual_overflow;
    const float clip_top = window_clip.y - visual_overflow;
    const float clip_right = window_clip.x + window_clip.width + visual_overflow;
    const float clip_bottom = window_clip.y + window_clip.height + visual_overflow;
    destination.clear();
    destination.reserve(ordered_scene_.commands().size());
    VisibleSceneStats stats;
    for (const auto& entry : components_->paint_traversal()) {
        const auto* binding = find_binding(entry.fragment);
        if (binding == nullptr || !components_->contains(entry.fragment)) {
            continue;
        }
        ++stats.fragments_considered;
        const auto& node = nodes.require(components_->root(entry.component));
        const float left = node.bounds.x + node.translation.x;
        const float top = node.bounds.y + node.translation.y;
        const bool offscreen =
            !components_->in_window_layer(entry.component) &&
            (node.bounds.width <= 0.0F || node.bounds.height <= 0.0F || left >= clip_right ||
             left + node.bounds.width <= clip_left || top >= clip_bottom || top + node.bounds.height <= clip_top);
        if (!offscreen) {
            ++stats.fragments_visible;
        }
        for (const auto command : binding->commands) {
            // EffectStore has already culled using actual blur/spread bounds;
            // those can extend well beyond the owner's layout rectangle.
            if (!offscreen || command.kind == graphics::SceneDrawKind::rounded_effect) {
                destination.append_command(command);
            }
        }
    }
    return stats;
}

const graphics::OrderedScene& ComponentSceneComposer::ordered_scene() const noexcept {
    return ordered_scene_;
}

std::span<const input::HitTestPaintEntry> ComponentSceneComposer::interaction_order() const noexcept {
    return interaction_order_;
}

const ComponentSceneDiagnostics& ComponentSceneComposer::diagnostics() const noexcept {
    return diagnostics_;
}

ComponentSceneComposer::FragmentBinding*
ComponentSceneComposer::find_binding(runtime::SceneFragmentId fragment) noexcept {
    if (!fragment.valid() || fragment.index >= bindings_.size()) {
        return nullptr;
    }
    auto& binding = bindings_[fragment.index];
    return binding.has_value() && binding->generation == fragment.generation ? &*binding : nullptr;
}

const ComponentSceneComposer::FragmentBinding*
ComponentSceneComposer::find_binding(runtime::SceneFragmentId fragment) const noexcept {
    if (!fragment.valid() || fragment.index >= bindings_.size()) {
        return nullptr;
    }
    const auto& binding = bindings_[fragment.index];
    return binding.has_value() && binding->generation == fragment.generation ? &*binding : nullptr;
}

void ComponentSceneComposer::ensure_owner_thread() const {
    if (!components_->is_owner_thread()) {
        throw std::logic_error("ComponentSceneComposer can only be used on its owner thread");
    }
}

} // namespace ryn::component
