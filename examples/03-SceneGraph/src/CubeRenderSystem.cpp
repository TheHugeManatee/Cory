#include "CubeRenderSystem.hpp"

#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>

#include <KDGpu/buffer_options.h>
#include <KDGpu/gpu_core.h>

#include <cstddef>

CubeRenderSystem::CubeRenderSystem(Cory::Context &ctx, uint32_t maxFramesInFlight)
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

    globalUbo_ = std::make_unique<Cory::UniformBufferObject<CubeUBO>>(ctx, maxFramesInFlight);

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
    // need explicit flush otherwise the mapped memory is not synced to the GPU
    globalUbo_->flush(frameCtx.inFlightIndex);

    auto &descriptorSets = ctx_->descriptors();
    constexpr Cory::DescriptorSets::BufferIndex kInstanceBufferIndex = 0;
    descriptorSets.write(frameCtx.inFlightIndex, *globalUbo_);

    const uint32_t instanceCount = static_cast<uint32_t>(renderState_.size());
    if (instanceCount > 0) {
        auto &instanceBuffer = instanceBufferForFrame(frameCtx.inFlightIndex, instanceCount);
        auto *mapped = static_cast<std::byte *>(instanceBuffer.buffer.map());
        std::memcpy(
            mapped, renderState_.data(), static_cast<size_t>(instanceCount) * sizeof(InstanceData));
        instanceBuffer.buffer.unmap();

        descriptorSets.write(Cory::BufferBindPoint::StorageBufferReadOnly,
                             frameCtx.inFlightIndex,
                             kInstanceBufferIndex,
                             instanceBuffer.buffer);
    }

    descriptorSets.bind(passRecorder, frameCtx.inFlightIndex);

    // Set dynamic states
    passRecorder.setCullMode(KDGpu::CullModeFlagBits::BackBit);
    passRecorder.setDepthTestEnabled(true);
    passRecorder.setDepthWriteEnabled(true);
    passRecorder.setDepthCompareOp(KDGpu::CompareOperation::Less);

    // bind the mesh buffers
    passRecorder.setVertexBuffer(0, mesh_->vertexBuffer);
    passRecorder.setIndexBuffer(mesh_->indexBuffer);

    if (instanceCount > 0) {
        passRecorder.drawIndexed(KDGpu::DrawIndexedCommand{
            .indexCount = mesh_->indexCount,
            .instanceCount = instanceCount,
            .firstIndex = 0,
            .vertexOffset = 0,
            .firstInstance = 0,
        });
    }

    passRecorder.end();
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
            .usage = KDGpu::BufferUsageFlagBits::StorageBufferBit,
            .memoryUsage = KDGpu::MemoryUsage::CpuToGpu,
        });
        instanceBuffer.capacity = requiredSize;
    }

    return instanceBuffer;
}
