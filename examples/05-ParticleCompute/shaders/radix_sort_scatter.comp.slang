
[[vk::binding(2, 0)]] StructuredBuffer<uint> gKeysIn;
[[vk::binding(3, 0)]] StructuredBuffer<uint> gIndicesIn;
[[vk::binding(4, 0)]] RWStructuredBuffer<uint> gKeysOut;
[[vk::binding(5, 0)]] RWStructuredBuffer<uint> gIndicesOut;
[[vk::binding(6, 0)]] StructuredBuffer<uint> gHistograms; // [numWorkgroups][16] (scanned)

struct PushConstants {
    uint numInstances;
    uint bitOffset;
};
[[vk::push_constant]] PushConstants p;

groupshared uint s_Hist[16];

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID, uint3 groupThreadId : SV_GroupThreadID, uint3 groupId : SV_GroupID)
{
    // Load this workgroup's scanned histogram base offsets
    if (groupThreadId.x < 16) {
        s_Hist[groupThreadId.x] = gHistograms[groupId.x * 16 + groupThreadId.x];
    }
    GroupMemoryBarrierWithGroupSync();

    uint i = dispatchThreadId.x;
    if (i < p.numInstances) {
        uint key = gKeysIn[i];
        uint index = gIndicesIn[i];
        uint digit = (key >> p.bitOffset) & 0xF;
        
        // We need to know the local offset of this item within this workgroup for this digit.
        // This is tricky without a local scan.
        // For simplicity (but lower performance), we can use InterlockedAdd on groupshared memory.
        
        uint localOffset;
        InterlockedAdd(s_Hist[digit], 1, localOffset);
        
        gKeysOut[localOffset] = key;
        gIndicesOut[localOffset] = index;
    }
}
