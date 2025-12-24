#include "PointSpriteRenderSystem.hpp"

#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>

#include <KDGpu/bind_group.h>
#include <KDGpu/bind_group_options.h>
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
    computeShader_ =
        ctx.shaders().createShader(Cory::ResourceLocator::Locate("pointsprite.comp.slang"));
    sortPreprocessShader_ =
        ctx.shaders().createShader(Cory::ResourceLocator::Locate("sort_preprocess.comp.slang"));
    sortHistogramShader_ =
        ctx.shaders().createShader(Cory::ResourceLocator::Locate("radix_sort_histogram.comp.slang"));
    sortScanShader_ =
        ctx.shaders().createShader(Cory::ResourceLocator::Locate("radix_sort_scan.comp.slang"));
    sortScatterShader_ =
        ctx.shaders().createShader(Cory::ResourceLocator::Locate("radix_sort_scatter.comp.slang"));
}

PointSpriteRenderSystem::~PointSpriteRenderSystem()
{
    if (ctx_) {
        auto &shaders = ctx_->shaders();
        shaders.release(vertexShader_);
        shaders.release(fragmentShader_);
        shaders.release(computeShader_);
        shaders.release(sortPreprocessShader_);
        shaders.release(sortHistogramShader_);
        shaders.release(sortScanShader_);
        shaders.release(sortScatterShader_);
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
    const uint32_t numWorkgroups = (instanceCount + 255) / 256;

    auto sortPreprocessPass = builder.declareComputePass(Cory::ComputePassDeclaration{
        .name = "PASS_SortPreprocess",
        .shader = sortPreprocessShader_,
        .pushConstantRanges = {{.offset = 0, .size = sizeof(uint32_t), .shaderStages = KDGpu::ShaderStageFlagBits::ComputeBit}},
    });
    auto sortHistogramPass = builder.declareComputePass(Cory::ComputePassDeclaration{
        .name = "PASS_SortHistogram",
        .shader = sortHistogramShader_,
        .pushConstantRanges = {{.offset = 0, .size = 8, .shaderStages = KDGpu::ShaderStageFlagBits::ComputeBit}},
    });
    auto sortScanPass = builder.declareComputePass(Cory::ComputePassDeclaration{
        .name = "PASS_SortScan",
        .shader = sortScanShader_,
        .pushConstantRanges = {{.offset = 0, .size = sizeof(uint32_t), .shaderStages = KDGpu::ShaderStageFlagBits::ComputeBit}},
    });
    auto sortScatterPass = builder.declareComputePass(Cory::ComputePassDeclaration{
        .name = "PASS_SortScatter",
        .shader = sortScatterShader_,
        .pushConstantRanges = {{.offset = 0, .size = 8, .shaderStages = KDGpu::ShaderStageFlagBits::ComputeBit}},
    });

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
    CubeUBO &uboVal = (*globalUbo_)[frameCtx.inFlightIndex];
    uboVal.view = viewMatrix;
    uboVal.projection = projectionMatrix;
    uboVal.viewProjection = viewProjection;
    uboVal.lightPosition = camera_.position;
    const glm::mat3 viewInverse = glm::mat3(glm::inverse(viewMatrix));
    uboVal.cameraRight = glm::normalize(viewInverse[0]);
    uboVal.cameraUp = glm::normalize(viewInverse[1]);
    globalUbo_->flush(frameCtx.inFlightIndex);

    if (instanceCount > 0) {
        auto &sortBuf = sortBuffersForFrame(frameCtx.inFlightIndex, instanceCount);
        auto &instanceBuffer = instanceBufferForFrame(frameCtx.inFlightIndex, instanceCount);

        auto *mapped = static_cast<std::byte *>(instanceBuffer.buffer.map());
        std::memcpy(
            mapped, renderState_.data(), static_cast<size_t>(instanceCount) * sizeof(InstanceData));
        instanceBuffer.buffer.unmap();
        auto bufferBarrier = [cmd = renderApi.cmd](const KDGpu::Buffer &buffer,
                                                   Gpu::AccessFlags srcMask,
                                                   Gpu::AccessFlags dstMask,
                                                   Gpu::PipelineStageFlags dstStages) {
            cmd->bufferMemoryBarrier(Gpu::BufferMemoryBarrierOptions{
                .srcStages = Gpu::PipelineStageFlagBit::ComputeShaderBit,
                .srcMask = srcMask,
                .dstStages = dstStages,
                .dstMask = dstMask,
                .buffer = buffer.handle(),
            });
        };
        const auto shaderWrite = Gpu::AccessFlagBit::ShaderStorageWriteBit;
        const auto shaderRead = Gpu::AccessFlagBit::ShaderStorageReadBit;
        const auto shaderReadWrite =
            Gpu::AccessFlags(Gpu::AccessFlagBit::ShaderStorageReadBit |
                             Gpu::AccessFlagBit::ShaderStorageWriteBit);

        // Preprocess: generate keys and initial indices
        {
            auto rec = sortPreprocessPass.begin(*renderApi.cmd);
            auto layout = sortPreprocessPass.pipelineLayoutHandle();
            
            ctx_->descriptors()
                .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, *globalUbo_)
                .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, 2, instanceBuffer.buffer)
                .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, 3, sortBuf.keys)
                .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, 4, sortBuf.indices)
                .flushWrites()
                .bind(rec, frameCtx.inFlightIndex);

            KDGpu::PushConstantRange pr{.offset = 0, .size = sizeof(uint32_t), .shaderStages = KDGpu::ShaderStageFlagBits::ComputeBit};
            rec.pushConstant(pr, &instanceCount, layout);
            rec.dispatchCompute(KDGpu::ComputeCommand{numWorkgroups, 1, 1});
            rec.end();
        }
        bufferBarrier(
            sortBuf.keys, shaderWrite, shaderRead, Gpu::PipelineStageFlagBit::ComputeShaderBit);
        bufferBarrier(sortBuf.indices,
                      shaderWrite,
                      shaderRead,
                      Gpu::PipelineStageFlagBit::ComputeShaderBit);

        // Radix sort passes
        KDGpu::Buffer *keysIn = &sortBuf.keys;
        KDGpu::Buffer *indicesIn = &sortBuf.indices;
        KDGpu::Buffer *keysOut = &sortBuf.keysPingPong;
        KDGpu::Buffer *indicesOut = &sortBuf.indicesPingPong;

        for (uint32_t bitOffset = 0; bitOffset < 32; bitOffset += 4) {
            // Histogram
            {
                auto rec = sortHistogramPass.begin(*renderApi.cmd);
                auto layout = sortHistogramPass.pipelineLayoutHandle();

                ctx_->descriptors()
                    .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, 2, *keysIn)
                    .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, 3, sortBuf.histograms)
                    .flushWrites()
                    .bind(rec, frameCtx.inFlightIndex);

                struct {
                    uint32_t numInstances;
                    uint32_t bitOffset;
                } pc{instanceCount, bitOffset};
                KDGpu::PushConstantRange pr{.offset = 0, .size = sizeof(pc), .shaderStages = KDGpu::ShaderStageFlagBits::ComputeBit};
                rec.pushConstant(pr, &pc, layout);
                rec.dispatchCompute(KDGpu::ComputeCommand{numWorkgroups, 1, 1});
                rec.end();
            }
            bufferBarrier(sortBuf.histograms,
                          shaderWrite,
                          shaderReadWrite,
                          Gpu::PipelineStageFlagBit::ComputeShaderBit);

            // Scan
            {
                auto rec = sortScanPass.begin(*renderApi.cmd);
                auto layout = sortScanPass.pipelineLayoutHandle();

                ctx_->descriptors()
                    .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, 2, sortBuf.histograms)
                    .flushWrites()
                    .bind(rec, frameCtx.inFlightIndex);

                KDGpu::PushConstantRange pr{.offset = 0, .size = sizeof(uint32_t), .shaderStages = KDGpu::ShaderStageFlagBits::ComputeBit};
                rec.pushConstant(pr, &numWorkgroups, layout);
                rec.dispatchCompute(KDGpu::ComputeCommand{1, 1, 1});
                rec.end();
            }
            bufferBarrier(sortBuf.histograms,
                          shaderWrite,
                          shaderRead,
                          Gpu::PipelineStageFlagBit::ComputeShaderBit);

            // Scatter
            {
                auto rec = sortScatterPass.begin(*renderApi.cmd);
                auto layout = sortScatterPass.pipelineLayoutHandle();

                ctx_->descriptors()
                    .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, 2, *keysIn)
                    .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, 3, *indicesIn)
                    .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, 4, *keysOut)
                    .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, 5, *indicesOut)
                    .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, 6, sortBuf.histograms)
                    .flushWrites()
                    .bind(rec, frameCtx.inFlightIndex);

                struct {
                    uint32_t numInstances;
                    uint32_t bitOffset;
                } pc{instanceCount, bitOffset};
                KDGpu::PushConstantRange pr{.offset = 0, .size = sizeof(pc), .shaderStages = KDGpu::ShaderStageFlagBits::ComputeBit};
                rec.pushConstant(pr, &pc, layout);
                rec.dispatchCompute(KDGpu::ComputeCommand{numWorkgroups, 1, 1});
                rec.end();
            }
            bufferBarrier(*keysOut,
                          shaderWrite,
                          shaderRead,
                          Gpu::PipelineStageFlagBit::ComputeShaderBit);
            bufferBarrier(*indicesOut,
                          shaderWrite,
                          shaderRead,
                          Gpu::PipelineStageFlagBit::ComputeShaderBit);

            // Swap ping-pong
            if (bitOffset + 4 < 32) {
                std::swap(keysIn, keysOut);
                std::swap(indicesIn, indicesOut);
            }
        }
        // Final scatter wrote into keysOut/indicesOut on the last iteration; use those
        keysIn = keysOut;
        indicesIn = indicesOut;
        
        // Final bind: vertex shader needs the sorted indices
        bufferBarrier(*indicesIn,
                      shaderWrite,
                      shaderRead,
                      Gpu::PipelineStageFlagBit::VertexShaderBit);

        auto &descriptorSets = ctx_->descriptors();
        descriptorSets
            .write(Cory::DescriptorSets::SetType::Static,
                   frameCtx.inFlightIndex,
                   *globalUbo_)
            .write(Cory::DescriptorSets::SetType::Static,
                   frameCtx.inFlightIndex,
                   2,
                   instanceBuffer.buffer)
            .write(Cory::DescriptorSets::SetType::Static,
                   frameCtx.inFlightIndex,
                   4,
                   *indicesIn)
            .flushWrites();
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

