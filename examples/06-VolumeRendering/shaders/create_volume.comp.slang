
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

    voxelPos /= 20.0f; // global scaling

    // Add animated deformations
    voxelPos += 0.15f * sin(globals->time + voxelPos.x * 2.0f) * float3(1, 0.5, 0.5);
    voxelPos += 0.1f * cos(globals->time * 0.7f + voxelPos.y * 5.0f) * float3(0.5, 1, 0.5);
    voxelPos += 0.12f * sin(globals->time * 0.5f + voxelPos.z * 2.5f) * float3(0.5, 0.5, 1);

    // Create a more complex density field with multiple features
    float distFromCenter = length(voxelPos) * 2.0f;
    
    // Primary density shell
    float mainShell = exp(-(distFromCenter - 1.5f) * (distFromCenter - 1.5f) * 2.0f);
    
    // Add internal turbulent features
    float turbulence = abs(sin(voxelPos.x * 5.0f + globals->time)) * 
                      abs(sin(voxelPos.y * 4.0f + globals->time * 0.8f)) *
                      abs(sin(voxelPos.z * 3.0f + globals->time * 0.6f));
    
    // Radial oscillations for dynamic movement
    float radialOscillation = 0.5f + 0.5f * sin(distFromCenter * 3.0f - globals->time);
    
    // Combine the features
    float density = (mainShell + turbulence * 0.2f) * radialOscillation;
    density = exp(-density * globals->densityScale);
    
    density = 1.0 - pow(density, 2.0f); // Enhance contrast
    
    volume[dispatchThreadId] = density;
}

