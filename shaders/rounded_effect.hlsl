#include "rounded_effect_coverage.hlsli"

struct VertexOutput {
    float4 position : SV_Position;
    nointerpolation float4 shapeRect : TEXCOORD0;
    nointerpolation float4 clipBounds : TEXCOORD1;
    nointerpolation float4 color : TEXCOORD2;
    nointerpolation float4 shadowParams : TEXCOORD3;
    nointerpolation float4 effectParams : TEXCOORD4;
    nointerpolation float4 materialParams : TEXCOORD5;
};

VertexOutput VSMain(uint vertexId : SV_VertexID, float4 clipRect : TEXCOORD0, float4 shapeRect : TEXCOORD1,
                    float4 clipBounds : TEXCOORD2, float4 color : TEXCOORD3, float4 shadowParams : TEXCOORD4,
                    float4 effectParams : TEXCOORD5, float4 materialParams : TEXCOORD6) {
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
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0 {
    float2 samplePosition = input.position.xy;
    if (any(samplePosition < input.clipBounds.xy) || any(samplePosition >= input.clipBounds.zw)) {
        discard;
    }
    float coverage = LegacyEffectCoverage(samplePosition, input.shapeRect, input.shadowParams, input.effectParams,
                                          input.materialParams);
    return float4(input.color.rgb, input.color.a * input.materialParams.x * coverage);
}
