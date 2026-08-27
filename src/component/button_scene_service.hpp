#pragma once

#include "component/component_scene.hpp"
#include "graphics/quad_primitive.hpp"
#include "runtime/component_host.hpp"
#include "runtime/node_store.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace ryn::component {

inline constexpr std::size_t button_visual_layer_count = 4;

enum class ButtonVisualLayer : std::uint8_t {
    focus_ring,
    border,
    background,
    loading_indicator,
};

using ButtonVisualData =
    std::array<graphics::QuadInstance, button_visual_layer_count>;

struct ButtonSceneId final {
    static constexpr std::uint32_t invalid_index =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalid_index};
    std::uint32_t generation{0};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalid_index && generation != 0;
    }

    friend constexpr bool operator==(ButtonSceneId, ButtonSceneId) = default;
};

struct ButtonSceneDiagnostics final {
    std::uint64_t creates{0};
    std::uint64_t destroys{0};
    std::uint64_t material_updates{0};
    std::uint64_t geometry_updates{0};
    std::uint64_t range_compactions{0};
    std::uint64_t fragment_remaps{0};
    std::uint64_t stale_rejections{0};
};

class ButtonSceneService final {
public:
    ButtonSceneService(
        runtime::ComponentHost& components,
        runtime::NodeStore& nodes,
        ComponentSceneComposer& composer) noexcept;

    void reserve(std::size_t button_capacity);
    [[nodiscard]] ButtonSceneId create(
        runtime::ComponentId component,
        runtime::NodeId node,
        runtime::SceneFragmentId fragment,
        std::optional<input::InteractionId> interaction,
        const ButtonVisualData& visuals);
    bool destroy(ButtonSceneId id);
    [[nodiscard]] std::size_t update(
        ButtonSceneId id,
        const ButtonVisualData& visuals);
    void synchronize_gpu(graphics::QuadGpuBuffer& gpu_buffer);

    [[nodiscard]] graphics::QuadInstanceRange visual_range(ButtonSceneId id) const;
    [[nodiscard]] graphics::QuadInstanceStore& instances() noexcept;
    [[nodiscard]] const graphics::QuadInstanceStore& instances() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const ButtonSceneDiagnostics& diagnostics() const noexcept;

private:
    struct Record final {
        ButtonSceneId id;
        runtime::ComponentId component;
        runtime::NodeId node;
        runtime::SceneFragmentId fragment;
        std::optional<input::InteractionId> interaction;
        graphics::QuadInstanceRange range;
    };

    struct Slot final {
        std::optional<Record> record;
        std::uint32_t generation{1};
    };

    [[nodiscard]] Record* find(ButtonSceneId id) noexcept;
    [[nodiscard]] const Record* find(ButtonSceneId id) const noexcept;
    [[nodiscard]] Record& require(ButtonSceneId id);
    [[nodiscard]] const Record& require(ButtonSceneId id) const;
    [[nodiscard]] std::uint32_t acquire_slot();
    void bind_fragment(const Record& record);
    void ensure_owner_thread() const;
    static void validate_visuals(const ButtonVisualData& visuals);
    static void advance_generation(Slot& slot) noexcept;

    runtime::ComponentHost* components_;
    runtime::NodeStore* nodes_;
    ComponentSceneComposer* composer_;
    graphics::QuadInstanceStore instances_;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_slots_;
    std::size_t live_records_{0};
    ButtonSceneDiagnostics diagnostics_;
};

} // namespace ryn::component
