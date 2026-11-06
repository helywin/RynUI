#include "graphics/effect_paint.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace ryn::graphics {
namespace {

constexpr float turn = 2 * std::numbers::pi_v<float>;

bool finite(runtime::Point point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

using Premultiplied = std::array<double, 4>;

Premultiplied premultiplied(Color color) noexcept {
    return {double(color.red()) * color.alpha(), double(color.green()) * color.alpha(),
            double(color.blue()) * color.alpha(), color.alpha()};
}

Color straight(Premultiplied color, float factor = 1) {
    if (color[3] <= 0 || factor == 0) {
        return {};
    }
    return {static_cast<float>(std::clamp(color[0] / color[3], 0.0, 1.0)),
            static_cast<float>(std::clamp(color[1] / color[3], 0.0, 1.0)),
            static_cast<float>(std::clamp(color[2] / color[3], 0.0, 1.0)),
            static_cast<float>(std::clamp(color[3] * factor, 0.0, 1.0))};
}

Premultiplied sample_ramp(const EffectGradientRamp& ramp, double coordinate) {
    const auto stops = ramp.stops();
    const auto next =
        std::upper_bound(stops.begin(), stops.end(), coordinate,
                         [](double value, const EffectGradientStop& stop) { return value < stop.position; });
    if (next == stops.begin()) {
        return premultiplied(stops.front().color);
    }
    if (next == stops.end()) {
        return premultiplied(stops.back().color);
    }
    const auto& previous = *(next - 1);
    const auto fraction = (coordinate - previous.position) / (double(next->position) - previous.position);
    auto result = premultiplied(previous.color);
    const auto end = premultiplied(next->color);
    for (std::size_t channel = 0; channel < result.size(); ++channel) {
        result[channel] += fraction * (end[channel] - result[channel]);
    }
    return result;
}

Premultiplied sample_field(const EffectPaintField& field, runtime::Point point) {
    if (!finite(point)) {
        throw std::invalid_argument("Effect paint sample point must be finite");
    }
    double coordinate{};
    if (const auto* linear = std::get_if<EffectLinearDomain>(&field.domain())) {
        const auto dx = double(linear->end.x) - linear->start.x;
        const auto dy = double(linear->end.y) - linear->start.y;
        coordinate =
            ((double(point.x) - linear->start.x) * dx + (double(point.y) - linear->start.y) * dy) / (dx * dx + dy * dy);
    } else {
        const auto& conic = std::get<EffectConicDomain>(field.domain());
        const auto dx = double(point.x) - conic.center.x;
        const auto dy = double(point.y) - conic.center.y;
        // The center has no direction; assign the field's start deterministically.
        auto angle = dx == 0 && dy == 0 ? 0.0 : std::atan2(dy, dx) - conic.start_radians;
        constexpr auto full_turn = 2 * std::numbers::pi_v<double>;
        angle = std::fmod(angle, full_turn);
        if (angle < 0) {
            angle += full_turn;
        }
        coordinate = angle / (conic.sweep_radians == turn ? full_turn : double(conic.sweep_radians));
    }
    return sample_ramp(field.ramp(), coordinate);
}

} // namespace

EffectGradientRamp::EffectGradientRamp(std::span<const EffectGradientStop> stops) {
    if (stops.size() < 2) {
        throw std::invalid_argument("Effect gradient ramp requires at least two stops");
    }
    for (std::size_t index = 0; index < stops.size(); ++index) {
        if (!std::isfinite(stops[index].position) || (index && stops[index].position < stops[index - 1].position)) {
            throw std::invalid_argument("Effect gradient stops must be finite and nondecreasing");
        }
    }
    stops_ = std::make_shared<const std::vector<EffectGradientStop>>(stops.begin(), stops.end());
}

EffectGradientRamp::EffectGradientRamp(std::initializer_list<EffectGradientStop> stops)
    : EffectGradientRamp(std::span<const EffectGradientStop>{stops.begin(), stops.size()}) {}

std::span<const EffectGradientStop> EffectGradientRamp::stops() const noexcept {
    return *stops_;
}

bool operator==(const EffectGradientRamp& left, const EffectGradientRamp& right) noexcept {
    return left.stops_ == right.stops_ || *left.stops_ == *right.stops_;
}

EffectPaintField::EffectPaintField(EffectGradientRamp ramp, EffectLinearDomain domain)
    : ramp_(std::move(ramp)), domain_(domain) {
    if (!finite(domain.start) || !finite(domain.end) || domain.start == domain.end) {
        throw std::invalid_argument("Effect linear domain must have finite distinct endpoints");
    }
}

EffectPaintField::EffectPaintField(EffectGradientRamp ramp, EffectConicDomain domain)
    : ramp_(std::move(ramp)), domain_(domain) {
    if (!finite(domain.center) || !std::isfinite(domain.start_radians) || !std::isfinite(domain.sweep_radians) ||
        domain.sweep_radians <= 0 || domain.sweep_radians > turn) {
        throw std::invalid_argument("Effect conic domain must have a finite center/angle and sweep in (0, 2pi]");
    }
    domain.start_radians = std::remainder(domain.start_radians, turn);
    // Give equivalent rotations one value, including the +/-pi seam.
    if (domain.start_radians == std::numbers::pi_v<float>) {
        domain.start_radians = -std::numbers::pi_v<float>;
    }
    domain_ = domain;
}

const EffectGradientRamp& EffectPaintField::ramp() const noexcept {
    return ramp_;
}

const EffectPaintDomain& EffectPaintField::domain() const noexcept {
    return domain_;
}

Color effect_ramp_reference(const EffectGradientRamp& ramp, float coordinate) {
    if (!std::isfinite(coordinate)) {
        throw std::invalid_argument("Effect gradient coordinate must be finite");
    }
    return straight(sample_ramp(ramp, coordinate));
}

Color effect_field_reference(const EffectPaintField& field, runtime::Point point) {
    return straight(sample_field(field, point));
}

Color effect_paint_reference(const EffectPaint& paint, runtime::Point point, float coverage, float opacity) {
    if (!std::isfinite(coverage) || coverage < 0 || coverage > 1 || !std::isfinite(opacity) || opacity < 0 ||
        opacity > 1) {
        throw std::invalid_argument("Effect paint coverage/opacity must be in [0, 1]");
    }
    auto result = sample_field(paint.foreground, point);
    if (paint.underlay) {
        const auto underlay = sample_field(*paint.underlay, point);
        const auto remaining = 1 - result[3];
        for (std::size_t channel = 0; channel < result.size(); ++channel) {
            result[channel] += underlay[channel] * remaining;
        }
    }
    return straight(result, coverage * opacity);
}

} // namespace ryn::graphics
