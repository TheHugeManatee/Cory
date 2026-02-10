import Raycast;
import Raymarch;
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
static const uint kMaxRaymarchStepsLimit = 1024u;
static const float kTransmittanceCutoff = 0.98f;
static const float3 kTransferLowColor = float3(0.08f, 0.20f, 0.42f);
static const float3 kTransferHighColor = float3(1.00f, 0.86f, 0.48f);

float hash13(float3 p)
{
    var p3 = frac(p * 0.1031f);
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.x + p3.y) * p3.z);
}

float sampleDensity(float3 uvw)
{
    return gBindlessTexture3D[globals->volumeTextureIndex].SampleLevel(saturate(uvw), 0.0f).r;
}

float3 estimateGradient(float3 uvw)
{
    let dims = float3(globals->volumeDimensions);
    let texel = 1.0f / max(dims, float3(1.0f));

    let offsetX = float3(texel.x, 0.0f, 0.0f);
    let offsetY = float3(0.0f, texel.y, 0.0f);
    let offsetZ = float3(0.0f, 0.0f, texel.z);

    let gx = sampleDensity(uvw + offsetX) - sampleDensity(uvw - offsetX);
    let gy = sampleDensity(uvw + offsetY) - sampleDensity(uvw - offsetY);
    let gz = sampleDensity(uvw + offsetZ) - sampleDensity(uvw - offsetZ);
    return float3(gx, gy, gz);
}

float4 evaluateTransferFunction(float density, float stepSize, float4 transferParams)
{
    let densityMin = transferParams.x;
    let densityMax = transferParams.y;
    let opacityScale = transferParams.z;
    let gamma = transferParams.w;

    let densityRange = max(densityMax - densityMin, 1e-5f);
    var mapped = saturate((density - densityMin) / densityRange);
    if (mapped <= 0.0f) {
        return float4(0.0f);
    }

    mapped = pow(mapped, gamma);
    let color = lerp(kTransferLowColor, kTransferHighColor, mapped);
    let opacity = saturate(1.0f - exp(-mapped * opacityScale * stepSize));
    return float4(color, opacity);
}

struct VolumeSample
{
    float3 rgb;
    float a;

    __init(float3 rgb = float3(0.0f), float a = 0.0f)
    {
        this.rgb = rgb;
        this.a = a;
    }
};

struct VolumeSampler : ISampler<VolumeSample>
{
    InstanceData instance;
    float stepSize;

    __init(InstanceData instance, float stepSize)
    {
        this.instance = instance;
        this.stepSize = stepSize;
    }

    VolumeSample eval(Ray ray, float t)
    {
        let localPos = ray.at(t);
        let uvw = localPos + 0.5f;
        let density = sampleDensity(uvw);
        let transfer = evaluateTransferFunction(density, stepSize, instance.transferParams);
        if (transfer.a <= 0.0f) {
            return VolumeSample();
        }

        var sampleColor = transfer.rgb;
        let gradient = estimateGradient(uvw);
        var localNormal = -ray.direction;
        if (dot(gradient, gradient) > 1e-7f) {
            localNormal = normalize(-gradient);
        }

        let worldNormal = normalize(mul((float3x3)instance.normalToWorld, localNormal));
        let worldPos = mul(instance.modelToWorld, float4(localPos, 1.0f)).xyz;
        let viewDir = normalize(globals->cameraPosition.xyz - worldPos);
        let lightDir = viewDir; // headlight-style lighting for simple direct illumination
        let halfDir = normalize(lightDir + viewDir);

        let ndotl = max(dot(worldNormal, lightDir), 0.0f);
        let spec = pow(max(dot(worldNormal, halfDir), 0.0f), 24.0f) * 0.20f;
        sampleColor = sampleColor * (0.15f + 0.85f * ndotl) + spec.xxx;
        return VolumeSample(sampleColor, transfer.a);
    }
};

struct FrontToBackState
{
    float3 color;
    float alpha;
};

