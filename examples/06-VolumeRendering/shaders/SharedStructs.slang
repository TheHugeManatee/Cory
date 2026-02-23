enum class VolumeRenderMode : uint32_t {
    DvrRaymarch,
    StochasticSingleBounce,
};

struct VolumeTransferParams
{
    float densityMin;
    float densityMax;
    float opacityScale;
    float gamma;
};

struct InstanceData
{
    float4x4 modelToWorld;
    float4x4 worldToModel;
    float4x4 normalToWorld;
    float4 color;
    VolumeTransferParams transferParams;
    float raymarchStepSizeMultiplier;
    uint samples;
    float raymarchJitteringEnabled;
    VolumeRenderMode renderMode ;
    uint volumeTextureIndex;
    uint3 volumeDimensions;
};

struct LightData
{
    float4 position;
    float4 radiance;
};

struct RaycastGlobals
{
    float4x4 invViewProjection;
    float4 cameraPosition;
    float time;
    uint instanceCount;
    uint colorTargetIsMsaa;
    float temporalBlendFactor;
    float alphaDeltaRejectThreshold;
    uint lightCount;
    uint _padding0;
    Ptr<InstanceData, Access::Read> instances;
    Ptr<LightData, Access::Read> lights;
};

static const float3 boxMin = float3(-0.5, -0.5, -0.5);
static const float3 boxMax = float3(0.5, 0.5, 0.5);
static const uint kMaxRaymarchStepsLimit = 1024u;
static const float kTransmittanceCutoff = 0.98f;
static const float3 kTransferLowColor = float3(0.08f, 0.20f, 0.42f);
static const float3 kTransferHighColor = float3(1.00f, 0.86f, 0.48f);
