#ifndef RYNUI_CAP_NUMBER
#define RYNUI_CAP_NUMBER

// Fixed 224-bit significand, separate sign and unbounded-for-input int exponent.
// No optional FP64, subnormal arithmetic, FMA residual or reassociation assumption.
// Multiplication uses base-2^16 digits: product + old digit + carry fits uint32.
// This candidate still needs the pinned shadercross roundtrip and GPU validation.
struct CapNumber {
    uint word[7];
    int exponent;
    uint negative;
};

uint CapDigit(CapNumber value, uint index) {
    return (value.word[index / 2u] >> (16u * (index % 2u))) & 65535u;
}

void CapSetDigit(inout CapNumber value, uint index, uint digit) {
    uint shift = 16u * (index % 2u);
    uint mask = 65535u << shift;
    value.word[index / 2u] = (value.word[index / 2u] & ~mask) | ((digit & 65535u) << shift);
}

bool CapZero(CapNumber value) {
    return value.word[6] == 0u;
}

CapNumber CapShift(CapNumber value, uint shift, bool left) {
    CapNumber result = (CapNumber)0;
    result.exponent = value.exponent;
    result.negative = value.negative;
    if (shift >= 224u) {
        return result;
    }
    uint whole = shift / 16u;
    uint part = shift % 16u;
    [loop] for (uint i = 0u; i < 14u; ++i) {
        if (left) {
            if (i >= whole) {
                CapSetDigit(result, i, (CapDigit(value, i - whole) << part) & 65535u);
                if (part != 0u && i > whole) {
                    CapSetDigit(result, i, CapDigit(result, i) | (CapDigit(value, i - whole - 1u) >> (16u - part)));
                }
            }
        } else if (i + whole < 14u) {
            CapSetDigit(result, i, CapDigit(value, i + whole) >> part);
            if (part != 0u && i + whole + 1u < 14u) {
                CapSetDigit(result, i,
                            CapDigit(result, i) | ((CapDigit(value, i + whole + 1u) << (16u - part)) & 65535u));
            }
        }
    }
    return result;
}

CapNumber CapNormalize(CapNumber value) {
    int high = -1;
    [loop] for (int i = 13; i >= 0; --i) {
        if (CapDigit(value, (uint)i) != 0u) {
            high = 16 * i + firstbithigh(CapDigit(value, (uint)i));
            break;
        }
    }
    if (high < 0) {
        return (CapNumber)0;
    }
    uint shift = (uint)(223 - high);
    value = CapShift(value, shift, true);
    value.exponent -= (int)shift;
    return value;
}

CapNumber CapFromBits(uint bits) {
    CapNumber value = (CapNumber)0;
    uint magnitude = bits & 0x7fffffffu;
    uint biased = magnitude >> 23u;
    uint significand = magnitude & 0x7fffffu;
    if (biased != 0u) {
        significand |= 0x800000u;
    }
    CapSetDigit(value, 0u, significand & 65535u);
    CapSetDigit(value, 1u, significand >> 16u);
    value.exponent = (biased == 0u ? -149 : (int)biased - 150) + 223;
    value.negative = bits >> 31u;
    return CapNormalize(value);
}

CapNumber CapSmall(float value) {
    return CapFromBits(asuint(value));
}

CapNumber CapNegate(CapNumber value) {
    value.negative = CapZero(value) ? 0u : 1u - value.negative;
    return value;
}

CapNumber CapAbs(CapNumber value) {
    value.negative = 0u;
    return value;
}

int CapMagnitudeCompare(CapNumber first, CapNumber second) {
    if (CapZero(first) || CapZero(second)) {
        return CapZero(first) ? (CapZero(second) ? 0 : -1) : 1;
    }
    if (first.exponent != second.exponent) {
        return first.exponent < second.exponent ? -1 : 1;
    }
    [loop] for (int i = 13; i >= 0; --i) {
        if (CapDigit(first, (uint)i) != CapDigit(second, (uint)i)) {
            return CapDigit(first, (uint)i) < CapDigit(second, (uint)i) ? -1 : 1;
        }
    }
    return 0;
}

int CapCompare(CapNumber first, CapNumber second) {
    if (first.negative != second.negative) {
        return first.negative != 0u ? -1 : 1;
    }
    int magnitude = CapMagnitudeCompare(first, second);
    return first.negative != 0u ? -magnitude : magnitude;
}

