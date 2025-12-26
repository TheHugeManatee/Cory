#include "PointSpriteRenderSystem.hpp"

#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
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
    , sorter_{ctx}
{
    globalUbo_ =
        std::make_unique<Cory::UniformBufferObject<PointSpriteGlobals>>(ctx, maxFramesInFlight);

    vertexShader_ =
        ctx.shaders().createShader(Cory::ResourceLocator::Locate("pointsprite.vert.slang"));
    fragmentShader_ =
        ctx.shaders().createShader(Cory::ResourceLocator::Locate("pointsprite.frag.slang"));
    {
        Cory::ShaderSource predicateSource{
            Cory::ResourceLocator::Locate("sort_preprocess.comp.slang")};
        predicateShader_ = ctx.shaders().createShader(
            std::move(predicateSource),
            {Gpu::PushConstantRange{.offset = 0,
                                    .size = sizeof(uint32_t) * 2,
                                    .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit}});
    }

    predicateLayout_ = ctx.pipelineCache().queryLayout(Gpu::PipelineLayoutOptions{
        .label = "PointSpritePredicateLayout",
        .bindGroupLayouts = ctx.descriptors().layouts(),
        .pushConstantRanges = {Gpu::PushConstantRange{
            .offset = 0,
            .size = sizeof(uint32_t) * 2,
            .shaderStages = Gpu::ShaderStageFlagBits::ComputeBit,
        }},
    });
    predicatePipeline_ = ctx.pipelineCache().queryComputePipeline(
        "PointSpritePredicate",
        Cory::ComputePipelineDescriptor{.shader = predicateShader_,
                                        .pipelineLayout = predicateLayout_});
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
        .vertexOptions = Gpu::VertexOptions{},
    });

    co_yield PassOutputs{.colorOut = writtenColorHandle, .depthOut = writtenDepthHandle};

    /// ^^^^     DECLARATION      ^^^^
    Cory::RenderInput renderApi = co_await builder.finishDeclaration();
    /// vvvv  RENDERING COMMANDS  vvvv

    Cory::FrameContext &frameCtx = *renderApi.frameCtx;

    float aspect = static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y);
    glm::mat4 viewMatrix = camera_.viewMatrix;
    glm::mat4 projectionMatrix =
        Cory::makePerspective(camera_.fovy, aspect, camera_.nearPlane, camera_.farPlane);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;

    // update the uniform buffer early so compute passes see the latest camera
    PointSpriteGlobals &uboVal = (*globalUbo_)[frameCtx.inFlightIndex];
    uboVal.view = viewMatrix;
    uboVal.projection = projectionMatrix;
    uboVal.viewProjection = viewProjection;
    uboVal.lightPosition = camera_.position;
    const glm::mat3 viewInverse = glm::mat3(glm::inverse(viewMatrix));
    uboVal.cameraRight = glm::normalize(viewInverse[0]);
    uboVal.cameraUp = glm::normalize(viewInverse[1]);
    globalUbo_->flush(frameCtx.inFlightIndex);

    if (instanceCount > 0) {
        auto &sortBuf = sorter_.scratchForFrame(frameCtx.inFlightIndex, instanceCount);
        auto &instanceBuffer = instanceBufferForFrame(frameCtx.inFlightIndex, instanceCount);

        auto *mapped = static_cast<std::byte *>(instanceBuffer.buffer.map());
        std::memcpy(
            mapped, renderState_.data(), static_cast<size_t>(instanceCount) * sizeof(InstanceData));
        instanceBuffer.buffer.unmap();

        {
            auto pass = renderApi.cmd->beginComputePass({});
            pass.setPipeline(predicatePipeline_);
            ctx_->descriptors()
                .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, *globalUbo_)
                .write(Cory::DescriptorSets::SetType::Static,
                       frameCtx.inFlightIndex,
                       2,
                       instanceBuffer.buffer)
                .write(Cory::DescriptorSets::SetType::Static,
                       frameCtx.inFlightIndex,
                       3,
                       sortBuf.keysA)
                .flushWrites()
                .bind(pass, frameCtx.inFlightIndex);
            struct {
                uint32_t numInstances;
                uint32_t pad;
            } pc{instanceCount, 0u};
            pass.pushConstant(Gpu::PushConstantRange{.offset = 0,
                                                     .size = sizeof(pc),
                                                     .shaderStages =
                                                         Gpu::ShaderStageFlagBits::ComputeBit},
                              &pc);
            pass.dispatchCompute({sortBuf.workgroups, 1, 1});
            pass.end();
        }

        auto &sortedIndices = sorter_.sort(*renderApi.cmd,
                                           ctx_->descriptors(),
                                           sortBuf,
                                           sortBuf.keysA,
                                           instanceCount,
                                           sortBuf.indicesA,
                                           frameCtx.inFlightIndex);

        ctx_->descriptors()
            .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, *globalUbo_)
            .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, 2, instanceBuffer.buffer)
            .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, 4, sortedIndices)
            .flushWrites();

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

    // Set dynamic states
    passRecorder.setCullMode(Gpu::CullModeFlagBits::None);
    passRecorder.setDepthTestEnabled(true);
    passRecorder.setDepthWriteEnabled(false);
    passRecorder.setDepthCompareOp(Gpu::CompareOperation::Less);

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

auto PointSpriteRenderSystem::instanceBufferForFrame(uint32_t frameIndex, uint32_t instanceCount) -> InstanceBuffer &
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
