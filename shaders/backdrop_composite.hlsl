#include "rounded_effect_coverage.hlsli"
#if defined(RYNUI_FRAGMENT_SHADER)
SamplerState BackgroundSampler : register(s0, space2);
Texture2D<float4> Background : register(t0, space2);
SamplerState FilteredSampler : register(s1, space2);
Texture2D<float4> Filtered : register(t1, space2);

cbuffer PassViewport : register(b0, space3) {
    float4 fullViewport;
    float4 targetOriginExtent;
};

cbuffer BackdropComposite : register(b1, space3) {
    float4 shapeRect;
    float4 radiusAA;
    float4 hardClip;
    float4 sourceOriginExtent;
    float4 tintPM;
    float4 activation;
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
    if (any(p < hardClip.xy) || any(p >= hardClip.zw) || any(shapeRect.zw <= 0.0)) {
        discard;
    }
    int3 address = int3(int2(p - sourceOriginExtent.xy), 0);
    float4 b = Background.Load(address);
    float4 f = Filtered.Load(address);
    float4 e = tintPM + f * (1.0 - tintPM.a);
    float m = GaussianEdge(RoundedRectDistance(p, shapeRect, radiusAA.x), 0.0, radiusAA.y) * activation.x;
    // A replacement pass: the pipeline MUST disable hardware blending.
    return lerp(b, e, m);
}
#endif
