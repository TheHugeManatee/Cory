#include "VolumeRenderSystem.hpp"

#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <Cory/Renderer/Synchronization.hpp>

#include <KDGpu/gpu_core.h>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/vec2.hpp>
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
    uint32_t padding0;
    Cory::BufferDeviceAddress instances;
};

VolumeRenderSystem::VolumeRenderSystem(Cory::Context &ctx)
    : Base()
    , ctx_(&ctx)
{
    // Create mesh using Cory::DynamicGeometry, as in 02-CubeDemo
    cube_ = Cory::DynamicGeometry::createCube(ctx);

    vertexShader_ = ctx.shaders().createShader(Cory::ResourceLocator::Locate("cube.vert.slang"));
    fragmentShader_ = ctx.shaders().createShader(Cory::ResourceLocator::Locate("cube.frag.slang"));
    raycastShader_ =
        ctx.shaders().createShader(Cory::ResourceLocator::Locate("raycast_boxes.comp.slang"));
}

VolumeRenderSystem::~VolumeRenderSystem()
{
    if (ctx_) {
        auto &shaders = ctx_->shaders();
        shaders.release(vertexShader_);
        shaders.release(fragmentShader_);
        shaders.release(raycastShader_);
    }
}

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
VolumeRenderSystem::cubeRaycastTask(Cory::RenderTaskBuilder builder,
                                    Cory::TransientTextureHandle colorTarget,
                                    Cory::TransientTextureHandle depthTarget)
{
    auto [colorHandle, colorInfo] = builder.readWrite(
        colorTarget,
        Gpu::TextureUsageFlagBits::ColorAttachmentBit | Gpu::TextureUsageFlagBits::StorageBit,
        Cory::Sync::AccessType::General);

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
    const auto& shader = renderApi.ctx->shaders()[raycastShader];
    if (!shader.valid()) {
        CO_CORE_ERROR("Invalid raycast shader in VolumeRenderSystem: {}", shader.error());
        raycastPass.end(std::move(raycastRecorder));
        co_return;
    }
    raycastRecorder.bindShader(shader.shaderHandle());
    renderApi.bindingContext->bindStorageImage2D(colorHandle, Gpu::TextureLayout::General);

    const uint32_t instanceCount = static_cast<uint32_t>(renderState_.size());
    auto drawData = renderApi.bindingContext->alloc<RaycastGlobals>();
    drawData->invViewProjection = invViewProjection;
    drawData->cameraPosition = glm::vec4{camera_.position, 1.0f};
    drawData->imageSize = colorInfo.size;
    drawData->instanceCount = instanceCount;
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
