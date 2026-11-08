#pragma once

#include "graphics/glyph_scene.hpp"
#include "graphics/scene_composition.hpp"
#include "input/interaction_registry.hpp"
#include "runtime/component_host.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ryn::component {

struct ComponentSceneDiagnostics final {
    std::uint64_t rebuilds{0};
    std::uint64_t fragments_emitted{0};
    std::uint64_t commands_emitted{0};
    std::uint64_t interactions_emitted{0};
    std::uint64_t stale_bindings_skipped{0};
};

struct VisibleSceneStats final {
    std::uint64_t fragments_considered{};
    std::uint64_t fragments_visible{};
};

class ComponentSceneComposer final {
public:
    ComponentSceneComposer(runtime::ComponentHost& components, input::InteractionRegistry& interactions,
                           input::HitTestSnapshot& hit_test) noexcept;

    void reserve(std::size_t fragment_capacity, std::size_t command_capacity, std::size_t interaction_capacity);
    void set_fragment(runtime::SceneFragmentId fragment, std::span<const graphics::SceneDrawCommand> commands,
                      std::optional<input::InteractionId> interaction = std::nullopt,
                      std::optional<runtime::Rect> interaction_clip = std::nullopt);
    bool remove_fragment(runtime::SceneFragmentId fragment);
    // Self shadow excludes only its own group; ancestor masks remain authoritative.
    void set_fragment_composition(runtime::SceneFragmentId fragment, bool outside_own_mask,
                                  graphics::DrawCoverageRole coverage = graphics::DrawCoverageRole::Normal,
                                  std::optional<graphics::SceneCompositionRequest> backdrop = std::nullopt);
    void rebuild(runtime::Rect window_clip, std::optional<runtime::Rect> document_clip = std::nullopt);

    void invalidate_composition() noexcept {
        bindings_dirty_ = true;
    }

    [[nodiscard]] bool needs_rebuild() const noexcept {
        return bindings_dirty_;
    }

    [[nodiscard]] VisibleSceneStats build_visible_scene(const runtime::NodeStore& nodes, runtime::Rect window_clip,
                                                        graphics::OrderedScene& destination) const;

    [[nodiscard]] const graphics::OrderedScene& ordered_scene() const noexcept;
    [[nodiscard]] std::span<const input::HitTestPaintEntry> interaction_order() const noexcept;
    [[nodiscard]] const ComponentSceneDiagnostics& diagnostics() const noexcept;

private:
    struct FragmentBinding final {
        std::uint32_t generation{0};
        std::vector<graphics::SceneDrawCommand> commands;
        std::optional<input::InteractionId> interaction;
        std::optional<runtime::Rect> interaction_clip;
        bool outside_own_mask{};
        graphics::DrawCoverageRole coverage{};
        std::optional<graphics::SceneCompositionRequest> backdrop;
    };

    [[nodiscard]] FragmentBinding* find_binding(runtime::SceneFragmentId fragment) noexcept;
    [[nodiscard]] const FragmentBinding* find_binding(runtime::SceneFragmentId fragment) const noexcept;
    void ensure_owner_thread() const;

    runtime::ComponentHost* components_;
    input::InteractionRegistry* interactions_;
    input::HitTestSnapshot* hit_test_;
    std::vector<std::optional<FragmentBinding>> bindings_;
    graphics::OrderedScene ordered_scene_;
    std::vector<input::HitTestPaintEntry> interaction_order_;
    ComponentSceneDiagnostics diagnostics_;
    bool bindings_dirty_{};
};

} // namespace ryn::component
