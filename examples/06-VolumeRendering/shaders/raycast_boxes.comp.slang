import Raycast;
import Msaa;

struct InstanceData
{
    float4x4 modelToWorld;
    float4x4 worldToModel;
    float4x4 normalToWorld;
    float4 color;
    float4 parameters;
};

struct RaycastGlobals
{
    float4x4 invViewProjection;
    float4 cameraPosition;
    uint2 imageSize;
    uint instanceCount;
    uint padding0;
    Ptr<InstanceData, Access::Read> instances;
};

[[vk::push_constant]] RaycastGlobals *globals;

[[vk::binding(5, 1)]] RWTexture2DMS<float4> gColor;

static const float3 boxMin = float3(-0.5, -0.5, -0.5);
static const float3 boxMax = float3(0.5, 0.5, 0.5);

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint imageWidth = 0;
    uint imageHeight = 0;
    uint sampleCount = 0;
    gColor.GetDimensions(imageWidth, imageHeight, sampleCount);

    if (dispatchThreadId.x >= imageWidth || dispatchThreadId.y >= imageHeight) {
        return;
    }

    int2 coords = int2(dispatchThreadId.xy);
    float2 resolution = float2(imageWidth, imageHeight);
    float3 rayOrigin = globals->cameraPosition.xyz;

    [loop]
    for (uint sample = 0; sample < sampleCount; ++sample) {
        float2 samplePosition = getMsaaSamplePosition(sampleCount, sample);
        float2 pixel = float2(coords) + samplePosition;
        float2 ndc = pixel / resolution * 2.0f - 1.0f;

        float4 clip = float4(ndc, 1.0f, 1.0f);
        float4 worldPos = mul(globals->invViewProjection, clip);
        worldPos /= worldPos.w;

        float3 rayDirection = normalize(worldPos.xyz - rayOrigin);
        bool hit = false;
        for (uint i = 0u; i < globals->instanceCount; ++i) {
            InstanceData instance = globals->instances[i];
            float3 localOrigin = mul(instance.worldToModel, float4(rayOrigin, 1.0f)).xyz;
            float3 localDirection = mul(instance.worldToModel, float4(rayDirection, 0.0f)).xyz;

            if (intersectsBox(localOrigin, localDirection, boxMin, boxMax)) {
                hit = true;
                break;
            }
        }

        if (hit) {
            gColor[coords, sample] = float4(1.0f, 0.0f, 0.0f, 1.0f);
        }
    }
}
