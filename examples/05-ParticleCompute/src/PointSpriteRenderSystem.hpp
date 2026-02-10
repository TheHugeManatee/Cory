#pragma once

#include "Common.hpp"

#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/RadixSorter.hpp>
#include <Cory/SceneGraph/System.hpp>
#include <Cory/Systems/CommonComponents.hpp>

#include <KDGpu/buffer.h>
#include <type_traits>
#include <vector>

struct PointSpriteGlobals {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 viewProjection;
    glm::vec3 lightPosition;
    float padding0;
    glm::vec3 cameraRight;
    float padding1;
    glm::vec3 cameraUp;
    float padding2;
    Cory::BufferDeviceAddress sortKeys;
    Cory::BufferDeviceAddress sortIndices;
    Cory::BufferDeviceAddress instances;
};

struct alignas(16) InstanceData {
    glm::vec4 positionAndSize{0.0f}; // world position.xyz + half-size in w
    glm::vec4 color{1.0f};
    glm::vec4 parameters{0.0f};
};

static_assert(std::is_trivially_copyable_v<InstanceData>);

struct InstanceBuffer {
    Gpu::Buffer buffer;
    Gpu::DeviceSize capacity{0};
};

class PointSpriteRenderSystem
    : public Cory::BasicSystem<PointSpriteRenderSystem, PointSpriteComponent> {
  public:
    explicit PointSpriteRenderSystem(Cory::Context &ctx);
    ~PointSpriteRenderSystem();

    void beforeUpdate(Cory::SceneGraph &sg, uint64_t frameNumber);

    void update(Cory::SceneGraph &sg,
                Cory::TickInfo tick,
                Cory::Entity entity,
                const PointSpriteComponent &spriteData);

    struct PassOutputs {
        Cory::TransientTextureHandle colorOut;
        Cory::TransientTextureHandle depthOut;
    };
    Cory::RenderTaskDeclaration<PassOutputs>
    spriteRenderTask(Cory::RenderTaskBuilder builder,
                     Cory::TransientTextureHandle colorTarget,
                     Cory::TransientTextureHandle depthTarget);

  private:
    InstanceBuffer &instanceBufferForFrame(uint32_t frameIndex, uint32_t instanceCount);

    std::vector<InstanceData> renderState_;
    std::vector<InstanceBuffer> instanceBuffers_;
    Cory::Components::CameraComponent camera_;

    Cory::Context *ctx_{nullptr};
    Cory::ShaderHandle vertexShader_;
    Cory::ShaderHandle fragmentShader_;
    Cory::ShaderHandle predicateShader_;
    KDGpu::PipelineLayoutHandle predicateLayout_;

    Cory::RadixSorter sorter_;
};
