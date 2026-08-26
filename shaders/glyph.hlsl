SamplerState AtlasSampler : register(s0, space2);
Texture2D<float> AtlasTexture : register(t0, space2);

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float2 clipPosition : TEXCOORD1;
    float4 clipBounds : TEXCOORD2;
    float4 color : TEXCOORD3;
    float opacity : TEXCOORD4;
};

VertexOutput VSMain(
    uint vertexId : SV_VertexID,
    float4 positionSize : TEXCOORD0,
    float4 uvRect : TEXCOORD1,
    float4 clipBounds : TEXCOORD2,
    float4 color : TEXCOORD3,
    float4 translationOpacity : TEXCOORD4)
{
    static const float2 corners[6] = {
        float2(0.0, 0.0),
        float2(1.0, 0.0),
        float2(1.0, 1.0),
        float2(0.0, 0.0),
        float2(1.0, 1.0),
        float2(0.0, 1.0)
    };

    float2 corner = corners[vertexId];
    float2 clipPosition = positionSize.xy
        + translationOpacity.xy
        + corner * positionSize.zw;
    VertexOutput output;
    output.position = float4(clipPosition, 0.0, 1.0);
    output.uv = lerp(uvRect.xy, uvRect.zw, corner);
    output.clipPosition = clipPosition;
    output.clipBounds = clipBounds;
    output.color = color;
    output.opacity = translationOpacity.z;
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    float clipDistance = min(
        min(input.clipPosition.x - input.clipBounds.x,
            input.clipBounds.z - input.clipPosition.x),
        min(input.clipBounds.y - input.clipPosition.y,
            input.clipPosition.y - input.clipBounds.w));
    clip(clipDistance);
    float coverage = AtlasTexture.Sample(AtlasSampler, input.uv).r;
    return float4(
        input.color.rgb,
        input.color.a * input.opacity * coverage);
}
