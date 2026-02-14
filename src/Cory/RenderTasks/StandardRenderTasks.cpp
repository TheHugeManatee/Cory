#include "StandardRenderTasks.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Base/GlmUtils.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>

#include <KDGpu/command_recorder.h>

namespace Cory::StandardRenderTasks {

RenderTaskDeclaration<ClearPassOutputs>
clearAttachments(RenderTaskBuilder builder,
                 TransientTextureHandle colorTarget,
                 std::optional<TransientTextureHandle> depthTarget,
                 Gpu::ColorClearValue clearColor,
                 Gpu::DepthStencilClearValue clearDepth)
{
    auto clearPass = builder.declareRenderPass(RenderPassDeclaration{
        .name = "PASS_ClearAttachments",
        .options = PassOptionFlagBits::SkipPipelineBind,
        .attachments = {{
            {
                .target = colorTarget,
                .load = Gpu::AttachmentLoadOperation::Clear,
                .store = Gpu::AttachmentStoreOperation::Store,
                .clearColor = clearColor,
                .blend = std::nullopt,
            },
        }},
        .depthAttachment = depthTarget.has_value()
                               ? std::optional<DepthStencilAttachment>{DepthStencilAttachment{
                                     .target = *depthTarget,
                                     .load = Gpu::AttachmentLoadOperation::Clear,
                                     .store = Gpu::AttachmentStoreOperation::Store,
                                     .clearDepthStencil = clearDepth,
                                 }}
                               : std::nullopt,
    });

    const auto colorOut = clearPass.colorOutputs().front();
    const auto depthOut = clearPass.depthOutput();
    RenderInput renderApi =
        co_await builder.finishDeclaration(ClearPassOutputs{.color = colorOut, .depth = depthOut});

    auto recorder = clearPass.begin(renderApi);
    clearPass.end(std::move(recorder));
}

RenderTaskDeclaration<TransientTextureHandle> resolve(RenderTaskBuilder builder,
                                                      TransientTextureHandle sourceImage,
                                                      TransientTextureHandle targetImage)
{
    auto colorInfo = builder.read(sourceImage, RenderTaskBuilder::TextureReadPreset::TransferSrc);
    auto [outputWriteHandle, swapchainInfo] =
        builder.write(targetImage, RenderTaskBuilder::TextureWritePreset::TransferDst);

    RenderInput renderApi = co_await builder.finishDeclaration(outputWriteHandle);

    auto extent = Cory::glmu::to<Gpu::Extent3D>(swapchainInfo.size);

    // Get the actual resources from the FG resource manager
    auto windowImage = renderApi.resources->image(sourceImage);
    auto swapchainImage = renderApi.resources->image(outputWriteHandle);

    // Depending on the MSAA state of the window image, either resolve or blit to the swapchain
    if (colorInfo.sampleCount != Gpu::SampleCountFlagBits::Samples1Bit) {
        CO_CORE_ASSERT(swapchainInfo.sampleCount == Gpu::SampleCountFlagBits::Samples1Bit,
                       "Resolve destination must be single-sampled, got {}",
                       static_cast<uint32_t>(swapchainInfo.sampleCount));
        renderApi.cmd->resolveTexture(Gpu::TextureResolveOptions{
            .srcTexture = windowImage,
            .srcLayout = Gpu::TextureLayout::TransferSrcOptimal,
            .dstTexture = swapchainImage,
            .dstLayout = Gpu::TextureLayout::TransferDstOptimal,
            .regions = {Gpu::TextureResolveRegion{
                .srcSubresource = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit},
                .dstSubresource = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit},
                .extent = extent,
            }}});
    }
    else {
        // Blit the rendered image to the swapchain image
        renderApi.cmd->blitTexture(
            {.srcTexture = windowImage,
             .srcLayout = Gpu::TextureLayout::TransferSrcOptimal,
             .dstTexture = swapchainImage,
             .dstLayout = Gpu::TextureLayout::TransferDstOptimal,
             .regions = {Gpu::TextureBlitRegion{
                 .srcSubresource = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit},
                 .srcOffset = {},
                 .srcExtent = extent,
                 .dstSubresource = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit},
                 .dstOffset = {0, 0, 0},
                 .dstExtent = extent,
             }},
             .scalingFilter = Gpu::FilterMode::Linear});
    }
}
} // namespace Cory::StandardRenderTasks
