#pragma once

#include "Common.hpp"

#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/ShaderHotReloader.hpp>
#include <Cory/SceneGraph/System.hpp>
#include <Cory/Systems/CommonComponents.hpp>

#include <KDGpu/buffer.h>
#include <KDGpu/sampler.h>

#include <cstdint>
#include <type_traits>
#include <vector>

struct alignas(16) InstanceData {
    glm::mat4 modelToWorld{1.0f};
    glm::mat4 worldToModel{1.0f};
    glm::mat4 normalToWorld{1.0f};
    glm::vec4 color{1.0f};
    glm::vec4 transferParams{0.0f};
    glm::vec4 raymarchParams{0.0f};
};

static_assert(std::is_trivially_copyable_v<InstanceData>);
static_assert(sizeof(InstanceData) == 3 * sizeof(glm::mat4) + 3 * sizeof(glm::vec4));

struct alignas(16) VolumeGenerationParams {
    glm::vec3 volumeSpacing{1.0f};
    float densityScale{1.0f};
    glm::uvec3 volumeDimensions{64u, 64u, 64u};
    float time{0.0f};
};
static_assert(std::is_trivially_copyable_v<VolumeGenerationParams>);

/**
 * @brief VolumeRenderSystem - renders volume components as raymarched cubes
 *
 *
 */
class VolumeRenderSystem
    : public Cory::BasicSystem<VolumeRenderSystem, VolumeComponent, Cory::Components::Transform> {
  public:
    explicit VolumeRenderSystem(Cory::Context &ctx);
    ~VolumeRenderSystem();

    void beforeUpdate(Cory::SceneGraph &sg, uint64_t frameNumber);

    void update(Cory::SceneGraph &sg,
                Cory::TickInfo tick,
                Cory::Entity entity,
                const VolumeComponent &volume,
                const Cory::Components::Transform &transform);

    struct PassOutputs {
        Cory::TransientTextureHandle colorOut;
        Cory::TransientTextureHandle depthOut;
    };
    Cory::RenderTaskDeclaration<PassOutputs>
    rasterizationTask(Cory::RenderTaskBuilder builder,
                      Cory::TransientTextureHandle colorTarget,
                      Cory::TransientTextureHandle depthTarget);

    Cory::RenderTaskDeclaration<Cory::TransientTextureHandle>
    cubeRaycastTask(Cory::RenderTaskBuilder builder,
                    Cory::TransientTextureHandle colorTarget,
                    Cory::TransientTextureHandle depthTarget,
                    Cory::TransientTextureHandle volumeTarget,
                    float temporalBlendFactor,
                    int32_t iterations,
                    float alphaDeltaRejectThreshold);

    Cory::RenderTaskDeclaration<Cory::TransientTextureHandle>
    cubeRaycastDebugTask(Cory::RenderTaskBuilder builder,
                         Cory::TransientTextureHandle colorTarget,
                         Cory::TransientTextureHandle depthTarget,
                         Cory::TransientTextureHandle volumeTarget);

    Cory::RenderTaskDeclaration<Cory::TransientTextureHandle>
    volumeGenerationTask(Cory::RenderTaskBuilder builder);

  private:
    std::vector<InstanceData> renderState_;
    Cory::Components::CameraComponent camera_;

    Cory::Mesh cube_;

    Cory::ShaderHandle vertexShader_;
    Cory::ShaderHandle fragmentShader_;
    Cory::ShaderHandle raycastShader_;
    Cory::ShaderHandle raycastDebugShader_;
    Cory::ShaderHandle createVolumeShader_;
    Cory::ShaderHotReloader shaderHotReloader_;

    Gpu::Sampler volumeSampler_;
    VolumeGenerationParams volumeParams_{.volumeSpacing = glm::vec3{1.0f},
                                         .densityScale = 1.0f,
                                         .volumeDimensions = glm::uvec3{128u, 128u, 128u},
                                         .time = 0.0f};
};
