struct CubeUBO
{
    float4x4 projection;
    float4x4 view;
    float4x4 viewProjection;
    float3 lightPosition;
    uint bufferIndex;
};

[[vk::push_constant]] CubeUBO* gGlobals;

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
    const float ambient = 0.1f;
    const float3 posToLight = gGlobals->lightPosition - input.worldPosition;
    const float3 lightVector = normalize(posToLight);
    const float diffuse = abs(dot(input.normal, lightVector));
    const float4 albedo = lerp(input.color, input.instanceColor, input.blend);
    const float lighting = min(diffuse + ambient, 1.0f);
    return lighting * albedo;
}
