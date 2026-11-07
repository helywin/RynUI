#include "graphics/rounded_effect.hpp"
#include "graphics/arc_cap.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace ryn::graphics {
namespace {

[[nodiscard]] bool finite(float value) noexcept {
    return std::isfinite(value);
}

void validate_rect(runtime::Rect rect, const char* message) {
    if (!finite(rect.x) || !finite(rect.y) || !finite(rect.width) || !finite(rect.height) || rect.width < 0.0F ||
        rect.height < 0.0F) {
        throw std::invalid_argument(message);
    }
}

[[nodiscard]] LogicalRoundedRect translated(LogicalRoundedRect shape, runtime::Point translation) noexcept {
    shape.rect.x += translation.x;
    shape.rect.y += translation.y;
    return shape;
}

[[nodiscard]] LogicalRoundedRect spread_shape(LogicalRoundedRect shape, float spread) noexcept {
    shape.rect.x -= spread;
    shape.rect.y -= spread;
    shape.rect.width += 2.0F * spread;
    shape.rect.height += 2.0F * spread;
    if (shape.rect.width <= 0.0F || shape.rect.height <= 0.0F) {
        shape.rect.width = 0.0F;
        shape.rect.height = 0.0F;
        shape.radius = 0.0F;
        return shape;
    }
    shape.radius = std::clamp(shape.radius + spread, 0.0F, 0.5F * std::min(shape.rect.width, shape.rect.height));
    return shape;
}

[[nodiscard]] float smoothstep(float edge0, float edge1, float value) noexcept {
    if (edge0 == edge1) {
        return value < edge0 ? 0.0F : 1.0F;
    }
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] float gaussian_edge(float signed_distance, float sigma, float antialias_width) noexcept {
    if (sigma <= 0.0F) {
        const float half_aa = antialias_width * 0.5F;
        return 1.0F - smoothstep(-half_aa, half_aa, signed_distance);
    }
    constexpr float inverse_sqrt_two = 0.7071067811865475F;
    return std::clamp(0.5F * std::erfc(signed_distance * inverse_sqrt_two / sigma), 0.0F, 1.0F);
}

[[nodiscard]] bool empty(runtime::Rect rect) noexcept {
    return rect.width <= 0.0F || rect.height <= 0.0F;
}

[[nodiscard]] runtime::Rect expand(runtime::Rect rect, float amount) noexcept {
    return {
        rect.x - amount,
        rect.y - amount,
        std::max(0.0F, rect.width + 2.0F * amount),
        std::max(0.0F, rect.height + 2.0F * amount),
    };
}

[[nodiscard]] runtime::Rect translate(runtime::Rect rect, runtime::Point point) noexcept {
    rect.x += point.x;
    rect.y += point.y;
    return rect;
}

constexpr float pi = std::numbers::pi_v<float>;
constexpr float turn = 2 * pi;

double bound_down(double value) noexcept {
    return std::nextafter(value, -std::numeric_limits<double>::infinity());
}

double bound_up(double value) noexcept {
    return std::nextafter(value, std::numeric_limits<double>::infinity());
}

EffectBounds64 nonround_bounds64(const RoundedEffectInstance& instance, double guard) {
    validate_rounded_effect(instance);
    if (!std::isfinite(guard) || guard < 0) {
        throw std::invalid_argument("Rounded effect antialias guard must be non-negative");
    }
    const auto& geometry = instance.geometry;
    const auto* arc = std::get_if<RoundedArcPath>(&geometry.path);
    if (!arc || arc->cap == ArcCap::Round) {
        throw std::invalid_argument("Non-round bounds require a non-round arc");
    }
    const auto rect = translated(geometry.shape, geometry.translation).rect;
    if (geometry.outline_width == 0 || arc->sweep_radians == 0 || geometry.shape.radius == 0) {
        return {rect.x, rect.y, rect.x, rect.y};
    }
    // The body fits in R+H. Every Square corner fits in R+2H;
    // this conservative envelope needs no rounded world-space cap corners.
    const auto reach =
        bound_up((arc->cap == ArcCap::Square && arc->sweep_radians < turn ? double(geometry.outline_width)
                                                                          : double(geometry.outline_width) / 2) +
                 guard);
    return {bound_down(double(rect.x) - reach), bound_down(double(rect.y) - reach),
            bound_up(bound_up(double(rect.x) + rect.width) + reach),
            bound_up(bound_up(double(rect.y) + rect.height) + reach)};
}

EffectBounds64 clip_bounds64(EffectBounds64 bounds, runtime::Rect clip) noexcept {
    if (clip.width == 0 || clip.height == 0) {
        return {clip.x, clip.y, clip.x, clip.y};
    }
    // Also enclose the established float hard-clip endpoint if it rounded out.
    const auto right = std::max(bound_up(double(clip.x) + clip.width), double(clip.x + clip.width));
    const auto bottom = std::max(bound_up(double(clip.y) + clip.height), double(clip.y + clip.height));
    return {std::max(bounds.left, double(clip.x)), std::max(bounds.top, double(clip.y)), std::min(bounds.right, right),
            std::min(bounds.bottom, bottom)};
}

float outward_origin(double value) noexcept {
    const auto maximum = double(std::numeric_limits<float>::max());
    value = std::clamp(value, -maximum, maximum);
    auto result = static_cast<float>(value);
    if (double(result) > value) {
        result = std::nextafter(result, -std::numeric_limits<float>::infinity());
    }
    return result;
}

float outward_extent(float origin, double endpoint) noexcept {
    const auto distance = std::max(0.0, bound_up(endpoint - double(origin)));
    auto result = distance > std::numeric_limits<float>::max() ? std::numeric_limits<float>::infinity()
                                                               : static_cast<float>(distance);
    if (double(result) < distance || double(origin + result) < endpoint) {
        result = std::nextafter(result, std::numeric_limits<float>::infinity());
    }
    if (double(origin + result) < endpoint) {
        auto target = endpoint > std::numeric_limits<float>::max() ? std::numeric_limits<float>::infinity()
                                                                   : static_cast<float>(endpoint);
        if (double(target) < endpoint) {
            target = std::nextafter(target, std::numeric_limits<float>::infinity());
        }
        const auto extent = double(target) - origin;
        result = static_cast<float>(extent);
        if (double(result) < extent || double(origin + result) < endpoint) {
            result = std::nextafter(result, std::numeric_limits<float>::infinity());
        }
    }
    return result;
}

runtime::Rect outward_rect(EffectBounds64 bounds) noexcept {
    const auto left = outward_origin(bounds.left);
    const auto top = outward_origin(bounds.top);
    if (bounds.empty()) {
        return {left, top, 0, 0};
    }
    return {left, top, outward_extent(left, bounds.right), outward_extent(top, bounds.bottom)};
}

bool intersects_draw_clip(const RoundedEffectInstance& instance, runtime::Rect clip, double nonround_guard) {
    const auto* arc = std::get_if<RoundedArcPath>(&instance.geometry.path);
    return arc && arc->cap != ArcCap::Round ? !nonround_arc_draw_bounds(instance, clip, nonround_guard).empty()
                                            : !empty(intersect_effect_bounds(rounded_effect_bounds(instance), clip));
}

float positive_mod(float value, float period) noexcept {
    const auto result = std::fmod(value, period);
    return result < 0 ? result + period : result;
}

struct ClosestBoundary final {
    float square_distance{std::numeric_limits<float>::infinity()};
    float coordinate{};
    float length{};

