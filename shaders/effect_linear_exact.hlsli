#ifndef RYNUI_EFFECT_LINEAR_EXACT
#define RYNUI_EFFECT_LINEAR_EXACT

// Original raw72 uses quantum 2^-447, then N<<149 and stop products at 2^-596.
// The bounded12 instance uses exact common factors q/c, never truncation.
struct EffectFactor {
    uint m;
    int e;
    int l;
    bool negative;
};

uint EffectSignificand(uint bits) {
    return (bits & 0x7fffffu) | ((bits & 0x7f800000u) == 0u ? 0u : 0x800000u);
}

uint EffectIntegerShift(uint bits) {
    uint biased = (bits >> 23u) & 255u;
    return biased == 0u ? 0u : biased - 1u;
}

EffectFactor EffectRawFactor(uint bits) {
    EffectFactor result = (EffectFactor)0;
    result.m = EffectSignificand(bits);
    result.e = (int)EffectIntegerShift(bits);
    result.negative = (bits >> 31u) != 0u;
    return result;
}

bool EffectDecode(uint bits, out EffectFactor result) {
    result = (EffectFactor)0;
    uint biased = (bits >> 23u) & 255u;
    if (biased == 255u) {
        return false;
    }
    uint u = EffectSignificand(bits);
    if (u == 0u) {
        return true; // +/-0: no ctz/bitlen, no exponent sentinel arithmetic.
    }
    uint z = 0u;
    [unroll] for (uint step = 16u; step > 0u; step >>= 1u) {
        if ((u & ((1u << step) - 1u)) == 0u) {
            u >>= step;
            z += step;
        }
    }
    result.m = u;
    result.e = (biased == 0u ? -149 : (int)biased - 150) + (int)z;
    result.l = 1;
    [unroll] for (uint step = 16u; step > 0u; step >>= 1u) {
        if ((u >> step) != 0u) {
            u >>= step;
            result.l += (int)step;
        }
    }
    result.negative = (bits >> 31u) != 0u;
    return true;
}

struct EffectExactSmallProduct {
    uint digit[5];
};

EffectExactSmallProduct EffectExactSmallTimesSignificand(EffectExactSmallProduct value, uint significand) {
    EffectExactSmallProduct result = (EffectExactSmallProduct)0;
    [unroll] for (uint j = 0u; j < 2u; ++j) {
        uint multiplier = (significand >> (16u * j)) & 65535u;
        uint carry = 0u;
        [unroll] for (uint i = 0u; i < 5u - j; ++i) {
            // 65535^2 + 65535 + 65535 = 2^32-1; positive product <2^72.
            uint product = value.digit[i] * multiplier + result.digit[i + j] + carry;
            result.digit[i + j] = product & 65535u;
            carry = product >> 16u;
        }
    }
    return result;
}

#define RYNUI_FIXED_TYPE EffectExact
#define RYNUI_FIXED_NAME(name) EffectExact##name
#define RYNUI_FIXED_DIGITS 72
#include "effect_linear_fixed.hlsli"
#undef RYNUI_FIXED_TYPE
#undef RYNUI_FIXED_NAME
#undef RYNUI_FIXED_DIGITS

#define RYNUI_FIXED_TYPE EffectNarrow
#define RYNUI_FIXED_NAME(name) EffectNarrow##name
#define RYNUI_FIXED_DIGITS 12
#include "effect_linear_fixed.hlsli"
#undef RYNUI_FIXED_TYPE
#undef RYNUI_FIXED_NAME
#undef RYNUI_FIXED_DIGITS

void EffectExactProduct(inout EffectExact sum, uint a, uint b, uint c, bool subtract) {
    EffectExactFactorProduct(sum, EffectRawFactor(a), EffectRawFactor(b), EffectRawFactor(c), 0, subtract);
}

