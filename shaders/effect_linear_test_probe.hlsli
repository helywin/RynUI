#ifndef RYNUI_EFFECT_LINEAR_TEST_PROBE
#define RYNUI_EFFECT_LINEAR_TEST_PROBE

// Test PS only, using the normal link/field/stop bindings without extra resources.
float4 EffectLinearTestProbe(float2 sample_position, uint2 fields) {
    uint value;
#if RYNUI_TEST_LINEAR_PROBE == 6 || RYNUI_TEST_LINEAR_PROBE == 7
    // Byte calibration uses the same real link and two storage bindings as the
    // arithmetic probes. Keep their validation observable to the compiler.
    EffectField field = effectFields[fields.x];
    if (field.meta.x != 1u || field.meta.z == 0u) {
        discard;
    }
    if (!isfinite(effectStops[field.meta.y].position.x)) {
        discard;
    }
#endif
#if RYNUI_TEST_LINEAR_PROBE == 6
    value = 0xc17e39adu; // Low24 byte calibration.
#elif RYNUI_TEST_LINEAR_PROBE == 7
    value = 0xc1u; // High8 byte calibration.
#else
    uint route;
    EffectLinearSelection selection = SelectLinear(sample_position, effectFields[fields.x], route);
#if RYNUI_TEST_LINEAR_PROBE == 5
    uint underlayRoute = 255u;
    if (fields.y != 0xffffffffu) {
        uint observed;
        EffectLinearSelection underlay = SelectLinear(sample_position, effectFields[fields.y], observed);
        underlayRoute = observed;
    }
    value = route | (underlayRoute << 8u);
#elif RYNUI_TEST_LINEAR_PROBE == 1
    value = selection.next & 0xffffffu;
#elif RYNUI_TEST_LINEAR_PROBE == 2
    value = selection.next >> 24u;
#elif RYNUI_TEST_LINEAR_PROBE == 3
    value = asuint(selection.fraction) & 0xffffffu;
#elif RYNUI_TEST_LINEAR_PROBE == 4
    value = asuint(selection.fraction) >> 24u;
#else
#error Unknown RYNUI_TEST_LINEAR_PROBE
#endif
#endif
    return float4((float)(value & 255u), (float)((value >> 8u) & 255u), (float)((value >> 16u) & 255u), 255.0) / 255.0;
}

#endif