    void consider(runtime::Point point, runtime::Point candidate, float along) noexcept {
        const auto dx = point.x - candidate.x;
        const auto dy = point.y - candidate.y;
        const auto square = dx * dx + dy * dy;
        if (square < square_distance) {
            square_distance = square;
            coordinate = along;
        }
    }

    void line(runtime::Point point, runtime::Point first, runtime::Point last) noexcept {
        const auto dx = last.x - first.x;
        const auto dy = last.y - first.y;
        const auto extent = std::hypot(dx, dy);
        const auto t =
            extent > 0
                ? std::clamp(((point.x - first.x) * dx + (point.y - first.y) * dy) / (extent * extent), 0.0F, 1.0F)
                : 0.0F;
        consider(point, {first.x + t * dx, first.y + t * dy}, length + t * extent);
        length += extent;
    }

    void arc(runtime::Point point, runtime::Point center, float radius, float start) noexcept {
        if (radius <= 0) {
            return;
        }
        const auto sample = [&](float angle) {
            consider(point, {center.x + radius * std::cos(start + angle), center.y + radius * std::sin(start + angle)},
                     length + angle * radius);
        };
        sample(0);
        sample(pi / 2);
        const auto angle = positive_mod(std::atan2(point.y - center.y, point.x - center.x) - start, turn);
        if (angle <= pi / 2) {
            sample(angle);
        }
        length += pi / 2 * radius;
    }
};

float corner_radius(runtime::Point point, LogicalRoundedRect shape, std::array<bool, 4> corners) noexcept {
    const auto right = point.x >= shape.rect.x + shape.rect.width / 2;
    const auto bottom = point.y >= shape.rect.y + shape.rect.height / 2;
    const auto corner = bottom ? (right ? 2U : 3U) : (right ? 1U : 0U);
    return corners[corner] ? shape.radius : 0;
}

[[nodiscard]] RoundedEffectGeometry geometry_of(const RoundedEffectInstance& instance) {
    return instance.geometry;
}

[[nodiscard]] bool drawable(const RoundedEffectInstance& instance) noexcept {
    // Opacity and color alpha are material state and may animate every frame.
    // Keep those instances packed so fades do not rebuild scene topology;
    // `visible` is the explicit structural culling switch.
    return instance.material.visible;
}

void append_command(std::vector<SceneDrawCommand>& output, SceneDrawCommand command) {
    if (command.instance_count == 0) {
        return;
    }
    if (!output.empty()) {
        auto& previous = output.back();
        if (previous.kind == command.kind && previous.atlas_page == command.atlas_page &&
            static_cast<std::uint64_t>(previous.first_instance) + previous.instance_count == command.first_instance) {
            const auto merged = static_cast<std::uint64_t>(previous.instance_count) + command.instance_count;
            if (merged > std::numeric_limits<std::uint32_t>::max()) {
                throw std::length_error("Rounded effect Scene command range exceeds uint32_t");
            }
            previous.instance_count = static_cast<std::uint32_t>(merged);
            return;
        }
    }
    output.push_back(command);
}

} // namespace

struct RoundedEffectStore::Slot final {
    std::optional<RoundedEffectInstance> instance;
    std::optional<std::uint32_t> packed_index;
    std::uint32_t generation{1};
};

RoundedEffectStore::RoundedEffectStore() = default;
RoundedEffectStore::~RoundedEffectStore() = default;
RoundedEffectStore::RoundedEffectStore(RoundedEffectStore&&) noexcept = default;
RoundedEffectStore& RoundedEffectStore::operator=(RoundedEffectStore&&) noexcept = default;

RoundedEffectInstance make_shadow_effect(LogicalRoundedRect shape, const ShadowLayer& layer, runtime::Point translation,
                                         std::optional<EffectClip> ancestor_clip) {
    RoundedEffectInstance instance{
        {
            shape,
            layer.kind == ShadowKind::outer ? RoundedEffectKind::outer_shadow : RoundedEffectKind::inset_shadow,
            layer.offset,
            layer.blur,
            layer.spread,
            0.0F,
            0.0F,
            translation,
            std::move(ancestor_clip),
        },
        {layer.color, 1.0F, true},
    };
    validate_rounded_effect(instance);
    return instance;
}

RoundedEffectInstance make_outline_effect(LogicalRoundedRect shape, float width, float offset, Color color,
                                          float opacity, runtime::Point translation,
                                          std::optional<EffectClip> ancestor_clip) {
    RoundedEffectInstance instance{
        {
            shape,
            RoundedEffectKind::outline,
            {},
            0.0F,
            0.0F,
            width,
            offset,
            translation,
            std::move(ancestor_clip),
        },
        {color, opacity, true},
    };
    validate_rounded_effect(instance);
    return instance;
}

RoundedEffectInstance make_arc_effect(runtime::Point center, float radius, float width, float start_degrees,
                                      float sweep_degrees, Color color, float opacity, runtime::Point translation,
                                      std::optional<EffectClip> ancestor_clip) {
    if (!finite(start_degrees) || !finite(sweep_degrees) || sweep_degrees < 0 || sweep_degrees > 360 ||
        !finite(radius) || radius < 0) {
        throw std::invalid_argument("Circular arc angles/radius are invalid");
    }
    RoundedEffectInstance instance;
    instance.geometry.shape = {{center.x - radius, center.y - radius, 2 * radius, 2 * radius}, radius};
    instance.geometry.kind = RoundedEffectKind::arc;
    instance.geometry.outline_width = width;
    instance.geometry.translation = translation;
    instance.geometry.ancestor_clip = std::move(ancestor_clip);
    instance.geometry.path = RoundedArcPath{std::remainder(start_degrees, 360.0F) * pi / 180, sweep_degrees * pi / 180};
    instance.material = {color, opacity, true};
    validate_rounded_effect(instance);
    return instance;
}

