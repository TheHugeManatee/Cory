struct CubeUBO
{
    float4x4 projection;
    float4x4 view;
    float4x4 viewProjection;
    float3 lightPosition;
    float padding;
};

struct InstanceData
{
    float4x4 modelTransform;
    float4x4 normalTransform;
    float4 color;
    float4 parameters;
};

struct Push { CubeUBO* globals; };
[[vk::push_constant]] Push gPush;

[[vk::binding(0, 2)]] StructuredBuffer<InstanceData> gReadOnlyBuffers[];

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
    CubeUBO gGlobals = *(gPush.globals);
    const InstanceData instance = gReadOnlyBuffers[0][input.instanceId];
    const float4 localPosition = float4(input.position, 1.0f);
    const float4x4 normalMatrix = instance.normalTransform;

    float4 viewPosition = mul(gGlobals.view, mul(instance.modelTransform, localPosition));

    VSOutput output;
    output.position = mul(gGlobals.projection, viewPosition);
    output.worldPosition = viewPosition.xyz;
    output.normal = mul(normalMatrix, float4(input.normal, 0.0f)).xyz;
    output.color = input.color;
    output.instanceColor = instance.color;
    output.blend = instance.parameters.x;
    return output;
}
