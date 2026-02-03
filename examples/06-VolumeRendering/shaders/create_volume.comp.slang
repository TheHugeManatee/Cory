
struct VolumeGenerationParams {
    float3 volumeSpacing;
    float densityScale;
    uint3 volumeDimensions;
    float time;
}

[[vk::push_constant]] VolumeGenerationParams *globals;

[[vk::binding(3, 1)]] RWTexture3D<float> volume;

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId: SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= globals->volumeDimensions.x ||
        dispatchThreadId.y >= globals->volumeDimensions.y ||
        dispatchThreadId.z >= globals->volumeDimensions.z) {
        return;
    }

    float3 voxelPos = float3(dispatchThreadId) * globals->volumeSpacing;
    float3 volumeSize = float3(globals->volumeDimensions) * globals->volumeSpacing;
    voxelPos -= volumeSize * 0.5f;

    // Deform the spherical density volume over time
    voxelPos += 0.1f * sin(globals->time + voxelPos.yzx * 3.0f);

    float density = exp(-length(voxelPos) * globals->densityScale);
    volume[dispatchThreadId] = density;
}

