#include "VolumeRenderSystem.hpp"

#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
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
    glm::uvec2 imageSize;
    uint32_t instanceCount;
    uint32_t volumeTextureIndex;
    uint32_t padding0;
    Cory::BufferDeviceAddress instances;
};

VolumeRenderSystem::VolumeRenderSystem(Cory::Context &ctx)
    : Base()
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
        .path = Cory::ResourceLocator::Locate("raycast_boxes.comp.slang"),
        .stage = Gpu::ShaderStageFlagBits::ComputeBit,
        .label = "raycast_boxes.comp.slang",
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

void VolumeRenderSystem::beforeUpdate(Cory::SceneGraph &sg)
{
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
    volumeParams_.time = static_cast<float>(tick.now.time_since_epoch().count());

    renderState_.push_back({
        .modelToWorld = transform.modelToWorld * glm::scale(volume.size),
        .worldToModel = inverse(transform.modelToWorld * glm::scale(volume.size)),
        .normalToWorld = transpose(inverse(transform.modelToWorld)),
        .color = Cory::Color{1.0, 0.0, 0.0, 1.0},
        .parameters = glm::vec4{0.2, 1.0f, 8.0f, 0.0f},
    });
}

Cory::RenderTaskDeclaration<VolumeRenderSystem::PassOutputs>
VolumeRenderSystem::cubeRenderTask(Cory::RenderTaskBuilder builder,
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

    shaderHotReloader_.processPendingReloads(renderApi.frameCtx->frameNumber);

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

        renderApi.bindingContext->flush();
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

    volumeParams_.time = float(renderApi.frameCtx->frameNumber) / 60.0f; // TODO where's my time at
    auto params = renderApi.bindingContext->alloc<VolumeGenerationParams>();
    *params.cpu = volumeParams_;
    renderApi.bindingContext->push(params.gpu);
    renderApi.bindingContext->flush();

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
    drawData->imageSize = colorInfo.size;
    drawData->instanceCount = instanceCount;
    drawData->volumeTextureIndex = volumeTextureIndex;
    drawData->padding0 = 0;

    if (instanceCount > 0) {
        auto alloc = renderApi.bindingContext->alloc<InstanceData>(instanceCount);
        std::memcpy(alloc.cpu,
                    renderState_.data(),
                    static_cast<size_t>(instanceCount) * sizeof(InstanceData));
        drawData->instances = alloc.gpu;

        renderApi.bindingContext->push(drawData.gpu);
        renderApi.bindingContext->flush();

        constexpr uint32_t kThreadGroupSizeX = 16u;
        constexpr uint32_t kThreadGroupSizeY = 16u;
        const uint32_t groupsX = (colorInfo.size.x + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
        const uint32_t groupsY = (colorInfo.size.y + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;

        raycastRecorder.dispatchCompute({groupsX, groupsY, 1});
    }
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
    drawData->imageSize = colorInfo.size;
    drawData->instanceCount = instanceCount;
    drawData->volumeTextureIndex = 0;
    drawData->padding0 = 0;

    if (instanceCount > 0) {
        auto alloc = renderApi.bindingContext->alloc<InstanceData>(instanceCount);
        std::memcpy(alloc.cpu,
                    renderState_.data(),
                    static_cast<size_t>(instanceCount) * sizeof(InstanceData));
        drawData->instances = alloc.gpu;

        renderApi.bindingContext->push(drawData.gpu);
        renderApi.bindingContext->flush();

        constexpr uint32_t kThreadGroupSizeX = 16u;
        constexpr uint32_t kThreadGroupSizeY = 16u;
        const uint32_t groupsX = (colorInfo.size.x + kThreadGroupSizeX - 1u) / kThreadGroupSizeX;
        const uint32_t groupsY = (colorInfo.size.y + kThreadGroupSizeY - 1u) / kThreadGroupSizeY;

        debugRecorder.dispatchCompute({groupsX, groupsY, 1});
    }
    debugPass.end(std::move(debugRecorder));
}
