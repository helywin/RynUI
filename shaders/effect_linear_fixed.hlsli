// Intentionally no include guard: exactly two static instances in the wrapper.
// RYNUI_FIXED_TYPE / RYNUI_FIXED_NAME / RYNUI_FIXED_DIGITS are cleaned there.
struct RYNUI_FIXED_TYPE {
    uint digit[RYNUI_FIXED_DIGITS];
};

RYNUI_FIXED_TYPE RYNUI_FIXED_NAME(Add)(RYNUI_FIXED_TYPE a, RYNUI_FIXED_TYPE b, bool subtract) {
    RYNUI_FIXED_TYPE result;
    uint carry = subtract ? 1u : 0u;
    [loop] for (uint i = 0u; i < RYNUI_FIXED_DIGITS; ++i) {
        uint term = subtract ? 65535u - b.digit[i] : b.digit[i];
        uint sum = a.digit[i] + term + carry;
        result.digit[i] = sum & 65535u;
        carry = sum >> 16u;
    }
    return result;
}

RYNUI_FIXED_TYPE RYNUI_FIXED_NAME(Shift)(RYNUI_FIXED_TYPE value, uint shift) {
    RYNUI_FIXED_TYPE result = (RYNUI_FIXED_TYPE)0;
    uint whole = shift / 16u;
    uint part = shift % 16u;
    [loop] for (uint i = 0u; i < RYNUI_FIXED_DIGITS; ++i) {
        if (i >= whole) {
            result.digit[i] = (value.digit[i - whole] << part) & 65535u;
            if (part != 0u && i > whole) {
                result.digit[i] |= value.digit[i - whole - 1u] >> (16u - part);
            }
        }
    }
    return result;
}

RYNUI_FIXED_TYPE RYNUI_FIXED_NAME(TimesDigit)(RYNUI_FIXED_TYPE value, uint multiplier) {
    RYNUI_FIXED_TYPE result;
    uint carry = 0u;
    [loop] for (uint i = 0u; i < RYNUI_FIXED_DIGITS; ++i) {
        uint product = value.digit[i] * multiplier + carry;
        result.digit[i] = product & 65535u;
        carry = product >> 16u;
    }
    return result;
}

RYNUI_FIXED_TYPE RYNUI_FIXED_NAME(TimesSignificand)(RYNUI_FIXED_TYPE value, uint significand) {
    RYNUI_FIXED_TYPE low = RYNUI_FIXED_NAME(TimesDigit)(value, significand & 65535u);
    RYNUI_FIXED_TYPE high = RYNUI_FIXED_NAME(Shift)(RYNUI_FIXED_NAME(TimesDigit)(value, significand >> 16u), 16u);
    return RYNUI_FIXED_NAME(Add)(low, high, false);
}

void RYNUI_FIXED_NAME(FactorProduct)(inout RYNUI_FIXED_TYPE sum, EffectFactor a, EffectFactor b, EffectFactor c, int q,
                                     bool subtract) {
    [branch] if (a.m == 0u || b.m == 0u || c.m == 0u) {
        return;
    }
    EffectExactSmallProduct small = (EffectExactSmallProduct)0;
    small.digit[0] = a.m & 65535u;
    small.digit[1] = a.m >> 16u;
    small = EffectExactSmallTimesSignificand(small, b.m);
    small = EffectExactSmallTimesSignificand(small, c.m);
    RYNUI_FIXED_TYPE product = (RYNUI_FIXED_TYPE)0;
    [unroll] for (uint i = 0u; i < 5u; ++i) {
        product.digit[i] = small.digit[i];
    }
    // Raw72 shifts <=759. Canonical12 setup proved nonnegative/no high loss.
    product = RYNUI_FIXED_NAME(Shift)(product, (uint)(a.e + b.e + c.e - q));
    sum = RYNUI_FIXED_NAME(Add)(sum, product, subtract != ((a.negative != b.negative) != c.negative));
}

void RYNUI_FIXED_NAME(Axis)(EffectFactor sample_position, EffectFactor start, EffectFactor end,
                            EffectFactor translation, EffectFactor scale, EffectFactor one, int q,
                            inout RYNUI_FIXED_TYPE n, inout RYNUI_FIXED_TYPE d) {
    // Preserve the original ten terms, signs and accumulation order per axis.
    RYNUI_FIXED_NAME(FactorProduct)(n, sample_position, end, one, q, false);
    RYNUI_FIXED_NAME(FactorProduct)(n, sample_position, start, one, q, true);
    RYNUI_FIXED_NAME(FactorProduct)(n, scale, translation, end, q, true);
    RYNUI_FIXED_NAME(FactorProduct)(n, scale, translation, start, q, false);
    RYNUI_FIXED_NAME(FactorProduct)(n, scale, start, end, q, true);
    RYNUI_FIXED_NAME(FactorProduct)(n, scale, start, start, q, false);
    RYNUI_FIXED_NAME(FactorProduct)(d, scale, end, end, q, false);
    RYNUI_FIXED_NAME(FactorProduct)(d, scale, end, start, q, true);
    RYNUI_FIXED_NAME(FactorProduct)(d, scale, end, start, q, true);
    RYNUI_FIXED_NAME(FactorProduct)(d, scale, start, start, q, false);
}

bool RYNUI_FIXED_NAME(Less)(RYNUI_FIXED_TYPE a, RYNUI_FIXED_TYPE b) {
    uint aSign = a.digit[RYNUI_FIXED_DIGITS - 1] >> 15u;
    uint bSign = b.digit[RYNUI_FIXED_DIGITS - 1] >> 15u;
    if (aSign != bSign) {
        return aSign != 0u;
    }
    [loop] for (int i = RYNUI_FIXED_DIGITS - 1; i >= 0; --i) {
        if (a.digit[i] != b.digit[i]) {
            return a.digit[i] < b.digit[i];
        }
    }
    return false;
}

float RYNUI_FIXED_NAME(UnitRatio)(RYNUI_FIXED_TYPE remainder, RYNUI_FIXED_TYPE denominator) {
    uint fraction = 0u;
    [loop] for (uint i = 0u; i < 25u; ++i) {
        remainder = RYNUI_FIXED_NAME(Shift)(remainder, 1u);
        fraction <<= 1u;
        if (!RYNUI_FIXED_NAME(Less)(remainder, denominator)) {
            remainder = RYNUI_FIXED_NAME(Add)(remainder, denominator, true);
            fraction |= 1u;
        }
    }
    uint rounded = fraction >> 1u;
    if ((fraction & 1u) != 0u && ((rounded & 1u) != 0u || RYNUI_FIXED_NAME(Less)((RYNUI_FIXED_TYPE)0, remainder))) {
        ++rounded;
    }
    return (float)rounded * (1.0 / 16777216.0);
}
