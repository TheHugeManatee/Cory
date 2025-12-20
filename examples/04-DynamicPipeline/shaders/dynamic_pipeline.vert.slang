struct VSInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float4 color : COLOR0;
};

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 normal : NORMAL;
    [[vk::location(1)]] float4 color : COLOR0;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float stretch = 0.15f * sin(input.position.x * 4.0f);
    float2 warped = float2(input.position.x, input.position.y + stretch);

    output.position = float4(warped, input.position.z, 1.0f);
    output.normal = normalize(input.normal);
    output.color = input.color;
    return output;
}
