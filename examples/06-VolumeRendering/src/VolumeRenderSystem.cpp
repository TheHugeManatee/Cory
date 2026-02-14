#include "VolumeRenderSystem.hpp"

#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
#include <Cory/RenderTasks/StandardRenderTasks.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <Cory/Renderer/Synchronization.hpp>

#include <KDGpu/gpu_core.h>
#include <KDGpu/sampler_options.h>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

struct DrawData {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 viewProjection;
    glm::vec3 lightPosition;
    float padding0;
    Cory::BufferDeviceAddress instances;
};

struct RaycastGlobals {
    glm::mat4 invViewProjection;
    glm::vec4 cameraPosition;
    glm::uvec3 volumeDimensions;
    float time;
    uint32_t instanceCount;
    uint32_t volumeTextureIndex;
    uint32_t colorTargetIsMsaa;
    float temporalBlendFactor;
    uint32_t iterations;
    float alphaDeltaRejectThreshold;
    uint32_t padding0;
    Cory::BufferDeviceAddress instances;
};

VolumeRenderSystem::VolumeRenderSystem(Cory::Context &ctx)
    : Base()
    , ctx_{&ctx}
{
    // Create mesh using Cory::DynamicGeometry, as in 02-CubeDemo
    cube_ = Cory::DynamicGeometry::createCube(ctx);

    shaderHotReloader_.initialize(ctx);
    shaderHotReloader_.addShader({
        .path = Cory::ResourceLocator::Locate("cube.vert.slang"),
        .stage = Gpu::ShaderStageFlagBits::VertexBit,
        .label = "cube.vert.slang",
        .shaderHandle = &vertexShader_,
    });
    shaderHotReloader_.addShader({
        .path = Cory::ResourceLocator::Locate("cube.frag.slang"),
        .stage = Gpu::ShaderStageFlagBits::FragmentBit,
        .label = "cube.frag.slang",
        .shaderHandle = &fragmentShader_,
    });
    shaderHotReloader_.addShader({
        .path = Cory::ResourceLocator::Locate("raymarch.comp.slang"),
        .stage = Gpu::ShaderStageFlagBits::ComputeBit,
        .label = "raymarch.comp.slang",
        .shaderHandle = &raycastShader_,
    });
    shaderHotReloader_.addShader({
        .path = Cory::ResourceLocator::Locate("raycast_boxes_debug.comp.slang"),
        .stage = Gpu::ShaderStageFlagBits::ComputeBit,
        .label = "raycast_boxes_debug.comp.slang",
        .shaderHandle = &raycastDebugShader_,
    });
    shaderHotReloader_.addShader({
        .path = Cory::ResourceLocator::Locate("create_volume.comp.slang"),
        .stage = Gpu::ShaderStageFlagBits::ComputeBit,
        .label = "create_volume.comp.slang",
        .shaderHandle = &createVolumeShader_,
    });

    volumeSampler_ = ctx.device().createSampler(Gpu::SamplerOptions{
        .label = "VolumeRenderSystem volume sampler",
        .magFilter = Gpu::FilterMode::Linear,
        .minFilter = Gpu::FilterMode::Linear,
        .mipmapFilter = Gpu::MipmapFilterMode::Linear,
        .u = Gpu::AddressMode::ClampToEdge,
        .v = Gpu::AddressMode::ClampToEdge,
        .w = Gpu::AddressMode::ClampToEdge,
    });
}

VolumeRenderSystem::~VolumeRenderSystem() {}

void VolumeRenderSystem::resetTemporalHistory()
{
    temporalHistory_.resource = {};
    temporalHistory_.extent = {0u, 0u};
    temporalHistory_.format = {};
    temporalHistory_.sampleCount = Gpu::SampleCountFlagBits::Samples1Bit;
    temporalHistory_.valid = false;
    temporalHistory_.forceTemporalReset = true;
}

