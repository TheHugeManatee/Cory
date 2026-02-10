import Raycast;
import Msaa;
import CoryBindings;

struct InstanceData
{
    float4x4 modelToWorld;
    float4x4 worldToModel;
    float4x4 normalToWorld;
    float4 color;
    float4 transferParams;
    float4 raymarchParams;
};

struct RaycastGlobals
{
    float4x4 invViewProjection;
    float4 cameraPosition;
    uint3 volumeDimensions;
    float time;
    uint instanceCount;
    uint volumeTextureIndex;
    uint colorTargetIsMsaa;
    uint padding1;
    Ptr<InstanceData, Access::Read> instances;
};

[[vk::push_constant]] RaycastGlobals *globals;

static const float3 boxMin = float3(-0.5, -0.5, -0.5);
static const float3 boxMax = float3(0.5, 0.5, 0.5);

struct RayBoxHit
{
    bool hit;
    float t;
    float3 localPos;
};

RayBoxHit nearestLocalHit(float3 rayOrigin, float3 rayDirection)
{
    let box = AABB(boxMin, boxMax);
    var closestHit = RayBoxHit(false, 0.0f, float3(0.0f));
    var closestT = 1e30f;

    [loop]
    for (uint i = 0u; i < globals->instanceCount; ++i) {
        let instance = globals->instances[i];
        let localOrigin = mul(instance.worldToModel, float4(rayOrigin, 1.0f)).xyz;
        let localDirection = mul(instance.worldToModel, float4(rayDirection, 0.0f)).xyz;
        let hitInfo = intersect(Ray(localOrigin, localDirection), box);
        if (!hitInfo.hit) {
            continue;
        }

        let tHit = max(hitInfo.tEnter, 0.0f);
        if (tHit < closestT) {
            closestT = tHit;
            closestHit.hit = true;
            closestHit.t = tHit;
            closestHit.localPos = localOrigin + localDirection * tHit;
        }
    }

    return closestHit;
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint imageWidth = 0;
    uint imageHeight = 0;
    uint sampleCount = 1;
    if (globals->colorTargetIsMsaa != 0u) {
        gBindlessStorageImage2DMS.GetDimensions(imageWidth, imageHeight, sampleCount);
    }
    else {
        gBindlessStorageImage2D.GetDimensions(imageWidth, imageHeight);
    }

    if (dispatchThreadId.x >= imageWidth || dispatchThreadId.y >= imageHeight) {
        return;
    }

    let coords = int2(dispatchThreadId.xy);
    let resolution = float2(imageWidth, imageHeight);
    let rayOrigin = globals->cameraPosition.xyz;

    [loop]
    for (uint sample = 0; sample < sampleCount; ++sample) {
        let samplePosition = getMsaaSamplePosition(sampleCount, sample);
        let pixel = float2(coords) + samplePosition;
        let ndc = pixel / resolution * 2.0f - 1.0f;

        let clip = float4(ndc, 1.0f, 1.0f);
        var worldPos = mul(globals->invViewProjection, clip);
        worldPos /= worldPos.w;
        let rayDirection = normalize(worldPos.xyz - rayOrigin);

        let hit = nearestLocalHit(rayOrigin, rayDirection);
        if (!hit.hit) {
            continue;
        }

        let local01 = saturate(hit.localPos + 0.5f);
        if (globals->colorTargetIsMsaa != 0u) {
            gBindlessStorageImage2DMS[coords, sample] = float4(local01, 1.0f);
        }
        else {
            gBindlessStorageImage2D[coords] = float4(local01, 1.0f);
        }
    }
}
