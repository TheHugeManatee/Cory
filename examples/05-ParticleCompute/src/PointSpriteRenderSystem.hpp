#pragma once

#include "Common.hpp"

#include <Cory/Framegraph/Common.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>
#include <Cory/SceneGraph/System.hpp>
#include <Cory/Systems/CommonComponents.hpp>

#include <type_traits>
#include <vector>

struct CubeUBO {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 viewProjection;
    glm::vec3 lightPosition;
    float padding0;
    glm::vec3 cameraRight;
    float padding1;
    glm::vec3 cameraUp;
    float padding2;
};

struct alignas(16) InstanceData {
    glm::vec4 positionAndSize{0.0f}; // world position.xyz + half-size in w
    glm::vec4 color{1.0f};
    glm::vec4 parameters{0.0f};
};

static_assert(std::is_trivially_copyable_v<InstanceData>);

struct InstanceBuffer {
    KDGpu::Buffer buffer;
    KDGpu::DeviceSize capacity{0};
};

class PointSpriteRenderSystem
    : public Cory::BasicSystem<PointSpriteRenderSystem, PointSpriteComponent> {
  public:
    explicit PointSpriteRenderSystem(Cory::Context &ctx, uint32_t maxFramesInFlight);
    ~PointSpriteRenderSystem();

    void beforeUpdate(Cory::SceneGraph &sg);

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
    std::unique_ptr<Cory::UniformBufferObject<CubeUBO>> globalUbo_;
    Cory::ShaderHandle vertexShader_;
    Cory::ShaderHandle fragmentShader_;
    Cory::ShaderHandle computeShader_;

    Cory::ShaderHandle sortPreprocessShader_;
    Cory::ShaderHandle sortHistogramShader_;
    Cory::ShaderHandle sortScanShader_;
    Cory::ShaderHandle sortScatterShader_;

    struct SortBuffers {
        KDGpu::Buffer keys;
        KDGpu::Buffer indices;
        KDGpu::Buffer keysPingPong;
        KDGpu::Buffer indicesPingPong;
        KDGpu::Buffer histograms;
        KDGpu::DeviceSize capacity{0};
    };
    std::vector<SortBuffers> sortBuffers_;

    SortBuffers &sortBuffersForFrame(uint32_t frameIndex, uint32_t instanceCount);
};
