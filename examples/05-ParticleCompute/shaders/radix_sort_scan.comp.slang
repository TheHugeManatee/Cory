
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> gHistograms; // [numWorkgroups][16]

struct PushConstants {
    uint numWorkgroups;
    uint pad;
};
[[vk::push_constant]] PushConstants p;

// Simple prefix sum over the entire histogram buffer.
// Assuming numWorkgroups * 16 is small enough for one workgroup (e.g. 1024 * 16 = 16384, maybe too big for one wg?)
// For 3000 particles, numWorkgroups = ceil(3000/256) = 12. 12 * 16 = 192. Definitely fits in one wg.

[numthreads(256, 1, 1)]
void main(uint3 groupThreadId : SV_GroupThreadID)
{
    uint tid = groupThreadId.x;
    uint totalBins = p.numWorkgroups * 16;
    
    // We want to prefix sum the histograms such that:
    // GlobalOffset[digit] = Sum(counts[0..numWorkgroups-1][0..digit-1])
    // LocalOffset[wg][digit] = GlobalOffset[digit] + Sum(counts[0..wg-1][digit])
    
    // First, let's do a simple serial scan in one thread if it's small.
    if (tid == 0) {
        uint sum = 0;
        // The order should be: all counts for digit 0, then all counts for digit 1, ...
        // Wait, the scatter needs to know where each workgroup should start for a given digit.
        
        // Let's re-organize the histogram in memory for easier access: [digit][wg]
        // Actually let's keep [wg][digit] but scan carefully.
        
        for (uint digit = 0; digit < 16; ++digit) {
            for (uint wg = 0; wg < p.numWorkgroups; ++wg) {
                uint idx = wg * 16 + digit;
                uint val = gHistograms[idx];
                gHistograms[idx] = sum;
                sum += val;
            }
        }
    }
}