CapNumber CapAdd(CapNumber first, CapNumber second) {
    if (CapZero(first)) {
        return second;
    }
    if (CapZero(second)) {
        return first;
    }
    if (CapMagnitudeCompare(first, second) < 0) {
        CapNumber temporary = first;
        first = second;
        second = temporary;
    }
    second = CapShift(second, (uint)(first.exponent - second.exponent), false);
    CapNumber result = (CapNumber)0;
    result.exponent = first.exponent;
    result.negative = first.negative;
    uint carry = 0u;
    [loop] for (uint i = 0u; i < 14u; ++i) {
        if (first.negative == second.negative) {
            uint sum = CapDigit(first, i) + CapDigit(second, i) + carry;
            CapSetDigit(result, i, sum & 65535u);
            carry = sum >> 16u;
        } else {
            uint difference = 65536u + CapDigit(first, i) - CapDigit(second, i) - carry;
            CapSetDigit(result, i, difference & 65535u);
            carry = difference < 65536u ? 1u : 0u;
        }
    }
    if (first.negative == second.negative && carry != 0u) {
        result = CapShift(result, 1u, false);
        CapSetDigit(result, 13u, CapDigit(result, 13u) | 32768u);
        ++result.exponent;
        return result;
    }
    return CapNormalize(result);
}

CapNumber CapSubtract(CapNumber first, CapNumber second) {
    return CapAdd(first, CapNegate(second));
}

CapNumber CapMultiply(CapNumber first, CapNumber second) {
    if (CapZero(first) || CapZero(second)) {
        return (CapNumber)0;
    }
    uint product[28];
    [loop] for (uint i = 0u; i < 28u; ++i) {
        product[i] = 0u;
    }
    [loop] for (uint a = 0u; a < 14u; ++a) {
        uint carry = 0u;
        [loop] for (uint b = 0u; b < 14u; ++b) {
            uint sum = CapDigit(first, a) * CapDigit(second, b) + product[a + b] + carry;
            product[a + b] = sum & 65535u;
            carry = sum >> 16u;
        }
        product[a + 14u] = carry;
    }
    bool high = (product[27] & 32768u) != 0u;
    uint shift = high ? 224u : 223u;
    uint whole = shift / 16u;
    uint part = shift % 16u;
    CapNumber result = (CapNumber)0;
    result.exponent = first.exponent + second.exponent + (high ? 1 : 0);
    result.negative = first.negative ^ second.negative;
    [loop] for (uint i = 0u; i < 14u; ++i) {
        CapSetDigit(result, i, product[i + whole] >> part);
        if (part != 0u && i + whole + 1u < 28u) {
            CapSetDigit(result, i, CapDigit(result, i) | ((product[i + whole + 1u] << (16u - part)) & 65535u));
        }
    }
    return result;
}

CapNumber CapScale2(CapNumber value, int exponent) {
    if (!CapZero(value)) {
        value.exponent += exponent;
    }
    return value;
}

float CapMantissa(CapNumber value) {
    uint fraction = ((CapDigit(value, 13u) & 32767u) << 8u) | (CapDigit(value, 12u) >> 8u);
    return asfloat(0x3f800000u | fraction);
}

CapNumber CapDivide(CapNumber first, CapNumber second) {
    // Callers establish a nonzero denominator from exact geometric cases.
    if (CapZero(first)) {
        return (CapNumber)0;
    }
    CapNumber reciprocal = CapSmall(1.0 / CapMantissa(second));
    reciprocal.exponent -= second.exponent;
    CapNumber magnitude = CapAbs(second);
    [loop] for (uint i = 0u; i < 5u; ++i) {
        reciprocal = CapMultiply(reciprocal, CapSubtract(CapSmall(2.0), CapMultiply(magnitude, reciprocal)));
    }
    reciprocal.negative = second.negative;
    return CapMultiply(first, reciprocal);
}

CapNumber CapDivideSmall(CapNumber value, uint divisor) {
    uint remainder = 0u;
    [loop] for (int i = 13; i >= 0; --i) {
        uint current = (remainder << 16u) | CapDigit(value, (uint)i);
        CapSetDigit(value, (uint)i, current / divisor);
        remainder = current % divisor;
    }
    return CapNormalize(value);
}

CapNumber CapSqrt(CapNumber value) {
    if (CapZero(value)) {
        return value;
    }
    int odd = value.exponent & 1;
    CapNumber reciprocal = CapSmall(rsqrt(CapMantissa(value) * (odd != 0 ? 2.0 : 1.0)));
    reciprocal.exponent -= (value.exponent - odd) / 2;
    [loop] for (uint i = 0u; i < 5u; ++i) {
        CapNumber square = CapMultiply(reciprocal, reciprocal);
        CapNumber correction = CapSubtract(CapSmall(1.5), CapScale2(CapMultiply(value, square), -1));
        reciprocal = CapMultiply(reciprocal, correction);
    }
    return CapMultiply(value, reciprocal);
}

CapNumber CapMin(CapNumber first, CapNumber second) {
    if (CapCompare(first, second) <= 0) {
        return first;
    }
    return second;
}

