#pragma once

#include "component/component_scene.hpp"
#include "graphics/quad_primitive.hpp"
#include "graphics/rounded_effect.hpp"
#include "runtime/component_host.hpp"
#include "runtime/node_store.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace ryn::component {

inline constexpr std::size_t button_loading_segment_count = 8;
inline constexpr std::size_t button_visual_layer_count =
    2 + button_loading_segment_count;
inline constexpr std::size_t retained_surface_visual_capacity = 16;

enum class ButtonVisualLayer : std::uint8_t {
    border,
    background,
    loading_indicator,
};

[[nodiscard]] constexpr std::size_t button_loading_segment_index(
    std::size_t segment) noexcept {
    return static_cast<std::size_t>(ButtonVisualLayer::loading_indicator) + segment;
}

using ButtonVisualData =
    std::array<graphics::QuadInstance, button_visual_layer_count>;

struct ButtonEffectData final {
    graphics::LogicalRoundedRect shape;
    ShadowList shadows;
    float shadow_opacity{1.0F};
    float focus_width{3.0F};
    float focus_offset{1.0F};
    Color focus_color;
    float focus_opacity{};
    bool focus_enabled{true};
    runtime::Point translation;
    std::optional<graphics::EffectClip> ancestor_clip;

    friend bool operator==(const ButtonEffectData&, const ButtonEffectData&) = default;
};

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
    std::uint64_t effect_material_updates{0};
    std::uint64_t effect_geometry_updates{0};
    std::uint64_t effect_topology_updates{0};
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
        const ButtonVisualData& visuals,
        const ButtonEffectData& effects = ButtonEffectData{});
    [[nodiscard]] ButtonSceneId create_surface(
        runtime::ComponentId component,
        runtime::NodeId node,
        runtime::SceneFragmentId fragment,
        std::span<const graphics::QuadInstance> visuals,
        const ButtonEffectData& effects = ButtonEffectData{});
    bool destroy(ButtonSceneId id);
    [[nodiscard]] std::size_t update(
        ButtonSceneId id,
        const ButtonVisualData& visuals);
    [[nodiscard]] std::size_t update_surface(
        ButtonSceneId id,
        std::span<const graphics::QuadInstance> visuals);
    [[nodiscard]] std::size_t update_effects(
        ButtonSceneId id,
        const ButtonEffectData& effects);
    [[nodiscard]] bool compact_effects(runtime::Rect window_clip);
    void synchronize_gpu(graphics::QuadGpuBuffer& gpu_buffer);

    [[nodiscard]] graphics::QuadInstanceRange visual_range(ButtonSceneId id) const;
    [[nodiscard]] const graphics::RoundedEffectInstance& focus_effect(
        ButtonSceneId id) const;
    [[nodiscard]] std::span<const graphics::RoundedEffectId> shadow_effects(
        ButtonSceneId id) const;
    [[nodiscard]] graphics::QuadInstanceStore& instances() noexcept;
    [[nodiscard]] const graphics::QuadInstanceStore& instances() const noexcept;
    [[nodiscard]] graphics::RoundedEffectStore& effects() noexcept;
    [[nodiscard]] const graphics::RoundedEffectStore& effects() const noexcept;
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
        ButtonEffectData effects;
        std::vector<graphics::RoundedEffectId> shadow_ids;
        graphics::RoundedEffectId focus_id;
        graphics::RoundedEffectPrimitive effect_primitive;
    };

    struct Slot final {
        std::optional<Record> record;
        std::uint32_t generation{1};
    };

    [[nodiscard]] Record* find(ButtonSceneId id) noexcept;
    [[nodiscard]] const Record* find(ButtonSceneId id) const noexcept;
    [[nodiscard]] Record& require(ButtonSceneId id);
    [[nodiscard]] const Record& require(ButtonSceneId id) const;
    [[nodiscard]] ButtonSceneId create_record(
        runtime::ComponentId component,
        runtime::NodeId node,
        runtime::SceneFragmentId fragment,
        std::optional<input::InteractionId> interaction,
        std::span<const graphics::QuadInstance> visuals,
        const ButtonEffectData& effects);
    [[nodiscard]] std::uint32_t acquire_slot();
    void bind_fragment(const Record& record);
    void create_effects(Record& record);
    void remove_effects(Record& record) noexcept;
    void ensure_owner_thread() const;
    static void validate_visuals(
        std::span<const graphics::QuadInstance> visuals);
    static void advance_generation(Slot& slot) noexcept;

    runtime::ComponentHost* components_;
    runtime::NodeStore* nodes_;
    ComponentSceneComposer* composer_;
    graphics::QuadInstanceStore instances_;
    graphics::RoundedEffectScene effect_scene_;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_slots_;
    std::size_t live_records_{0};
    ButtonSceneDiagnostics diagnostics_;
};

} // namespace ryn::component
