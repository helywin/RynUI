#include "scene_pass.hlsli"
#if defined(RYNUI_VERTEX_SHADER)
#if !defined(RYNUI_COMPOSITION_PASS)
cbuffer QuadViewport : register(b0, space1) {
    float4 viewportPixels;
};
#else
#define viewportPixels fullViewportPixels
#endif
#endif

struct VertexOutput {
    float4 position : SV_Position;
    float2 centeredPixels : TEXCOORD0;
    float4 color : TEXCOORD1;
    float opacity : TEXCOORD2;
    float cornerRadius : TEXCOORD3;
    nointerpolation float2 pixelExtent : TEXCOORD4;
};

#if defined(RYNUI_VERTEX_SHADER)
VertexOutput VSMain(uint vertexId : SV_VertexID, float4 clipRect : TEXCOORD0, float4 color : TEXCOORD1,
                    float opacity : TEXCOORD2, float cornerRadius : TEXCOORD3, float2 translation : TEXCOORD4) {
    static const float2 corners[6] = {float2(0.0, 0.0), float2(1.0, 0.0), float2(1.0, 1.0),
                                      float2(0.0, 0.0), float2(1.0, 1.0), float2(0.0, 1.0)};

    VertexOutput output;
    float2 pixelExtent = abs(clipRect.zw) * viewportPixels.xy * 0.5;
    // Shape and raster bounds differ: the SDF has coverage outside its edge.
    float guard = all(pixelExtent > 0.0) ? 1.0 : 0.0;
    float2 cornerSign = corners[vertexId] * 2.0 - 1.0;
    float2 guardPixels = cornerSign * guard;
    float2 guardNdc = guardPixels * float2(2.0 / viewportPixels.x, -2.0 / viewportPixels.y);
    output.position = float4(clipRect.xy + translation + corners[vertexId] * clipRect.zw + guardNdc, 0.0, 1.0);
    output.centeredPixels = (corners[vertexId] - 0.5) * pixelExtent + guardPixels;
    output.color = color;
    output.opacity = opacity;
    output.cornerRadius = cornerRadius;
    output.pixelExtent = pixelExtent;
    output.position = ScenePassPosition(output.position);
    return output;
}
#endif

#if defined(RYNUI_FRAGMENT_SHADER)
float4 PSMain(VertexOutput input) : SV_Target0 {
#if defined(RYNUI_COMPOSITION_PASS)
    float2 globalPixel = ScenePassSample(input.position.xy);
    if (any(globalPixel < 0.0) || any(globalPixel >= fragmentFullViewportPixels.xy)) {
        discard;
    }
#endif
    float2 pixelExtent = input.pixelExtent;
    if (any(pixelExtent <= 0.0)) {
        return float4(input.color.rgb, 0.0);
    }
    float radiusPixels = saturate(input.cornerRadius) * min(pixelExtent.x, pixelExtent.y);
    float2 roundedBox = abs(input.centeredPixels) - (0.5 * pixelExtent - float2(radiusPixels, radiusPixels));
    float distance = length(max(roundedBox, 0.0)) + min(max(roundedBox.x, roundedBox.y), 0.0) - radiusPixels;
    float coverage = 1.0 - smoothstep(-0.5, 0.5, distance);
#if defined(RYNUI_COMPOSITION_PASS)
    if (drawCoverageControl.x == 1u) {
        coverage = 1.0;
    }
#endif
    return float4(input.color.rgb, input.color.a * input.opacity * coverage);
}
#endif
