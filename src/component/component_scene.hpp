#pragma once

#include "graphics/glyph_scene.hpp"
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

class ComponentSceneComposer final {
public:
    ComponentSceneComposer(
        runtime::ComponentHost& components,
        input::InteractionRegistry& interactions,
        input::HitTestSnapshot& hit_test) noexcept;

    void reserve(
        std::size_t fragment_capacity,
        std::size_t command_capacity,
        std::size_t interaction_capacity);
    void set_fragment(
        runtime::SceneFragmentId fragment,
        std::span<const graphics::SceneDrawCommand> commands,
        std::optional<input::InteractionId> interaction = std::nullopt,
        std::optional<runtime::Rect> interaction_clip = std::nullopt);
    bool remove_fragment(runtime::SceneFragmentId fragment);
    void rebuild(runtime::Rect window_clip);

    [[nodiscard]] const graphics::OrderedScene& ordered_scene() const noexcept;
    [[nodiscard]] std::span<const input::HitTestPaintEntry>
        interaction_order() const noexcept;
    [[nodiscard]] const ComponentSceneDiagnostics& diagnostics() const noexcept;

private:
    struct FragmentBinding final {
        std::uint32_t generation{0};
        std::vector<graphics::SceneDrawCommand> commands;
        std::optional<input::InteractionId> interaction;
        std::optional<runtime::Rect> interaction_clip;
    };

    [[nodiscard]] FragmentBinding* find_binding(
        runtime::SceneFragmentId fragment) noexcept;
    [[nodiscard]] const FragmentBinding* find_binding(
        runtime::SceneFragmentId fragment) const noexcept;
    void ensure_owner_thread() const;

    runtime::ComponentHost* components_;
    input::InteractionRegistry* interactions_;
    input::HitTestSnapshot* hit_test_;
    std::vector<std::optional<FragmentBinding>> bindings_;
    graphics::OrderedScene ordered_scene_;
    std::vector<input::HitTestPaintEntry> interaction_order_;
    ComponentSceneDiagnostics diagnostics_;
};

} // namespace ryn::component
