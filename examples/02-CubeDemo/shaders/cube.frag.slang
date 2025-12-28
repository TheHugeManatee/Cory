struct CubeUBO
{
    float4x4 projection;
    float4x4 view;
    float4x4 viewProjection;
    float3 lightPosition;
    float padding;
};

struct Push { CubeUBO* globals; };
[[vk::push_constant]] Push gPush;

struct FSInput
{
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 color : COLOR0;
    float4 instanceColor : COLOR1;
    float blend : TEXCOORD2;
};

float4 main(FSInput input) : SV_Target0
{
    CubeUBO gGlobals = *(gPush.globals);
    const float3 posToLight = gGlobals.lightPosition - input.worldPosition;
    const float3 lightVector = normalize(posToLight);
    const float diffuse = max(0.0f, dot(input.normal, lightVector));
    const float4 albedo = lerp(input.color, input.instanceColor, input.blend);
    return diffuse * albedo;
}
