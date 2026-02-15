#pragma once

#include <Cory/Renderer/Common.hpp>

#include <glm/vec3.hpp>

#include <filesystem>
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
    // runtime-assigned texture data managed by VolumeManagerSystem
    Gpu::TextureViewHandle textureView{};
    glm::uvec3 textureDimensions{0u};
    bool hasTexture{false};
    bool fullQuality{false};
    // raymarch step size in units of voxels sampled per step
    float raymarchStepSizeMultiplier{2.0f};
    // toggles interval jittering for start/end raymarch bounds
    bool raymarchJitteringEnabled{true};
    VolumeTransferFunction transferFunction{};
};

struct StreamedVolume {
    std::string datasetId{};
    std::filesystem::path manifestPath{};
};

struct ProceduralVolume {
    glm::uvec3 dimensions{128u, 128u, 128u};
    float densityScale{1.0f};
    bool updateEveryFrame{true};
    float animationSpeed{1.0f};
    bool regenerate{false};
};