auto PointSpriteRenderSystem::sortBuffersForFrame(uint32_t frameIndex, uint32_t instanceCount) -> SortBuffers &
{
    if (sortBuffers_.size() <= frameIndex) {
        sortBuffers_.resize(frameIndex + 1);
    }
    auto &buf = sortBuffers_[frameIndex];
    const Gpu::DeviceSize requiredSize = static_cast<Gpu::DeviceSize>(instanceCount) * sizeof(uint32_t);
    if (buf.capacity < requiredSize) {
        buf.capacity = requiredSize;
        uint32_t numWorkgroups = (instanceCount + 255) / 256;

        KDGpu::BufferOptions opt{
            .size = requiredSize,
            .usage = KDGpu::BufferUsageFlagBits::StorageBufferBit,
            .memoryUsage = KDGpu::MemoryUsage::GpuOnly
        };
        opt.label = "SortKeys";
        buf.keys = ctx_->device().createBuffer(opt);
        opt.label = "SortIndices";
        buf.indices = ctx_->device().createBuffer(opt);
        opt.label = "SortKeysPingPong";
        buf.keysPingPong = ctx_->device().createBuffer(opt);
        opt.label = "SortIndicesPingPong";
        buf.indicesPingPong = ctx_->device().createBuffer(opt);

        opt.label = "SortHistograms";
        opt.size = numWorkgroups * 16 * sizeof(uint32_t);
        buf.histograms = ctx_->device().createBuffer(opt);
    }
    return buf;
}
