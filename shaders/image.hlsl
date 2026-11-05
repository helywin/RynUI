SamplerState AtlasSampler : register(s0, space2);
Texture2D<float4> AtlasTexture : register(t0, space2);

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float2 clipPosition : TEXCOORD1;
    float4 clipBounds : TEXCOORD2;
    float4 color : TEXCOORD3;
    float opacity : TEXCOORD4;
    float4 circleClip : TEXCOORD5;
};

VertexOutput VSMain(uint vertexId : SV_VertexID, float4 positionSize : TEXCOORD0, float4 uvRect : TEXCOORD1,
                    float4 clipBounds : TEXCOORD2, float4 color : TEXCOORD3, float4 translationOpacity : TEXCOORD4,
                    float4 rotationBasis : TEXCOORD5, float4 circleClip : TEXCOORD6) {
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
    output.circleClip = circleClip;
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0 {
    float clipDistance = min(min(input.clipPosition.x - input.clipBounds.x, input.clipBounds.z - input.clipPosition.x),
                             min(input.clipBounds.y - input.clipPosition.y, input.clipPosition.y - input.clipBounds.w));
    clip(clipDistance);
    float circleCoverage = 1;
    if (input.circleClip.z > input.circleClip.x && input.circleClip.y > input.circleClip.w) {
        float2 shapeUV = (input.clipPosition - input.circleClip.xy) / (input.circleClip.zw - input.circleClip.xy);
        float distance = length(shapeUV - 0.5);
        circleCoverage = saturate((0.5 - distance) / max(fwidth(distance), 0.00001) + 0.5);
    }
    float4 sample = AtlasTexture.Sample(AtlasSampler, input.uv);
    return float4(sample.rgb * input.color.rgb, sample.a * input.color.a * input.opacity * circleCoverage);
}
