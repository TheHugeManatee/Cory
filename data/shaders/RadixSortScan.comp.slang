
[[vk::binding(1, 2)]] RWStructuredBuffer<uint> gReadWriteBuffers[]; // [numWorkgroups][16]

struct PushConstants {
    uint numWorkgroups;
    uint histogramIndex;
};
[[vk::push_constant]] PushConstants p;

// Simple prefix sum over the entire histogram buffer.
[numthreads(256, 1, 1)]
void main(uint3 groupThreadId: SV_GroupThreadID)
{
    uint tid = groupThreadId.x;
    uint totalBins = p.numWorkgroups * 16;

    // We want to prefix sum the histograms such that:
    // GlobalOffset[digit] = Sum(counts[0..numWorkgroups-1][0..digit-1])
    // LocalOffset[wg][digit] = GlobalOffset[digit] + Sum(counts[0..wg-1][digit])
    if (tid == 0) {
        uint sum = 0;

        for (uint digit = 0; digit < 16; ++digit) {
            for (uint wg = 0; wg < p.numWorkgroups; ++wg) {
                uint idx = wg * 16 + digit;
                uint val = gReadWriteBuffers[p.histogramIndex][idx];
                gReadWriteBuffers[p.histogramIndex][idx] = sum;
                sum += val;
            }
        }
    }
}
