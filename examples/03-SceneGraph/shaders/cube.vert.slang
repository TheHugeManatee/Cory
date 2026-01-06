struct CubeUBO
{
    float4x4 projection;
    float4x4 view;
    float4x4 viewProjection;
    float3 lightPosition;
    float padding0;
    Ptr<InstanceData, Access::Read> instances;

};

struct InstanceData
{
    float4x4 modelToWorld;
    float4x4 normalToWorld;
    float4 color;
    float4 parameters;
};

[[vk::binding(0, 2)]] StructuredBuffer<InstanceData> gReadOnlyBuffers[];
[[vk::push_constant]] CubeUBO* gGlobals;

struct VSInput
{
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 color : COLOR0;
    uint instanceId : SV_InstanceID;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 color : COLOR0;
    float4 instanceColor : COLOR1;
    float blend : TEXCOORD2;
};

VSOutput main(VSInput input)
{
    const InstanceData instance = gGlobals->instances[input.instanceId];
    const float4 localPosition = float4(input.position, 1.0f);
    const float4x4 normalMatrix = instance.normalToWorld;

    float4 worldPosition = mul(instance.modelToWorld, localPosition);

    VSOutput output;
    output.position = mul(gGlobals->viewProjection, worldPosition);
    output.worldPosition = worldPosition.xyz;
    output.normal = mul(normalMatrix, float4(input.normal, 0.0f)).xyz;
    output.color = input.color;
    output.instanceColor = instance.color;
    output.blend = instance.parameters.x;
    return output;
}
