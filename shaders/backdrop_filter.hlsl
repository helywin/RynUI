#if defined(RYNUI_FRAGMENT_SHADER)
SamplerState SourceSampler : register(s0, space2);
Texture2D<float4> SourceTexture : register(t0, space2);
StructuredBuffer<float4> GaussianWeights : register(t1, space2);

cbuffer PassViewport : register(b0, space3) {
    float4 fullViewport;
    float4 targetOriginExtent;
};

cbuffer GaussianFilter : register(b1, space3) {
    float4 sourceOriginExtent;
    uint4 axisRadiusFirstCount;
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
    int2 globalPixel = int2(input.position.xy + targetOriginExtent.xy);
    int2 origin = int2(sourceOriginExtent.xy);
    int2 maximum = int2(fullViewport.xy) - 1;
    int2 axis = axisRadiusFirstCount.x == 0u ? int2(1, 0) : int2(0, 1);
    uint first = axisRadiusFirstCount.z;
    float4 result =
        SourceTexture.Load(int3(clamp(globalPixel, int2(0, 0), maximum) - origin, 0)) * GaussianWeights[first].x;
    for (uint i = 1u; i < axisRadiusFirstCount.w; ++i) {
        int2 delta = axis * int(i);
        float4 a = SourceTexture.Load(int3(clamp(globalPixel - delta, int2(0, 0), maximum) - origin, 0));
        float4 b = SourceTexture.Load(int3(clamp(globalPixel + delta, int2(0, 0), maximum) - origin, 0));
        result += (a + b) * GaussianWeights[first + i].x;
    }
    return result;
}
#endif
