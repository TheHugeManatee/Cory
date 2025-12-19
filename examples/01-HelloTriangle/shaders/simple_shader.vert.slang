struct PushConstants
{
    float4 color;
    float2x2 transform;
    float2 offset;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> gPushConstants;

struct VSInput
{
    float3 position : TEXCOORD0;
    float3 color : TEXCOORD1;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float2 transformed = mul(gPushConstants.transform, input.position.xy) + gPushConstants.offset;
    output.position = float4(transformed, 0.0f, 1.0f);
    output.color = float4(input.color, 1.0f);
    return output;
}