RoundedEffectInstance make_arc_effect(runtime::Point center, float radius, float width, float start_degrees,
                                      float sweep_degrees, ArcCap cap, Color color, float opacity,
                                      runtime::Point translation, std::optional<EffectClip> ancestor_clip) {
    auto instance = make_arc_effect(center, radius, width, start_degrees, sweep_degrees, color, opacity, translation,
                                    std::move(ancestor_clip));
    std::get<RoundedArcPath>(instance.geometry.path).cap = cap;
    validate_rounded_effect(instance);
    return instance;
}

RoundedEffectInstance make_dashed_outline_effect(LogicalRoundedRect shape, float width, RoundedDashPath pattern,
                                                 Color color, float opacity, runtime::Point translation,
                                                 std::optional<EffectClip> ancestor_clip) {
    RoundedEffectInstance instance;
    instance.geometry.shape = shape;
    instance.geometry.kind = RoundedEffectKind::dashed_outline;
    instance.geometry.outline_width = width;
    instance.geometry.translation = translation;
    instance.geometry.ancestor_clip = std::move(ancestor_clip);
    instance.geometry.path = pattern;
    instance.material = {color, opacity, true};
    validate_rounded_effect(instance);
    return instance;
}

RoundedEffectInstance make_tooltip_fill_effect(LogicalRoundedRect shape, TooltipArrowPath arrow, Color color,
                                               float opacity, runtime::Point translation,
                                               std::optional<EffectClip> ancestor_clip) {
    RoundedEffectInstance instance;
    instance.geometry.shape = shape;
    instance.geometry.kind = RoundedEffectKind::tooltip_fill;
    instance.geometry.translation = translation;
    instance.geometry.ancestor_clip = std::move(ancestor_clip);
    instance.geometry.path = arrow;
    instance.material = {color, opacity, true};
    validate_rounded_effect(instance);
    return instance;
}

void validate_rounded_effect(const RoundedEffectInstance& instance) {
    const auto& geometry = instance.geometry;
    validate_rect(geometry.shape.rect, "Rounded effect shape must be finite and non-negative");
    const float maximum_radius = 0.5F * std::min(geometry.shape.rect.width, geometry.shape.rect.height);
    if (!finite(geometry.shape.radius) || geometry.shape.radius < 0.0F || geometry.shape.radius > maximum_radius ||
        !finite(geometry.offset.x) || !finite(geometry.offset.y) || !finite(geometry.blur) || geometry.blur < 0.0F ||
        !finite(geometry.spread) || !finite(geometry.outline_width) || geometry.outline_width < 0.0F ||
        !finite(geometry.outline_offset) || geometry.outline_offset < 0.0F || !finite(geometry.translation.x) ||
        !finite(geometry.translation.y)) {
        throw std::invalid_argument("Rounded effect geometry is invalid");
    }
    switch (geometry.kind) {
    case RoundedEffectKind::outer_shadow:
    case RoundedEffectKind::inset_shadow:
        if (geometry.outline_width != 0.0F || geometry.outline_offset != 0.0F ||
            !std::holds_alternative<std::monostate>(geometry.path)) {
            throw std::invalid_argument("Shadow effect cannot contain outline geometry");
        }
        break;
    case RoundedEffectKind::outline:
        if (geometry.outline_width <= 0.0F || geometry.blur != 0.0F || geometry.spread != 0.0F ||
            geometry.offset != LogicalOffset{} || !std::holds_alternative<std::monostate>(geometry.path)) {
            throw std::invalid_argument("Outline effect geometry is invalid");
        }
        break;
    case RoundedEffectKind::arc: {
        const auto* arc = std::get_if<RoundedArcPath>(&geometry.path);
        if (!arc || !finite(arc->start_radians) || std::abs(arc->start_radians) > pi || !finite(arc->sweep_radians) ||
            arc->sweep_radians < 0 || arc->sweep_radians > turn || arc->cap > ArcCap::Square ||
            geometry.shape.rect.width != geometry.shape.rect.height ||
            geometry.shape.radius != geometry.shape.rect.width / 2 || geometry.outline_offset != 0 ||
            geometry.offset != LogicalOffset{} || geometry.blur != 0 || geometry.spread != 0) {
            throw std::invalid_argument("Circular arc geometry is invalid");
        }
        break;
    }
    case RoundedEffectKind::dashed_outline: {
        const auto* dash = std::get_if<RoundedDashPath>(&geometry.path);
        if (!dash || !finite(dash->length) || dash->length <= 0 || !finite(dash->gap) || dash->gap < 0 ||
            !finite(dash->phase) || !finite(dash->length + dash->gap) || geometry.outline_width <= 0 ||
            geometry.outline_width > maximum_radius || geometry.outline_offset != 0 ||
            geometry.offset != LogicalOffset{} || geometry.blur != 0 || geometry.spread != 0) {
            throw std::invalid_argument("Dashed outline geometry is invalid");
        }
        const auto sample = rounded_rect_path_sample({}, geometry.shape, dash->rounded_corners);
        if (!finite(sample.length) || !finite(sample.length / (dash->length + dash->gap))) {
            throw std::invalid_argument("Dashed outline perimeter is not representable");
        }
        break;
    }
    case RoundedEffectKind::tooltip_fill: {
        const auto* arrow = std::get_if<TooltipArrowPath>(&geometry.path);
        const auto horizontal = arrow && arrow->side <= TooltipArrowSide::top;
        const auto extent = horizontal ? geometry.shape.rect.width : geometry.shape.rect.height;
        const auto tolerance = extent * std::numeric_limits<float>::epsilon() * 4;
        if (!arrow || arrow->side > TooltipArrowSide::left || geometry.shape.rect.width <= 0 ||
            geometry.shape.rect.height <= 0 || !finite(arrow->center) || !finite(arrow->half_width) ||
            arrow->half_width <= 0 || !finite(arrow->length) || arrow->length <= 0 ||
            arrow->center - arrow->half_width < geometry.shape.radius - tolerance ||
            arrow->center + arrow->half_width > extent - geometry.shape.radius + tolerance ||
            geometry.outline_width != 0 || geometry.outline_offset != 0 || geometry.offset != LogicalOffset{} ||
            geometry.blur != 0 || geometry.spread != 0 || !finite(geometry.shape.rect.x - arrow->length) ||
            !finite(geometry.shape.rect.y - arrow->length) ||
            !finite(geometry.shape.rect.x + geometry.shape.rect.width + arrow->length) ||
            !finite(geometry.shape.rect.y + geometry.shape.rect.height + arrow->length)) {
            throw std::invalid_argument("Tooltip arrow must fit a straight body edge");
        }
        break;
    }
    default:
        throw std::invalid_argument("Rounded effect kind is invalid");
    }
    if (!finite(instance.material.opacity) || instance.material.opacity < 0.0F || instance.material.opacity > 1.0F) {
        throw std::invalid_argument("Rounded effect opacity must be in [0, 1]");
    }
    if (geometry.ancestor_clip.has_value()) {
        validate_rect(geometry.ancestor_clip->bounds, "Rounded effect ancestor clip must be finite and non-negative");
    }
}

