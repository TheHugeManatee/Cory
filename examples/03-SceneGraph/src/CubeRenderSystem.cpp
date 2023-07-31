#include "CubeRenderSystem.hpp"

#include <Cory/Application/DynamicGeometry.hpp>
#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Framegraph/CommandList.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/ResourceManager.hpp>

#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/Mesh.h>
#include <Magnum/Vk/PipelineLayout.h>

using namespace Magnum;

CubeRenderSystem::CubeRenderSystem(Cory::Context &ctx, uint32_t maxFramesInFlight)
    : Base()
    , ctx_(&ctx)
{
    mesh_ = std::make_unique<Vk::Mesh>(Cory::DynamicGeometry::createCube(ctx));

    globalUbo_ = std::make_unique<Cory::UniformBufferObject<CubeUBO>>(*ctx_, maxFramesInFlight);

    vertexShader_ = ctx_->resources().createShader(Cory::ResourceLocator::Locate("cube.vert"));
    fragmentShader_ = ctx_->resources().createShader(Cory::ResourceLocator::Locate("cube.frag"));
}

CubeRenderSystem::~CubeRenderSystem()
{
    auto &resources = ctx_->resources();
    resources.release(vertexShader_);
    resources.release(fragmentShader_);
}

void CubeRenderSystem::beforeUpdate(Cory::SceneGraph &sg)
{
    renderState_.clear();
    // update the camera's state
    forEach<CameraComponent>(sg,
                             [this](Cory::Entity e, CameraComponent &camera) { camera_ = camera; });
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

    VkClearColorValue clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    float clearDepth = 1.0f;

    auto [writtenColorHandle, colorInfo] =
        builder.write(colorTarget, Cory::Sync::AccessType::ColorAttachmentWrite);
    auto [writtenDepthHandle, depthInfo] =
        builder.write(depthTarget, Cory::Sync::AccessType::DepthStencilAttachmentWrite);

    auto cubePass = builder.declareRenderPass("PASS_Cubes")
                        .shaders({vertexShader_, fragmentShader_})
                        .attach(colorTarget,
                                VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR,
                                VK_ATTACHMENT_STORE_OP_STORE,
                                clearColor)
                        .attachDepth(depthTarget,
                                     VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR,
                                     VK_ATTACHMENT_STORE_OP_STORE,
                                     clearDepth)
                        .finish();

    co_yield PassOutputs{.colorOut = writtenColorHandle, .depthOut = writtenDepthHandle};

    /// ^^^^     DECLARATION      ^^^^
    Cory::RenderInput renderApi = co_await builder.finishDeclaration();
    /// vvvv  RENDERING COMMANDS  vvvv

    cubePass.begin(*renderApi.cmd);

    float aspect = static_cast<float>(colorInfo.size.x) / static_cast<float>(colorInfo.size.y);
    glm::mat4 viewMatrix = camera_.viewMatrix;
    glm::mat4 projectionMatrix =
        Cory::makePerspective(camera_.fovy, aspect, camera_.nearPlane, camera_.farPlane);
    glm::mat4 viewProjection = projectionMatrix * viewMatrix;

    Cory::FrameContext &frameCtx = *renderApi.frameCtx;

    // update the uniform buffer
    CubeUBO &ubo = (*globalUbo_)[frameCtx.index];
    ubo.view = viewMatrix;
    ubo.projection = projectionMatrix;
    ubo.viewProjection = viewProjection;
    // need explicit flush otherwise the mapped memory is not synced to the GPU
    globalUbo_->flush(frameCtx.index);

    ctx_->descriptorSets()
        .write(Cory::DescriptorSets::SetType::Static, frameCtx.index, *globalUbo_)
        .flushWrites()
        .bind(renderApi.cmd->handle(), frameCtx.index, ctx_->defaultPipelineLayout());

    // records commands for each cube
    recordCommands(*renderApi.cmd);

    cubePass.end(*renderApi.cmd);
}

void CubeRenderSystem::recordCommands(Cory::CommandList &cmd)
{
    for (auto &anim : renderState_) {
        // update push constants
        ctx_->device()->CmdPushConstants(cmd->handle(),
                                         ctx_->defaultPipelineLayout(),
                                         VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
                                         0,
                                         sizeof(anim),
                                         &anim);

        cmd.handle().draw(*mesh_);
    }
}