struct FrontToBackAccumulator : IAccumulator<VolumeSample, FrontToBackState, float4>
{
    void init(out FrontToBackState state, Ray ray, MarchConfig cfg)
    {
        state.color = float3(0.0f);
        state.alpha = 0.0f;
    }

    void step(inout FrontToBackState state, VolumeSample s, Ray ray, float t)
    {
        state.color += (1.0f - state.alpha) * s.a * s.rgb;
        state.alpha += (1.0f - state.alpha) * s.a;
    }

    bool shouldTerminate(FrontToBackState state)
    {
        return state.alpha >= kTransmittanceCutoff;
    }

    float4 finalize(FrontToBackState state)
    {
        return float4(state.color, state.alpha);
    }
};

float4 raymarchVolume(float3 localOrigin,
                      float3 localDirection,
                      InstanceData instance,
                      float phaseJitter)
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

    let dirLength = max(length(localDirection), 1e-6f);
    let maxDim = max(globals->volumeDimensions.x,
                     max(globals->volumeDimensions.y, globals->volumeDimensions.z));
    let voxelSize = 1.0f / float(max(maxDim, 1u));
    let stepMultiplier = max(instance.raymarchParams.x, 0.01f);
    let jitterEnabled = instance.raymarchParams.y > 0.5f;
    let stepSize = stepMultiplier * voxelSize / dirLength;
    let segmentLength = tEnd - tStart;
    let jitterCentered = (phaseJitter - 0.5f) * (jitterEnabled ? 1.0f : 0.0f);
    // Shift sampling phase by up to +/- half a step while keeping the integration interval fixed.
    let marchStart = tStart + jitterCentered * stepSize;
    let stepCount = min(max(uint(ceil(segmentLength / stepSize)) + 1u, 1u), kMaxRaymarchStepsLimit);

    let marchRay = Ray(localOrigin, localDirection, marchStart, tEnd);
    let cfg = MarchConfig(stepSize, stepCount);
    let sampler = VolumeSampler(instance, stepSize);
    let accum = FrontToBackAccumulator();

    return raymarch(marchRay, cfg, sampler, accum);
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
        let jitterSeed = float3(float2(coords) + samplePosition,
                                globals->time * 91.7f + float(sample) * 17.0f);
        let phaseJitter = hash13(jitterSeed + float3(13.1f, 7.7f, 3.3f));
        var volumeColor = float4(0.0f);

        [loop]
        for (uint i = 0u; i < globals->instanceCount; ++i) {
            let instance = globals->instances[i];
            let localOrigin = mul(instance.worldToModel, float4(rayOrigin, 1.0f)).xyz;
            let localDirection = mul(instance.worldToModel, float4(rayDirection, 0.0f)).xyz;
            let instanceVolume = raymarchVolume(localOrigin, localDirection, instance, phaseJitter);
            if (instanceVolume.a > 0.0f) {
                volumeColor.rgb += (1.0f - volumeColor.a) * instanceVolume.rgb;
                volumeColor.a += (1.0f - volumeColor.a) * instanceVolume.a;
                if (volumeColor.a >= kTransmittanceCutoff) {
                    break;
                }
            }
        }

        if (volumeColor.a > 0.0f) {
            if (globals->colorTargetIsMsaa != 0u) {
                let dst = gBindlessStorageImage2DMS[coords, sample];
                let outColor = float4(volumeColor.rgb + (1.0f - volumeColor.a) * dst.rgb,
                                      volumeColor.a + (1.0f - volumeColor.a) * dst.a);
                gBindlessStorageImage2DMS[coords, sample] = outColor;
            }
            else {
                let dst = gBindlessStorageImage2D[coords];
                let outColor = float4(volumeColor.rgb + (1.0f - volumeColor.a) * dst.rgb,
                                      volumeColor.a + (1.0f - volumeColor.a) * dst.a);
                gBindlessStorageImage2D[coords] = outColor;
            }
        }
    }
}
