#include "scene_pass.hlsli"
#if defined(RYNUI_FRAGMENT_SHADER)
SamplerState AtlasSampler : register(s0, space2);
Texture2D<float> AtlasTexture : register(t0, space2);

#endif
struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float2 clipPosition : TEXCOORD1;
    float4 clipBounds : TEXCOORD2;
    float4 color : TEXCOORD3;
    float opacity : TEXCOORD4;
    nointerpolation uint coverageTransfer : TEXCOORD5;
};

#if defined(RYNUI_VERTEX_SHADER)
VertexOutput VSMain(uint vertexId : SV_VertexID, float4 positionSize : TEXCOORD0, float4 uvRect : TEXCOORD1,
                    float4 clipBounds : TEXCOORD2, float4 color : TEXCOORD3, float4 translationOpacity : TEXCOORD4,
                    float4 rotationBasis : TEXCOORD5) {
    static const float2 corners[6] = {float2(0.0, 0.0), float2(1.0, 0.0), float2(1.0, 1.0),
                                      float2(0.0, 0.0), float2(1.0, 1.0), float2(0.0, 1.0)};

    float2 corner = corners[vertexId];
    float2 delta = corner * positionSize.zw;
    float2 clipPosition =
        positionSize.xy + translationOpacity.xy + float2(dot(delta, rotationBasis.xy), dot(delta, rotationBasis.zw));
    VertexOutput output;
    output.position = float4(clipPosition, 0.0, 1.0);
    output.uv = lerp(uvRect.xy, uvRect.zw, corner);
    output.clipPosition = clipPosition;
    output.clipBounds = clipBounds;
    output.color = color;
    output.opacity = translationOpacity.z;
    output.coverageTransfer = uint(translationOpacity.w);
    output.position = ScenePassPosition(output.position);
    return output;
}
#endif

#if defined(RYNUI_FRAGMENT_SHADER)
float DecodeGlyphSrgb(float encoded) {
    return encoded <= 0.04045 ? encoded / 12.92 : pow((encoded + 0.055) / 1.055, 2.4);
}

float EncodeGlyphSrgb(float linearValue) {
    return linearValue <= 0.0031308 ? linearValue * 12.92 : 1.055 * pow(linearValue, 1.0 / 2.4) - 0.055;
}

uint3 CanonicalMaskColor(uint3 rgb) {
    uint3 highBits = rgb & 224u;
    return highBits | (highBits >> 3u) | (highBits >> 6u);
}

float TextMaskLuminance(float3 encodedColor) {
    // Skia canonicalizes the paint color before reducing it to a gray A8
    // gamma-table row. The actual painted RGB remains the original color.
    uint3 rgb = CanonicalMaskColor(uint3(floor(saturate(encodedColor) * 255.0 + 0.5)));
    uint luminance = (rgb.r * 54u + rgb.g * 183u + rgb.b * 19u) >> 8u;
    return float(CanonicalMaskColor(luminance.xxx).r) / 255.0;
}

float TextContrastCoverage(float coverage, float3 encodedColor, bool srgb) {
    if (coverage <= 0.0 || coverage >= 1.0) {
        return coverage;
    }
    // Skia encodes the nominal 1.2 gamma in unsigned 2.6 fixed point
    // before producing the grayscale text mask table.
    const float maskGamma = 76.0 / 64.0;
    float foreground = TextMaskLuminance(encodedColor);
    float backdrop = 1.0 - foreground;
    float backdropLinear = srgb ? DecodeGlyphSrgb(backdrop) : pow(backdrop, maskGamma);
    float contrast = srgb ? 1.0 : 0.2;
    float adjusted = coverage + (1.0 - coverage) * coverage * backdropLinear * contrast;
    float range = foreground - backdrop;
    if (abs(range) > 1.0 / 256.0) {
        float foregroundLinear = srgb ? DecodeGlyphSrgb(foreground) : pow(foreground, maskGamma);
        float linearValue = lerp(backdropLinear, foregroundLinear, adjusted);
        float encoded = srgb ? EncodeGlyphSrgb(linearValue) : pow(linearValue, 1.0 / maskGamma);
        adjusted = (encoded - backdrop) / range;
    }
    return floor(255.0 * saturate(adjusted) + 0.5) / 255.0;
}

float4 PSMain(VertexOutput input) : SV_Target0 {
    float clipDistance = min(min(input.clipPosition.x - input.clipBounds.x, input.clipBounds.z - input.clipPosition.x),
                             min(input.clipBounds.y - input.clipPosition.y, input.clipPosition.y - input.clipBounds.w));
    clip(clipDistance);
    float coverage = AtlasTexture.Sample(AtlasSampler, input.uv).r;
    if (input.coverageTransfer != 0) {
        coverage = TextContrastCoverage(coverage, input.color.rgb, input.coverageTransfer == 1);
    }
    if (input.coverageTransfer != 0 && all(input.color.rgb == 0.0)) {
        // Skia's black A8 blitter first combines its integer source alpha and mask.
        // Both pinned coverage transfers produce that byte mask before blending.
        const uint sourceAlpha = uint(floor(saturate(input.color.a * input.opacity) * 255.0 + 0.5));
        const uint mask = uint(floor(saturate(coverage) * 255.0 + 0.5));
        const uint ink = sourceAlpha * (mask + 1u) / 256u;
        return float4(0.0, 0.0, 0.0, float(ink) / 255.0);
    }
    return float4(input.color.rgb, input.color.a * input.opacity * coverage);
}
#endif
