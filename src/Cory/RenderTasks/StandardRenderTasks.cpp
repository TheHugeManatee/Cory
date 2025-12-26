#include "StandardRenderTasks.hpp"

#include <Cory/Base/GlmUtils.hpp>
#include <Cory/Framegraph/FramegraphResourceManager.hpp>

#include <KDGpu/command_recorder.h>

namespace Cory::StandardRenderTasks {

RenderTaskDeclaration<TransientTextureHandle> resolve(RenderTaskBuilder builder,
                                                      TransientTextureHandle sourceImage,
                                                      TransientTextureHandle targetImage)
{
    auto colorInfo = builder.read(sourceImage, Sync::AccessType::TransferRead);
    auto [outputWriteHandle, swapchainInfo] =
        builder.write(targetImage, Sync::AccessType::TransferWrite);

    co_yield outputWriteHandle;
    RenderInput renderApi = co_await builder.finishDeclaration();

    auto extent = Cory::glmu::to<Gpu::Extent3D>(swapchainInfo.size);

    // Get the actual resources from the FG resource manager
    auto windowImage = renderApi.resources->image(sourceImage);
    auto swapchainImage = renderApi.resources->image(outputWriteHandle);

    // Depending on the MSAA state of the window image, either resolve or blit to the swapchain
    if (colorInfo.sampleCount != KDGpu::SampleCountFlagBits::Samples1Bit) {
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
             .scalingFilter = KDGpu::FilterMode::Linear});
    }
}
} // namespace Cory::StandardRenderTasks
