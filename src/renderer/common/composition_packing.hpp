#pragma once
#include "graphics/scene_composition.hpp"
#include "graphics/glyph_scene.hpp"
#include "renderer/common/scene_metrics.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ryn::detail {
inline constexpr std::uint32_t rounded_clip_abi_version = 1;
inline constexpr std::uint32_t backdrop_filter_abi_version = 1;

struct alignas(16) PassViewportV1 final {
    std::array<float, 4> full_viewport;
    std::array<float, 4> target_origin_extent;
};

struct alignas(16) DrawCoverageControlV1 final {
    std::array<std::uint32_t, 4> value{};
};

struct alignas(16) RoundedClipUniformV1 final {
    std::array<float, 4> shape;
    std::array<float, 4> radius_aa;
    std::array<float, 4> hard_clip;
    std::array<float, 4> source_origin_extent;
};

struct alignas(16) GaussianFilterUniformV1 final {
    std::array<float, 4> source_origin_extent;
    std::array<std::uint32_t, 4> axis_radius_first_count;
};

struct alignas(16) GaussianWeightV1 final {
    std::array<float, 4> value{};
};

struct alignas(16) BackdropCompositeUniformV1 final {
    RoundedClipUniformV1 clip;
    std::array<float, 4> tint_pm;
    std::array<float, 4> activation;
};

static_assert(sizeof(PassViewportV1) == 32 && sizeof(DrawCoverageControlV1) == 16);
static_assert(sizeof(RoundedClipUniformV1) == 64 && sizeof(GaussianFilterUniformV1) == 32);
static_assert(sizeof(GaussianWeightV1) == 16 && sizeof(BackdropCompositeUniformV1) == 96);
static_assert(offsetof(PassViewportV1, target_origin_extent) == 16);
static_assert(offsetof(RoundedClipUniformV1, hard_clip) == 32);
static_assert(offsetof(BackdropCompositeUniformV1, tint_pm) == 64);

struct CompositionCrop final {
    std::uint32_t x{};
    std::uint32_t y{};
    std::uint32_t width{};
    std::uint32_t height{};
    friend bool operator==(CompositionCrop, CompositionCrop) = default;
};

struct CompositionTargetSpec final {
    std::uint32_t width{};
    std::uint32_t height{};
    friend bool operator==(CompositionTargetSpec, CompositionTargetSpec) = default;
};

struct PackedCompositionRequest final {
    RoundedClipUniformV1 clip{};
    CompositionCrop crop;
    CompositionCrop source_crop;
    std::uint32_t target{};
    std::uint32_t first_weight{};
    std::uint32_t weight_count{};
    BackdropCompositeUniformV1 backdrop{};
};

struct CompositionPlan final {
    graphics::SceneCompositionProgram program;
    std::vector<graphics::SceneDrawCommand> commands;
    SceneDeviceMetrics metrics;
    std::vector<PackedCompositionRequest> requests;
    std::vector<CompositionTargetSpec> targets;
    std::vector<GaussianWeightV1> weights;
    std::uint32_t snapshot_target{};
    std::uint32_t horizontal_target{};
    std::uint32_t filtered_target{};
    std::uint64_t allocated_bytes{};
};
struct SceneBackendCapabilities;
[[nodiscard]] CompositionPlan pack_composition(const graphics::OrderedScene& scene, SceneDeviceMetrics metrics,
                                               const SceneBackendCapabilities& capabilities);
[[nodiscard]] PassViewportV1 composition_viewport(SceneDeviceMetrics metrics, CompositionCrop crop);
} // namespace ryn::detail
