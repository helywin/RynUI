#ifndef RYNUI_EFFECT_PAINT_SAMPLING
#define RYNUI_EFFECT_PAINT_SAMPLING

#include "effect_linear_exact.hlsli"
#include "effect_conic_candidate.hlsli"

struct EffectField {
    uint4 meta;
    uint4 raw_domain_bits;
    uint4 affine_bits;
};

struct EffectStop {
    float4 position;
    float4 premul;
};

StructuredBuffer<EffectField> effectFields : register(t0, space2);
StructuredBuffer<EffectStop> effectStops : register(t1, space2);

float4 SampleEffectField(float2 sample_position, uint index) {
    EffectField field = effectFields[index];
    bool linear_field = field.meta.x == 1u;
    EffectExact numerator = (EffectExact)0;
    EffectExact denominator = (EffectExact)0;
    CapNumber conic = (CapNumber)0;
    if (linear_field) {
        EffectLinearAxis(asuint(sample_position.x), field.raw_domain_bits.x, field.raw_domain_bits.z,
                         field.affine_bits.x, field.affine_bits.z, numerator, denominator);
        EffectLinearAxis(asuint(sample_position.y), field.raw_domain_bits.y, field.raw_domain_bits.w,
                         field.affine_bits.y, field.affine_bits.z, numerator, denominator);
        numerator = EffectExactShift(numerator, 149u);
    } else {
        conic = EffectConicCoordinate(sample_position, field.raw_domain_bits, field.affine_bits);
    }
    uint low = 0u;
    uint high = field.meta.z;
    // Exact Linear N-stop*D predicates, including adjacent and duplicate stops.
    // Conic uses the same wide coordinate for selection and interpolation;
    // its full-domain error/predicate certificate remains an explicit gate.
    [loop] for (uint iteration = 0u; iteration < 32u && low < high; ++iteration) {
        uint middle = low + (high - low) / 2u;
        float stop = effectStops[field.meta.y + middle].position.x;
        bool less;
        if (linear_field) {
            less = EffectExactLess(numerator, EffectExactStopProduct(denominator, stop));
        } else {
            less = CapCompare(conic, CapSmall(stop)) < 0;
        }
        if (less) {
            high = middle;
        } else {
            low = middle + 1u;
        }
    }
    if (low == 0u) {
        return effectStops[field.meta.y].premul;
    }
    EffectStop previous = effectStops[field.meta.y + low - 1u];
    if (low == field.meta.z) {
        return previous.premul;
    }
    EffectStop next = effectStops[field.meta.y + low];
    float fraction;
    if (linear_field) {
        EffectExact start = EffectExactStopProduct(denominator, previous.position.x);
        EffectExact end = EffectExactStopProduct(denominator, next.position.x);
        fraction = EffectExactUnitRatio(EffectExactAdd(numerator, start, true), EffectExactAdd(end, start, true));
    } else {
        CapNumber delta = CapSubtract(conic, CapSmall(previous.position.x));
        CapNumber span = CapSubtract(CapSmall(next.position.x), CapSmall(previous.position.x));
        fraction = saturate(CapToFloat(CapDivide(delta, span)));
    }
    return lerp(previous.premul, next.premul, fraction);
}

#endif