void VolumeRenderSystem::ensureTemporalHistoryTexture(const Cory::FrameContext &frameCtx)
{
    CO_CORE_ASSERT(ctx_ != nullptr, "Context is not set in VolumeRenderSystem");
    auto &resourceManager = ctx_->framegraphResources();

    const auto extent = frameCtx.extent;
    const auto format = frameCtx.colorFormat;
    const auto sampleCount = frameCtx.sampleCount;
    const bool needsRecreate =
        !temporalHistory_.resource.valid() || temporalHistory_.extent != extent ||
        temporalHistory_.format != format || temporalHistory_.sampleCount != sampleCount;
    if (!needsRecreate) {
        return;
    }

    auto historyTexture = resourceManager.declareTexture(Cory::TextureInfo{
        .name = "TEX_VolumeTemporalHistory",
        .size = glm::uvec3{extent, 1u},
        .format = format,
        .usage = Gpu::TextureUsageFlagBits::StorageBit |
                 Gpu::TextureUsageFlagBits::ColorAttachmentBit |
                 Gpu::TextureUsageFlagBits::TransferSrcBit |
                 Gpu::TextureUsageFlagBits::TransferDstBit,
        .sampleCount = sampleCount,
        .textureType = Gpu::TextureType::TextureType2D,
    });
    resourceManager.allocate(std::vector<Cory::FramegraphTextureHandle>{historyTexture});
    temporalHistory_.resource = historyTexture;
    temporalHistory_.extent = extent;
    temporalHistory_.format = format;
    temporalHistory_.sampleCount = sampleCount;
    temporalHistory_.valid = false;
    temporalHistory_.forceTemporalReset = true;
}

void VolumeRenderSystem::beforeUpdate(Cory::SceneGraph &sg, uint64_t frameNumber)
{
    shaderHotReloader_.processPendingReloads(frameNumber);
    renderState_.clear();
    // update the camera's state
    forEach<Cory::Components::CameraComponent>(
        sg, [this](Cory::Entity e, auto &camera) { camera_ = camera; });
}

void VolumeRenderSystem::update(Cory::SceneGraph &sg,
                                Cory::TickInfo tick,
                                Cory::Entity entity,
                                const VolumeComponent &volume,
                                const Cory::Components::Transform &transform)
{
    lastFrameDeltaSeconds_ = std::max(static_cast<float>(tick.delta.count()), 1e-6f);
    volumeParams_.time = static_cast<float>(tick.now.time_since_epoch().count());

    renderState_.push_back({
        .modelToWorld = transform.modelToWorld * glm::scale(volume.size),
        .worldToModel = inverse(transform.modelToWorld * glm::scale(volume.size)),
        .normalToWorld = transpose(inverse(transform.modelToWorld)),
        .color = Cory::Color{1.0, 0.0, 0.0, 1.0},
        .transferParams = glm::vec4{std::clamp(volume.transferFunction.densityMin, 0.0f, 1.0f),
                                    std::clamp(volume.transferFunction.densityMax,
                                               volume.transferFunction.densityMin + 0.001f,
                                               1.0f),
                                    std::max(volume.transferFunction.opacityScale, 0.01f),
                                    std::max(volume.transferFunction.gamma, 0.01f)},
        .raymarchParams = glm::vec4{std::max(volume.raymarchStepSizeMultiplier, 0.01f),
                                    volume.raymarchJitteringEnabled ? 1.0f : 0.0f,
                                    0.0f,
                                    0.0f},
    });
}

