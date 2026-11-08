#include "rounded_effect_coverage.hlsli"
#if defined(RYNUI_FRAGMENT_SHADER)
SamplerState SourceSampler : register(s0, space2);
Texture2D<float4> SourceTexture : register(t0, space2);

cbuffer PassViewport : register(b0, space3) {
    float4 fullViewport;
    float4 targetOriginExtent;
};

cbuffer RoundedClip : register(b1, space3) {
    float4 shapeRect;
    float4 radiusAA;
    float4 hardClip;
    float4 sourceOriginExtent;
};

#endif
struct VertexOutput {
    float4 position : SV_Position;
};

VertexOutput VSMain(uint id : SV_VertexID) {
    VertexOutput output;
    float2 corner = float2((id << 1) & 2, id & 2);
    output.position = float4(corner * float2(2, -2) + float2(-1, 1), 0, 1);
    return output;
}

#if defined(RYNUI_FRAGMENT_SHADER)
float4 PSMain(VertexOutput input) : SV_Target0 {
    float2 p = input.position.xy + targetOriginExtent.xy;
    if (any(p < hardClip.xy) || any(p >= hardClip.zw) || any(p < sourceOriginExtent.xy) ||
        any(p >= sourceOriginExtent.xy + sourceOriginExtent.zw) || any(shapeRect.zw <= 0)) {
        discard;
    }
    float coverage = GaussianEdge(RoundedRectDistance(p, shapeRect, radiusAA.x), 0.0, radiusAA.y);
    // Source is already PM. ONE blending is part of this pass's ABI.
    return SourceTexture.Load(int3(int2(p - sourceOriginExtent.xy), 0)) * coverage;
}
#endif
