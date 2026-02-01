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

[[vk::binding(2, 1)]] RWTexture2DMS<float4> gColor;

static const float3 kBoxMin = float3(-0.5, -0.5, -0.5);
static const float3 kBoxMax = float3(0.5, 0.5, 0.5);
static const float kEpsilon = 1e-6;
static const float kInfinity = 1e20;

bool intersectsBox(float3 origin, float3 direction)
{
    float tNear = -kInfinity;
    float tFar = kInfinity;

    for (int axis = 0; axis < 3; ++axis) {
        float originValue = origin[axis];
        float dirValue = direction[axis];
        float minBound = kBoxMin[axis];
        float maxBound = kBoxMax[axis];

        if (abs(dirValue) < kEpsilon) {
            if (originValue < minBound || originValue > maxBound) {
                return false;
            }
            continue;
        }

        float invDir = 1.0 / dirValue;
        float t0 = (minBound - originValue) * invDir;
        float t1 = (maxBound - originValue) * invDir;
        if (t0 > t1) {
            float tmp = t0;
            t0 = t1;
            t1 = tmp;
        }

        tNear = max(tNear, t0);
        tFar = min(tFar, t1);
        if (tNear > tFar) {
            return false;
        }
    }

    return tFar >= max(tNear, 0.0);
}

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

    float2 pixel = float2(dispatchThreadId.xy);
    float2 resolution = float2(imageWidth, imageHeight);
    float2 ndc = (pixel + 0.5f) / resolution * 2.0f - 1.0f;

    float4 clip = float4(ndc, 1.0f, 1.0f);
    float4 worldPos = mul(globals->invViewProjection, clip);
    worldPos /= worldPos.w;

    float3 rayOrigin = globals->cameraPosition.xyz;
    float3 rayDirection = normalize(worldPos.xyz - rayOrigin);

    bool hit = false;
    for (uint i = 0u; i < globals->instanceCount; ++i) {
        InstanceData instance = globals->instances[i];
        float3 localOrigin = mul(instance.worldToModel, float4(rayOrigin, 1.0f)).xyz;
        float3 localDirection = mul(instance.worldToModel, float4(rayDirection, 0.0f)).xyz;

        if (intersectsBox(localOrigin, localDirection)) {
            hit = true;
            break;
        }
    }

    if (hit) {
        int2 coords = int2(dispatchThreadId.xy);
        [loop]
        for (uint sample = 0; sample < sampleCount; ++sample) {
            gColor[coords, sample] = float4(1.0f, 0.0f, 0.0f, 1.0f);
        }
    }
}
