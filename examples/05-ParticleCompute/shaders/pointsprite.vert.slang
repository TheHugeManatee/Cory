struct CubeUBO
{
    float4x4 projection;
    float4x4 view;
    float4x4 viewProjection;
    float3 lightPosition;
    float padding0;
    float3 cameraRight;
    float padding1;
    float3 cameraUp;
    float padding2;
};

struct InstanceData
{
    float4 positionAndSize;
    float4 color;
    float4 parameters;
};

[[vk::binding(0, 0)]] ConstantBuffer<CubeUBO> gGlobals : register(b0, space0);
[[vk::binding(2, 0)]] StructuredBuffer<InstanceData> gInstances : register(t2, space0);
[[vk::binding(4, 0)]] StructuredBuffer<uint> gIndices : register(t4, space0);

struct VSInput
{
    uint vertexId : SV_VertexID;
    uint instanceId : SV_InstanceID;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 localPos : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    const uint sortedIndex = gIndices[input.instanceId];
    const InstanceData instance = gInstances[sortedIndex];
    static const float2 quadCorners[6] = {
        float2(-1.0, -1.0),
        float2(1.0, -1.0),
        float2(1.0, 1.0),
        float2(-1.0, -1.0),
        float2(1.0, 1.0),
        float2(-1.0, 1.0),
    };

    const uint cornerIndex = input.vertexId % 6;
    const float2 corner = quadCorners[cornerIndex];
    const float halfSize = instance.positionAndSize.w;

    const float3 center = instance.positionAndSize.xyz;
    const float3 right = gGlobals.cameraRight;
    const float3 up = gGlobals.cameraUp;

    const float3 worldOffset = corner.x * right * halfSize + corner.y * up * halfSize;
    const float3 worldPosition = center + worldOffset;

    VSOutput output;
    output.position = mul(gGlobals.viewProjection, float4(worldPosition, 1.0));
    output.color = instance.color;
    output.localPos = corner;
    return output;
}
