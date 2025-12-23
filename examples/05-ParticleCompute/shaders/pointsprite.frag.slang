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

ConstantBuffer<CubeUBO> gGlobals : register(b0, space0);

struct FSInput
{
    float4 color : COLOR0;
    float2 localPos : TEXCOORD0;
};

float4 main(FSInput input) : SV_Target0
{
    const float2 uv = input.localPos;
    const float radial = saturate(1.0f - dot(uv, uv));
    const float alpha = pow(radial, 2.0f);
    const float4 albedo = input.color;
    float4 color = albedo;
    color.rgb *= alpha;
    color.a *= alpha;
    return color;
}
