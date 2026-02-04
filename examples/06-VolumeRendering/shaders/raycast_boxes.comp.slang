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
    uint volumeTextureIndex;
    uint padding0;
    Ptr<InstanceData, Access::Read> instances;
};

[[vk::push_constant]] RaycastGlobals *globals;

[[vk::binding(1, 1)]] Sampler3D<float> gVolumeTextures[];
[[vk::binding(5, 1)]] RWTexture2DMS<float4> gColor;

static const float3 boxMin = float3(-0.5, -0.5, -0.5);
static const float3 boxMax = float3(0.5, 0.5, 0.5);
static const uint kMaxRaymarchSteps = 96u;
static const float kTransmittanceCutoff = 0.98f;
static const float kDensityScale = 1.0f;

float4 raymarchVolume(float3 localOrigin, float3 localDirection, InstanceData instance)
{
    let box = AABB(boxMin, boxMax);
    let localRay = Ray(localOrigin, localDirection);
    let hitInfo = intersect(localRay, box);
    if (!hitInfo.hit) {
        return float4(0.0f);
    }

    let tStart = max(hitInfo.tEnter, 0.0f);
    let tEnd = hitInfo.tExit;
    if (tEnd <= tStart) {
        return float4(0.0f);
    }

    let segmentLength = tEnd - tStart;
    let stepSize = segmentLength / float(kMaxRaymarchSteps);
    var accumColor = float3(0.0f);
    var accumAlpha = 0.0f;

    [loop]
    for (uint step = 0u; step < kMaxRaymarchSteps; ++step) {
        let t = tStart + (float(step) + 0.5f) * stepSize;
        if (t > tEnd) {
            break;
        }

        let localPos = localOrigin + localDirection * t;
        let uvw = localPos + 0.5f;
        let density = gVolumeTextures[globals->volumeTextureIndex].SampleLevel(uvw, 0.0f).r;
        if (density <= 0.0f) {
            continue;
        }

        let opacity = saturate(1.0f - exp(-density * kDensityScale * stepSize));
        let sampleColor = float3(density, density, density);//instance.color.rgb * density;

        accumColor += (1.0f - accumAlpha) * opacity * sampleColor;
        accumAlpha += (1.0f - accumAlpha) * opacity;
        if (accumAlpha >= kTransmittanceCutoff) {
            break;
        }
    }

    return float4(accumColor, accumAlpha);
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
        var volumeColor = float4(0.0f);

        [loop]
        for (uint i = 0u; i < globals->instanceCount; ++i) {
            let instance = globals->instances[i];
            let localOrigin = mul(instance.worldToModel, float4(rayOrigin, 1.0f)).xyz;
            let localDirection = mul(instance.worldToModel, float4(rayDirection, 0.0f)).xyz;
            let instanceVolume = raymarchVolume(localOrigin, localDirection, instance);
            if (instanceVolume.a > 0.0f) {
                volumeColor.rgb += (1.0f - volumeColor.a) * instanceVolume.rgb;
                volumeColor.a += (1.0f - volumeColor.a) * instanceVolume.a;
                if (volumeColor.a >= kTransmittanceCutoff) {
                    break;
                }
            }
        }

        if (volumeColor.a > 0.0f) {
            let dst = gColor[coords, sample];
            let outColor = float4(volumeColor.rgb + (1.0f - volumeColor.a) * dst.rgb,
                                  volumeColor.a + (1.0f - volumeColor.a) * dst.a);
            gColor[coords, sample] = outColor;
        }
    }
}
