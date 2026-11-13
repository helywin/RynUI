#ifndef RYNUI_EFFECT_LINEAR_EXACT
#define RYNUI_EFFECT_LINEAR_EXACT

// Signed two's-complement integer, base 2^16, no optional 64-bit arithmetic.
// A binary32 value is an integer * 2^-149, magnitude < 2^128.
// Exact affine: A=E-S, Q=P-scale*(T+S), N=Q.A, D=scale*(A.A).
// N,D have degree <=3, quantum 2^-447, |N|<2^388, D<2^387.
// N-stop*D has degree <=4, quantum 2^-596, magnitude <2^516:
// at most 1112 magnitude bits. A sign and even 25 conservative doublings
// fit within 1152 bits. Every expanded partial sum obeys the same bound.
#define RYNUI_EFFECT_EXACT_DIGITS 72

struct EffectExact {
    uint digit[RYNUI_EFFECT_EXACT_DIGITS];
};

EffectExact EffectExactAdd(EffectExact a, EffectExact b, bool subtract) {
    EffectExact result;
    uint carry = subtract ? 1u : 0u;
    [loop] for (uint i = 0u; i < RYNUI_EFFECT_EXACT_DIGITS; ++i) {
        uint term = subtract ? 65535u - b.digit[i] : b.digit[i];
        uint sum = a.digit[i] + term + carry;
        result.digit[i] = sum & 65535u;
        carry = sum >> 16u;
    }
    return result;
}

EffectExact EffectExactShift(EffectExact value, uint shift) {
    EffectExact result = (EffectExact)0;
    uint whole = shift / 16u;
    uint part = shift % 16u;
    [loop] for (uint i = 0u; i < RYNUI_EFFECT_EXACT_DIGITS; ++i) {
        if (i >= whole) {
            result.digit[i] = (value.digit[i - whole] << part) & 65535u;
            if (part != 0u && i > whole) {
                result.digit[i] |= value.digit[i - whole - 1u] >> (16u - part);
            }
        }
    }
    return result;
}

EffectExact EffectExactTimesDigit(EffectExact value, uint multiplier) {
    EffectExact result;
    uint carry = 0u;
    [loop] for (uint i = 0u; i < RYNUI_EFFECT_EXACT_DIGITS; ++i) {
        // (2^16-1)^2 + (2^16-1) < 2^32.
        uint product = value.digit[i] * multiplier + carry;
        result.digit[i] = product & 65535u;
        carry = product >> 16u;
    }
    return result;
}

EffectExact EffectExactTimesSignificand(EffectExact value, uint significand) {
    EffectExact low = EffectExactTimesDigit(value, significand & 65535u);
    EffectExact high = EffectExactShift(EffectExactTimesDigit(value, significand >> 16u), 16u);
    return EffectExactAdd(low, high, false);
}

uint EffectSignificand(uint bits) {
    return (bits & 0x7fffffu) | ((bits & 0x7f800000u) == 0u ? 0u : 0x800000u);
}

uint EffectIntegerShift(uint bits) {
    uint biased = (bits >> 23u) & 255u;
    return biased == 0u ? 0u : biased - 1u;
}

void EffectExactProduct(inout EffectExact sum, uint a, uint b, uint c, bool subtract) {
    EffectExact product = (EffectExact)0;
    uint significand = EffectSignificand(a);
    product.digit[0] = significand & 65535u;
    product.digit[1] = significand >> 16u;
    product = EffectExactTimesSignificand(product, EffectSignificand(b));
    product = EffectExactTimesSignificand(product, EffectSignificand(c));
    product = EffectExactShift(product, EffectIntegerShift(a) + EffectIntegerShift(b) + EffectIntegerShift(c));
    sum = EffectExactAdd(sum, product, subtract != (((a ^ b ^ c) >> 31u) != 0u));
}

void EffectLinearAxis(uint sample_position, uint start, uint end, uint translation, uint scale, inout EffectExact n,
                      inout EffectExact d) {
    // Degree-two terms gain an exact factor 1 so all terms use 2^-447.
    const uint one = 0x3f800000u;
    EffectExactProduct(n, sample_position, end, one, false);
    EffectExactProduct(n, sample_position, start, one, true);
    EffectExactProduct(n, scale, translation, end, true);
    EffectExactProduct(n, scale, translation, start, false);
    EffectExactProduct(n, scale, start, end, true);
    EffectExactProduct(n, scale, start, start, false);
    EffectExactProduct(d, scale, end, end, false);
    EffectExactProduct(d, scale, end, start, true);
    EffectExactProduct(d, scale, end, start, true);
    EffectExactProduct(d, scale, start, start, false);
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

bool EffectExactLess(EffectExact a, EffectExact b) {
    uint aSign = a.digit[RYNUI_EFFECT_EXACT_DIGITS - 1] >> 15u;
    uint bSign = b.digit[RYNUI_EFFECT_EXACT_DIGITS - 1] >> 15u;
    if (aSign != bSign) {
        return aSign != 0u;
    }
    [loop] for (int i = RYNUI_EFFECT_EXACT_DIGITS - 1; i >= 0; --i) {
        if (a.digit[i] != b.digit[i]) {
            return a.digit[i] < b.digit[i];
        }
    }
    return false;
}

float EffectExactUnitRatio(EffectExact remainder, EffectExact denominator) {
    uint fraction = 0u;
    // Endpoint selection established 0 <= remainder < denominator.
    // Absolute fraction error is <= 2^-25, independent of stop spacing.
    [loop] for (uint i = 0u; i < 25u; ++i) {
        remainder = EffectExactShift(remainder, 1u);
        fraction <<= 1u;
        if (!EffectExactLess(remainder, denominator)) {
            remainder = EffectExactAdd(remainder, denominator, true);
            fraction |= 1u;
        }
    }
    uint rounded = fraction >> 1u;
    if ((fraction & 1u) != 0u && ((rounded & 1u) != 0u || EffectExactLess((EffectExact)0, remainder))) {
        ++rounded;
    }
    return (float)rounded * (1.0 / 16777216.0);
}

#endif
