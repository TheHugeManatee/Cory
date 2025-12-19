struct PushConstants
{
    float4 color;
    float2x2 transform;
    float2 offset;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> gPushConstants;

struct FSInput
{
    float4 color : COLOR0;
};

float4 main(FSInput input) : SV_Target0
{
    return gPushConstants.color;
}