void EffectLinearAxis(uint sample_position, uint start, uint end, uint translation, uint scale, inout EffectExact n,
                      inout EffectExact d) {
    // Raw one=(2^23,126), not canonical (1,0). Preserve the original grid.
    EffectExactAxis(EffectRawFactor(sample_position), EffectRawFactor(start), EffectRawFactor(end),
                    EffectRawFactor(translation), EffectRawFactor(scale), EffectRawFactor(0x3f800000u), 0, n, d);
}

EffectExact EffectExactStopProduct(EffectExact d, float stop) {
    uint bits = asuint(stop);
    EffectExact result = EffectExactTimesSignificand(d, EffectSignificand(bits));
    result = EffectExactShift(result, EffectIntegerShift(bits));
    if ((bits >> 31u) != 0u) {
        result = EffectExactAdd((EffectExact)0, result, true);
    }
    return result;
}

// Private failure codes mirror CPU Failure; no resource/mode/ABI field is added.
static const uint EffectLinearFailureNone = 0u;
static const uint EffectLinearFailureInput = 1u;
static const uint EffectLinearFailureSetup = 2u;
static const uint EffectLinearFailureDenominator = 3u;
static const uint EffectLinearFailureComparison = 4u;
static const uint EffectLinearFailurePair = 5u;

struct EffectSetup192 {
    EffectFactor coordinates[8];
    EffectFactor scale;
    int q;
    int high;
};

uint EffectSetupLinear192(uint2 sample_position, uint4 domain, uint4 affine, out EffectSetup192 result) {
    result = (EffectSetup192)0;
    uint raw[8] = {sample_position.x, sample_position.y, domain.x, domain.y, domain.z, domain.w, affine.x, affine.y};
    int b = 0;
    int h = 1;
    [unroll] for (uint i = 0u; i < 8u; ++i) {
        if (!EffectDecode(raw[i], result.coordinates[i])) {
            return EffectLinearFailureInput;
        }
        EffectFactor value = result.coordinates[i];
        if (value.m != 0u) {
            b = min(b, value.e);
            h = max(h, value.e + value.l);
        }
    }
    if (!EffectDecode(affine.z, result.scale) || result.scale.m == 0u || result.scale.negative) {
        return EffectLinearFailureInput;
    }
    result.q = 2 * b + min(0, result.scale.e);
    result.high = 2 * h + max(0, result.scale.e + result.scale.l) + 4;
    return result.high - result.q + 1 <= 192 ? EffectLinearFailureNone : EffectLinearFailureSetup;
}

void EffectCoordinate192(EffectSetup192 setup, out EffectNarrow n, out EffectNarrow d) {
    n = (EffectNarrow)0;
    d = (EffectNarrow)0;
    EffectFactor one = (EffectFactor)0;
    one.m = 1u;
    one.l = 1;
    EffectNarrowAxis(setup.coordinates[0], setup.coordinates[2], setup.coordinates[4], setup.coordinates[6],
                     setup.scale, one, setup.q, n, d);
    EffectNarrowAxis(setup.coordinates[1], setup.coordinates[3], setup.coordinates[5], setup.coordinates[7],
                     setup.scale, one, setup.q, n, d);
}

struct EffectStopGrid {
    int k;
    int g;
};

void EffectIncludeStop(inout EffectStopGrid grid, EffectFactor stop) {
    if (stop.m != 0u) {
        grid.k = min(grid.k, stop.e);
        grid.g = max(grid.g, stop.e + stop.l);
    }
}

bool EffectFits192(EffectSetup192 setup, EffectStopGrid grid, int headroom) {
    return setup.high + grid.g - (setup.q + grid.k) + headroom <= 192;
}

EffectNarrow EffectStop192(EffectNarrow d, EffectFactor stop, EffectStopGrid grid) {
    if (stop.m == 0u) {
        return (EffectNarrow)0;
    }
    EffectNarrow result = EffectNarrowTimesSignificand(d, stop.m);
    result = EffectNarrowShift(result, (uint)(stop.e - grid.k));
    if (stop.negative) {
        result = EffectNarrowAdd((EffectNarrow)0, result, true);
    }
    return result;
}

#endif
