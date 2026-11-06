#pragma once

#include "renderer/common/rounded_effect_packing.hpp"

#include <limits>
#include <span>
#include <vector>

namespace ryn::detail {

inline constexpr std::uint32_t effect_paint_abi_version = 1;
inline constexpr std::uint32_t effect_paint_no_field = std::numeric_limits<std::uint32_t>::max();

enum class EffectPaintBufferKind { link, field, stop };
enum class EffectPaintFieldKind : std::uint32_t { linear = 1, conic = 2 };

struct alignas(16) EffectPaintGpuLink final {
    std::uint32_t foreground_field{effect_paint_no_field};
    std::uint32_t underlay_field{effect_paint_no_field};
    std::uint32_t arc_cap{};
    std::uint32_t reserved{};
    friend constexpr bool operator==(const EffectPaintGpuLink&, const EffectPaintGpuLink&) = default;
};

struct alignas(16) EffectPaintGpuField final {
    std::uint32_t kind{};
    std::uint32_t first_stop{};
    std::uint32_t stop_count{};
    std::uint32_t reserved{};
    std::array<float, 4> domain{};
    friend constexpr bool operator==(const EffectPaintGpuField&, const EffectPaintGpuField&) = default;
};

struct alignas(16) EffectPaintGpuStop final {
    std::array<float, 4> position{};
    std::array<float, 4> premultiplied_rgba{};
    friend constexpr bool operator==(const EffectPaintGpuStop&, const EffectPaintGpuStop&) = default;
};

static_assert(sizeof(EffectPaintGpuLink) == 16 && alignof(EffectPaintGpuLink) == 16);
static_assert(offsetof(EffectPaintGpuLink, foreground_field) == 0);
static_assert(offsetof(EffectPaintGpuLink, underlay_field) == 4);
static_assert(offsetof(EffectPaintGpuLink, arc_cap) == 8);
static_assert(offsetof(EffectPaintGpuLink, reserved) == 12);
static_assert(sizeof(EffectPaintGpuField) == 32 && alignof(EffectPaintGpuField) == 16);
static_assert(offsetof(EffectPaintGpuField, kind) == 0);
static_assert(offsetof(EffectPaintGpuField, first_stop) == 4);
static_assert(offsetof(EffectPaintGpuField, stop_count) == 8);
static_assert(offsetof(EffectPaintGpuField, reserved) == 12);
static_assert(offsetof(EffectPaintGpuField, domain) == 16);
static_assert(sizeof(EffectPaintGpuStop) == 32 && alignof(EffectPaintGpuStop) == 16);
static_assert(offsetof(EffectPaintGpuStop, position) == 0);
static_assert(offsetof(EffectPaintGpuStop, premultiplied_rgba) == 16);

struct EffectPaintPackedTables final {
    std::vector<EffectPaintGpuLink> links;
    std::vector<EffectPaintGpuField> fields;
    std::vector<EffectPaintGpuStop> stops;
};

// Shared by preflight/allocation. Checks count and byte multiplication before narrowing.
[[nodiscard]] std::uint32_t effect_buffer_capacity(std::size_t required, std::uint32_t current, std::size_t stride,
                                                   std::uint64_t maximum_bytes);
[[nodiscard]] EffectPaintGpuField pack_effect_paint_field(const graphics::EffectPaintField& field,
                                                          runtime::Point translation, SceneDeviceMetrics metrics,
                                                          std::uint32_t first_stop = 0);
void pack_effect_paint_tables(std::span<const graphics::RoundedEffectInstance> source, SceneDeviceMetrics metrics,
                              EffectPaintPackedTables& destination);
void validate_effect_paint_link(const EffectPaintGpuLink& link, std::span<const EffectPaintGpuField> fields,
                                std::span<const EffectPaintGpuStop> stops);
[[nodiscard]] std::array<float, 4> effect_paint_gpu_fragment_reference(runtime::Point point_pixels,
                                                                       const RoundedEffectGpuInstance& base,
                                                                       const EffectPaintGpuLink& link,
                                                                       std::span<const EffectPaintGpuField> fields,
                                                                       std::span<const EffectPaintGpuStop> stops);

} // namespace ryn::detail
