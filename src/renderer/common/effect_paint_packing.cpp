#include "renderer/common/effect_paint_packing.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace ryn::detail {
namespace {

constexpr float turn = 2 * std::numbers::pi_v<float>;

void validate_domain(const EffectPaintGpuField& field) {
    if (field.reserved || !std::ranges::all_of(field.domain, [](float value) { return std::isfinite(value); })) {
        throw std::invalid_argument("Effect paint packed domain is not finite or reserved is nonzero");
    }
    if (field.kind == static_cast<std::uint32_t>(EffectPaintFieldKind::linear)) {
        const auto dx = field.domain[2] - field.domain[0];
        const auto dy = field.domain[3] - field.domain[1];
        const auto square = dx * dx + dy * dy;
        if (!std::isfinite(square) || square <= 0) {
            throw std::invalid_argument("Effect paint packed Linear domain is degenerate or overflows");
        }
    } else if (field.kind == static_cast<std::uint32_t>(EffectPaintFieldKind::conic)) {
        if (std::abs(field.domain[2]) > std::numbers::pi_v<float> || field.domain[3] <= 0 || field.domain[3] > turn) {
            throw std::invalid_argument("Effect paint packed Conic domain is invalid");
        }
    } else {
        throw std::invalid_argument("Effect paint packed field kind is invalid");
    }
}

void validate_field(const EffectPaintGpuField& field, std::span<const EffectPaintGpuStop> stops) {
    validate_domain(field);
    if (field.stop_count < 2 || std::uint64_t(field.first_stop) + field.stop_count > stops.size()) {
        throw std::out_of_range("Effect paint stop span is invalid");
    }
    const auto span = stops.subspan(field.first_stop, field.stop_count);
    for (std::size_t index = 0; index < span.size(); ++index) {
        const auto& stop = span[index];
        if (!std::isfinite(stop.position[0]) || stop.position[1] != 0 || stop.position[2] != 0 ||
            stop.position[3] != 0 || (index && stop.position[0] < span[index - 1].position[0])) {
            throw std::invalid_argument("Effect paint packed stop positions are invalid");
        }
        const auto alpha = stop.premultiplied_rgba[3];
        if (!std::isfinite(alpha) || alpha < 0 || alpha > 1) {
            throw std::invalid_argument("Effect paint packed stop alpha is invalid");
        }
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const auto value = stop.premultiplied_rgba[channel];
            if (!std::isfinite(value) || value < 0 || value > alpha) {
                throw std::invalid_argument("Effect paint packed stop RGB is not premultiplied");
            }
        }
    }
}

std::array<double, 4> sample_field(runtime::Point point, const EffectPaintGpuField& field,
                                   std::span<const EffectPaintGpuStop> stops) {
    double coordinate{};
    const auto dx = double(point.x) - field.domain[0];
    const auto dy = double(point.y) - field.domain[1];
    if (field.kind == static_cast<std::uint32_t>(EffectPaintFieldKind::linear)) {
        const auto ax = double(field.domain[2]) - field.domain[0];
        const auto ay = double(field.domain[3]) - field.domain[1];
        coordinate = (dx * ax + dy * ay) / (ax * ax + ay * ay);
    } else {
        constexpr auto tau = 2 * std::numbers::pi_v<double>;
        auto angle = dx == 0 && dy == 0 ? 0.0 : std::atan2(dy, dx) - field.domain[2];
        angle = std::fmod(angle, tau);
        if (angle < 0) {
            angle += tau;
        }
        coordinate = angle / (field.domain[3] == turn ? tau : double(field.domain[3]));
    }
    const auto span = stops.subspan(field.first_stop, field.stop_count);
    const auto next = std::upper_bound(span.begin(), span.end(), coordinate,
                                       [](double value, const auto& stop) { return value < stop.position[0]; });
    const auto& previous = next == span.begin() ? span.front() : *(next - 1);
    std::array<double, 4> result;
    std::ranges::copy(previous.premultiplied_rgba, result.begin());
    if (next != span.begin() && next != span.end()) {
        const auto t = (coordinate - previous.position[0]) / (double(next->position[0]) - previous.position[0]);
        for (std::size_t channel = 0; channel < result.size(); ++channel) {
            result[channel] += t * (next->premultiplied_rgba[channel] - result[channel]);
        }
    }
    return result;
}

