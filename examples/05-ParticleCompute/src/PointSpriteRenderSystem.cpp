#include "PointSpriteRenderSystem.hpp"

#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Base/Utils.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/Shader.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/gpu_core.h>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <cstddef>
#include <span>
#include <variant>

namespace {
InstanceBuffer &getInstanceBufferForFrame(std::vector<InstanceBuffer> &instanceBuffers,
                                          Cory::Context &ctx,
                                          uint32_t frameIndex,
                                          uint32_t instanceCount)
{
    if (instanceBuffers.size() <= frameIndex) {
        instanceBuffers.resize(frameIndex + 1);
    }

    const Gpu::DeviceSize requiredSize =
        static_cast<Gpu::DeviceSize>(instanceCount) * sizeof(InstanceData);
    auto &instanceBuffer = instanceBuffers[frameIndex];
    if (!instanceBuffer.buffer.isValid() || instanceBuffer.capacity < requiredSize) {
        instanceBuffer.buffer = ctx.device().createBuffer(Gpu::BufferOptions{
            .label = "SceneGraph Sprite Instance Buffer",
            .size = requiredSize,
            .usage = Gpu::BufferUsageFlagBits::StorageBufferBit,
            .memoryUsage = Gpu::MemoryUsage::CpuToGpu,
        });
        instanceBuffer.capacity = requiredSize;
    }

    return instanceBuffer;
}

Cory::RenderTaskDeclaration<Cory::TransientBufferHandle>
pointSpriteSortPreprocessTask(Cory::RenderTaskBuilder builder,
                              Cory::ShaderHandle predicateShader,
                              Cory::Components::CameraComponent camera,
                              std::span<const InstanceData> instanceData,
                              std::vector<InstanceBuffer> &instanceBuffers,
                              Cory::TransientBufferHandle sortKeys,
                              float aspectRatio,
                              uint32_t instanceCount)
{
    auto [writtenSortKeys, sortKeysInfo] =
        builder.write(sortKeys, Cory::Sync::AccessType::ComputeShaderWrite);
    (void)sortKeysInfo;
    auto predicatePass = builder.declareComputePass(Cory::ComputePassDeclaration{
        .name = "PASS_PointSpriteSortPreprocess",
        .shader = predicateShader,
    });

    Cory::RenderInput renderApi = co_await builder.finishDeclaration(writtenSortKeys);

    glm::mat4 viewMatrix = camera.viewMatrix;
    glm::mat4 projectionMatrix =
        Cory::makePerspective(camera.fovy, aspectRatio, camera.nearPlane, camera.farPlane);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;

    auto globals = renderApi.bindingContext->alloc<PointSpriteGlobals>();
    globals->view = viewMatrix;
    globals->projection = projectionMatrix;
    globals->viewProjection = viewProjection;
    globals->lightPosition = camera.position;
    const glm::mat3 viewInverse = glm::mat3(glm::inverse(viewMatrix));
    globals->cameraRight = glm::normalize(viewInverse[0]);
    globals->cameraUp = glm::normalize(viewInverse[1]);
    globals->instanceBufferIndex = 0;
    globals->sortedIndicesBufferIndex = 0;
    globals->sortKeysBufferIndex = 0;

    auto &instanceBuffer = getInstanceBufferForFrame(
        instanceBuffers, *renderApi.ctx, renderApi.frameCtx->inFlightIndex, instanceCount);
    auto *mapped = static_cast<std::byte *>(instanceBuffer.buffer.map());
    std::memcpy(
        mapped, instanceData.data(), static_cast<size_t>(instanceCount) * sizeof(InstanceData));
    instanceBuffer.buffer.unmap();

    globals->instanceBufferIndex = renderApi.bindingContext->bindBuffer(
        instanceBuffer.buffer.handle(), Cory::BufferBindPoint::StorageBufferReadOnly);
    globals->sortKeysBufferIndex = renderApi.bindingContext->bindBuffer(
        writtenSortKeys, Cory::BufferBindPoint::StorageBufferReadWrite);

    auto pass = predicatePass.begin(renderApi);
    pass.bindShader(renderApi.ctx->shaders()[predicateShader].shaderHandle());

    struct SortPreprocessPushConstants {
        uint32_t numInstances;
        uint32_t pad;
        Cory::BufferDeviceAddress globals;
    } pc{instanceCount, 0u, globals.gpu};

    renderApi.bindingContext->flush();
    renderApi.bindingContext->push(pc);
    pass.dispatchCompute({Cory::divideRoundUp(instanceCount, 256u), 1, 1});
    predicatePass.end(std::move(pass));
}
} // namespace

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
    using namespace Cory;
    Gpu::ColorClearValue clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    Gpu::DepthStencilClearValue clearDepthStencil = {1.0f, 0};

    auto [writtenColorHandle, colorInfo] =
        builder.write(colorTarget, Sync::AccessType::ColorAttachmentWrite);
    auto [writtenDepthHandle, depthInfo] =
        builder.write(depthTarget, Sync::AccessType::DepthStencilAttachmentWrite);

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

    TransientBufferHandle sortKeys;
    RadixSorter::SortOutput sortOutput{};
    if (instanceCount > 0) {
        sortKeys = builder.create("BUF_PointSpriteSortKeys",
                                  static_cast<Gpu::DeviceSize>(instanceCount) * sizeof(uint32_t),
                                  Gpu::BufferUsageFlagBits::StorageBufferBit,
                                  Sync::AccessType::ComputeShaderWrite);
    }

    auto spritePass = builder.declareRenderPass(RenderPassDeclaration{
        .name = "PASS_PointSprites",
        .options = PassOptionFlagBits::DisableMeshInput,
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
            DepthStencilAttachment{
                .target = depthTarget,
                .load = Gpu::AttachmentLoadOperation::Clear,
                .store = Gpu::AttachmentStoreOperation::Store,
                .clearDepthStencil = clearDepthStencil,
            },
        .dynamicStates = {.cullMode = CullMode::None,
                          .depthTest = DepthTest::Less,
                          .depthWrite = DepthWrite::Disabled},
    });
    if (instanceCount > 0) {
        auto predicateTask = pointSpriteSortPreprocessTask(
            builder.subtask("PointSpriteSortPreprocess"),
            predicateShader_,
            camera_,
            std::span<const InstanceData>{renderState_.data(), renderState_.size()},
            instanceBuffers_,
            sortKeys,
            static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y),
            instanceCount);

        sortOutput = sorter_.sort(builder, predicateTask.output(), instanceCount);
        builder.read(sortOutput.indices, Sync::AccessType::VertexShaderReadOther);
    }

    /// ^^^^     DECLARATION      ^^^^
    RenderInput renderApi = co_await builder.finishDeclaration(PassOutputs{
        .colorOut = writtenColorHandle,
        .depthOut = writtenDepthHandle,
    });
    /// vvvv  RENDERING COMMANDS  vvvv

    float aspect = static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y);
    glm::mat4 viewMatrix = camera_.viewMatrix;
    glm::mat4 projectionMatrix =
        makePerspective(camera_.fovy, aspect, camera_.nearPlane, camera_.farPlane);
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
    globals->instanceBufferIndex = 0;
    globals->sortedIndicesBufferIndex = 0;
    globals->sortKeysBufferIndex = 0;

    if (instanceCount > 0) {
        auto &instanceBuffer =
            instanceBufferForFrame(renderApi.frameCtx->inFlightIndex, instanceCount);
        globals->instanceBufferIndex = renderApi.bindingContext->bindBuffer(
            instanceBuffer.buffer.handle(), BufferBindPoint::StorageBufferReadOnly);
        globals->sortedIndicesBufferIndex = renderApi.bindingContext->bindBuffer(
            sortOutput.indices, BufferBindPoint::StorageBufferReadWrite);
    }

    auto passRecorder = spritePass.begin(renderApi);

    // instance data already uploaded before sorting

    renderApi.bindingContext->flush();
    renderApi.bindingContext->push(globals.gpu);

    if (instanceCount > 0) {
        passRecorder.draw(Gpu::DrawCommand{
            .vertexCount = 6,
            .instanceCount = instanceCount,
            .firstVertex = 0,
            .firstInstance = 0,
        });
    }

    spritePass.end(std::move(passRecorder));
}

auto PointSpriteRenderSystem::instanceBufferForFrame(uint32_t frameIndex, uint32_t instanceCount)
    -> InstanceBuffer &
{
    return getInstanceBufferForFrame(instanceBuffers_, *ctx_, frameIndex, instanceCount);
}
