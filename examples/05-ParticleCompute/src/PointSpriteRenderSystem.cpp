#include "PointSpriteRenderSystem.hpp"

#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/gpu_core.h>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <cstddef>

PointSpriteRenderSystem::PointSpriteRenderSystem(Cory::Context &ctx, uint32_t maxFramesInFlight)
    : Base()
    , ctx_(&ctx)
{
    globalUbo_ = std::make_unique<Cory::UniformBufferObject<CubeUBO>>(ctx, maxFramesInFlight);

    vertexShader_ =
        ctx.shaders().createShader(Cory::ResourceLocator::Locate("pointsprite.vert.slang"));
    fragmentShader_ =
        ctx.shaders().createShader(Cory::ResourceLocator::Locate("pointsprite.frag.slang"));
}

PointSpriteRenderSystem::~PointSpriteRenderSystem()
{
    if (ctx_) {
        auto &shaders = ctx_->shaders();
        shaders.release(vertexShader_);
        shaders.release(fragmentShader_);
    }
}

void PointSpriteRenderSystem::beforeUpdate(Cory::SceneGraph &sg)
{
    renderState_.clear();
    // update the camera's state
    forEach<Cory::Components::CameraComponent>(
        sg, [this](Cory::Entity e, auto &camera) { camera_ = camera; });
}

void PointSpriteRenderSystem::update(Cory::SceneGraph &sg,
                                     Cory::TickInfo tick,
                                     Cory::Entity entity,
                                     const PointSpriteComponent &spriteData)
{
    const glm::vec3 worldPosition = spriteData.position;
    const float spriteRadius = spriteData.radius;

    renderState_.push_back({
        .positionAndSize = glm::vec4{worldPosition, spriteRadius},
        .color = spriteData.color,
        .parameters = glm::vec4{1.0, 0.0f, 0.0f, 0.0f},
    });
}

