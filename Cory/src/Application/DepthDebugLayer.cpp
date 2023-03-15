#include <Cory/Application/DepthDebugLayer.hpp>

#include <Cory/Base/ResourceLocator.hpp>
#include <Cory/Framegraph/CommandList.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/TextureManager.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/ResourceManager.hpp>

#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/Device.h>

namespace Cory {

RenderTaskDeclaration<LayerPassOutputs> DepthDebugLayer::renderTask(Cory::RenderTaskBuilder builder,
                                                                    LayerPassOutputs previousLayer)
{
    VkClearColorValue clearColor{0.0f, 0.0f, 0.0f, 1.0f};

    auto [writtenColorHandle, colorInfo] =
        builder.write(previousLayer.color, Cory::Sync::AccessType::ColorAttachmentWrite);
    auto depthInfo =
        builder.read(previousLayer.depth,
                     Cory::Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto cubePass = builder.declareRenderPass("PASS_DepthDebug")
                        .shaders({fullscreenTriShader_, depthDebugShader_})
                        .disableMeshInput() // fullscreen triangle pass
                        .attach(previousLayer.color,
                                VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD,
                                VK_ATTACHMENT_STORE_OP_STORE,
                                clearColor)
                        .finish();

    co_yield LayerPassOutputs{.color = writtenColorHandle, .depth = {}};

    /// ^^^^     DECLARATION      ^^^^
    Cory::RenderInput renderApi = co_await builder.finishDeclaration();
    /// vvvv  RENDERING COMMANDS  vvvv

    ////////////

    // TODO figure out if this was ever necessary when this layer was part of CubeDemo class
    // globalUbo_->flush(frameCtx.index);

    Context &ctx = *renderApi.ctx;
    FrameContext &frameCtx = *renderApi.frameCtx;

    Cory::TextureManager &resources = *renderApi.resources;

    // TODO get layout from texturemanager!
    // auto layout = Sync::GetVkImageLayout(resources.state(previousLayer.depth).lastAccess);
    std::array layouts{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    std::array textures{resources.imageView(previousLayer.depth)};
    std::array samplers{ctx.defaultSampler()};

    auto &descriptorSets = *renderApi.descriptors;
    descriptorSets
        .write(Cory::DescriptorSets::SetType::Static, frameCtx.index, layouts, textures, samplers)
        .flushWrites()
        .bind(renderApi.cmd->handle(), frameCtx.index, ctx.defaultPipelineLayout());
    ////////////

    cubePass.begin(*renderApi.cmd);

    ctx.device()->CmdDraw(renderApi.cmd->handle(), 3, 1, 0, 0);
    cubePass.end(*renderApi.cmd);
}
void DepthDebugLayer::onAttach(Context &ctx)
{
    fullscreenTriShader_ =
        ctx.resources().createShader(ResourceLocator::Locate("fullscreenTri.vert"));
    depthDebugShader_ = ctx.resources().createShader(ResourceLocator::Locate("depthDebug.frag"));
}
void DepthDebugLayer::onDetach(Context &ctx)
{
    ctx.resources().release(depthDebugShader_);
    ctx.resources().release(fullscreenTriShader_);
}
void DepthDebugLayer::onEvent() { ApplicationLayer::onEvent(); }
void DepthDebugLayer::onUpdate() { ApplicationLayer::onUpdate(); }
} // namespace Cory