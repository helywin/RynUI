#include "scene_pass.hlsli"
#include "rounded_effect_coverage.hlsli"
#if defined(RYNUI_FRAGMENT_SHADER)
#include "effect_paint_sampling.hlsli"
#if defined(RYNUI_TEST_LINEAR_PROBE)
#include "effect_linear_test_probe.hlsli"
#endif
#endif

struct VertexOutput {
    float4 position : SV_Position;
    nointerpolation float4 shapeRect : TEXCOORD0;
    nointerpolation float4 clipBounds : TEXCOORD1;
    nointerpolation float4 color : TEXCOORD2;
    nointerpolation float4 shadowParams : TEXCOORD3;
    nointerpolation float4 effectParams : TEXCOORD4;
    nointerpolation float4 materialParams : TEXCOORD5;
    nointerpolation uint4 paintLink : TEXCOORD6;
#if defined(RYNUI_CAP_GEOMETRY)
    nointerpolation uint4 capRect : TEXCOORD7;
    nointerpolation uint4 capStrokeScale : TEXCOORD8;
#endif
};

#if defined(RYNUI_VERTEX_SHADER)
VertexOutput VSMain(uint vertexId : SV_VertexID, float4 clipRect : TEXCOORD0, float4 shapeRect : TEXCOORD1,
                    float4 clipBounds : TEXCOORD2, float4 color : TEXCOORD3, float4 shadowParams : TEXCOORD4,
                    float4 effectParams : TEXCOORD5, float4 materialParams : TEXCOORD6, uint4 paintLink : TEXCOORD7
#if defined(RYNUI_CAP_GEOMETRY)
                    ,
                    uint4 capRect : TEXCOORD8, uint4 capStrokeScale : TEXCOORD9
#endif
) {
    static const float2 corners[6] = {float2(0.0, 0.0), float2(1.0, 0.0), float2(1.0, 1.0),
                                      float2(0.0, 0.0), float2(1.0, 1.0), float2(0.0, 1.0)};
    VertexOutput output;
    output.position = float4(clipRect.xy + corners[vertexId] * clipRect.zw, 0.0, 1.0);
    output.shapeRect = shapeRect;
    output.clipBounds = clipBounds;
    output.color = color;
    output.shadowParams = shadowParams;
    output.effectParams = effectParams;
    output.materialParams = materialParams;
    output.paintLink = paintLink;
#if defined(RYNUI_CAP_GEOMETRY)
    output.capRect = capRect;
    output.capStrokeScale = capStrokeScale;
#endif
    output.position = ScenePassPosition(output.position);
    return output;
}

#endif
#if defined(RYNUI_FRAGMENT_SHADER)
float4 PSMain(VertexOutput input) : SV_Target0 {
    float2 samplePosition = ScenePassSample(input.position.xy);
    if (any(samplePosition < input.clipBounds.xy) || any(samplePosition >= input.clipBounds.zw)) {
        discard;
    }
#if defined(RYNUI_TEST_LINEAR_PROBE)
    // Shadercross compacts unused PS inputs when translating SPIR-V to DXIL.
    // Keep the production VS/PS register signature for every byte probe.
    if (!all(isfinite(input.shapeRect)) || !all(isfinite(input.color)) || !all(isfinite(input.shadowParams)) ||
        !all(isfinite(input.effectParams)) || !all(isfinite(input.materialParams))) {
        discard;
    }
    return EffectLinearTestProbe(samplePosition, input.paintLink.xy);
#endif
    float coverage;
#if defined(RYNUI_CAP_GEOMETRY)
    if (input.effectParams.w == 3.0 && input.paintLink.z != 0u) {
        coverage =
            NonroundArcCoverageRaw(samplePosition, input.capRect, input.capStrokeScale, asuint(input.shadowParams.y),
                                   asuint(input.shadowParams.z), input.paintLink.z);
    } else {
#endif
        coverage = LegacyEffectCoverage(samplePosition, input.shapeRect, input.shadowParams, input.effectParams,
                                        input.materialParams);
#if defined(RYNUI_CAP_GEOMETRY)
    }
#endif
    [branch] if (input.paintLink.x == 0xffffffffu) {
        return float4(input.color.rgb, input.color.a * input.materialParams.x * coverage);
    }
    float factor = coverage * input.materialParams.x;
    [branch] if (factor == 0.0) {
        return 0.0;
    }
    float4 combined = SampleEffectField(samplePosition, input.paintLink.x);
    [branch] if (input.paintLink.y != 0xffffffffu && combined.a != 1.0) {
        float4 underlay = SampleEffectField(samplePosition, input.paintLink.y);
        combined += underlay * (1.0 - combined.a);
    }
    if (combined.a <= 0.0) {
        return 0.0;
    }
    return float4(combined.rgb / combined.a, combined.a * factor);
}
#endif
