#pragma once

#include <glm/vec3.hpp>

#include <string>

struct VolumeTransferFunction {
    float densityMin{0.08f};
    float densityMax{0.92f};
    float opacityScale{24.0f};
    float gamma{1.15f};
};

struct VolumeComponent {
    // size of the volume in world units
    glm::vec3 size{1.0f, 2.0f, 1.0f};
    // dataset id resolved from volume catalog/manifest (empty means procedural fallback)
    std::string datasetId{};
    // raymarch step size in units of voxels sampled per step
    float raymarchStepSizeMultiplier{2.0f};
    // toggles interval jittering for start/end raymarch bounds
    bool raymarchJitteringEnabled{true};
    VolumeTransferFunction transferFunction{};
};
