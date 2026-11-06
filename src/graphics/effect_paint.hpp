#pragma once

#include "runtime/geometry.hpp"

#include <ryn/design_token.hpp>

#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace ryn::graphics {

struct EffectGradientStop final {
    float position{};
    Color color;

    friend constexpr bool operator==(EffectGradientStop, EffectGradientStop) = default;
};

// Copies caller storage. Positions are finite and nondecreasing, with duplicates
// and positions outside [0, 1] preserved. Component-specific stop fixup belongs
// to the component, not this common representation.
class EffectGradientRamp final {
public:
    explicit EffectGradientRamp(std::span<const EffectGradientStop> stops);
    EffectGradientRamp(std::initializer_list<EffectGradientStop> stops);
    EffectGradientRamp(const EffectGradientRamp&) noexcept = default;
    EffectGradientRamp& operator=(const EffectGradientRamp&) noexcept = default;

    [[nodiscard]] std::span<const EffectGradientStop> stops() const noexcept;
    friend bool operator==(const EffectGradientRamp& left, const EffectGradientRamp& right) noexcept;

private:
    std::shared_ptr<const std::vector<EffectGradientStop>> stops_;
};

struct EffectLinearDomain final {
    runtime::Point start;
    runtime::Point end;

    friend constexpr bool operator==(EffectLinearDomain, EffectLinearDomain) = default;
};

struct EffectConicDomain final {
    runtime::Point center;
    float start_radians{};
    float sweep_radians{};

    friend constexpr bool operator==(EffectConicDomain, EffectConicDomain) = default;
};

using EffectPaintDomain = std::variant<EffectLinearDomain, EffectConicDomain>;

class EffectPaintField final {
public:
    EffectPaintField(EffectGradientRamp ramp, EffectLinearDomain domain);
    EffectPaintField(EffectGradientRamp ramp, EffectConicDomain domain);

    [[nodiscard]] const EffectGradientRamp& ramp() const noexcept;
    [[nodiscard]] const EffectPaintDomain& domain() const noexcept;
    friend bool operator==(const EffectPaintField&, const EffectPaintField&) = default;

private:
    EffectGradientRamp ramp_;
    EffectPaintDomain domain_;
};

struct EffectPaint final {
    EffectPaintField foreground;
    std::optional<EffectPaintField> underlay;

    friend bool operator==(const EffectPaint&, const EffectPaint&) = default;
};

// References operate in the existing Color encoding, interpolate premultiplied
// RGBA, then return straight RGBA. A duplicate stop is right-continuous.
[[nodiscard]] Color effect_ramp_reference(const EffectGradientRamp& ramp, float coordinate);
[[nodiscard]] Color effect_field_reference(const EffectPaintField& field, runtime::Point point);
// Compose both fields first, then apply the common coverage/opacity once.
[[nodiscard]] Color effect_paint_reference(const EffectPaint& paint, runtime::Point point, float coverage = 1,
                                           float opacity = 1);

} // namespace ryn::graphics