float rounded_rect_signed_distance(runtime::Point point, LogicalRoundedRect shape) noexcept {
    if (shape.rect.width <= 0.0F || shape.rect.height <= 0.0F) {
        return std::numeric_limits<float>::infinity();
    }
    const float radius = std::clamp(shape.radius, 0.0F, 0.5F * std::min(shape.rect.width, shape.rect.height));
    const float center_x = shape.rect.x + shape.rect.width * 0.5F;
    const float center_y = shape.rect.y + shape.rect.height * 0.5F;
    const float half_width = shape.rect.width * 0.5F;
    const float half_height = shape.rect.height * 0.5F;
    const float qx = std::abs(point.x - center_x) - (half_width - radius);
    const float qy = std::abs(point.y - center_y) - (half_height - radius);
    const float outside = std::hypot(std::max(qx, 0.0F), std::max(qy, 0.0F));
    const float inside = std::min(std::max(qx, qy), 0.0F);
    return outside + inside - radius;
}

RoundedPathSample rounded_rect_path_sample(runtime::Point point, LogicalRoundedRect shape,
                                           std::array<bool, 4> rounded_corners) {
    const auto r = std::clamp(shape.radius, 0.0F, std::min(shape.rect.width, shape.rect.height) / 2);
    const auto x = shape.rect.x;
    const auto y = shape.rect.y;
    const auto right = x + shape.rect.width;
    const auto bottom = y + shape.rect.height;
    const std::array radii{rounded_corners[0] ? r : 0, rounded_corners[1] ? r : 0, rounded_corners[2] ? r : 0,
                           rounded_corners[3] ? r : 0};
    ClosestBoundary closest;
    closest.line(point, {x + radii[0], y}, {right - radii[1], y});
    closest.arc(point, {right - radii[1], y + radii[1]}, radii[1], -pi / 2);
    closest.line(point, {right, y + radii[1]}, {right, bottom - radii[2]});
    closest.arc(point, {right - radii[2], bottom - radii[2]}, radii[2], 0);
    closest.line(point, {right - radii[2], bottom}, {x + radii[3], bottom});
    closest.arc(point, {x + radii[3], bottom - radii[3]}, radii[3], pi / 2);
    closest.line(point, {x, bottom - radii[3]}, {x, y + radii[0]});
    closest.arc(point, {x + radii[0], y + radii[0]}, radii[0], pi);
    return {std::sqrt(closest.square_distance), closest.coordinate, closest.length};
}

float arc_signed_distance(runtime::Point point, LogicalRoundedRect centerline, float width,
                          RoundedArcPath arc) noexcept {
    if (arc.cap != ArcCap::Round) {
        return nonround_arc_signed_distance(point, centerline, width, arc);
    }
    if (arc.sweep_radians <= 0 || width <= 0) {
        return std::numeric_limits<float>::infinity();
    }
    const auto radius = centerline.rect.width / 2;
    const runtime::Point relative{point.x - centerline.rect.x - radius, point.y - centerline.rect.y - radius};
    const auto angle = positive_mod(std::atan2(relative.y, relative.x) - arc.start_radians, turn);
    auto distance = std::abs(std::hypot(relative.x, relative.y) - radius);
    if (arc.sweep_radians < turn && angle > arc.sweep_radians) {
        const auto endpoint_distance = [&](float theta) {
            return std::hypot(relative.x - radius * std::cos(theta), relative.y - radius * std::sin(theta));
        };
        distance =
            std::min(endpoint_distance(arc.start_radians), endpoint_distance(arc.start_radians + arc.sweep_radians));
    }
    return distance - width / 2;
}

float dashed_outline_coverage(runtime::Point point, LogicalRoundedRect shape, float width, RoundedDashPath dash,
                              float antialias_width) {
    if (shape.rect.width <= 0 || shape.rect.height <= 0) {
        return 0;
    }
    auto outer = shape;
    outer.radius = corner_radius(point, shape, dash.rounded_corners);
    const auto outer_coverage = gaussian_edge(rounded_rect_signed_distance(point, outer), 0, antialias_width);
    auto inner = shape;
    inner.rect = {shape.rect.x + width, shape.rect.y + width, shape.rect.width - 2 * width,
                  shape.rect.height - 2 * width};
    inner.radius = std::max(0.0F, outer.radius - width);
    const auto inner_coverage = gaussian_edge(rounded_rect_signed_distance(point, inner), 0, antialias_width);
    auto centerline = shape;
    centerline.rect = {shape.rect.x + width / 2, shape.rect.y + width / 2, shape.rect.width - width,
                       shape.rect.height - width};
    centerline.radius = std::max(0.0F, shape.radius - width / 2);
    const auto sample = rounded_rect_path_sample(point, centerline, dash.rounded_corners);
    if (sample.length <= 0 || dash.gap == 0) {
        return outer_coverage * (1 - inner_coverage);
    }
    const auto requested_period = dash.length + dash.gap;
    const auto count = std::max(1.0F, std::floor(sample.length / requested_period + 0.5F));
    const auto period = sample.length / count;
    const auto length = period * (dash.length / requested_period);
    const auto phase = positive_mod(sample.coordinate + std::remainder(dash.phase, period), period);
    const auto distance = phase <= length ? std::min(phase, length - phase) : -std::min(phase - length, period - phase);
    return outer_coverage * (1 - inner_coverage) * smoothstep(-antialias_width / 2, antialias_width / 2, distance);
}

