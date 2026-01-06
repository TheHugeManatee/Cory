
struct InstanceData {
    float4 positionAndSize;
    float4 color;
    float4 parameters;
};

struct PointSpriteGlobals {
    float4x4 projection;
    float4x4 view;
    float4x4 viewProjection;

    float3 lightPosition;
    float padding0;

    float3 cameraRight;
    float padding1;

    float3 cameraUp;
    float padding2;

    Ptr<uint, Access::ReadWrite> sortKeys;
    Ptr<uint, Access::Read> sortIndices;
    Ptr<InstanceData, Access::Read> instances;
};

// Bindings
[[vk::binding(0, 2)]] StructuredBuffer<InstanceData> gReadOnlyBuffers[];
[[vk::binding(1, 2)]] RWStructuredBuffer<uint> gReadWriteBuffers[];

struct PushConstants {
    uint numInstances;
    uint pad;
    PointSpriteGlobals *globals;
};
[[vk::push_constant]] PushConstants p;

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId: SV_DispatchThreadID)
{
    uint i = dispatchThreadId.x;
    if (i >= p.numInstances) return;

    float3 pos = p.globals->instances[i].positionAndSize.xyz;
    float4 viewPos = mul(p.globals->view, float4(pos, 1.0));

    // View space Z is negative in front of camera in many systems
    float dist = -viewPos.z; // Distance from camera plane

    // Float to sortable uint conversion - this converts float to uint preserving order
    uint bits = asuint(dist);
    uint key = (bits & 0x80000000u) != 0 ? ~bits                 // negative: flip all bits
                                         : (bits ^ 0x80000000u); // positive: flip sign bit

    p.globals->sortKeys[i] = key;
}
