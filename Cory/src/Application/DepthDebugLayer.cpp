#include <Cory/Application/DepthDebugLayer.hpp>

#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Base/Utils.hpp>
#include <Cory/Framegraph/CommandList.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/TextureManager.hpp>
#include <Cory/ImGui/Inputs.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/ResourceManager.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>

#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/Device.h>

namespace Cory {

struct Uniforms {
    glm::vec2 center;
    glm::vec2 size;
    glm::vec2 window;
};
struct DepthDebugLayer::State {
    Cory::ShaderHandle fullscreenTriShader;
    Cory::ShaderHandle depthDebugShader;
    Cory::UniformBufferObject<Uniforms> ubo;
};

DepthDebugLayer::DepthDebugLayer()
    : ApplicationLayer("DepthDebug")
{
}

DepthDebugLayer::~DepthDebugLayer()
{
    if (state_) { CO_CORE_WARN("DepthDebugLayer was not before it was destroyed!"); }
}

void DepthDebugLayer::onAttach(Context &ctx, LayerAttachInfo info)
{
    CO_CORE_ASSERT(state_ == nullptr, "Layer was already attached!");

    auto &res = ctx.resources();
    state_ = std::make_unique<State>(
        State{.fullscreenTriShader{res.createShader(ResourceLocator::Locate("fullscreenTri.vert"))},
              .depthDebugShader{res.createShader(ResourceLocator::Locate("depthDebug.frag"))},
              .ubo{Cory::UniformBufferObject<Uniforms>(ctx, info.maxFramesInFlight)}});
}

void DepthDebugLayer::onDetach(Context &ctx)
{
    auto &res = ctx.resources();
    res.release(state_->fullscreenTriShader);
    res.release(state_->depthDebugShader);

    state_.reset();
}

bool DepthDebugLayer::onEvent(Event event)
{
    return std::visit(lambda_visitor{[this](const MouseMovedEvent &e) {
                                         center = e.position;
                                         return true;
                                     },
                                     [this](auto &&e) { return false; }},
                      event);
}

void DepthDebugLayer::onUpdate()
{
    if (::ImGui::Begin("DepthDebugLayer")) {
        Cory::ImGui::Slider("center", center, 0.0f, 1.0f);
        Cory::ImGui::Slider("size", size, 0.0f, 1.0f);
        Cory::ImGui::Slider("window", window, 0.0f, 1.0f);
    }
    ::ImGui::End();
}

RenderTaskDeclaration<LayerPassOutputs> DepthDebugLayer::renderTask(Cory::RenderTaskBuilder builder,
                                                                    LayerPassOutputs previousLayer)
{
    VkClearColorValue clearColor{0.0f, 0.0f, 0.0f, 1.0f};

    auto [writtenColorHandle, colorInfo] =
        builder.readWrite(previousLayer.color, Cory::Sync::AccessType::ColorAttachmentReadWrite);
    auto depthInfo =
        builder.read(previousLayer.depth,
                     Cory::Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto cubePass = builder.declareRenderPass("PASS_DepthDebug")
                        .shaders({state_->fullscreenTriShader, state_->depthDebugShader})
                        .disableMeshInput() // fullscreen triangle pass
                        .attach(previousLayer.color,
                                VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD,
                                VK_ATTACHMENT_STORE_OP_STORE,
                                clearColor)
                        .finish();

    /// ^^^^     DECLARATION      ^^^^
    co_yield LayerPassOutputs{.color = writtenColorHandle, .depth = previousLayer.depth};
    Cory::RenderInput renderApi = co_await builder.finishDeclaration();
    /// vvvv  RENDERING COMMANDS  vvvv

    Context &ctx = *renderApi.ctx;
    FrameContext &frameCtx = *renderApi.frameCtx;

    // update the uniform buffer
    Uniforms &frameUniforms = state_->ubo[frameCtx.index];
    frameUniforms.size = size.get();
    frameUniforms.center = center.get();
    frameUniforms.window = window.get();
    state_->ubo.flush(frameCtx.index);

    Cory::TextureManager &resources = *renderApi.resources;

    std::array layouts{Sync::GetVkImageLayout(resources.state(previousLayer.depth).lastAccess)};
    std::array textures{resources.imageView(previousLayer.depth)};
    std::array samplers{ctx.defaultSampler()};

    auto &descriptorSets = *renderApi.descriptors;
    descriptorSets
        .write(Cory::DescriptorSets::SetType::Frame, frameCtx.index, layouts, textures, samplers)
        .write(Cory::DescriptorSets::SetType::Frame, frameCtx.index, state_->ubo)
        .flushWrites()
        .bind(renderApi.cmd->handle(), frameCtx.index, ctx.defaultPipelineLayout());

    cubePass.begin(*renderApi.cmd);

    ctx.device()->CmdDraw(renderApi.cmd->handle(), 3, 1, 0, 0);
    cubePass.end(*renderApi.cmd);
}

} // namespace Cory
