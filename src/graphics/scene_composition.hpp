#pragma once
#include "runtime/node_store.hpp"
#include <cstdint>
#include <vector>

namespace ryn::graphics {
class OrderedScene;
inline constexpr std::uint32_t scene_composition_version = 1;
// Node generation is the request generation; requests cannot outlive their CPU owner.
using MaskedGroupId = runtime::NodeId;
using BackdropId = runtime::NodeId;
enum class SceneCompositionOpKind : std::uint8_t { DrawRange, BeginMaskedGroup, EndMaskedGroup, Backdrop };
enum class DrawCoverageRole : std::uint32_t { Normal, GroupOwnsOuterCoverage };

struct SceneCompositionRequest final {
    runtime::NodeId owner;
    runtime::RoundedContentClip shape;
    runtime::Rect hard_clip;
    bool backdrop{};
    float sigma{};
    runtime::Color tint{};
    float activation{1};
    friend bool operator==(const SceneCompositionRequest&, const SceneCompositionRequest&) = default;
};

struct SceneCompositionOp final {
    SceneCompositionOpKind kind{SceneCompositionOpKind::DrawRange};
    std::uint32_t first{};
    std::uint32_t count{};
    std::uint32_t request{};
    DrawCoverageRole coverage{DrawCoverageRole::Normal};
    friend bool operator==(const SceneCompositionOp&, const SceneCompositionOp&) = default;
};

// Attached to exactly one finalized OrderedScene. Includes hidden live requests
// so culling/opacity cannot bypass semantic preflight.
struct SceneCompositionProgram final {
    std::uint32_t version{scene_composition_version};
    std::uint64_t scene_revision{};
    std::vector<SceneCompositionRequest> requests;
    std::vector<SceneCompositionOp> operations;
};
} // namespace ryn::graphics