Cory::RenderTaskDeclaration<PointSpriteRenderSystem::PassOutputs>
PointSpriteRenderSystem::spriteRenderTask(Cory::RenderTaskBuilder builder,
                                          Cory::TransientTextureHandle colorTarget,
                                          Cory::TransientTextureHandle depthTarget)
{
    Gpu::ColorClearValue clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    Gpu::DepthStencilClearValue clearDepthStencil = {1.0f, 0};

    auto [writtenColorHandle, colorInfo] =
        builder.write(colorTarget, Cory::Sync::AccessType::ColorAttachmentWrite);
    auto [writtenDepthHandle, depthInfo] =
        builder.write(depthTarget, Cory::Sync::AccessType::DepthStencilAttachmentWrite);

    auto cubePass = builder.declareRenderPass(Cory::RenderPassDeclaration{
        .name = "PASS_PointSprites",
        .options = Cory::PassOptionFlagBits::DisableMeshInput,
        .shaders = {vertexShader_, fragmentShader_},
        .attachments = {{
            {
                .target = colorTarget,
                .load = Gpu::AttachmentLoadOperation::Clear,
                .store = Gpu::AttachmentStoreOperation::Store,
                .clearColor = clearColor,
            },
        }},
        .depthAttachment =
            Cory::DepthStencilAttachment{
                .target = depthTarget,
                .load = Gpu::AttachmentLoadOperation::Clear,
                .store = Gpu::AttachmentStoreOperation::Store,
                .clearDepthStencil = clearDepthStencil,
            },
        .vertexOptions = Gpu::VertexOptions{},
    });

    co_yield PassOutputs{.colorOut = writtenColorHandle, .depthOut = writtenDepthHandle};

    /// ^^^^     DECLARATION      ^^^^
    Cory::RenderInput renderApi = co_await builder.finishDeclaration();
    /// vvvv  RENDERING COMMANDS  vvvv

    auto passRecorder = cubePass.begin(*renderApi.cmd);

    float aspect = static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y);
    glm::mat4 viewMatrix = camera_.viewMatrix;
    glm::mat4 projectionMatrix =
        Cory::makePerspective(camera_.fovy, aspect, camera_.nearPlane, camera_.farPlane);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;

    Cory::FrameContext &frameCtx = *renderApi.frameCtx;

    // update the uniform buffer
    CubeUBO &ubo = (*globalUbo_)[frameCtx.inFlightIndex];
    ubo.view = viewMatrix;
    ubo.projection = projectionMatrix;
    ubo.viewProjection = viewProjection;
    ubo.lightPosition = camera_.position;
    const glm::mat3 viewInverse = glm::mat3(glm::inverse(viewMatrix));
    ubo.cameraRight = glm::normalize(viewInverse[0]);
    ubo.cameraUp = glm::normalize(viewInverse[1]);
    // need explicit flush otherwise the mapped memory is not synced to the GPU
    globalUbo_->flush(frameCtx.inFlightIndex);

    auto &descriptorSets = ctx_->descriptors();
    descriptorSets.write(
        Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, *globalUbo_);

    const uint32_t instanceCount = static_cast<uint32_t>(renderState_.size());
    if (instanceCount > 0) {
        auto &instanceBuffer = instanceBufferForFrame(frameCtx.inFlightIndex, instanceCount);
        auto *mapped = static_cast<std::byte *>(instanceBuffer.buffer.map());
        std::memcpy(
            mapped, renderState_.data(), static_cast<size_t>(instanceCount) * sizeof(InstanceData));
        instanceBuffer.buffer.unmap();

        descriptorSets.write(
            Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, instanceBuffer.buffer);
    }

    descriptorSets.flushWrites().bind(passRecorder, frameCtx.inFlightIndex);

    // Set dynamic states
    passRecorder.setCullMode(Gpu::CullModeFlagBits::None);
    passRecorder.setDepthTestEnabled(true);
    passRecorder.setDepthWriteEnabled(true);
    passRecorder.setDepthCompareOp(Gpu::CompareOperation::Less);
    passRecorder.setColorBlendEnabled(0, {true});
    static constexpr Gpu::ColorBlendEquation alphaBlend{
        .srcColorBlendFactor = Gpu::BlendFactor::SrcAlpha,
        .dstColorBlendFactor = Gpu::BlendFactor::OneMinusSrcAlpha,
        .colorBlendOp = Gpu::BlendOperation::Add,
        .srcAlphaBlendFactor = Gpu::BlendFactor::One,
        .dstAlphaBlendFactor = Gpu::BlendFactor::Zero,
        .alphaBlendOp = Gpu::BlendOperation::Add,
    };
    passRecorder.setColorBlendEquations(0, {alphaBlend});

    if (instanceCount > 0) {
        passRecorder.draw(Gpu::DrawCommand{
            .vertexCount = 6,
            .instanceCount = instanceCount,
            .firstVertex = 0,
            .firstInstance = 0,
        });
    }

    passRecorder.end();
}

InstanceBuffer &PointSpriteRenderSystem::instanceBufferForFrame(uint32_t frameIndex,
                                                                uint32_t instanceCount)
{
    if (instanceBuffers_.size() <= frameIndex) {
        instanceBuffers_.resize(frameIndex + 1);
    }

    const Gpu::DeviceSize requiredSize =
        static_cast<Gpu::DeviceSize>(instanceCount) * sizeof(InstanceData);
    auto &instanceBuffer = instanceBuffers_[frameIndex];
    if (!instanceBuffer.buffer.isValid() || instanceBuffer.capacity < requiredSize) {
        instanceBuffer.buffer = ctx_->device().createBuffer(Gpu::BufferOptions{
            .label = "SceneGraph Sprite Instance Buffer",
            .size = requiredSize,
            .usage = Gpu::BufferUsageFlagBits::StorageBufferBit,
            .memoryUsage = Gpu::MemoryUsage::CpuToGpu,
        });
        instanceBuffer.capacity = requiredSize;
    }

    return instanceBuffer;
}