Cory::RenderTaskDeclaration<VolumeRenderSystem::PassOutputs>
VolumeRenderSystem::rasterizationTask(Cory::RenderTaskBuilder builder,
                                      Cory::TransientTextureHandle colorTarget,
                                      Cory::TransientTextureHandle depthTarget)
{
    KDGpu::ColorClearValue clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    KDGpu::DepthStencilClearValue clearDepthStencil = {1.0f, 0};

    const auto &colorInfo = builder.textureInfo(colorTarget);
    auto cubePass = builder.declareRenderPass(Cory::RenderPassDeclaration{
        .name = "PASS_Cubes",
        .shaders = {vertexShader_, fragmentShader_},
        .attachments = {{
            {
                .target = colorTarget,
                .load = KDGpu::AttachmentLoadOperation::Clear,
                .store = KDGpu::AttachmentStoreOperation::Store,
                .clearColor = clearColor,
            },
        }},
        .depthAttachment =
            Cory::DepthStencilAttachment{
                .target = depthTarget,
                .load = KDGpu::AttachmentLoadOperation::Clear,
                .store = KDGpu::AttachmentStoreOperation::Store,
                .clearDepthStencil = clearDepthStencil,
            },
    });
    const auto colorOut = cubePass.colorOutputs().front();
    const auto depthOut = cubePass.depthOutput().value();

    /// ^^^^     DECLARATION      ^^^^
    Cory::RenderInput renderApi =
        co_await builder.finishDeclaration(PassOutputs{.colorOut = colorOut, .depthOut = depthOut});
    /// vvvv  RENDERING COMMANDS  vvvv

    auto passRecorder = cubePass.begin(renderApi);

    float aspect = static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y);
    glm::mat4 viewMatrix = camera_.viewMatrix;
    glm::mat4 projectionMatrix =
        Cory::makePerspective(camera_.fovy, aspect, camera_.nearPlane, camera_.farPlane);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;

    // update the uniform buffer
    auto drawData = renderApi.bindingContext->alloc<DrawData>();
    drawData->view = viewMatrix;
    drawData->projection = projectionMatrix;
    drawData->viewProjection = viewProjection;
    drawData->lightPosition = camera_.position;
    drawData->instances = 0;
    renderApi.bindingContext->push(drawData.gpu);

    const uint32_t instanceCount = static_cast<uint32_t>(renderState_.size());
    if (instanceCount > 0) {
        auto alloc = renderApi.bindingContext->alloc<InstanceData>(instanceCount);
        std::memcpy(alloc.cpu,
                    renderState_.data(),
                    static_cast<size_t>(instanceCount) * sizeof(InstanceData));
        drawData->instances = alloc.gpu;

        // bind the mesh buffers
        passRecorder.setVertexBuffer(0, cube_.vertexBuffer);
        passRecorder.setIndexBuffer(cube_.indexBuffer);

        passRecorder.drawIndexed(KDGpu::DrawIndexedCommand{
            .indexCount = cube_.indexCount,
            .instanceCount = instanceCount,
            .firstIndex = 0,
            .vertexOffset = 0,
            .firstInstance = 0,
        });
    }

    cubePass.end(std::move(passRecorder));
}

Cory::RenderTaskDeclaration<VolumeRenderSystem::PassOutputs>
VolumeRenderSystem::volumeFrameTask(Cory::RenderTaskBuilder builder,
                                    Cory::Framegraph &framegraph,
                                    const Cory::FrameContext &frameCtx,
                                    Cory::TransientTextureHandle colorTarget,
                                    Cory::TransientTextureHandle depthTarget)
{
    const bool useRasterizePath = debugRasterize.get();
    const bool useDebugRaycastPath = !useRasterizePath && debugRaycast.get();
    if (useRasterizePath) {
        const auto rasterization = rasterizationTask(
                                       builder.subtask("Rasterization"), colorTarget, depthTarget)
                                       .output();
        resetTemporalHistory();
        co_await builder.finishDeclaration(PassOutputs{
            .colorOut = rasterization.colorOut,
            .depthOut = rasterization.depthOut,
        });
        co_return;
    }

    const auto volumeGeneration = volumeGenerationTask(builder.subtask("VolumeGenerate")).output();
    const auto clearAttachments = Cory::StandardRenderTasks::clearAttachments(
                                      builder.subtask("ClearAttachments"), colorTarget, depthTarget)
                                      .output();

    if (useDebugRaycastPath) {
        const auto debugRaycastOutput =
            cubeRaycastDebugTask(builder.subtask("VolumeRaycastDebug"),
                                 clearAttachments.color,
                                 clearAttachments.depth.value_or(depthTarget),
                                 volumeGeneration)
                .output();
        resetTemporalHistory();
        co_await builder.finishDeclaration(PassOutputs{
            .colorOut = debugRaycastOutput,
            .depthOut = clearAttachments.depth.value_or(depthTarget),
        });
        co_return;
    }

    ensureTemporalHistoryTexture(frameCtx);
    CO_CORE_ASSERT(temporalHistory_.resource.valid(), "Temporal history resource expected to be valid");
    auto temporalHistoryInput =
        framegraph.declareInput(Cory::TransientTextureHandle{temporalHistory_.resource});
    const auto raycastResult = cubeRaycastTask(builder.subtask("VolumeRaycast"),
                                               temporalHistoryInput,
                                               clearAttachments.depth.value_or(depthTarget),
                                               volumeGeneration)
                                   .output();

    // Keep the persistent history handle in sync with the latest version after read/write.
    temporalHistory_.resource = Cory::FramegraphTextureHandle{raycastResult};
    temporalHistory_.valid = true;
    temporalHistory_.forceTemporalReset = false;

    co_await builder.finishDeclaration(PassOutputs{
        .colorOut = raycastResult,
        .depthOut = clearAttachments.depth.value_or(depthTarget),
    });
}

