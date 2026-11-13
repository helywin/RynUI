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

struct EffectLinearSelection {
    uint next;
    float fraction;
};

bool TryLinearSelection192(float2 sample_position, EffectField field, out EffectLinearSelection selection,
                           out uint failure) {
    selection = (EffectLinearSelection)0;
    EffectSetup192 setup;
    failure = EffectSetupLinear192(asuint(sample_position), field.raw_domain_bits, field.affine_bits, setup);
    [branch] if (failure != EffectLinearFailureNone) {
        return false;
    }
    EffectNarrow n;
    EffectNarrow d;
    EffectCoordinate192(setup, n, d);
    if (!EffectNarrowLess((EffectNarrow)0, d)) {
        failure = EffectLinearFailureDenominator;
        return false;
    }
    uint low = 0u;
    uint high = field.meta.z;
    [loop] for (uint iteration = 0u; iteration < 32u && low < high; ++iteration) {
        uint middle = low + (high - low) / 2u;
        EffectFactor stop;
        if (!EffectDecode(asuint(effectStops[field.meta.y + middle].position.x), stop)) {
            failure = EffectLinearFailureInput;
            return false;
        }
        EffectStopGrid grid = (EffectStopGrid)0;
        EffectIncludeStop(grid, stop);
        if (!EffectFits192(setup, grid, 1)) {
            failure = EffectLinearFailureComparison;
            return false;
        }
        if (EffectNarrowLess(EffectNarrowShift(n, (uint)(-grid.k)), EffectStop192(d, stop, grid))) {
            high = middle;
        } else {
            low = middle + 1u;
        }
    }
    EffectLinearSelection result = (EffectLinearSelection)0;
    result.next = low;
    if (low != 0u && low < field.meta.z) {
        EffectFactor previous;
        EffectFactor next;
        if (!EffectDecode(asuint(effectStops[field.meta.y + low - 1u].position.x), previous) ||
            !EffectDecode(asuint(effectStops[field.meta.y + low].position.x), next)) {
            failure = EffectLinearFailureInput;
            return false;
        }
        EffectStopGrid grid = (EffectStopGrid)0;
        EffectIncludeStop(grid, previous);
        EffectIncludeStop(grid, next);
        if (!EffectFits192(setup, grid, 3)) {
            failure = EffectLinearFailurePair;
            return false;
        }
        EffectNarrow aligned = EffectNarrowShift(n, (uint)(-grid.k));
        EffectNarrow start = EffectStop192(d, previous, grid);
        EffectNarrow end = EffectStop192(d, next, grid);
        result.fraction =
            EffectNarrowUnitRatio(EffectNarrowAdd(aligned, start, true), EffectNarrowAdd(end, start, true));
    }
    selection = result;
    return true;
}

EffectLinearSelection LinearSelectionWide(float2 sample_position, EffectField field) {
    EffectExact numerator = (EffectExact)0;
    EffectExact denominator = (EffectExact)0;
    EffectLinearAxis(asuint(sample_position.x), field.raw_domain_bits.x, field.raw_domain_bits.z, field.affine_bits.x,
                     field.affine_bits.z, numerator, denominator);
    EffectLinearAxis(asuint(sample_position.y), field.raw_domain_bits.y, field.raw_domain_bits.w, field.affine_bits.y,
                     field.affine_bits.z, numerator, denominator);
    numerator = EffectExactShift(numerator, 149u);
    uint low = 0u;
    uint high = field.meta.z;
    [loop] for (uint iteration = 0u; iteration < 32u && low < high; ++iteration) {
        uint middle = low + (high - low) / 2u;
        float stop = effectStops[field.meta.y + middle].position.x;
        if (EffectExactLess(numerator, EffectExactStopProduct(denominator, stop))) {
            high = middle;
        } else {
            low = middle + 1u;
        }
    }
    EffectLinearSelection result = (EffectLinearSelection)0;
    result.next = low;
    if (low != 0u && low < field.meta.z) {
        EffectExact start = EffectExactStopProduct(denominator, effectStops[field.meta.y + low - 1u].position.x);
        EffectExact end = EffectExactStopProduct(denominator, effectStops[field.meta.y + low].position.x);
        result.fraction =
            EffectExactUnitRatio(EffectExactAdd(numerator, start, true), EffectExactAdd(end, start, true));
    }
    return result;
}

EffectLinearSelection SelectLinear(float2 sample_position, EffectField field, out uint route) {
    EffectLinearSelection result;
#if defined(RYNUI_TEST_LINEAR_FORCE_WIDE)
    route = 6u; // Compile-time test variant only, never a field/padding flag.
#else
    [branch] if (TryLinearSelection192(sample_position, field, result, route)) {
        return result;
    }
#endif
    // A failure restarts the complete raw72 algorithm, low=0/high=count.
    return LinearSelectionWide(sample_position, field);
}

float4 SampleEffectField(float2 sample_position, uint index) {
    EffectField field = effectFields[index];
    bool linear_field = field.meta.x == 1u;
    uint low = 0u;
    float fraction = 0.0;
    if (linear_field) {
        uint route;
        EffectLinearSelection selection = SelectLinear(sample_position, field, route);
        low = selection.next;
        fraction = selection.fraction;
    } else {
        // Conic retains its original coordinate, search and interpolation.
        CapNumber conic = EffectConicCoordinate(sample_position, field.raw_domain_bits, field.affine_bits);
        uint high = field.meta.z;
        [loop] for (uint iteration = 0u; iteration < 32u && low < high; ++iteration) {
            uint middle = low + (high - low) / 2u;
            float stop = effectStops[field.meta.y + middle].position.x;
            if (CapCompare(conic, CapSmall(stop)) < 0) {
                high = middle;
            } else {
                low = middle + 1u;
            }
        }
        if (low != 0u && low < field.meta.z) {
            CapNumber delta = CapSubtract(conic, CapSmall(effectStops[field.meta.y + low - 1u].position.x));
            CapNumber span = CapSubtract(CapSmall(effectStops[field.meta.y + low].position.x),
                                         CapSmall(effectStops[field.meta.y + low - 1u].position.x));
            fraction = saturate(CapToFloat(CapDivide(delta, span)));
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
    return lerp(previous.premul, next.premul, fraction);
}

#endif