CapNumber CapMax(CapNumber first, CapNumber second) {
    if (CapCompare(first, second) >= 0) {
        return first;
    }
    return second;
}

struct CapPoint {
    CapNumber x;
    CapNumber y;
};

CapPoint CapPointAdd(CapPoint first, CapPoint second) {
    CapPoint result;
    result.x = CapAdd(first.x, second.x);
    result.y = CapAdd(first.y, second.y);
    return result;
}

CapPoint CapPointNegate(CapPoint value) {
    value.x = CapNegate(value.x);
    value.y = CapNegate(value.y);
    return value;
}

CapPoint CapPointSubtract(CapPoint first, CapPoint second) {
    return CapPointAdd(first, CapPointNegate(second));
}

CapPoint CapPointScale(CapPoint value, CapNumber scale) {
    value.x = CapMultiply(value.x, scale);
    value.y = CapMultiply(value.y, scale);
    return value;
}

CapPoint CapPerpendicular(CapPoint value) {
    CapPoint result;
    result.x = CapNegate(value.y);
    result.y = value.x;
    return result;
}

CapNumber CapDot(CapPoint first, CapPoint second) {
    return CapAdd(CapMultiply(first.x, second.x), CapMultiply(first.y, second.y));
}

CapNumber CapCross(CapPoint first, CapPoint second) {
    return CapSubtract(CapMultiply(first.x, second.y), CapMultiply(first.y, second.x));
}

CapNumber CapLength(CapPoint value) {
    return CapSqrt(CapDot(value, value));
}

CapPoint CapUnit(CapPoint value) {
    return CapPointScale(value, CapDivide(CapSmall(1.0), CapLength(value)));
}

CapNumber CapPi() {
    static const uint digits[14] = {0xbea6u, 0x020bu, 0xcc74u, 0x8a67u, 0x4e08u, 0x2902u, 0x1cd1u,
                                    0x80dcu, 0x628bu, 0xc4c6u, 0xc234u, 0x2168u, 0xdaa2u, 0xc90fu};
    CapNumber result = (CapNumber)0;
    result.exponent = 1;
    [loop] for (uint i = 0u; i < 14u; ++i) {
        CapSetDigit(result, i, digits[i]);
    }
    return result;
}

float CapToFloat(CapNumber value) {
    if (CapZero(value) || value.exponent < -126) {
        return 0.0;
    }
    float magnitude = value.exponent > 126 ? 3.402823466e+38 : CapMantissa(value) * exp2((float)value.exponent);
    return value.negative != 0u ? -magnitude : magnitude;
}

CapPoint CapDirection(CapNumber angle) {
    CapNumber halfPi = CapScale2(CapPi(), -1);
    int quadrant = (int)round(CapToFloat(angle) / 1.5707963267948966);
    CapNumber reduced = CapSubtract(angle, CapMultiply(CapSmall((float)quadrant), halfPi));
    CapNumber quarterPi = CapScale2(halfPi, -1);
    // Input angles are in [-pi_f, 3*pi_f]. The float estimate differs by at
    // most one quadrant; a fixed correction avoids a data-dependent loop.
    if (CapCompare(reduced, quarterPi) > 0) {
        reduced = CapSubtract(reduced, halfPi);
        ++quadrant;
    } else if (CapCompare(reduced, CapNegate(quarterPi)) < 0) {
        reduced = CapAdd(reduced, halfPi);
        --quadrant;
    }
    CapNumber square = CapMultiply(reduced, reduced);
    CapNumber sine = reduced;
    CapNumber sineTerm = reduced;
    CapNumber cosine = CapSmall(1.0);
    CapNumber cosineTerm = cosine;
    // sin through x^43, cos through x^42; |x| <= pi/4. The first
    // omitted terms are bounded by |x|^45/45! and |x|^44/44!.
    [loop] for (uint i = 1u; i <= 21u; ++i) {
        sineTerm = CapNegate(CapDivideSmall(CapMultiply(sineTerm, square), (2u * i) * (2u * i + 1u)));
        cosineTerm = CapNegate(CapDivideSmall(CapMultiply(cosineTerm, square), (2u * i - 1u) * (2u * i)));
        sine = CapAdd(sine, sineTerm);
        cosine = CapAdd(cosine, cosineTerm);
    }
    CapPoint result;
    uint side = (uint)((quadrant % 4 + 4) % 4);
    result.x = cosine;
    result.y = sine;
    if (side == 1u) {
        result.x = CapNegate(sine);
        result.y = cosine;
    } else if (side == 2u) {
        result.x = CapNegate(cosine);
        result.y = CapNegate(sine);
    } else if (side == 3u) {
        result.x = sine;
        result.y = CapNegate(cosine);
    }
    return result;
}

#endif
