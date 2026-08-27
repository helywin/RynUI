#pragma once

#include "graphics/glyph_scene.hpp"
#include "runtime/geometry.hpp"

#include <ryn/design_token.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace ryn::graphics {

enum class RoundedEffectKind : std::uint8_t {
    outer_shadow,
    inset_shadow,
    outline,
};

struct LogicalRoundedRect final {
    runtime::Rect rect;
    float radius{};

    friend constexpr bool operator==(
        LogicalRoundedRect,
        LogicalRoundedRect) = default;
};

struct EffectClip final {
    std::uint64_t identity{};
    runtime::Rect bounds;

    friend constexpr bool operator==(EffectClip, EffectClip) = default;
};

struct RoundedEffectGeometry final {
    LogicalRoundedRect shape;
    RoundedEffectKind kind{RoundedEffectKind::outer_shadow};
    LogicalOffset offset;
    float blur{};
    float spread{};
    float outline_width{};
    float outline_offset{};
    runtime::Point translation;
    std::optional<EffectClip> ancestor_clip;

    friend bool operator==(
        const RoundedEffectGeometry&,
        const RoundedEffectGeometry&) = default;
};

struct RoundedEffectMaterial final {
    Color color;
    float opacity{1.0F};
    bool visible{true};

    friend constexpr bool operator==(
        RoundedEffectMaterial,
        RoundedEffectMaterial) = default;
};

struct RoundedEffectInstance final {
    RoundedEffectGeometry geometry;
    RoundedEffectMaterial material;

    friend bool operator==(
        const RoundedEffectInstance&,
        const RoundedEffectInstance&) = default;
};

[[nodiscard]] RoundedEffectInstance make_shadow_effect(
    LogicalRoundedRect shape,
    const ShadowLayer& layer,
    runtime::Point translation = {},
    std::optional<EffectClip> ancestor_clip = std::nullopt);

[[nodiscard]] RoundedEffectInstance make_outline_effect(
    LogicalRoundedRect shape,
    float width,
    float offset,
    Color color,
    float opacity = 1.0F,
    runtime::Point translation = {},
    std::optional<EffectClip> ancestor_clip = std::nullopt);

void validate_rounded_effect(const RoundedEffectInstance& instance);

[[nodiscard]] float rounded_rect_signed_distance(
    runtime::Point point,
    LogicalRoundedRect shape) noexcept;

[[nodiscard]] float rounded_effect_coverage(
    runtime::Point point,
    const RoundedEffectInstance& instance,
    float antialias_width = 1.0F);

[[nodiscard]] runtime::Rect rounded_effect_bounds(
    const RoundedEffectInstance& instance,
    float antialias_guard = 1.0F);

[[nodiscard]] runtime::Rect intersect_effect_bounds(
    runtime::Rect bounds,
    runtime::Rect clip) noexcept;

struct RoundedEffectId final {
    static constexpr std::uint32_t invalid_index =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalid_index};
    std::uint32_t generation{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalid_index && generation != 0;
    }

    friend constexpr bool operator==(
        RoundedEffectId,
        RoundedEffectId) = default;
};

struct RoundedEffectInstanceRange final {
    std::uint32_t first{};
    std::uint32_t count{};

    friend constexpr bool operator==(
        RoundedEffectInstanceRange,
        RoundedEffectInstanceRange) = default;
};

struct RoundedEffectStoreDiagnostics final {
    std::uint64_t additions{};
    std::uint64_t removals{};
    std::uint64_t slot_reuses{};
    std::uint64_t compactions{};
    std::uint64_t idle_compactions{};
    std::uint64_t capacity_growths{};
    std::uint64_t material_updates{};
    std::uint64_t geometry_updates{};
    std::size_t live_instances{};
    std::size_t packed_instances{};
    std::size_t culled_instances{};
};

class RoundedEffectStore final {
public:
    RoundedEffectStore();
    ~RoundedEffectStore();
    RoundedEffectStore(const RoundedEffectStore&) = delete;
    RoundedEffectStore& operator=(const RoundedEffectStore&) = delete;
    RoundedEffectStore(RoundedEffectStore&&) noexcept;
    RoundedEffectStore& operator=(RoundedEffectStore&&) noexcept;

