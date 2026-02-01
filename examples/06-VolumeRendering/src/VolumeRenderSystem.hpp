#pragma once

#include "Common.hpp"

#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/SceneGraph/System.hpp>
#include <Cory/Systems/CommonComponents.hpp>
#include <Cory/Application/DynamicGeometry.hpp>

#include <KDGpu/buffer.h>

#include <cstdint>
#include <type_traits>
#include <vector>

struct alignas(16) InstanceData {
    glm::mat4 modelToWorld{1.0f};
    glm::mat4 normalToWorld{1.0f};
    glm::vec4 color{1.0f};
    glm::vec4 parameters{0.0f};
};

static_assert(std::is_trivially_copyable_v<InstanceData>);
static_assert(sizeof(InstanceData) == 2 * sizeof(glm::mat4) + 2 * sizeof(glm::vec4));

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

    void beforeUpdate(Cory::SceneGraph &sg);

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
    cubeRenderTask(Cory::RenderTaskBuilder builder,
                   Cory::TransientTextureHandle colorTarget,
                   Cory::TransientTextureHandle depthTarget);

  private:
    std::vector<InstanceData> renderState_;
    Cory::Components::CameraComponent camera_;

    Cory::Context *ctx_{nullptr};
    Cory::Mesh cube_;
    Cory::ShaderHandle vertexShader_;
    Cory::ShaderHandle fragmentShader_;
};