float coverage(runtime::Point point, const RoundedEffectGpuInstance& base, std::uint32_t cap) {
    if (!cap) {
        return rounded_effect_gpu_coverage_reference(point, base);
    }
    if (point.x < base.clip_bounds[0] || point.y < base.clip_bounds[1] || point.x >= base.clip_bounds[2] ||
        point.y >= base.clip_bounds[3]) {
        return 0;
    }
    if (base.effect_params[3] != 3) {
        throw std::invalid_argument("Effect paint non-round cap requires an arc base");
    }
    const graphics::LogicalRoundedRect shape{
        {base.shape_rect[0], base.shape_rect[1], base.shape_rect[2], base.shape_rect[3]}, base.shadow_params[0]};
    const auto distance = graphics::arc_signed_distance(
        point, shape, base.effect_params[1],
        {base.shadow_params[1], base.shadow_params[2], static_cast<graphics::ArcCap>(cap)});
    const auto half_aa = base.material_params[1] / 2;
    const auto t = std::clamp((distance + half_aa) / (2 * half_aa), 0.0F, 1.0F);
    return 1 - t * t * (3 - 2 * t);
}

} // namespace

std::uint32_t effect_buffer_capacity(std::size_t required, std::uint32_t current, std::size_t stride,
                                     std::uint64_t maximum_bytes) {
    if (!stride || required > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Effect paint buffer count/stride exceeds integer limits");
    }
    auto capacity = static_cast<std::uint32_t>(required);
    if (capacity && capacity <= (std::uint32_t{1} << 31)) {
        capacity = std::bit_ceil(capacity);
    }
    capacity = std::max(capacity, current);
    if (capacity > maximum_bytes / stride || capacity > std::numeric_limits<std::size_t>::max() / stride) {
        throw std::length_error("Effect paint buffer capacity exceeds renderer input limit");
    }
    return capacity;
}

EffectPaintGpuField pack_effect_paint_field(const graphics::EffectPaintField& field, runtime::Point translation,
                                            SceneDeviceMetrics metrics, std::uint32_t first_stop) {
    validate_scene_device_metrics(metrics);
    const auto count = field.ramp().stops().size();
    if (count > std::numeric_limits<std::uint32_t>::max() ||
        std::uint64_t(first_stop) + count > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Effect paint packed stop span exceeds uint32_t");
    }
    const auto point = [&](runtime::Point value) {
        return runtime::Point{(value.x + translation.x) * metrics.display_scale,
                              (value.y + translation.y) * metrics.display_scale};
    };
    EffectPaintGpuField result;
    result.first_stop = first_stop;
    result.stop_count = static_cast<std::uint32_t>(count);
    if (const auto* linear = std::get_if<graphics::EffectLinearDomain>(&field.domain())) {
        result.kind = static_cast<std::uint32_t>(EffectPaintFieldKind::linear);
        const auto start = point(linear->start);
        const auto end = point(linear->end);
        result.domain = {start.x, start.y, end.x, end.y};
    } else {
        result.kind = static_cast<std::uint32_t>(EffectPaintFieldKind::conic);
        const auto& conic = std::get<graphics::EffectConicDomain>(field.domain());
        const auto center = point(conic.center);
        result.domain = {center.x, center.y, conic.start_radians, conic.sweep_radians};
    }
    validate_domain(result);
    return result;
}