Cory::RenderTaskDeclaration<Cory::TransientTextureHandle>
VolumeRenderSystem::volumeGenerationTask(Cory::RenderTaskBuilder builder)
{
    auto volumeHandle = builder.create("TEX_VolumeData",
                                       volumeParams_.volumeDimensions,
                                       Gpu::Format::R32_SFLOAT,
                                       Gpu::TextureUsageFlagBits::StorageBit |
                                           Gpu::TextureUsageFlagBits::SampledBit,
                                       Cory::Sync::AccessType::ComputeShaderWrite,
                                       Gpu::TextureType::TextureType3D);

    auto volumePass = builder.declareComputePass(Cory::ComputePassDeclaration{
        .name = "PASS_VolumeGeneration",
        .shader = createVolumeShader_,
    });

    Cory::RenderInput renderApi = co_await builder.finishDeclaration(volumeHandle);

    auto recorder = volumePass.begin(renderApi);
    const auto &shader = renderApi.ctx->shaders()[createVolumeShader_];
    if (!shader.valid()) {
        CO_CORE_ERROR("Invalid volume generation shader in VolumeRenderSystem: {}", shader.error());
        volumePass.end(std::move(recorder));
        co_return;
    }

    recorder.bindShader(shader.shaderHandle());
    renderApi.bindingContext->bindStorageImage3D(volumeHandle, Gpu::TextureLayout::General);

    auto params = renderApi.bindingContext->alloc<VolumeGenerationParams>();
    *params.cpu = volumeParams_;
    renderApi.bindingContext->push(params.gpu);

    const glm::uvec3 dims = volumeParams_.volumeDimensions;
    constexpr uint32_t kGroupSizeX = 16u;
    constexpr uint32_t kGroupSizeY = 16u;
    const uint32_t groupsX = (dims.x + kGroupSizeX - 1u) / kGroupSizeX;
    const uint32_t groupsY = (dims.y + kGroupSizeY - 1u) / kGroupSizeY;
    const uint32_t groupsZ = dims.z;

    recorder.dispatchCompute({groupsX, groupsY, groupsZ});
    volumePass.end(std::move(recorder));
    co_return;
}

