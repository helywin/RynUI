#include "renderer/common/composition_packing.hpp"
#include "graphics/glyph_scene.hpp"
#include "renderer/common/scene_capabilities.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
namespace {
void require(bool value, const char* message) {
    if (!value) {
        throw std::invalid_argument(message);
    }
}

CompositionCrop crop(const std::array<float, 4>& shape, const std::array<float, 4>& hard, SceneDeviceMetrics metrics,
                     double halo) {
    const double left =
        std::clamp(std::floor(std::max(double(shape[0]) - halo, double(hard[0]))), 0.0, double(metrics.pixel_width));
    const double top =
        std::clamp(std::floor(std::max(double(shape[1]) - halo, double(hard[1]))), 0.0, double(metrics.pixel_height));
    const double right = std::clamp(std::ceil(std::min(double(shape[0]) + shape[2] + halo, double(hard[2]))), left,
                                    double(metrics.pixel_width));
    const double bottom = std::clamp(std::ceil(std::min(double(shape[1]) + shape[3] + halo, double(hard[3]))), top,
                                     double(metrics.pixel_height));
    return {std::uint32_t(left), std::uint32_t(top), std::uint32_t(right - left), std::uint32_t(bottom - top)};
}

std::array<float, 4> origin(CompositionCrop c) {
    return {float(c.x), float(c.y), float(std::max(1U, c.width)), float(std::max(1U, c.height))};
}
} // namespace

PassViewportV1 composition_viewport(SceneDeviceMetrics m, CompositionCrop c) {
    return {{float(m.pixel_width), float(m.pixel_height), 0, 0}, origin(c)};
}