    void reserve(std::size_t capacity);
    [[nodiscard]] RoundedEffectId add(RoundedEffectInstance instance);
    [[nodiscard]] std::vector<RoundedEffectId> add_batch(
        std::span<const RoundedEffectInstance> instances);
    bool remove(RoundedEffectId id);
    [[nodiscard]] bool contains(RoundedEffectId id) const noexcept;

    [[nodiscard]] const RoundedEffectInstance& at(RoundedEffectId id) const;
    [[nodiscard]] bool update_material(
        RoundedEffectId id,
        RoundedEffectMaterial material);
    [[nodiscard]] bool update_geometry(
        RoundedEffectId id,
        RoundedEffectGeometry geometry);

    [[nodiscard]] bool compact(runtime::Rect window_clip);
    [[nodiscard]] std::optional<std::uint32_t> packed_index(
        RoundedEffectId id) const;
    [[nodiscard]] std::span<const RoundedEffectInstance>
        packed_instances() const noexcept;
    [[nodiscard]] std::span<const std::byte> bytes(
        RoundedEffectInstanceRange range) const;

    [[nodiscard]] std::span<const RoundedEffectInstanceRange>
        material_dirty_ranges() const noexcept;
    [[nodiscard]] std::span<const RoundedEffectInstanceRange>
        geometry_dirty_ranges() const noexcept;
    void clear_dirty_ranges() noexcept;

    [[nodiscard]] std::size_t live_count() const noexcept;
    [[nodiscard]] std::size_t slot_capacity() const noexcept;
    [[nodiscard]] const RoundedEffectStoreDiagnostics& diagnostics() const noexcept;

private:
    struct Slot;

    [[nodiscard]] Slot* find_slot(RoundedEffectId id) noexcept;
    [[nodiscard]] const Slot* find_slot(RoundedEffectId id) const noexcept;
    [[nodiscard]] Slot& require_slot(RoundedEffectId id);
    [[nodiscard]] const Slot& require_slot(RoundedEffectId id) const;
    static void advance_generation(Slot& slot) noexcept;
    static void mark_dirty(
        std::vector<RoundedEffectInstanceRange>& ranges,
        RoundedEffectInstanceRange range);
    static void validate_clip(runtime::Rect clip);
    void note_capacity_growth(
        std::size_t slots_before,
        std::size_t order_before,
        std::size_t packed_before) noexcept;

    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_slots_;
    std::vector<RoundedEffectId> draw_order_;
    std::vector<RoundedEffectInstance> packed_instances_;
    std::vector<RoundedEffectInstanceRange> material_dirty_ranges_;
    std::vector<RoundedEffectInstanceRange> geometry_dirty_ranges_;
    std::optional<runtime::Rect> compact_clip_;
    RoundedEffectStoreDiagnostics diagnostics_;
    bool compact_dirty_{true};
};

struct RoundedEffectPrimitive final {
    std::vector<RoundedEffectId> before_fill;
    std::vector<RoundedEffectId> after_fill;
};

class RoundedEffectScene final {
public:
    [[nodiscard]] RoundedEffectPrimitive append_shadow_list(
        LogicalRoundedRect shape,
        const ShadowList& shadows,
        runtime::Point translation = {},
        std::optional<EffectClip> ancestor_clip = std::nullopt);
    [[nodiscard]] RoundedEffectPrimitive append_outline(
        LogicalRoundedRect shape,
        float width,
        float offset,
        Color color,
        float opacity = 1.0F,
        runtime::Point translation = {},
        std::optional<EffectClip> ancestor_clip = std::nullopt);
    bool remove(const RoundedEffectPrimitive& primitive);

    void compose_surface(
        const RoundedEffectPrimitive& primitive,
        SceneDrawCommand fill,
        std::vector<SceneDrawCommand>& output) const;

    [[nodiscard]] RoundedEffectStore& store() noexcept;
    [[nodiscard]] const RoundedEffectStore& store() const noexcept;

private:
    void append_effect_commands(
        std::span<const RoundedEffectId> effects,
        std::vector<SceneDrawCommand>& output) const;

    RoundedEffectStore store_;
};

} // namespace ryn::graphics