float tooltip_signed_distance(runtime::Point point, LogicalRoundedRect shape, TooltipArrowPath arrow) noexcept {
    point.x -= shape.rect.x;
    point.y -= shape.rect.y;
    auto width = shape.rect.width;
    auto height = shape.rect.height;
    auto center = arrow.center;
    if (arrow.side == TooltipArrowSide::top) {
        point = {width - point.x, height - point.y};
        center = width - center;
    } else if (arrow.side == TooltipArrowSide::right) {
        point = {height - point.y, point.x};
        center = height - center;
        std::swap(width, height);
    } else if (arrow.side == TooltipArrowSide::left) {
        point = {point.y, width - point.x};
        std::swap(width, height);
    }
    const auto radius = shape.radius;
    ClosestBoundary boundary;
    boundary.line(point, {radius, 0}, {width - radius, 0});
    boundary.arc(point, {width - radius, radius}, radius, -pi / 2);
    boundary.line(point, {width, radius}, {width, height - radius});
    boundary.arc(point, {width - radius, height - radius}, radius, 0);
    // The base is internal to the union. Excluding it from the boundary
    // removes both the glyph phase seam and double-alpha overlap.
    boundary.line(point, {width - radius, height}, {center + arrow.half_width, height});
    boundary.line(point, {center + arrow.half_width, height}, {center, height + arrow.length});
    boundary.line(point, {center, height + arrow.length}, {center - arrow.half_width, height});
    boundary.line(point, {center - arrow.half_width, height}, {radius, height});
    boundary.arc(point, {radius, height - radius}, radius, pi / 2);
    boundary.line(point, {0, height - radius}, {0, radius});
    boundary.arc(point, {radius, radius}, radius, pi);
    const auto in_body = rounded_rect_signed_distance(point, {{0, 0, width, height}, radius}) <= 0;
    const auto in_arrow = point.y >= height && point.y <= height + arrow.length &&
                          std::abs(point.x - center) <= arrow.half_width * (1 - (point.y - height) / arrow.length);
    const auto distance = std::sqrt(boundary.square_distance);
    return in_body || in_arrow ? -distance : distance;
}

float rounded_effect_coverage(runtime::Point point, const RoundedEffectInstance& instance, float antialias_width) {
    validate_rounded_effect(instance);
    if (!finite(antialias_width) || antialias_width <= 0.0F) {
        throw std::invalid_argument("Rounded effect antialias width must be positive");
    }
    const auto& geometry = instance.geometry;
    const auto base = translated(geometry.shape, geometry.translation);
    switch (geometry.kind) {
    case RoundedEffectKind::outer_shadow: {
        auto shadow = spread_shape(base, geometry.spread);
        shadow.rect.x += geometry.offset.x;
        shadow.rect.y += geometry.offset.y;
        return gaussian_edge(rounded_rect_signed_distance(point, shadow), geometry.blur * 0.5F, antialias_width);
    }
    case RoundedEffectKind::inset_shadow: {
        const float surface_coverage = gaussian_edge(rounded_rect_signed_distance(point, base), 0.0F, antialias_width);
        auto shifted = base;
        shifted.rect.x += geometry.offset.x;
        shifted.rect.y += geometry.offset.y;
        const float distance_inside = -rounded_rect_signed_distance(point, shifted) - geometry.spread;
        return surface_coverage * gaussian_edge(distance_inside, geometry.blur * 0.5F, antialias_width);
    }
    case RoundedEffectKind::outline: {
        const float distance = rounded_rect_signed_distance(point, base);
        const float half_aa = antialias_width * 0.5F;
        const float inner = smoothstep(geometry.outline_offset - half_aa, geometry.outline_offset + half_aa, distance);
        const float outer = 1.0F - smoothstep(geometry.outline_offset + geometry.outline_width - half_aa,
                                              geometry.outline_offset + geometry.outline_width + half_aa, distance);
        return std::clamp(inner * outer, 0.0F, 1.0F);
    }
    case RoundedEffectKind::arc:
        return gaussian_edge(
            arc_signed_distance(point, base, geometry.outline_width, std::get<RoundedArcPath>(geometry.path)), 0,
            antialias_width);
    case RoundedEffectKind::dashed_outline:
        return dashed_outline_coverage(point, base, geometry.outline_width, std::get<RoundedDashPath>(geometry.path),
                                       antialias_width);
    case RoundedEffectKind::tooltip_fill:
        return gaussian_edge(tooltip_signed_distance(point, base, std::get<TooltipArrowPath>(geometry.path)), 0,
                             antialias_width);
    }
    return 0.0F;
}

Color rounded_effect_fragment_reference(runtime::Point point, const RoundedEffectInstance& instance,
                                        float antialias_width, float ancestor_opacity) {
    if (!finite(ancestor_opacity) || ancestor_opacity < 0 || ancestor_opacity > 1 || !finite(point.x) ||
        !finite(point.y)) {
        throw std::invalid_argument("Rounded effect sample point/ancestor opacity is invalid");
    }
    auto coverage = rounded_effect_coverage(point, instance, antialias_width);
    const auto& clip = instance.geometry.ancestor_clip;
    if (!instance.material.visible ||
        (clip && (point.x < clip->bounds.x || point.y < clip->bounds.y ||
                  point.x >= clip->bounds.x + clip->bounds.width || point.y >= clip->bounds.y + clip->bounds.height))) {
        coverage = 0;
    }
    const auto opacity = instance.material.opacity * ancestor_opacity;
    if (instance.material.paint) {
        const runtime::Point local{point.x - instance.geometry.translation.x,
                                   point.y - instance.geometry.translation.y};
        return effect_paint_reference(*instance.material.paint, local, coverage, opacity);
    }
    const auto color = instance.material.color;
    return {color.red(), color.green(), color.blue(), color.alpha() * coverage * opacity};
}

runtime::Rect rounded_effect_bounds(const RoundedEffectInstance& instance, float antialias_guard) {
    validate_rounded_effect(instance);
    if (!finite(antialias_guard) || antialias_guard < 0.0F) {
        throw std::invalid_argument("Rounded effect antialias guard must be non-negative");
    }
    const auto& geometry = instance.geometry;
    if (const auto* arc = std::get_if<RoundedArcPath>(&geometry.path); arc && arc->cap != ArcCap::Round) {
        auto endpoints = nonround_bounds64(instance, antialias_guard);
        if (geometry.ancestor_clip) {
            endpoints = clip_bounds64(endpoints, geometry.ancestor_clip->bounds);
        }
        return outward_rect(endpoints);
    }
    runtime::Rect bounds = translate(geometry.shape.rect, geometry.translation);
    if (empty(bounds) && geometry.kind != RoundedEffectKind::arc) {
        return bounds;
    }
    if (geometry.kind == RoundedEffectKind::arc &&
        (geometry.outline_width <= 0 || std::get<RoundedArcPath>(geometry.path).sweep_radians <= 0 ||
         (geometry.shape.radius == 0 && std::get<RoundedArcPath>(geometry.path).cap != ArcCap::Round))) {
        return {bounds.x, bounds.y, 0, 0};
    }
    switch (geometry.kind) {
    case RoundedEffectKind::outer_shadow: {
        const auto spread = spread_shape({bounds, geometry.shape.radius}, geometry.spread);
        if (empty(spread.rect)) {
            return spread.rect;
        }
        bounds = spread.rect;
        bounds.x += geometry.offset.x;
        bounds.y += geometry.offset.y;
        bounds = expand(bounds, 1.5F * geometry.blur + antialias_guard);
        break;
    }
    case RoundedEffectKind::inset_shadow:
        bounds = expand(bounds, antialias_guard);
        break;
    case RoundedEffectKind::outline:
        bounds = expand(bounds, geometry.outline_offset + geometry.outline_width + antialias_guard);
        break;
    case RoundedEffectKind::arc: {
        bounds = expand(bounds, geometry.outline_width / 2 + antialias_guard);
        break;
    }
    case RoundedEffectKind::dashed_outline:
        bounds = expand(bounds, antialias_guard);
        break;
    case RoundedEffectKind::tooltip_fill: {
        const auto& arrow = std::get<TooltipArrowPath>(geometry.path);
        if (arrow.side == TooltipArrowSide::bottom) {
            bounds.height += arrow.length;
        } else if (arrow.side == TooltipArrowSide::top) {
            bounds.y -= arrow.length;
            bounds.height += arrow.length;
        } else if (arrow.side == TooltipArrowSide::right) {
            bounds.width += arrow.length;
        } else {
            bounds.x -= arrow.length;
            bounds.width += arrow.length;
        }
        bounds = expand(bounds, antialias_guard);
        break;
    }
    }
    if (geometry.ancestor_clip.has_value()) {
        bounds = intersect_effect_bounds(bounds, geometry.ancestor_clip->bounds);
    }
    return bounds;
}

