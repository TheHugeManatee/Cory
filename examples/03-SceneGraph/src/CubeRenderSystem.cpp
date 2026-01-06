#include "CubeRenderSystem.hpp"

#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Framegraph/ShaderBindingContext.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/gpu_core.h>

#include <cstddef>

CubeRenderSystem::CubeRenderSystem(Cory::Context &ctx)
    : Base()
    , ctx_(&ctx)
{
    // Create mesh using Cory::DynamicGeometry, as in 02-CubeDemo
    auto cube = Cory::DynamicGeometry::createCube(ctx);
    mesh_ = std::make_unique<CubeMesh>(CubeMesh{
        .vertexBuffer = std::move(cube.vertexBuffer),
        .indexBuffer = std::move(cube.indexBuffer),
        .indexCount = cube.indexCount,
    });

    vertexShader_ = ctx.shaders().createShader(Cory::ResourceLocator::Locate("cube.vert.slang"));
    fragmentShader_ = ctx.shaders().createShader(Cory::ResourceLocator::Locate("cube.frag.slang"));
}

CubeRenderSystem::~CubeRenderSystem()
{
    if (ctx_) {
        auto &shaders = ctx_->shaders();
        shaders.release(vertexShader_);
        shaders.release(fragmentShader_);
    }
}

void CubeRenderSystem::beforeUpdate(Cory::SceneGraph &sg)
{
    renderState_.clear();
    // update the camera's state
    forEach<Cory::Components::CameraComponent>(
        sg, [this](Cory::Entity e, auto &camera) { camera_ = camera; });
}

void CubeRenderSystem::update(Cory::SceneGraph &sg,
                              Cory::TickInfo tick,
                              Cory::Entity entity,
                              const AnimationComponent &anim,
                              const Cory::Components::Transform &transform)
{
    renderState_.push_back({
        .modelToWorld = transform.modelToWorld,
        .normalToWorld = transpose(inverse(transform.modelToWorld)),
        .color = anim.color,
        .parameters = glm::vec4{anim.blend, 0.0f, 0.0f, 0.0f},
    });
}

Cory::RenderTaskDeclaration<CubeRenderSystem::PassOutputs>
CubeRenderSystem::cubeRenderTask(Cory::RenderTaskBuilder builder,
                                 Cory::TransientTextureHandle colorTarget,
                                 Cory::TransientTextureHandle depthTarget)
{
    KDGpu::ColorClearValue clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    KDGpu::DepthStencilClearValue clearDepthStencil = {1.0f, 0};

    auto [writtenColorHandle, colorInfo] =
        builder.write(colorTarget, Cory::Sync::AccessType::ColorAttachmentWrite);
    auto [writtenDepthHandle, depthInfo] =
        builder.write(depthTarget, Cory::Sync::AccessType::DepthStencilAttachmentWrite);

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

    /// ^^^^     DECLARATION      ^^^^
    Cory::RenderInput renderApi = co_await builder.finishDeclaration(PassOutputs{
        .colorOut = writtenColorHandle,
        .depthOut = writtenDepthHandle,
    });
    /// vvvv  RENDERING COMMANDS  vvvv

    auto passRecorder = cubePass.begin(renderApi);

    float aspect = static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y);
    glm::mat4 viewMatrix = camera_.viewMatrix;
    glm::mat4 projectionMatrix =
        Cory::makePerspective(camera_.fovy, aspect, camera_.nearPlane, camera_.farPlane);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;

    Cory::FrameContext &frameCtx = *renderApi.frameCtx;

    // update the uniform buffer
    auto drawData = renderApi.bindingContext->alloc<CubeUBO>();
    drawData->view = viewMatrix;
    drawData->projection = projectionMatrix;
    drawData->viewProjection = viewProjection;
    drawData->lightPosition = camera_.position;
    drawData->instances = 0;
    renderApi.bindingContext->push(drawData.gpu);

    const uint32_t instanceCount = static_cast<uint32_t>(renderState_.size());
    CO_CORE_ASSERT(instanceCount > 0, "No instances to render in CubeRenderSystem!");

    auto &instanceBuffer = instanceBufferForFrame(frameCtx.inFlightIndex, instanceCount);
    auto *mapped = static_cast<std::byte *>(instanceBuffer.buffer.map());
    std::memcpy(
        mapped, renderState_.data(), static_cast<size_t>(instanceCount) * sizeof(InstanceData));
    instanceBuffer.buffer.unmap();
    drawData->instances = instanceBuffer.buffer.bufferDeviceAddress();

    // bind the mesh buffers
    passRecorder.setVertexBuffer(0, mesh_->vertexBuffer);
    passRecorder.setIndexBuffer(mesh_->indexBuffer);

    renderApi.bindingContext->flush();
    passRecorder.drawIndexed(KDGpu::DrawIndexedCommand{
        .indexCount = mesh_->indexCount,
        .instanceCount = instanceCount,
        .firstIndex = 0,
        .vertexOffset = 0,
        .firstInstance = 0,
    });

    cubePass.end(std::move(passRecorder));
}

InstanceBuffer &CubeRenderSystem::instanceBufferForFrame(uint32_t frameIndex,
                                                         uint32_t instanceCount)
{
    if (instanceBuffers_.size() <= frameIndex) {
        instanceBuffers_.resize(frameIndex + 1);
    }

    const KDGpu::DeviceSize requiredSize =
        static_cast<KDGpu::DeviceSize>(instanceCount) * sizeof(InstanceData);
    auto &instanceBuffer = instanceBuffers_[frameIndex];
    if (!instanceBuffer.buffer.isValid() || instanceBuffer.capacity < requiredSize) {
        instanceBuffer.buffer = ctx_->device().createBuffer(KDGpu::BufferOptions{
            .label = "SceneGraph Instance Buffer",
            .size = requiredSize,
            .usage = KDGpu::BufferUsageFlagBits::StorageBufferBit |
                     KDGpu::BufferUsageFlagBits::ShaderDeviceAddressBit,
            .memoryUsage = KDGpu::MemoryUsage::CpuToGpu,
        });
        instanceBuffer.capacity = requiredSize;
    }

    return instanceBuffer;
}
