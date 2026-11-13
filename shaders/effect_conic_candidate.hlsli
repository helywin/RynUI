#ifndef RYNUI_EFFECT_CONIC_CANDIDATE
#define RYNUI_EFFECT_CONIC_CANDIDATE

#include "cap_number.hlsli"

// Candidate evaluation, deliberately separate from the exact dyadic Linear
// predicates. No claim of a certified all-input transcendental error bound.
// The product capability stays OFF until stop-crossing uncertainty is closed.
CapNumber EffectAtanUnit(CapNumber ratio) {
    bool reduce = CapCompare(ratio, CapSmall(0.5)) > 0;
    CapNumber value = ratio;
    if (reduce) {
        value = CapDivide(CapSubtract(ratio, CapSmall(1.0)), CapAdd(ratio, CapSmall(1.0)));
    }
    CapNumber square = CapMultiply(value, value);
    CapNumber term = value;
    CapNumber result = value;
    // |value| <= 1/2. Pure series remainder < 2^-232;
    // truncation/reciprocal/error certification is a separate outstanding gate.
    [loop] for (uint i = 1u; i < 116u; ++i) {
        term = CapNegate(CapMultiply(term, square));
        result = CapAdd(result, CapDivideSmall(term, 2u * i + 1u));
    }
    if (reduce) {
        return CapAdd(CapScale2(CapPi(), -2), result);
    }
    return result;
}

CapNumber EffectConicCoordinate(float2 sample_position, uint4 domain, uint4 affine) {
    CapNumber scale = CapFromBits(affine.z);
    CapNumber dx = CapSubtract(CapSmall(sample_position.x),
                               CapMultiply(scale, CapAdd(CapFromBits(affine.x), CapFromBits(domain.x))));
    CapNumber dy = CapSubtract(CapSmall(sample_position.y),
                               CapMultiply(scale, CapAdd(CapFromBits(affine.y), CapFromBits(domain.y))));
    if (CapZero(dx) && CapZero(dy)) {
        return (CapNumber)0;
    }
    CapNumber x = CapAbs(dx);
    CapNumber y = CapAbs(dy);
    CapNumber halfPi = CapScale2(CapPi(), -1);
    CapNumber angle;
    if (CapCompare(x, y) >= 0) {
        angle = EffectAtanUnit(CapDivide(y, x));
    } else {
        angle = CapSubtract(halfPi, EffectAtanUnit(CapDivide(x, y)));
    }
    if (dx.negative != 0u) {
        angle = CapSubtract(CapPi(), angle);
    }
    if (dy.negative != 0u) {
        angle = CapNegate(angle);
    }
    CapNumber tau = CapScale2(CapPi(), 1);
    angle = CapSubtract(angle, CapFromBits(domain.z));
    // pi_f is slightly larger than real pi: angle-start can extend just
    // beyond either +/-tau, so one addition alone is insufficient.
    [unroll] for (uint i = 0u; i < 2u; ++i) {
        if (angle.negative != 0u) {
            angle = CapAdd(angle, tau);
        } else if (CapCompare(angle, tau) >= 0) {
            angle = CapSubtract(angle, tau);
        }
    }
    // Preserve the CPU full-turn sentinel and true-tau wrap/divisor.
    CapNumber sweep = CapFromBits(domain.w);
    if (domain.w == asuint(6.28318530717958647692)) {
        sweep = tau;
    }
    return CapDivide(angle, sweep);
}

#endif
