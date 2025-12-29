static const float3 fullScreenTriangle[3] = {
    float3(-1.0, 1.0, 0.0),
    float3(3.0, 1.0, 0.0),
    float3(-1.0, -3.0, 0.0),
};

struct VSOut {
    float4 position : SV_Position;
    [[vk::location(0)]]
    float2 outTex : TEXCOORD0;
};

VSOut main(uint vertexID: SV_VertexID)
{
    VSOut o;
    float3 p = fullScreenTriangle[vertexID];
    o.position = float4(p, 1.0);
    o.outTex = (p.xy + 1.0) * 0.5;
    return o;
}
