struct VertexOutput
{
    float4 position : SV_Position;
    nointerpolation float4 shapeRect : TEXCOORD0;
    nointerpolation float4 clipBounds : TEXCOORD1;
    nointerpolation float4 color : TEXCOORD2;
    nointerpolation float4 shadowParams : TEXCOORD3;
    nointerpolation float4 effectParams : TEXCOORD4;
    nointerpolation float4 materialParams : TEXCOORD5;
};

VertexOutput VSMain(
    uint vertexId : SV_VertexID,
    float4 clipRect : TEXCOORD0,
    float4 shapeRect : TEXCOORD1,
    float4 clipBounds : TEXCOORD2,
    float4 color : TEXCOORD3,
    float4 shadowParams : TEXCOORD4,
    float4 effectParams : TEXCOORD5,
    float4 materialParams : TEXCOORD6)
{
    static const float2 corners[6] = {
        float2(0.0, 0.0),
        float2(1.0, 0.0),
        float2(1.0, 1.0),
        float2(0.0, 0.0),
        float2(1.0, 1.0),
        float2(0.0, 1.0)
    };

    VertexOutput output;
    output.position = float4(
        clipRect.xy + corners[vertexId] * clipRect.zw,
        0.0,
        1.0);
    output.shapeRect = shapeRect;
    output.clipBounds = clipBounds;
    output.color = color;
    output.shadowParams = shadowParams;
    output.effectParams = effectParams;
    output.materialParams = materialParams;
    return output;
}

float RoundedRectDistance(float2 samplePosition, float4 rect, float radius)
{
    radius = clamp(radius, 0.0, 0.5 * min(rect.z, rect.w));
    float2 center = rect.xy + 0.5 * rect.zw;
    float2 halfExtent = 0.5 * rect.zw;
    float2 q = abs(samplePosition - center) - (halfExtent - radius.xx);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float ErfApprox(float value)
{
    const float signValue = value < 0.0 ? -1.0 : 1.0;
    const float absoluteValue = abs(value);
    const float t = 1.0 / (1.0 + 0.3275911 * absoluteValue);
    const float polynomial = (((((1.061405429 * t - 1.453152027) * t)
        + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t;
    return signValue * (1.0 - polynomial * exp(-absoluteValue * absoluteValue));
}

float GaussianEdge(float signedDistance, float sigma)
{
    if (sigma <= 0.0) {
        return signedDistance <= 0.0 ? 1.0 : 0.0;
    }
    return saturate(0.5 * (1.0 - ErfApprox(
        signedDistance * 0.7071067811865475 / sigma)));
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    float2 samplePosition = input.position.xy;
    float clipDistance = min(
        min(samplePosition.x - input.clipBounds.x,
            input.clipBounds.z - samplePosition.x),
        min(samplePosition.y - input.clipBounds.y,
            input.clipBounds.w - samplePosition.y));
    clip(clipDistance);

    float4 shape = input.shapeRect;
    float radius = input.shadowParams.x;
    float2 offset = input.shadowParams.yz;
    float blur = input.shadowParams.w;
    float spread = input.effectParams.x;
    float outlineWidth = input.effectParams.y;
    float outlineOffset = input.effectParams.z;
    float kind = input.effectParams.w;
    float coverage = 0.0;

    if (kind < 0.5) {
        shape.xy -= spread.xx;
        shape.zw += 2.0 * spread.xx;
        clip(min(shape.z, shape.w));
        radius = clamp(radius + spread, 0.0, 0.5 * min(shape.z, shape.w));
        shape.xy += offset;
        coverage = GaussianEdge(
            RoundedRectDistance(samplePosition, shape, radius),
            0.5 * blur);
    } else if (kind < 1.5) {
        float surfaceDistance = RoundedRectDistance(samplePosition, shape, radius);
        clip(-surfaceDistance);
        shape.xy += offset;
        float distanceInside = -RoundedRectDistance(samplePosition, shape, radius) - spread;
        coverage = GaussianEdge(distanceInside, 0.5 * blur);
    } else {
        float distance = RoundedRectDistance(samplePosition, shape, radius);
        float halfAntialias = 0.5 * input.materialParams.y;
        float inner = smoothstep(
            outlineOffset - halfAntialias,
            outlineOffset + halfAntialias,
            distance);
        float outer = 1.0 - smoothstep(
            outlineOffset + outlineWidth - halfAntialias,
            outlineOffset + outlineWidth + halfAntialias,
            distance);
        coverage = saturate(inner * outer);
    }

    return float4(
        input.color.rgb,
        input.color.a * input.materialParams.x * coverage);
}
