#pragma once

#include "component/component_scene.hpp"
#include "graphics/quad_primitive.hpp"
#include "graphics/rounded_effect.hpp"
#include "runtime/component_host.hpp"
#include "runtime/node_store.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace ryn::component {

inline constexpr std::size_t retained_surface_visual_capacity = 16;
// A content range carries a variable number of visuals because decorations are
// produced per shaped line and therefore change count on reflow. The limit only
// exists to reject absurd inputs, not to bound a layout.
inline constexpr std::size_t retained_content_visual_capacity = 4096;

struct RetainedSurfaceEffects final {
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

    friend bool operator==(const RetainedSurfaceEffects&, const RetainedSurfaceEffects&) = default;
};

struct RetainedSurfaceId final {
    static constexpr std::uint32_t invalid_index = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalid_index};
    std::uint32_t generation{0};
    bool content_range{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalid_index && generation != 0;
    }

    friend constexpr bool operator==(RetainedSurfaceId, RetainedSurfaceId) = default;
};

struct RetainedSurfaceDiagnostics final {
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

class RetainedSurfaceService final {
public:
    RetainedSurfaceService(runtime::ComponentHost& components, runtime::NodeStore& nodes,
                           ComponentSceneComposer& composer) noexcept;

    void reserve(std::size_t surface_capacity, std::size_t visual_capacity);
    [[nodiscard]] RetainedSurfaceId create(runtime::ComponentId component, runtime::NodeId node,
                                           runtime::SceneFragmentId fragment,
                                           std::optional<input::InteractionId> interaction,
                                           std::span<const graphics::QuadInstance> visuals,
                                           const RetainedSurfaceEffects& effects = RetainedSurfaceEffects{});
    [[nodiscard]] RetainedSurfaceId create_surface(runtime::ComponentId component, runtime::NodeId node,
                                                   runtime::SceneFragmentId fragment,
                                                   std::span<const graphics::QuadInstance> visuals,
                                                   const RetainedSurfaceEffects& effects = RetainedSurfaceEffects{},
                                                   std::optional<input::InteractionId> interaction = std::nullopt);
    bool destroy(RetainedSurfaceId id);
    [[nodiscard]] std::size_t update(RetainedSurfaceId id, std::span<const graphics::QuadInstance> visuals);
    [[nodiscard]] std::size_t update_surface(RetainedSurfaceId id, std::span<const graphics::QuadInstance> visuals);
    // Content ranges back a caller-owned fragment (a text decoration layer, for
    // example) with a variable number of quads. `set_content_range` creates or
    // re-targets the range, `update_content_range` re-publishes it, and both
    // reallocate the range when the count changes so the owning component can add
    // or drop decoration quads on reflow.
    [[nodiscard]] RetainedSurfaceId create_content_range(runtime::SceneFragmentId fragment,
                                                         std::span<const graphics::QuadInstance> visuals);
    [[nodiscard]] std::size_t set_content_range(RetainedSurfaceId id, runtime::SceneFragmentId fragment,
                                                std::span<const graphics::QuadInstance> visuals);
    [[nodiscard]] std::size_t update_content_range(RetainedSurfaceId id,
                                                   std::span<const graphics::QuadInstance> visuals);
    bool destroy_content_range(RetainedSurfaceId id);
    std::size_t update_content_effects(RetainedSurfaceId id, std::span<const graphics::RoundedEffectInstance> effects);
    [[nodiscard]] std::size_t update_effects(RetainedSurfaceId id, const RetainedSurfaceEffects& effects);
    [[nodiscard]] bool compact_effects(runtime::Rect window_clip);

    [[nodiscard]] graphics::QuadInstanceRange visual_range(RetainedSurfaceId id) const;
    [[nodiscard]] const graphics::RoundedEffectInstance& focus_effect(RetainedSurfaceId id) const;
    [[nodiscard]] std::span<const graphics::RoundedEffectId> shadow_effects(RetainedSurfaceId id) const;
    [[nodiscard]] graphics::QuadInstanceStore& instances() noexcept;
    [[nodiscard]] const graphics::QuadInstanceStore& instances() const noexcept;
    [[nodiscard]] graphics::RoundedEffectStore& effects() noexcept;
    [[nodiscard]] const graphics::RoundedEffectStore& effects() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const RetainedSurfaceDiagnostics& diagnostics() const noexcept;

private:
    struct Record final {
        RetainedSurfaceId id;
        runtime::ComponentId component;
        runtime::NodeId node;
        runtime::SceneFragmentId fragment;
        std::optional<input::InteractionId> interaction;
        graphics::QuadInstanceRange range;
        RetainedSurfaceEffects effects;
        std::vector<graphics::RoundedEffectId> shadow_ids;
        graphics::RoundedEffectId focus_id;
        graphics::RoundedEffectPrimitive effect_primitive;
    };

    // A content range is owned by a caller-provided fragment instead of a
    // component record, so it has no effects, no interaction and no node.
    struct ContentRecord final {
        RetainedSurfaceId id;
        runtime::SceneFragmentId fragment;
        graphics::QuadInstanceRange range;
        std::vector<graphics::RoundedEffectId> effects;
    };

    struct ContentSlot final {
        std::optional<ContentRecord> record;
        std::uint32_t generation{1};
    };

    struct Slot final {
        std::optional<Record> record;
        std::uint32_t generation{1};
    };

    [[nodiscard]] Record* find(RetainedSurfaceId id) noexcept;
    [[nodiscard]] const Record* find(RetainedSurfaceId id) const noexcept;
    [[nodiscard]] Record& require(RetainedSurfaceId id);
    [[nodiscard]] const Record& require(RetainedSurfaceId id) const;
    [[nodiscard]] RetainedSurfaceId create_record(runtime::ComponentId component, runtime::NodeId node,
                                                  runtime::SceneFragmentId fragment,
                                                  std::optional<input::InteractionId> interaction,
                                                  std::span<const graphics::QuadInstance> visuals,
                                                  const RetainedSurfaceEffects& effects);
    [[nodiscard]] std::uint32_t acquire_slot();
    [[nodiscard]] std::uint32_t acquire_content_slot();
    [[nodiscard]] ContentRecord* find_content(RetainedSurfaceId id) noexcept;
    [[nodiscard]] ContentRecord& require_content(RetainedSurfaceId id);
    void publish_content(ContentRecord& record);
    void remap_after_replace(graphics::QuadInstanceRange old_range, std::uint32_t new_count,
                             const graphics::QuadInstanceRange* owner);
    [[nodiscard]] std::size_t republish_range(graphics::QuadInstanceRange& range,
                                              std::span<const graphics::QuadInstance> visuals);
    void bind_fragment(const Record& record);
    void create_effects(Record& record);
    void remove_effects(Record& record) noexcept;
    void ensure_owner_thread() const;
    static void validate_visuals(std::span<const graphics::QuadInstance> visuals);
    static void validate_content_visuals(std::span<const graphics::QuadInstance> visuals);
    static void validate_finite_visuals(std::span<const graphics::QuadInstance> visuals);
    static void advance_generation(Slot& slot) noexcept;

    runtime::ComponentHost* components_;
    runtime::NodeStore* nodes_;
    ComponentSceneComposer* composer_;
    graphics::QuadInstanceStore instances_;
    graphics::RoundedEffectScene effect_scene_;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_slots_;
    std::size_t live_records_{0};
    std::vector<ContentSlot> content_slots_;
    std::vector<std::uint32_t> free_content_slots_;
    RetainedSurfaceDiagnostics diagnostics_;
};

} // namespace ryn::component
