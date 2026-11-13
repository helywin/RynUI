#include "renderer/common/effect_paint_packing.hpp"
#include "renderer/common/effect_linear_exact.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace ryn::detail {
namespace {

constexpr float turn = 2 * std::numbers::pi_v<float>;

void validate_domain(const EffectPaintGpuField& field) {
    const auto domain = std::bit_cast<std::array<float, 4>>(field.raw_domain_bits);
    const auto affine = std::bit_cast<std::array<float, 4>>(field.affine_bits);
    if (field.reserved || field.affine_bits[3] ||
        !std::ranges::all_of(domain, [](float value) { return std::isfinite(value); }) || !std::isfinite(affine[0]) ||
        !std::isfinite(affine[1]) || !std::isfinite(affine[2]) || affine[2] <= 0) {
        throw std::invalid_argument("Effect paint packed domain is not finite or reserved is nonzero");
    }
    const auto physical = [&](float value, std::size_t axis) {
        return (value + affine[axis]) * affine[2];
    };
    const auto sx = physical(domain[0], 0);
    const auto sy = physical(domain[1], 1);
    if (!std::isfinite(sx) || !std::isfinite(sy)) {
        throw std::invalid_argument("Effect paint physical domain overflows");
    }
    if (field.kind == static_cast<std::uint32_t>(EffectPaintFieldKind::linear)) {
        // Preserve the v1 admission envelope; these rounded values are never
        // stored or used for sampling. Expanding the accepted domain is separate.
        const auto dx = physical(domain[2], 0) - sx;
        const auto dy = physical(domain[3], 1) - sy;
        const auto square = dx * dx + dy * dy;
        if (!std::isfinite(square) || square <= 0) {
            throw std::invalid_argument("Effect paint packed Linear domain is degenerate or overflows");
        }
    } else if (field.kind == static_cast<std::uint32_t>(EffectPaintFieldKind::conic)) {
        if (std::abs(domain[2]) > std::numbers::pi_v<float> || domain[3] <= 0 || domain[3] > turn) {
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
    const auto span = stops.subspan(field.first_stop, field.stop_count);
    if (field.kind == static_cast<std::uint32_t>(EffectPaintFieldKind::linear)) {
        using namespace effect_linear_exact;
        const Inputs raw{{std::bit_cast<std::uint32_t>(point.x), std::bit_cast<std::uint32_t>(point.y),
                          field.raw_domain_bits[0], field.raw_domain_bits[1], field.raw_domain_bits[2],
                          field.raw_domain_bits[3], field.affine_bits[0], field.affine_bits[1]},
                         field.affine_bits[2]};
        const auto selection =
            select(raw, field.stop_count, [&](std::uint32_t index) { return span[index].position[0]; });
        const auto next = span.begin() + selection.next;
        const auto& previous = next == span.begin() ? span.front() : *(next - 1);
        std::array<double, 4> result;
        std::ranges::copy(previous.premultiplied_rgba, result.begin());
        if (next != span.begin() && next != span.end()) {
            const auto fraction = selection.fraction;
            for (std::size_t channel = 0; channel < result.size(); ++channel) {
                result[channel] += double(fraction) * (next->premultiplied_rgba[channel] - result[channel]);
            }
        }
        return result;
    }
    // Conic remains an approximate, disabled candidate; raw inputs do not
    // certify the transcendental predicates or the following double arithmetic.
    const auto domain = std::bit_cast<std::array<float, 4>>(field.raw_domain_bits);
    const auto affine = std::bit_cast<std::array<float, 4>>(field.affine_bits);
    double coordinate{};
    const auto dx = double(point.x) - double(affine[2]) * (double(affine[0]) + domain[0]);
    const auto dy = double(point.y) - double(affine[2]) * (double(affine[1]) + domain[1]);
    {
        constexpr auto tau = 2 * std::numbers::pi_v<double>;
        auto angle = dx == 0 && dy == 0 ? 0.0 : std::atan2(dy, dx) - domain[2];
        angle = std::fmod(angle, tau);
        if (angle < 0) {
            angle += tau;
        }
        coordinate = angle / (domain[3] == turn ? tau : double(domain[3]));
    }
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

float coverage(runtime::Point point, const RoundedEffectGpuInstance& base, std::uint32_t cap,
               const CapGeometryGpuV1* geometry) {
    if (!cap) {
        return rounded_effect_gpu_coverage_reference(point, base);
    }
    if (!geometry) {
        throw std::invalid_argument("Non-round packed reference requires Cap Geometry ABI v1 bytes");
    }
    return cap_geometry_coverage_reference(point, base, cap, *geometry);
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
    EffectPaintGpuField result;
    result.first_stop = first_stop;
    result.stop_count = static_cast<std::uint32_t>(count);
    result.affine_bits = std::bit_cast<std::array<std::uint32_t, 4>>(
        std::array{translation.x, translation.y, metrics.display_scale, 0.0F});
    if (const auto* linear = std::get_if<graphics::EffectLinearDomain>(&field.domain())) {
        result.kind = static_cast<std::uint32_t>(EffectPaintFieldKind::linear);
        result.raw_domain_bits = std::bit_cast<std::array<std::uint32_t, 4>>(
            std::array{linear->start.x, linear->start.y, linear->end.x, linear->end.y});
    } else {
        result.kind = static_cast<std::uint32_t>(EffectPaintFieldKind::conic);
        const auto& conic = std::get<graphics::EffectConicDomain>(field.domain());
        result.raw_domain_bits = std::bit_cast<std::array<std::uint32_t, 4>>(
            std::array{conic.center.x, conic.center.y, conic.start_radians, conic.sweep_radians});
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

std::array<float, 4>
effect_paint_gpu_fragment_reference(runtime::Point point_pixels, const RoundedEffectGpuInstance& base,
                                    const EffectPaintGpuLink& link, std::span<const EffectPaintGpuField> fields,
                                    std::span<const EffectPaintGpuStop> stops, const CapGeometryGpuV1* cap_geometry) {
    if (!std::isfinite(point_pixels.x) || !std::isfinite(point_pixels.y)) {
        throw std::invalid_argument("Effect paint packed sample point is invalid");
    }
    validate_effect_paint_link(link, fields, stops);
    const auto factor = coverage(point_pixels, base, link.arc_cap, cap_geometry) * base.material_params[0];
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