CompositionPlan pack_composition(const graphics::OrderedScene& scene, SceneDeviceMetrics metrics,
                                 const SceneBackendCapabilities& caps) {
    CompositionPlan out;
    out.metrics = metrics;
    if (!scene.composition()) {
        return out;
    }
    validate_scene_device_metrics(metrics);
    const auto& program = *scene.composition();
    require(program.version == graphics::scene_composition_version && program.scene_revision == scene.revision(),
            "Composition version or draw revision mismatch");
    require(caps.scene_composition_version == graphics::scene_composition_version && caps.masked_groups &&
                caps.rounded_clip && caps.rounded_clip_abi_version == rounded_clip_abi_version &&
                caps.cropped_target_coordinates && caps.premultiplied_targets && caps.target_sampling,
            "Renderer lacks Scene Composition/rounded clip ABI v1");
    require(program.operations.size() <= caps.maximum_composition_ops &&
                program.requests.size() <= caps.maximum_composition_groups,
            "Composition operation/request budget exceeded");
    require(metrics.pixel_width <= 8388608 && metrics.pixel_height <= 8388608,
            "Composition pixel centers cannot be represented");
    require(scene.commands().size() <= std::numeric_limits<std::uint32_t>::max(),
            "Composition draw stream exceeds uint32_t addressing");
    out.program = program;
    out.commands.assign(scene.commands().begin(), scene.commands().end());
    auto add_target = [&](CompositionCrop c) {
        CompositionTargetSpec spec{std::max(1U, c.width), std::max(1U, c.height)};
        require(spec.width <= caps.maximum_composition_target_width &&
                    spec.height <= caps.maximum_composition_target_height,
                "Composition target exceeds extent budget");
        const std::uint64_t bytes = std::uint64_t(spec.width) * spec.height * 4;
        require(bytes <= caps.maximum_composition_bytes &&
                    out.allocated_bytes <= caps.maximum_composition_bytes - bytes,
                "Composition actual target capacity exceeds budget");
        out.allocated_bytes += bytes;
        out.targets.push_back(spec);
        return static_cast<std::uint32_t>(out.targets.size() - 1);
    };
    add_target({0, 0, metrics.pixel_width, metrics.pixel_height});
    bool has_backdrop = false;
    for (const auto& request : program.requests) {
        require(request.owner.valid(), "Composition request has no live owner generation");
        const auto normalized = runtime::normalized_content_clip(request.shape);
        require(normalized == request.shape, "Composition shape must be normalized");
        const auto hard = runtime::normalized_content_clip({request.hard_clip, 0}).bounds;
        require(std::isfinite(request.activation) && request.activation >= 0 && request.activation <= 1,
                "Composition activation must be finite in 0..1");
        PackedCompositionRequest packed;
        const auto r = normalized.bounds;
        const double scale = metrics.display_scale;
        auto finite_float = [](double v) {
            require(std::isfinite(v) && std::abs(v) <= std::numeric_limits<float>::max(),
                    "Composition physical coordinate overflow");
            return static_cast<float>(v);
        };
        packed.clip.shape = {finite_float(r.x * scale), finite_float(r.y * scale), finite_float(r.width * scale),
                             finite_float(r.height * scale)};
        packed.clip.radius_aa = {finite_float(normalized.radius * scale), 1, 0, 0};
        packed.clip.hard_clip = {finite_float(hard.x * scale), finite_float(hard.y * scale),
                                 finite_float((double(hard.x) + hard.width) * scale),
                                 finite_float((double(hard.y) + hard.height) * scale)};
        for (const float value : packed.clip.shape) {
            require(std::isfinite(value + .5F) &&
                        std::nextafter(std::abs(value), std::numeric_limits<float>::infinity()) - std::abs(value) <=
                            .5F,
                    "Rounded clip physical coordinates cannot preserve half-pixel coverage");
        }
        packed.crop = crop(packed.clip.shape, packed.clip.hard_clip, metrics, 1);
        packed.clip.source_origin_extent = origin(packed.crop);
        if (request.backdrop) {
            require(caps.gaussian_backdrop && caps.backdrop_filter_abi_version == backdrop_filter_abi_version,
                    "Renderer lacks backdrop Gaussian ABI v1");
            require(std::isfinite(request.sigma) && request.sigma >= 0,
                    "Backdrop sigma must be finite and nonnegative");
            const double sigma = double(request.sigma) * scale;
            const double radius = std::ceil(3 * sigma);
            require(radius < double(std::numeric_limits<std::uint32_t>::max()), "Gaussian radius index overflow");
            require(out.weights.size() < std::numeric_limits<std::uint32_t>::max(), "Gaussian first weight overflow");
            require(std::isfinite(radius) && radius <= caps.maximum_gaussian_radius, "Gaussian radius exceeds budget");
            packed.first_weight = static_cast<std::uint32_t>(out.weights.size());
            packed.weight_count = static_cast<std::uint32_t>(radius) + 1;
            require(std::uint64_t(packed.first_weight) + packed.weight_count <=
                        std::numeric_limits<std::uint32_t>::max(),
                    "Gaussian weight address exceeds uint32_t");
            require((std::uint64_t(packed.first_weight) + packed.weight_count) * sizeof(GaussianWeightV1) <=
                        caps.maximum_gaussian_weight_bytes,
                    "Gaussian weights exceed budget");
            require((std::uint64_t(packed.first_weight) + packed.weight_count) * sizeof(GaussianWeightV1) <=
                        caps.maximum_buffer_bytes,
                    "Gaussian weights exceed backend buffer input limit");
            double sum = 1;
            for (std::uint32_t i = 1; i < packed.weight_count; ++i) {
                sum += 2 * std::exp(-double(i) * i / (2 * sigma * sigma));
            }
            for (std::uint32_t i = 0; i < packed.weight_count; ++i) {
                const double w = i == 0 ? 1 : std::exp(-double(i) * i / (2 * sigma * sigma));
                out.weights.push_back({{float(w / sum), 0, 0, 0}});
            }
            // Root-only v1 backdrops use full-window snapshots. This is the
            // conservative halo (no artificial crop edge, no downsampling).
            packed.source_crop = {0, 0, metrics.pixel_width, metrics.pixel_height};
            packed.clip.source_origin_extent = origin(packed.source_crop);
            const auto t = request.tint;
            require(std::isfinite(t.red) && std::isfinite(t.green) && std::isfinite(t.blue) && std::isfinite(t.alpha) &&
                        t.alpha >= 0 && t.alpha <= 1,
                    "Backdrop tint is invalid");
            packed.backdrop = {packed.clip,
                               {t.red * t.alpha, t.green * t.alpha, t.blue * t.alpha, t.alpha},
                               {request.activation, 0, 0, 0}};
        }
        out.requests.push_back(packed);
    }
    std::vector<std::uint32_t> stack;
    std::vector<bool> allocated(program.requests.size());
    std::uint64_t expected_draw = 0;
    for (const auto op : program.operations) {
        switch (op.kind) {
        case graphics::SceneCompositionOpKind::DrawRange:
            require(op.count && op.first == expected_draw &&
                        std::uint64_t(op.first) + op.count <= scene.commands().size(),
                    "Composition draw ranges must reference the final complete draw stream once");
            require(op.coverage == graphics::DrawCoverageRole::Normal ||
                        (op.coverage == graphics::DrawCoverageRole::GroupOwnsOuterCoverage && !stack.empty()),
                    "Unknown or unowned coverage role");
            if (op.coverage == graphics::DrawCoverageRole::GroupOwnsOuterCoverage) {
                for (std::uint32_t i = op.first; i < op.first + op.count; ++i) {
                    const auto kind = scene.commands()[i].kind;
                    require(kind == graphics::SceneDrawKind::quad || kind == graphics::SceneDrawKind::rounded_effect,
                            "Only an owned surface may delegate outer coverage");
                }
            }
            expected_draw += op.count;
            break;
        case graphics::SceneCompositionOpKind::BeginMaskedGroup:
            require(op.request < program.requests.size() && !program.requests[op.request].backdrop,
                    "Invalid masked request");
            require(stack.size() < caps.maximum_composition_depth, "Composition depth exceeded");
            require(std::find(stack.begin(), stack.end(), op.request) == stack.end(),
                    "Nested masked groups cannot share a render target");
            stack.push_back(op.request);
            // Sequential re-entry clears and reuses storage; nested groups
            // always have distinct targets. No contents are cached.
            if (!allocated[op.request]) {
                out.requests[op.request].target = add_target(out.requests[op.request].crop);
                allocated[op.request] = true;
            }
            break;
        case graphics::SceneCompositionOpKind::EndMaskedGroup:
            require(!stack.empty() && stack.back() == op.request, "Unbalanced composition group");
            stack.pop_back();
            break;
        case graphics::SceneCompositionOpKind::Backdrop:
            has_backdrop = true;
            require(stack.empty() && op.request < program.requests.size() && program.requests[op.request].backdrop,
                    "Backdrop must sample the preceding root target");
            break;
        default:
            throw std::invalid_argument("Unknown composition operation");
        }
    }
    require(stack.empty() && expected_draw == scene.commands().size(), "Incomplete composition program");
    if (has_backdrop) {
        const CompositionCrop full{0, 0, metrics.pixel_width, metrics.pixel_height};
        out.snapshot_target = add_target(full);
        out.horizontal_target = add_target(full);
        out.filtered_target = add_target(full);
    }
    const std::uint64_t sidecars = out.weights.size() * sizeof(GaussianWeightV1) +
                                   program.operations.size() * sizeof(graphics::SceneCompositionOp);
    require(sidecars <= caps.maximum_composition_bytes &&
                out.allocated_bytes <= caps.maximum_composition_bytes - sidecars,
            "Composition sidecar budget exceeded");
    out.allocated_bytes += sidecars;
    return out;
}
} // namespace ryn::detail
