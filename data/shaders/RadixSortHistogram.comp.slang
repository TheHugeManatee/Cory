
[[vk::binding(0, 2)]] StructuredBuffer<uint> gReadOnlyBuffers[];
[[vk::binding(1, 2)]] RWStructuredBuffer<uint> gReadWriteBuffers[]; // [numWorkgroups][16]

struct PushConstants {
    uint numInstances;
    uint bitOffset;
};
[[vk::push_constant]] PushConstants p;

groupshared uint s_Hist[16];

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID, uint3 groupThreadId : SV_GroupThreadID, uint3 groupId : SV_GroupID)
{
    if (groupThreadId.x < 16) {
        s_Hist[groupThreadId.x] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint i = dispatchThreadId.x;
    if (i < p.numInstances) {
        uint key = gReadOnlyBuffers[0][i];
        uint digit = (key >> p.bitOffset) & 0xF;
        InterlockedAdd(s_Hist[digit], 1);
    }
    GroupMemoryBarrierWithGroupSync();

    if (groupThreadId.x < 16) {
        gReadWriteBuffers[0][groupId.x * 16 + groupThreadId.x] = s_Hist[groupThreadId.x];
    }
}
