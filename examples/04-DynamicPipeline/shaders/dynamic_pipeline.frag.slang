struct FSInput
{
    [[vk::location(0)]] float3 normal : NORMAL;
    [[vk::location(1)]] float4 color : COLOR0;
};

float4 main(FSInput input) : SV_Target0
{
    float3 lightDir = normalize(float3(0.2f, 0.3f, 1.0f));
    float intensity = clamp(dot(normalize(input.normal), lightDir), 0.0f, 1.0f);
    float3 rgb = input.color.rgb * (0.3f + 0.7f * intensity);
    return float4(rgb, 1.0f);
}
