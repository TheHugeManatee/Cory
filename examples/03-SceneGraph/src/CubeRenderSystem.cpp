#include "CubeRenderSystem.hpp"

#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>
#include <KDGpu/gpu_core.h>

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

    vertexShader_ = ctx.shaders().createShader(Cory::ResourceLocator::Locate("cube.vert"));
    fragmentShader_ = ctx.shaders().createShader(Cory::ResourceLocator::Locate("cube.frag"));
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
    renderState_.push_back(
        {.modelToWorld = transform.modelToWorld, .color = anim.color, .blend = anim.blend});
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

    auto pushRanges = std::array<KDGpu::PushConstantRange, 1>{{{
        .offset = 0,
        .size = sizeof(CubePushConstantState),
        .shaderStages = KDGpu::ShaderStageFlagBits::AllGraphics,
    }}};
    auto cubePass = builder.declareRenderPass(
        Cory::RenderPassDeclaration{.name = "PASS_Cubes",
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
                                    .pushConstantRanges = {pushRanges.begin(), pushRanges.end()}});

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
    // need explicit flush otherwise the mapped memory is not synced to the GPU
    globalUbo_->flush(frameCtx.inFlightIndex);

    ctx_->descriptors()
        .write(Cory::DescriptorSets::SetType::Static, frameCtx.inFlightIndex, *globalUbo_)
        .flushWrites()
        .bind(passRecorder, frameCtx.inFlightIndex);

    // Set dynamic states
    passRecorder.setCullMode(KDGpu::CullModeFlagBits::BackBit);
    passRecorder.setDepthTestEnabled(true);
    passRecorder.setDepthWriteEnabled(true);
    passRecorder.setDepthCompareOp(KDGpu::CompareOperation::Less);

    // bind the mesh buffers
    passRecorder.setVertexBuffer(0, mesh_->vertexBuffer);
    passRecorder.setIndexBuffer(mesh_->indexBuffer);

    // records commands for each cube
    recordCommands(passRecorder);

    passRecorder.end();
}

void CubeRenderSystem::recordCommands(KDGpu::RenderPassCommandRecorder &recorder)
{
    auto pushRanges = std::array<KDGpu::PushConstantRange, 1>{{{
        .offset = 0,
        .size = sizeof(CubePushConstantState),
        .shaderStages = KDGpu::ShaderStageFlagBits::AllGraphics,
    }}};
    for (auto &anim : renderState_) {
        recorder.pushConstant(pushRanges[0], &anim);
        recorder.drawIndexed(KDGpu::DrawIndexedCommand{
            .indexCount = mesh_->indexCount,
            .instanceCount = 1,
            .firstIndex = 0,
            .vertexOffset = 0,
            .firstInstance = 0,
        });
    }
}
