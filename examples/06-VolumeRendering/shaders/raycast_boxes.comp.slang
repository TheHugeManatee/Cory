import Raycast;
import Raymarch;
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
    uint instanceCount;
    uint volumeTextureIndex;
    uint2 padding0;
    Ptr<InstanceData, Access::Read> instances;
};

[[vk::push_constant]] RaycastGlobals *globals;

[[vk::binding(1, 1)]] Sampler3D<float> gVolumeTextures[];
[[vk::binding(5, 1)]] RWTexture2DMS<float4> gColor;

static const float3 boxMin = float3(-0.5, -0.5, -0.5);
static const float3 boxMax = float3(0.5, 0.5, 0.5);
static const uint kMaxRaymarchSteps = 96u;
static const float kTransmittanceCutoff = 0.98f;
static const float3 kTransferLowColor = float3(0.08f, 0.20f, 0.42f);
static const float3 kTransferHighColor = float3(1.00f, 0.86f, 0.48f);
static const float kGradientStep = 1.0f / 64.0f;

float sampleDensity(float3 uvw)
{
    return gVolumeTextures[globals->volumeTextureIndex].SampleLevel(saturate(uvw), 0.0f).r;
}

float3 estimateGradient(float3 uvw)
{
    let offsetX = float3(kGradientStep, 0.0f, 0.0f);
    let offsetY = float3(0.0f, kGradientStep, 0.0f);
    let offsetZ = float3(0.0f, 0.0f, kGradientStep);

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
        let transfer = evaluateTransferFunction(density, stepSize, instance.parameters);
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
    let marchRay = Ray(localOrigin, localDirection, tStart, tEnd);
    let cfg = MarchConfig(stepSize, kMaxRaymarchSteps);
    let sampler = VolumeSampler(instance, stepSize);
    let accum = FrontToBackAccumulator();
    
    return raymarch(marchRay, cfg, sampler, accum);
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
