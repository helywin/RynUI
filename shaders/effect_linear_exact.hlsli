#ifndef RYNUI_EFFECT_LINEAR_EXACT
#define RYNUI_EFFECT_LINEAR_EXACT

// Signed two's-complement integer, base 2^16, no optional 64-bit arithmetic.
// A binary32 value is an integer * 2^-149, magnitude < 2^128.
// N,D have degree two and quantum 2^-298. N-stop*D has quantum
// 2^-447 and magnitude < 2^388: fewer than 836 magnitude bits.
// 896 bits also cover all 25 remainder doublings used by the unit ratio.
struct EffectExact {
    uint digit[56];
};

EffectExact EffectExactAdd(EffectExact a, EffectExact b, bool subtract) {
    EffectExact result;
    uint carry = subtract ? 1u : 0u;
    [loop] for (uint i = 0u; i < 56u; ++i) {
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
    [loop] for (uint i = 0u; i < 56u; ++i) {
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
    [loop] for (uint i = 0u; i < 56u; ++i) {
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

void EffectExactProduct(inout EffectExact sum, float first, float second, bool subtract) {
    uint a = asuint(first);
    uint b = asuint(second);
    EffectExact product = (EffectExact)0;
    uint significand = EffectSignificand(a);
    product.digit[0] = significand & 65535u;
    product.digit[1] = significand >> 16u;
    product = EffectExactTimesSignificand(product, EffectSignificand(b));
    product = EffectExactShift(product, EffectIntegerShift(a) + EffectIntegerShift(b));
    sum = EffectExactAdd(sum, product, subtract != (((a ^ b) >> 31u) != 0u));
}

void EffectLinearAxis(float sample_position, float start, float end, inout EffectExact n, inout EffectExact d) {
    // Expand products of original factors; never round point-start or end-start.
    EffectExactProduct(n, sample_position, end, false);
    EffectExactProduct(n, sample_position, start, true);
    EffectExactProduct(n, start, end, true);
    EffectExactProduct(n, start, start, false);
    EffectExactProduct(d, end, end, false);
    EffectExactProduct(d, end, start, true);
    EffectExactProduct(d, end, start, true);
    EffectExactProduct(d, start, start, false);
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
    uint aSign = a.digit[55] >> 15u;
    uint bSign = b.digit[55] >> 15u;
    if (aSign != bSign) {
        return aSign != 0u;
    }
    [loop] for (int i = 55; i >= 0; --i) {
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
