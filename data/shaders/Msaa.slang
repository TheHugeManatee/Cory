module Msaa;

public float2 getMsaaSamplePosition(uint sampleCount, uint sampleIndex)
{
    if (sampleCount <= 1)
        return float2(0.5f, 0.5f);
    switch (sampleCount) {
    case 2:
        return sampleIndex == 0 ? float2(0.25f, 0.25f) : float2(0.75f, 0.75f);
    case 4:
        switch (sampleIndex) {
        case 0:
            return float2(0.375f, 0.125f);
        case 1:
            return float2(0.875f, 0.375f);
        case 2:
            return float2(0.125f, 0.625f);
        default:
            return float2(0.625f, 0.875f);
        }
    case 8:
        switch (sampleIndex) {
        case 0:
            return float2(0.5625f, 0.3125f);
        case 1:
            return float2(0.4375f, 0.6875f);
        case 2:
            return float2(0.8125f, 0.5625f);
        case 3:
            return float2(0.3125f, 0.1875f);
        case 4:
            return float2(0.1875f, 0.8125f);
        case 5:
            return float2(0.0625f, 0.4375f);
        case 6:
            return float2(0.6875f, 0.9375f);
        default:
            return float2(0.9375f, 0.0625f);
        }
    case 16:
        switch (sampleIndex) {
        case 0:
            return float2(0.5625f, 0.5625f);
        case 1:
            return float2(0.4375f, 0.8125f);
        case 2:
            return float2(0.3125f, 0.3125f);
        case 3:
            return float2(0.6875f, 0.0625f);
        case 4:
            return float2(0.1875f, 0.6875f);
        case 5:
            return float2(0.8125f, 0.1875f);
        case 6:
            return float2(0.0625f, 0.4375f);
        case 7:
            return float2(0.9375f, 0.9375f);
        case 8:
            return float2(0.3125f, 0.8125f);
        case 9:
            return float2(0.6875f, 0.5625f);
        case 10:
            return float2(0.4375f, 0.0625f);
        case 11:
            return float2(0.8125f, 0.9375f);
        case 12:
            return float2(0.5625f, 0.3125f);
        case 13:
            return float2(0.4375f, 0.6875f);
        case 14:
            return float2(0.6875f, 0.4375f);
        default:
            return float2(0.3125f, 0.1875f);
        }
    default:
        return float2(0.5f, 0.5f);
    }
}
