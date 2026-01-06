struct PointSpriteGlobals {
    float4x4 projection;
    float4x4 view;
    float4x4 viewProjection;

    float3 lightPosition;
    float padding0;

    float3 cameraRight;
    float padding1;

    float3 cameraUp;
    float padding2;

    Ptr<uint, Access::Read> sortKeys;
    Ptr<uint, Access::Read> sortIndices;
    Ptr<InstanceData, Access::Read> instances;
};

struct InstanceData {
    float4 positionAndSize;
    float4 color;
    float4 parameters;
};

[[vk::push_constant]] PointSpriteGlobals *globals;

struct VSInput {
    uint vertexId : SV_VertexID;
    uint instanceId : SV_InstanceID;
};

struct VSOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 localPos : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    const uint sortedIndex = globals->sortIndices[input.instanceId];
    const InstanceData instance = globals->instances[sortedIndex];

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
    const float3 right = globals->cameraRight;
    const float3 up = globals->cameraUp;

    const float3 worldOffset = corner.x * right * halfSize + corner.y * up * halfSize;
    const float3 worldPosition = center + worldOffset;

    VSOutput output;
    output.position = mul(globals->viewProjection, float4(worldPosition, 1.0));
    output.color = instance.color;
    output.localPos = corner;
    return output;
}
