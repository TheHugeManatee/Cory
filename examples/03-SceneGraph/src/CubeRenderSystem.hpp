#pragma once

#include "Common.hpp"

#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/SceneGraph/System.hpp>
#include <Cory/Systems/CommonComponents.hpp>

#include <KDgpu/buffer.h>

#include <cstdint>
#include <type_traits>
#include <vector>

struct CubeUBO {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 viewProjection;
    glm::vec3 lightPosition;
    Cory::BufferHeapIndex bufferIndex;
};

struct alignas(16) InstanceData {
    glm::mat4 modelToWorld{1.0f};
    glm::mat4 normalToWorld{1.0f};
    glm::vec4 color{1.0f};
    glm::vec4 parameters{0.0f};
};

static_assert(std::is_trivially_copyable_v<InstanceData>);
static_assert(sizeof(InstanceData) == 2 * sizeof(glm::mat4) + 2 * sizeof(glm::vec4));

struct CubeMesh {
    Gpu::Buffer vertexBuffer;
    Gpu::Buffer indexBuffer;
    uint32_t indexCount;
};

struct InstanceBuffer {
    Gpu::Buffer buffer;
    Gpu::DeviceSize capacity{0};
};

class CubeRenderSystem
    : public Cory::BasicSystem<CubeRenderSystem, AnimationComponent, Cory::Components::Transform> {
  public:
    explicit CubeRenderSystem(Cory::Context &ctx);
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
    InstanceBuffer &instanceBufferForFrame(uint32_t frameIndex, uint32_t instanceCount);

    std::vector<InstanceData> renderState_;
    std::vector<InstanceBuffer> instanceBuffers_;
    Cory::Components::CameraComponent camera_;

    Cory::Context *ctx_{nullptr};
    std::unique_ptr<CubeMesh> mesh_;
    Cory::ShaderHandle vertexShader_;
    Cory::ShaderHandle fragmentShader_;
};
