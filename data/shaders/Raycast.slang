module Raycast;

static const float kEpsilon = 1e-6;
static const float kInfinity = 1e20;

public bool intersectsBox(float3 origin, float3 direction, float3 kBoxMin, float3 kBoxMax)
{
    float tNear = -kInfinity;
    float tFar = kInfinity;

    for (int axis = 0; axis < 3; ++axis) {
        float originValue = origin[axis];
        float dirValue = direction[axis];
        float minBound = kBoxMin[axis];
        float maxBound = kBoxMax[axis];

        if (abs(dirValue) < kEpsilon) {
            if (originValue < minBound || originValue > maxBound) {
                return false;
            }
            continue;
        }

        float invDir = 1.0 / dirValue;
        float t0 = (minBound - originValue) * invDir;
        float t1 = (maxBound - originValue) * invDir;
        if (t0 > t1) {
            float tmp = t0;
            t0 = t1;
            t1 = tmp;
        }

        tNear = max(tNear, t0);
        tFar = min(tFar, t1);
        if (tNear > tFar) {
            return false;
        }
    }

    return tFar >= max(tNear, 0.0);
}