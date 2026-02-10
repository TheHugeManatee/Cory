#pragma once

#include <glm/vec3.hpp>

struct VolumeTransferFunction {
    float densityMin{0.08f};
    float densityMax{0.92f};
    float opacityScale{24.0f};
    float gamma{1.15f};
};

struct VolumeComponent {
    // size of the volume in world units
    glm::vec3 size{1.0f, 2.0f, 1.0f};
    // raymarch step size in units of voxels sampled per step
    float raymarchStepSizeMultiplier{2.0f};
    VolumeTransferFunction transferFunction{};
};