void pack_effect_paint_tables(std::span<const graphics::RoundedEffectInstance> source, SceneDeviceMetrics metrics,
                              EffectPaintPackedTables& destination) {
    validate_scene_device_metrics(metrics);
    destination.links.clear();
    destination.fields.clear();
    destination.stops.clear();
    for (const auto& instance : source) {
        graphics::validate_rounded_effect(instance);
    }
    const bool active = std::ranges::any_of(source, [](const auto& instance) {
        const auto* arc = std::get_if<graphics::RoundedArcPath>(&instance.geometry.path);
        return instance.material.paint || (arc && arc->cap != graphics::ArcCap::Round);
    });
    if (!active) {
        return;
    }
    static_cast<void>(effect_buffer_capacity(source.size(), 0, sizeof(EffectPaintGpuLink),
                                             std::numeric_limits<std::uint64_t>::max()));
    destination.links.reserve(source.size());
    const auto append_field = [&](const graphics::EffectPaintField& field, runtime::Point translation) {
        if (destination.fields.size() >= effect_paint_no_field || destination.stops.size() > effect_paint_no_field) {
            throw std::length_error("Effect paint packed table index exceeds uint32_t");
        }
        const auto index = static_cast<std::uint32_t>(destination.fields.size());
        destination.fields.push_back(
            pack_effect_paint_field(field, translation, metrics, static_cast<std::uint32_t>(destination.stops.size())));
        for (const auto& stop : field.ramp().stops()) {
            const auto color = stop.color;
            destination.stops.push_back({{stop.position, 0, 0, 0},
                                         {color.red() * color.alpha(), color.green() * color.alpha(),
                                          color.blue() * color.alpha(), color.alpha()}});
        }
        return index;
    };
    for (const auto& instance : source) {
        EffectPaintGpuLink link;
        if (const auto* arc = std::get_if<graphics::RoundedArcPath>(&instance.geometry.path)) {
            link.arc_cap = static_cast<std::uint32_t>(arc->cap);
        }
        if (instance.material.paint) {
            link.foreground_field = append_field(instance.material.paint->foreground, instance.geometry.translation);
            if (instance.material.paint->underlay) {
                link.underlay_field = append_field(*instance.material.paint->underlay, instance.geometry.translation);
            }
        }
        destination.links.push_back(link);
    }
    if (destination.fields.empty()) {
        destination.fields.emplace_back();
        destination.stops.emplace_back();
    }
}

void validate_effect_paint_link(const EffectPaintGpuLink& link, std::span<const EffectPaintGpuField> fields,
                                std::span<const EffectPaintGpuStop> stops) {
    if (link.reserved || link.arc_cap > 2 ||
        (link.foreground_field == effect_paint_no_field && link.underlay_field != effect_paint_no_field)) {
        throw std::invalid_argument("Effect paint packed link is invalid");
    }
    for (const auto index : {link.foreground_field, link.underlay_field}) {
        if (index == effect_paint_no_field) {
            continue;
        }
        if (index >= fields.size()) {
            throw std::out_of_range("Effect paint field index is invalid");
        }
        validate_field(fields[index], stops);
    }
}

std::array<float, 4> effect_paint_gpu_fragment_reference(runtime::Point point_pixels,
                                                         const RoundedEffectGpuInstance& base,
                                                         const EffectPaintGpuLink& link,
                                                         std::span<const EffectPaintGpuField> fields,
                                                         std::span<const EffectPaintGpuStop> stops) {
    if (!std::isfinite(point_pixels.x) || !std::isfinite(point_pixels.y)) {
        throw std::invalid_argument("Effect paint packed sample point is invalid");
    }
    validate_effect_paint_link(link, fields, stops);
    const auto factor = coverage(point_pixels, base, link.arc_cap) * base.material_params[0];
    if (link.foreground_field == effect_paint_no_field) {
        return {base.color[0], base.color[1], base.color[2], base.color[3] * factor};
    }
    auto color = sample_field(point_pixels, fields[link.foreground_field], stops);
    if (link.underlay_field != effect_paint_no_field) {
        const auto underlay = sample_field(point_pixels, fields[link.underlay_field], stops);
        const auto remaining = 1 - color[3];
        for (std::size_t channel = 0; channel < color.size(); ++channel) {
            color[channel] += underlay[channel] * remaining;
        }
    }
    if (color[3] <= 0 || factor == 0) {
        return {};
    }
    return {static_cast<float>(color[0] / color[3]), static_cast<float>(color[1] / color[3]),
            static_cast<float>(color[2] / color[3]), static_cast<float>(color[3] * factor)};
}

} // namespace ryn::detail