EffectBounds64 nonround_arc_draw_bounds(const RoundedEffectInstance& instance, runtime::Rect clip,
                                        double antialias_guard) {
    validate_rect(clip, "Rounded effect draw clip is invalid");
    auto bounds = nonround_bounds64(instance, antialias_guard);
    if (instance.geometry.ancestor_clip) {
        bounds = clip_bounds64(bounds, instance.geometry.ancestor_clip->bounds);
    }
    return clip_bounds64(bounds, clip);
}

std::array<RoundedEffectInstance, 4> make_corner_outline_effects(LogicalRoundedRect shape,
                                                                 std::array<bool, 4> rounded_corners, float width,
                                                                 float offset, Color color, float opacity,
                                                                 runtime::Point translation,
                                                                 std::optional<EffectClip> ancestor_clip) {
    std::array<RoundedEffectInstance, 4> result;
    if (!finite(width) || width < 0 || !finite(opacity) || opacity < 0 || opacity > 1) {
        throw std::invalid_argument("Corner outline width/opacity is invalid");
    }
    const float center_x = shape.rect.x + translation.x + shape.rect.width / 2;
    const float center_y = shape.rect.y + translation.y + shape.rect.height / 2;
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = make_outline_effect({shape.rect, rounded_corners[i] ? shape.radius : 0}, width == 0 ? 1 : width,
                                        offset, color, width == 0 ? 0 : opacity, translation);
        const auto bounds = rounded_effect_bounds(result[i]);
        const bool right = i == 1 || i == 2;
        const bool bottom = i >= 2;
        runtime::Rect clip{right ? center_x : bounds.x, bottom ? center_y : bounds.y,
                           right ? bounds.x + bounds.width - center_x : center_x - bounds.x,
                           bottom ? bounds.y + bounds.height - center_y : center_y - bounds.y};
        if (ancestor_clip) {
            clip = intersect_effect_bounds(clip, ancestor_clip->bounds);
        }
        result[i].geometry.ancestor_clip = EffectClip{static_cast<std::uint64_t>(i + 1), clip};
        validate_rounded_effect(result[i]);
    }
    return result;
}

std::array<RoundedEffectInstance, 4> make_corner_fill_effects(LogicalRoundedRect shape,
                                                              std::array<bool, 4> rounded_corners, Color color,
                                                              float opacity, runtime::Point translation,
                                                              std::optional<EffectClip> ancestor_clip) {
    auto result = make_corner_outline_effects(shape, rounded_corners, 1, 0, color, opacity, translation, ancestor_clip);
    for (auto& instance : result) {
        // Zero-blur outer coverage is the filled rounded rectangle. Quadrant
        // clips make its corners independent without adding a GPU shape kind.
        instance.geometry.kind = RoundedEffectKind::outer_shadow;
        instance.geometry.outline_width = 0;
    }
    return result;
}

std::array<RoundedEffectInstance, 4> make_corner_shadow_effects(LogicalRoundedRect shape,
                                                                std::array<bool, 4> rounded_corners,
                                                                const ShadowLayer& layer, runtime::Point translation,
                                                                std::optional<EffectClip> ancestor_clip) {
    std::array<RoundedEffectInstance, 4> result;
    const float center_x = shape.rect.x + translation.x + layer.offset.x + shape.rect.width / 2;
    const float center_y = shape.rect.y + translation.y + layer.offset.y + shape.rect.height / 2;
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = make_shadow_effect({shape.rect, rounded_corners[i] ? shape.radius : 0}, layer, translation);
        const auto bounds = rounded_effect_bounds(result[i]);
        const bool right = i == 1 || i == 2;
        const bool bottom = i >= 2;
        runtime::Rect clip{right ? center_x : bounds.x, bottom ? center_y : bounds.y,
                           std::max(0.0F, right ? bounds.x + bounds.width - center_x : center_x - bounds.x),
                           std::max(0.0F, bottom ? bounds.y + bounds.height - center_y : center_y - bounds.y)};
        if (ancestor_clip) {
            clip = intersect_effect_bounds(clip, ancestor_clip->bounds);
        }
        result[i].geometry.ancestor_clip = EffectClip{static_cast<std::uint64_t>(i + 1), clip};
        validate_rounded_effect(result[i]);
    }
    return result;
}

runtime::Rect intersect_effect_bounds(runtime::Rect bounds, runtime::Rect clip) noexcept {
    const float left = std::max(bounds.x, clip.x);
    const float top = std::max(bounds.y, clip.y);
    const float right = std::min(bounds.x + bounds.width, clip.x + clip.width);
    const float bottom = std::min(bounds.y + bounds.height, clip.y + clip.height);
    if (right <= left || bottom <= top) {
        return {left, top, 0.0F, 0.0F};
    }
    return {left, top, right - left, bottom - top};
}

void RoundedEffectStore::reserve(std::size_t capacity) {
    const auto slots_before = slots_.capacity();
    const auto order_before = draw_order_.capacity();
    const auto packed_before = packed_instances_.capacity();
    slots_.reserve(capacity);
    free_slots_.reserve(capacity);
    draw_order_.reserve(capacity);
    packed_instances_.reserve(capacity);
    material_dirty_ranges_.reserve(capacity);
    geometry_dirty_ranges_.reserve(capacity);
    note_capacity_growth(slots_before, order_before, packed_before);
}

