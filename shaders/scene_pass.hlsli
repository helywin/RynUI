#ifndef RYNUI_SCENE_PASS
#define RYNUI_SCENE_PASS
#if defined(RYNUI_COMPOSITION_PASS) && defined(RYNUI_VERTEX_SHADER)
cbuffer CompositionVertexViewport : register(b0, space1) {
    float4 fullViewportPixels;
    float4 targetOriginExtent;
};

float4 ScenePassPosition(float4 position) {
    float2 pixels = (position.xy * float2(0.5, -0.5) + 0.5) * fullViewportPixels.xy;
    position.xy = ((pixels - targetOriginExtent.xy) / targetOriginExtent.zw * 2.0 - 1.0) * float2(1.0, -1.0);
    return position;
}
#else
float4 ScenePassPosition(float4 position) {
    return position;
}
#endif
#if defined(RYNUI_COMPOSITION_PASS) && defined(RYNUI_FRAGMENT_SHADER)
cbuffer CompositionFragmentViewport : register(b0, space3) {
    float4 fragmentFullViewportPixels;
    float4 fragmentTargetOriginExtent;
};

cbuffer CompositionDrawCoverage : register(b1, space3) {
    uint4 drawCoverageControl;
};

cbuffer CompositionCoverageOwner : register(b2, space3) {
    float4 coverageOwnerShape;
    float4 coverageOwnerRadius;
    float4 coverageOwnerHardClip;
    float4 coverageOwnerSource;
};

float2 ScenePassSample(float2 position) {
    return position + fragmentTargetOriginExtent.xy;
}
#else
float2 ScenePassSample(float2 position) {
    return position;
}
#endif
#endif