Cory::RenderTaskDeclaration<Cory::TransientTextureHandle>
VolumeRenderSystem::cubeRaycastTask(Cory::RenderTaskBuilder builder,
                                    Cory::TransientTextureHandle colorTarget,
                                    Cory::TransientTextureHandle depthTarget,
                                    Cory::TransientTextureHandle volumeTarget)
{
    builder.read(volumeTarget, Cory::RenderTaskBuilder::TextureReadPreset::ComputeSampled);

    auto [colorHandle, colorInfo] = builder.readWrite(
        colorTarget, Cory::RenderTaskBuilder::TextureReadWritePreset::GeneralStorage);

    auto raycastPass = builder.declareComputePass(Cory::ComputePassDeclaration{
        .name = "PASS_CubeRaycast",
        .shader = raycastShader_,
    });

    auto raycastShader = raycastShader_;
    Cory::RenderInput renderApi = co_await builder.finishDeclaration(colorHandle);

    float aspect = static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y);
    glm::mat4 viewMatrix = camera_.viewMatrix;
    glm::mat4 projectionMatrix =
        Cory::makePerspective(camera_.fovy, aspect, camera_.nearPlane, camera_.farPlane);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;
    glm::mat4 invViewProjection = glm::inverse(viewProjection);

    auto raycastRecorder = raycastPass.begin(renderApi);
    const auto &shader = renderApi.ctx->shaders()[raycastShader];
    if (!shader.valid()) {
        CO_CORE_ERROR("Invalid raycast shader in VolumeRenderSystem: {}", shader.error());
        raycastPass.end(std::move(raycastRecorder));
        co_return;
    }
    raycastRecorder.bindShader(shader.shaderHandle());
    if (colorInfo.sampleCount == Gpu::SampleCountFlagBits::Samples1Bit) {
        renderApi.bindingContext->bindStorageImage2D(colorHandle, Gpu::TextureLayout::General);
    }
    else {
        renderApi.bindingContext->bindStorageImage2DMS(colorHandle, Gpu::TextureLayout::General);
    }
    const auto volumeLayout = static_cast<Gpu::TextureLayout>(
        Cory::Sync::GetVkImageLayout(renderApi.resources->state(volumeTarget).lastAccess));
    const auto volumeTextureIndex = renderApi.bindingContext->bindTexture3D(
        volumeTarget, volumeLayout, volumeSampler_.handle());

    const uint32_t instanceCount = static_cast<uint32_t>(renderState_.size());
    auto drawData = renderApi.bindingContext->alloc<RaycastGlobals>();
    drawData->invViewProjection = invViewProjection;
    drawData->cameraPosition = glm::vec4{camera_.position, 1.0f};
    drawData->volumeDimensions = volumeParams_.volumeDimensions;
    drawData->time = volumeParams_.time;
    drawData->instanceCount = instanceCount;
    drawData->volumeTextureIndex = volumeTextureIndex;
    drawData->colorTargetIsMsaa =
        colorInfo.sampleCount == Gpu::SampleCountFlagBits::Samples1Bit ? 0u : 1u;
    const auto temporalTauSeconds = std::max(temporalEmaTauMs.get() * 0.001f, 1e-4f);
    const auto temporalAlpha =
        std::clamp(1.0f - std::exp(-lastFrameDeltaSeconds_ / temporalTauSeconds), 0.001f, 1.0f);
    const bool applyTemporal =
        temporalAccumulation.get() && temporalHistory_.valid && !temporalHistory_.forceTemporalReset;
    drawData->temporalBlendFactor = applyTemporal ? temporalAlpha : 1.0f;
    drawData->iterations = static_cast<uint32_t>(std::max(temporalIterations.get(), 1));
    drawData->alphaDeltaRejectThreshold = std::max(alphaDeltaRejectThreshold.get(), 0.0f);
    drawData->padding0 = 0u;

    drawData->instances = 0;
    if (instanceCount > 0) {
        auto alloc = renderApi.bindingContext->alloc<InstanceData>(instanceCount);
        std::memcpy(alloc.cpu,
                    renderState_.data(),
                    static_cast<size_t>(instanceCount) * sizeof(InstanceData));
        drawData->instances = alloc.gpu;
    }
    renderApi.bindingContext->push(drawData.gpu);

    constexpr uint32_t kThreadGroupSizeX = 16u;
    constexpr uint32_t kThreadGroupSizeY = 16u;
    const uint32_t groupsX = (colorInfo.size.x + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
    const uint32_t groupsY = (colorInfo.size.y + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;

    raycastRecorder.dispatchCompute({groupsX, groupsY, 1});
    raycastPass.end(std::move(raycastRecorder));
}

Cory::RenderTaskDeclaration<Cory::TransientTextureHandle>
VolumeRenderSystem::cubeRaycastDebugTask(Cory::RenderTaskBuilder builder,
                                         Cory::TransientTextureHandle colorTarget,
                                         Cory::TransientTextureHandle depthTarget,
                                         Cory::TransientTextureHandle volumeTarget)
{
    builder.read(volumeTarget, Cory::RenderTaskBuilder::TextureReadPreset::ComputeSampled);

    auto [colorHandle, colorInfo] = builder.readWrite(
        colorTarget, Cory::RenderTaskBuilder::TextureReadWritePreset::GeneralStorage);

    auto debugPass = builder.declareComputePass(Cory::ComputePassDeclaration{
        .name = "PASS_CubeRaycastDebugLocalPos",
        .shader = raycastDebugShader_,
    });

    auto debugShader = raycastDebugShader_;
    Cory::RenderInput renderApi = co_await builder.finishDeclaration(colorHandle);

    float aspect = static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y);
    glm::mat4 viewMatrix = camera_.viewMatrix;
    glm::mat4 projectionMatrix =
        Cory::makePerspective(camera_.fovy, aspect, camera_.nearPlane, camera_.farPlane);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;
    glm::mat4 invViewProjection = glm::inverse(viewProjection);

    auto debugRecorder = debugPass.begin(renderApi);
    const auto &shader = renderApi.ctx->shaders()[debugShader];
    if (!shader.valid()) {
        CO_CORE_ERROR("Invalid debug raycast shader in VolumeRenderSystem: {}", shader.error());
        debugPass.end(std::move(debugRecorder));
        co_return;
    }
    debugRecorder.bindShader(shader.shaderHandle());
    if (colorInfo.sampleCount == Gpu::SampleCountFlagBits::Samples1Bit) {
        std::ignore =
            renderApi.bindingContext->bindStorageImage2D(colorHandle, Gpu::TextureLayout::General);
    }
    else {
        std::ignore = renderApi.bindingContext->bindStorageImage2DMS(colorHandle,
                                                                     Gpu::TextureLayout::General);
    }

    const uint32_t instanceCount = static_cast<uint32_t>(renderState_.size());
    auto drawData = renderApi.bindingContext->alloc<RaycastGlobals>();
    drawData->invViewProjection = invViewProjection;
    drawData->cameraPosition = glm::vec4{camera_.position, 1.0f};
    drawData->volumeDimensions = volumeParams_.volumeDimensions;
    drawData->time = volumeParams_.time;
    drawData->instanceCount = instanceCount;
    drawData->volumeTextureIndex = 0;
    drawData->colorTargetIsMsaa =
        colorInfo.sampleCount == Gpu::SampleCountFlagBits::Samples1Bit ? 0u : 1u;
    drawData->temporalBlendFactor = 1.0f;
    drawData->iterations = 1u;
    drawData->alphaDeltaRejectThreshold = 0.0f;
    drawData->padding0 = 0u;

    if (instanceCount > 0) {
        auto alloc = renderApi.bindingContext->alloc<InstanceData>(instanceCount);
        std::memcpy(alloc.cpu,
                    renderState_.data(),
                    static_cast<size_t>(instanceCount) * sizeof(InstanceData));
        drawData->instances = alloc.gpu;

        renderApi.bindingContext->push(drawData.gpu);

        constexpr uint32_t kThreadGroupSizeX = 16u;
        constexpr uint32_t kThreadGroupSizeY = 16u;
        const uint32_t groupsX = (colorInfo.size.x + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
        const uint32_t groupsY = (colorInfo.size.y + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;

        debugRecorder.dispatchCompute({groupsX, groupsY, 1});
    }
    debugPass.end(std::move(debugRecorder));
}