RoundedEffectId RoundedEffectStore::add(RoundedEffectInstance instance) {
    const std::array values{instance};
    return add_batch(values).front();
}

std::vector<RoundedEffectId> RoundedEffectStore::add_batch(std::span<const RoundedEffectInstance> instances) {
    for (const auto& instance : instances) {
        validate_rounded_effect(instance);
    }
    if (instances.empty()) {
        return {};
    }
    if (diagnostics_.live_instances + instances.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("RoundedEffectStore exhausted instance identities");
    }

    const auto slots_before = slots_.capacity();
    const auto order_before = draw_order_.capacity();
    const auto packed_before = packed_instances_.capacity();
    const auto new_slots = instances.size() > free_slots_.size() ? instances.size() - free_slots_.size() : 0;
    slots_.reserve(slots_.size() + new_slots);
    free_slots_.reserve(slots_.size() + new_slots);
    draw_order_.reserve(draw_order_.size() + instances.size());
    packed_instances_.reserve(packed_instances_.size() + instances.size());
    std::vector<RoundedEffectId> result;
    result.reserve(instances.size());
    note_capacity_growth(slots_before, order_before, packed_before);

    for (const auto& instance : instances) {
        std::uint32_t index{};
        if (!free_slots_.empty()) {
            index = free_slots_.back();
            free_slots_.pop_back();
            ++diagnostics_.slot_reuses;
        } else {
            slots_.emplace_back();
            index = static_cast<std::uint32_t>(slots_.size() - 1);
        }
        auto& slot = slots_[index];
        slot.instance = instance;
        slot.packed_index.reset();
        const RoundedEffectId id{index, slot.generation};
        draw_order_.push_back(id);
        result.push_back(id);
    }
    diagnostics_.additions += instances.size();
    diagnostics_.live_instances += instances.size();
    compact_dirty_ = true;
    return result;
}

bool RoundedEffectStore::remove(RoundedEffectId id) {
    auto* slot = find_slot(id);
    if (slot == nullptr) {
        return false;
    }
    free_slots_.reserve(free_slots_.size() + 1);
    slot->instance.reset();
    slot->packed_index.reset();
    advance_generation(*slot);
    free_slots_.push_back(id.index);
    std::erase(draw_order_, id);
    ++diagnostics_.removals;
    --diagnostics_.live_instances;
    compact_dirty_ = true;
    return true;
}

bool RoundedEffectStore::contains(RoundedEffectId id) const noexcept {
    return find_slot(id) != nullptr;
}

const RoundedEffectInstance& RoundedEffectStore::at(RoundedEffectId id) const {
    return *require_slot(id).instance;
}

bool RoundedEffectStore::update_material(RoundedEffectId id, RoundedEffectMaterial material) {
    auto& slot = require_slot(id);
    auto candidate = *slot.instance;
    candidate.material = material;
    validate_rounded_effect(candidate);
    if (slot.instance->material == material) {
        return false;
    }
    const bool visibility_changed = drawable(*slot.instance) != drawable(candidate);
    if (!visibility_changed && slot.packed_index.has_value()) {
        // Prepare metadata before publishing the otherwise nonthrowing update.
        material_dirty_ranges_.append({*slot.packed_index, 1});
    }
    slot.instance->material = material;
    ++diagnostics_.material_updates;
    if (visibility_changed) {
        compact_dirty_ = true;
    } else if (slot.packed_index.has_value()) {
        packed_instances_[*slot.packed_index].material = material;
    }
    return true;
}

bool RoundedEffectStore::update_geometry(RoundedEffectId id, RoundedEffectGeometry geometry) {
    auto& slot = require_slot(id);
    auto candidate = *slot.instance;
    candidate.geometry = geometry;
    validate_rounded_effect(candidate);
    if (slot.instance->geometry == geometry) {
        return false;
    }
    bool needs_compaction = true;
    bool patch_packed = false;
    if (!compact_dirty_ && compact_clip_.has_value()) {
        const bool was_packed = slot.packed_index.has_value();
        const bool remains_drawable =
            drawable(candidate) && intersects_draw_clip(candidate, *compact_clip_, nonround_antialias_guard_);
        if (was_packed == remains_drawable) {
            needs_compaction = false;
            patch_packed = was_packed;
        }
    }
    if (patch_packed) {
        geometry_dirty_ranges_.append({*slot.packed_index, 1});
    }
    slot.instance->geometry = std::move(geometry);
    ++diagnostics_.geometry_updates;
    if (patch_packed) {
        packed_instances_[*slot.packed_index].geometry = slot.instance->geometry;
    }
    if (needs_compaction) {
        compact_dirty_ = true;
    }
    return true;
}

bool RoundedEffectStore::compact(runtime::Rect window_clip, double nonround_antialias_guard) {
    validate_clip(window_clip);
    if (!std::isfinite(nonround_antialias_guard) || nonround_antialias_guard < 0) {
        throw std::invalid_argument("Rounded effect compact AA guard is invalid");
    }
    if (!compact_dirty_ && compact_clip_ == window_clip && nonround_antialias_guard_ == nonround_antialias_guard) {
        ++diagnostics_.idle_compactions;
        return false;
    }
    const auto packed_before = packed_instances_.capacity();
    packed_instances_.clear();
    diagnostics_.culled_instances = 0;
    for (auto& slot : slots_) {
        slot.packed_index.reset();
    }
    for (const auto id : draw_order_) {
        auto* slot = find_slot(id);
        if (slot == nullptr || !drawable(*slot->instance)) {
            ++diagnostics_.culled_instances;
            continue;
        }
        if (!intersects_draw_clip(*slot->instance, window_clip, nonround_antialias_guard)) {
            ++diagnostics_.culled_instances;
            continue;
        }
        if (packed_instances_.size() >= std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("Rounded effect packed range exceeds uint32_t");
        }
        slot->packed_index = static_cast<std::uint32_t>(packed_instances_.size());
        packed_instances_.push_back(*slot->instance);
    }
    if (packed_instances_.capacity() != packed_before) {
        ++diagnostics_.capacity_growths;
    }
    material_dirty_ranges_.clear();
    geometry_dirty_ranges_.clear();
    if (!packed_instances_.empty()) {
        geometry_dirty_ranges_.append({
            0,
            static_cast<std::uint32_t>(packed_instances_.size()),
        });
    }
    compact_clip_ = window_clip;
    nonround_antialias_guard_ = nonround_antialias_guard;
    compact_dirty_ = false;
    ++diagnostics_.compactions;
    diagnostics_.packed_instances = packed_instances_.size();
    return true;
}

