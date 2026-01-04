
[[vk::binding(0, 2)]] StructuredBuffer<uint> gReadOnlyBuffers[];
[[vk::binding(1, 2)]] RWStructuredBuffer<uint> gReadWriteBuffers[];

struct PushConstants {
    uint numInstances;
    uint bitOffset;
    uint keysIndex;
    uint indicesIndex;
    uint histogramIndex;
    uint keysOutIndex;
    uint indicesOutIndex;
};
[[vk::push_constant]] PushConstants p;

groupshared uint s_Hist[16];

[numthreads(256, 1, 1)]
void main(uint3 groupThreadId: SV_GroupThreadID, uint3 groupId: SV_GroupID)
{
    // Load this workgroup's scanned histogram base offsets
    if (groupThreadId.x < 16) {
        s_Hist[groupThreadId.x] =
            gReadOnlyBuffers[p.histogramIndex][groupId.x * 16 + groupThreadId.x];
    }
    GroupMemoryBarrierWithGroupSync();

    // Stable scatter within each workgroup by processing indices in-order on one thread.
    if (groupThreadId.x == 0) {
        uint localCounts[16];
        [unroll]
        for (uint d = 0; d < 16; ++d) {
            localCounts[d] = 0;
        }

        uint start = groupId.x * 256;
        uint end = min(start + 256, p.numInstances);
        for (uint i = start; i < end; ++i) {
            uint key = gReadOnlyBuffers[p.keysIndex][i];
            uint index = gReadOnlyBuffers[p.indicesIndex][i];
            uint digit = (key >> p.bitOffset) & 0xF;
            uint localOffset = s_Hist[digit] + localCounts[digit];
            localCounts[digit] += 1;
            gReadWriteBuffers[p.keysOutIndex][localOffset] = key;
            gReadWriteBuffers[p.indicesOutIndex][localOffset] = index;
        }
    }
}
