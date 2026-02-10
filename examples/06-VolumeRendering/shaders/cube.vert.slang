struct InstanceData
{
    float4x4 modelToWorld;
    float4x4 worldToModel;
    float4x4 normalToWorld;
    float4 color;
    float4 transferParams;
    float4 raymarchParams;
};

struct ViewData
{
    float4x4 projection;
    float4x4 view;
    float4x4 viewProjection;
    float3 lightPosition;
    float padding0;
    Ptr<InstanceData, Access::Read> instances;
};
[[vk::push_constant]] ViewData *globals;



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
    const InstanceData instance = globals->instances[input.instanceId];
    const float4 localPosition = float4(input.position, 1.0f);
    const float4x4 normalMatrix = instance.normalToWorld;

    float4 viewPosition = mul(globals->view, mul(instance.modelToWorld, localPosition));

    VSOutput output;
    output.position = mul(globals->projection, viewPosition);
    output.worldPosition = viewPosition.xyz;
    output.normal = mul(normalMatrix, float4(input.normal, 0.0f)).xyz;
    output.color = input.color;
    output.instanceColor = instance.color;
    output.blend = instance.transferParams.x;
    return output;
}
