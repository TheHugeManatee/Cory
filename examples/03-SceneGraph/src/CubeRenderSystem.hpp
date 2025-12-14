#pragma once

#include "Common.hpp"

#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>
#include <Cory/SceneGraph/System.hpp>
#include <Cory/Systems/CommonComponents.hpp>

struct CubeUBO {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 viewProjection;
    glm::vec3 lightPosition;
};

struct CubePushConstantState {
    glm::mat4 modelToWorld{1.0f};
    glm::vec4 color{1.0, 0.0, 0.0, 1.0};
    float blend;
};

struct CubeMesh {
    KDGpu::Buffer vertexBuffer;
    KDGpu::Buffer indexBuffer;
    uint32_t indexCount;
};

class CubeRenderSystem
    : public Cory::BasicSystem<CubeRenderSystem, AnimationComponent, Cory::Components::Transform> {
  public:
    explicit CubeRenderSystem(Cory::Context &ctx, uint32_t maxFramesInFlight);
    ~CubeRenderSystem();

    void beforeUpdate(Cory::SceneGraph &sg);

    void update(Cory::SceneGraph &sg,
                Cory::TickInfo tick,
                Cory::Entity entity,
                const AnimationComponent &anim,
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
    void recordCommands(KDGpu::RenderPassCommandRecorder &recorder);

    std::vector<CubePushConstantState> renderState_;
    Cory::Components::CameraComponent camera_;

    Cory::Context *ctx_{nullptr};
    std::unique_ptr<CubeMesh> mesh_;
    std::unique_ptr<Cory::UniformBufferObject<CubeUBO>> globalUbo_;
    Cory::ShaderHandle vertexShader_;
    Cory::ShaderHandle fragmentShader_;
};