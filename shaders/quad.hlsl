struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    float opacity : TEXCOORD2;
    float cornerRadius : TEXCOORD3;
};

VertexOutput VSMain(
    uint vertexId : SV_VertexID,
    float4 clipRect : TEXCOORD0,
    float4 color : TEXCOORD1,
    float opacity : TEXCOORD2,
    float cornerRadius : TEXCOORD3,
    float2 translation : TEXCOORD4)
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
    output.position = float4(clipRect.xy + translation + corners[vertexId] * clipRect.zw, 0.0, 1.0);
    output.uv = corners[vertexId];
    output.color = color;
    output.opacity = opacity;
    output.cornerRadius = cornerRadius;
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    float radius = saturate(input.cornerRadius);
    float2 roundedBox = abs(input.uv - 0.5) - (0.5 - radius);
    float distance = length(max(roundedBox, 0.0))
        + min(max(roundedBox.x, roundedBox.y), 0.0)
        - radius;
    float antialiasWidth = max(fwidth(distance), 0.0001);
    float coverage = 1.0 - smoothstep(0.0, antialiasWidth, distance);
    return float4(input.color.rgb, input.color.a * input.opacity * coverage);
}