std::optional<std::uint32_t> RoundedEffectStore::packed_index(RoundedEffectId id) const {
    return require_slot(id).packed_index;
}

std::span<const RoundedEffectInstance> RoundedEffectStore::packed_instances() const noexcept {
    return packed_instances_;
}

std::span<const std::byte> RoundedEffectStore::bytes(RoundedEffectInstanceRange range) const {
    const auto end = static_cast<std::uint64_t>(range.first) + range.count;
    if (end > packed_instances_.size()) {
        throw std::out_of_range("Rounded effect byte range is out of bounds");
    }
    return std::as_bytes(std::span(packed_instances_).subspan(range.first, range.count));
}

std::span<const RoundedEffectInstanceRange> RoundedEffectStore::material_dirty_ranges() const noexcept {
    return material_dirty_ranges_.ranges();
}

std::span<const RoundedEffectInstanceRange> RoundedEffectStore::geometry_dirty_ranges() const noexcept {
    return geometry_dirty_ranges_.ranges();
}

void RoundedEffectStore::clear_dirty_ranges() noexcept {
    material_dirty_ranges_.clear();
    geometry_dirty_ranges_.clear();
}

std::size_t RoundedEffectStore::live_count() const noexcept {
    return diagnostics_.live_instances;
}

RoundedEffectRequirements RoundedEffectStore::live_requirements() const noexcept {
    RoundedEffectRequirements result;
    for (const auto& slot : slots_) {
        if (!slot.instance) {
            continue;
        }
        result.paint = result.paint || slot.instance->material.paint.has_value();
        if (const auto* arc = std::get_if<RoundedArcPath>(&slot.instance->geometry.path)) {
            result.nonround_caps = result.nonround_caps || arc->cap != ArcCap::Round;
        }
        if (result.paint && result.nonround_caps) {
            break;
        }
    }
    return result;
}

std::size_t RoundedEffectStore::slot_capacity() const noexcept {
    return slots_.capacity();
}

std::span<const RoundedEffectId> RoundedEffectStore::live_ids() const noexcept {
    return draw_order_;
}

const RoundedEffectStoreDiagnostics& RoundedEffectStore::diagnostics() const noexcept {
    return diagnostics_;
}

RoundedEffectStore::Slot* RoundedEffectStore::find_slot(RoundedEffectId id) noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    auto& slot = slots_[id.index];
    return slot.generation == id.generation && slot.instance.has_value() ? &slot : nullptr;
}

const RoundedEffectStore::Slot* RoundedEffectStore::find_slot(RoundedEffectId id) const noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    const auto& slot = slots_[id.index];
    return slot.generation == id.generation && slot.instance.has_value() ? &slot : nullptr;
}

RoundedEffectStore::Slot& RoundedEffectStore::require_slot(RoundedEffectId id) {
    if (auto* slot = find_slot(id)) {
        return *slot;
    }
    throw std::out_of_range("RoundedEffectId is stale or invalid");
}

const RoundedEffectStore::Slot& RoundedEffectStore::require_slot(RoundedEffectId id) const {
    if (const auto* slot = find_slot(id)) {
        return *slot;
    }
    throw std::out_of_range("RoundedEffectId is stale or invalid");
}

void RoundedEffectStore::advance_generation(Slot& slot) noexcept {
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
}

void RoundedEffectStore::validate_clip(runtime::Rect clip) {
    validate_rect(clip, "Rounded effect window clip must be finite and non-negative");
}

void RoundedEffectStore::note_capacity_growth(std::size_t slots_before, std::size_t order_before,
                                              std::size_t packed_before) noexcept {
    if (slots_.capacity() != slots_before || draw_order_.capacity() != order_before ||
        packed_instances_.capacity() != packed_before) {
        ++diagnostics_.capacity_growths;
    }
}

RoundedEffectPrimitive RoundedEffectScene::append_shadow_list(LogicalRoundedRect shape, const ShadowList& shadows,
                                                              runtime::Point translation,
                                                              std::optional<EffectClip> ancestor_clip) {
    std::vector<RoundedEffectInstance> pending;
    pending.reserve(shadows.size());
    for (const auto& layer : shadows.layers()) {
        pending.push_back(make_shadow_effect(shape, layer, translation, ancestor_clip));
    }
    RoundedEffectPrimitive primitive;
    primitive.before_fill.reserve(pending.size());
    primitive.after_fill.reserve(pending.size());
    const auto ids = store_.add_batch(pending);
    for (std::size_t index = 0; index < ids.size(); ++index) {
        if (pending[index].geometry.kind == RoundedEffectKind::inset_shadow) {
            primitive.after_fill.push_back(ids[index]);
        } else {
            primitive.before_fill.push_back(ids[index]);
        }
    }
    return primitive;
}

RoundedEffectPrimitive RoundedEffectScene::append_outline(LogicalRoundedRect shape, float width, float offset,
                                                          Color color, float opacity, runtime::Point translation,
                                                          std::optional<EffectClip> ancestor_clip) {
    RoundedEffectPrimitive primitive;
    primitive.before_fill.reserve(1);
    primitive.before_fill.push_back(
        store_.add(make_outline_effect(shape, width, offset, color, opacity, translation, std::move(ancestor_clip))));
    return primitive;
}

bool RoundedEffectScene::remove(const RoundedEffectPrimitive& primitive) {
    bool removed = false;
    for (const auto id : primitive.before_fill) {
        removed = store_.remove(id) || removed;
    }
    for (const auto id : primitive.after_fill) {
        removed = store_.remove(id) || removed;
    }
    return removed;
}

void RoundedEffectScene::compose_surface(const RoundedEffectPrimitive& primitive, SceneDrawCommand fill,
                                         std::vector<SceneDrawCommand>& output) const {
    append_effect_commands(primitive.before_fill, output);
    append_command(output, fill);
    append_effect_commands(primitive.after_fill, output);
}

RoundedEffectStore& RoundedEffectScene::store() noexcept {
    return store_;
}

const RoundedEffectStore& RoundedEffectScene::store() const noexcept {
    return store_;
}

void RoundedEffectScene::append_effect_commands(std::span<const RoundedEffectId> effects,
                                                std::vector<SceneDrawCommand>& output) const {
    for (const auto id : effects) {
        if (!store_.contains(id)) {
            continue;
        }
        const auto index = store_.packed_index(id);
        if (!index.has_value()) {
            continue;
        }
        append_command(output, {
                                   SceneDrawKind::rounded_effect,
                                   *index,
                                   1,
                                   invalid_glyph_atlas_page,
                               });
    }
}

} // namespace ryn::graphics
