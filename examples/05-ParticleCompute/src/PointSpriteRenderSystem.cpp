#include "PointSpriteRenderSystem.hpp"

#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/Shader.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/gpu_core.h>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <cstddef>

PointSpriteRenderSystem::PointSpriteRenderSystem(Cory::Context &ctx)
    : Base()
    , ctx_(&ctx)
    , sorter_{ctx}
{
    vertexShader_ =
        ctx.shaders().createShader(Cory::ResourceLocator::Locate("pointsprite.vert.slang"));
    fragmentShader_ =
        ctx.shaders().createShader(Cory::ResourceLocator::Locate("pointsprite.frag.slang"));
    {
        Cory::ShaderSource predicateSource{
            Cory::ResourceLocator::Locate("sort_preprocess.comp.slang")};
        predicateShader_ = ctx.shaders().createShader(std::move(predicateSource));
    }
}

PointSpriteRenderSystem::~PointSpriteRenderSystem()
{
    if (ctx_) {
        auto &shaders = ctx_->shaders();
        shaders.release(vertexShader_);
        shaders.release(fragmentShader_);
        shaders.release(predicateShader_);
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

    const uint32_t instanceCount = static_cast<uint32_t>(renderState_.size());

    static constexpr Gpu::ColorBlendEquation alphaBlend{
        .srcColorBlendFactor = Gpu::BlendFactor::SrcAlpha,
        .dstColorBlendFactor = Gpu::BlendFactor::OneMinusSrcAlpha,
        .colorBlendOp = Gpu::BlendOperation::Add,
        .srcAlphaBlendFactor = Gpu::BlendFactor::One,
        .dstAlphaBlendFactor = Gpu::BlendFactor::Zero,
        .alphaBlendOp = Gpu::BlendOperation::Add,
    };
    static constexpr Gpu::BlendOptions blendOpts{
        .blendingEnabled = true,
        .color = {.operation = alphaBlend.colorBlendOp,
                  .srcFactor = alphaBlend.srcColorBlendFactor,
                  .dstFactor = alphaBlend.dstColorBlendFactor},
        .alpha = {.operation = alphaBlend.alphaBlendOp,
                  .srcFactor = alphaBlend.srcAlphaBlendFactor,
                  .dstFactor = alphaBlend.dstAlphaBlendFactor}};

    auto spritePass = builder.declareRenderPass(Cory::RenderPassDeclaration{
        .name = "PASS_PointSprites",
        .options = Cory::PassOptionFlagBits::DisableMeshInput,
        .shaders = {vertexShader_, fragmentShader_},
        .attachments = {{
            {
                .target = colorTarget,
                .load = Gpu::AttachmentLoadOperation::Clear,
                .store = Gpu::AttachmentStoreOperation::Store,
                .clearColor = clearColor,
                .blend = blendOpts,
            },
        }},
        .depthAttachment =
            Cory::DepthStencilAttachment{
                .target = depthTarget,
                .load = Gpu::AttachmentLoadOperation::Clear,
                .store = Gpu::AttachmentStoreOperation::Store,
                .clearDepthStencil = clearDepthStencil,
            },
        .dynamicStates = {.cullMode = Cory::CullMode::None,
                          .depthTest = Cory::DepthTest::Less,
                          .depthWrite = Cory::DepthWrite::Disabled},
    });
    auto predicatePass = builder.declareComputePass(Cory::ComputePassDeclaration{
        .name = "PASS_PointSpriteSortPreprocess",
        .shader = predicateShader_,
    });
    auto sortPasses = sorter_.declarePasses(builder);

    /// ^^^^     DECLARATION      ^^^^
    Cory::RenderInput renderApi = co_await builder.finishDeclaration(PassOutputs{
        .colorOut = writtenColorHandle,
        .depthOut = writtenDepthHandle,
    });
    /// vvvv  RENDERING COMMANDS  vvvv

    Cory::FrameContext &frameCtx = *renderApi.frameCtx;

    float aspect = static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y);
    glm::mat4 viewMatrix = camera_.viewMatrix;
    glm::mat4 projectionMatrix =
        Cory::makePerspective(camera_.fovy, aspect, camera_.nearPlane, camera_.farPlane);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;

    // update the uniform buffer early so compute passes see the latest camera
    auto globals = renderApi.bindingContext->alloc<PointSpriteGlobals>();
    globals->view = viewMatrix;
    globals->projection = projectionMatrix;
    globals->viewProjection = viewProjection;
    globals->lightPosition = camera_.position;
    const glm::mat3 viewInverse = glm::mat3(glm::inverse(viewMatrix));
    globals->cameraRight = glm::normalize(viewInverse[0]);
    globals->cameraUp = glm::normalize(viewInverse[1]);

    if (instanceCount > 0) {
        auto &sortBuf = sorter_.scratchForFrame(frameCtx.inFlightIndex, instanceCount);
        auto &instanceBuffer = instanceBufferForFrame(frameCtx.inFlightIndex, instanceCount);
        constexpr Cory::BufferHeapIndex kInstanceBufferIndex = 0;
        constexpr Cory::BufferHeapIndex kSortedIndicesBufferIndex = 1;
        constexpr Cory::BufferHeapIndex kSortKeysBufferIndex = 0;

        auto *mapped = static_cast<std::byte *>(instanceBuffer.buffer.map());
        std::memcpy(
            mapped, renderState_.data(), static_cast<size_t>(instanceCount) * sizeof(InstanceData));
        instanceBuffer.buffer.unmap();

        {
            auto pass = predicatePass.begin(*renderApi.cmd);
            pass.bindShader(ctx_->shaders()[predicateShader_].shaderHandle());
            ctx_->descriptors()
                .write(Cory::BufferBindPoint::StorageBufferReadOnly,
                       frameCtx.inFlightIndex,
                       kInstanceBufferIndex,
                       instanceBuffer.buffer)
                .write(Cory::BufferBindPoint::StorageBufferReadWrite,
                       frameCtx.inFlightIndex,
                       kSortKeysBufferIndex,
                       sortBuf.keysA)
                .bind(pass, frameCtx.inFlightIndex);
            struct SortPreprocessPushConstants {
                uint32_t numInstances;
                uint32_t pad;
                Cory::BufferDeviceAddress globals;
            } pc{instanceCount, 0u, globals.gpu};
            pass.pushConstant(
                Gpu::PushConstantRange{.offset = 0,
                                       .size = sizeof(pc),
                                       .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit},
                &pc);
            pass.dispatchCompute({sortBuf.workgroups, 1, 1});
            pass.end();
        }

        auto &sortedIndices = sorter_.sort(*renderApi.cmd,
                                           ctx_->descriptors(),
                                           sortBuf,
                                           sortPasses,
                                           sortBuf.keysA,
                                           instanceCount,
                                           sortBuf.indicesA,
                                           frameCtx.inFlightIndex);

        ctx_->descriptors()
            .write(Cory::BufferBindPoint::StorageBufferReadOnly,
                   frameCtx.inFlightIndex,
                   kInstanceBufferIndex,
                   instanceBuffer.buffer)
            .write(Cory::BufferBindPoint::StorageBufferReadWrite,
                   frameCtx.inFlightIndex,
                   kSortedIndicesBufferIndex,
                   sortedIndices);

        renderApi.cmd->bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
            .srcStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
            .srcMask = Gpu::AccessFlagBit::ShaderStorageWriteBit,
            .dstStages = Gpu::PipelineStageFlagBit::VertexShaderBit |
                         Gpu::PipelineStageFlagBit::FragmentShaderBit,
            .dstMask = Gpu::AccessFlagBit::ShaderStorageReadBit,
            .buffer = sortedIndices.handle(),
        });
    }

    auto passRecorder = spritePass.begin(*renderApi.cmd);

    // instance data already uploaded before sorting

    auto &descriptorSets = ctx_->descriptors();
    descriptorSets.bind(passRecorder, frameCtx.inFlightIndex);
    const Cory::BufferDeviceAddress globalsAddress = globals.gpu;
    passRecorder.pushConstant(
        Gpu::PushConstantRange{
            .offset = 0,
            .size = sizeof(Cory::BufferDeviceAddress),
            .shaderStages = Gpu::ShaderStageFlagBits::All,
        },
        &globalsAddress,
        spritePass.pipelineLayoutHandle());

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

auto PointSpriteRenderSystem::instanceBufferForFrame(uint32_t frameIndex, uint32_t instanceCount)
    -> InstanceBuffer &
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
