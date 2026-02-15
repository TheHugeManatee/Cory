#include "StandardRenderTasks.hpp"

#include <Cory/Base/GlmUtils.hpp>
#include <Cory/Base/Log.hpp>
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

RenderTaskDeclaration<TransientTextureHandle> copyToTarget(RenderTaskBuilder builder,
                                                           TransientTextureHandle sourceImage,
                                                           TransientTextureHandle targetImage)
{
    auto sourceInfo = builder.read(sourceImage, RenderTaskBuilder::TextureReadPreset::TransferSrc);
    auto [outputWriteHandle, targetInfo] =
        builder.write(targetImage, RenderTaskBuilder::TextureWritePreset::TransferDst);

    RenderInput renderApi = co_await builder.finishDeclaration(outputWriteHandle);

    CO_CORE_ASSERT(sourceInfo.size == targetInfo.size,
                   "copyToTarget source and target size mismatch: {}x{} vs {}x{}",
                   sourceInfo.size.x,
                   sourceInfo.size.y,
                   targetInfo.size.x,
                   targetInfo.size.y);

    const auto extent = Cory::glmu::to<Gpu::Extent3D>(targetInfo.size);

    // Get the actual resources from the FG resource manager
    const auto srcImage = renderApi.resources->image(sourceImage);
    const auto dstImage = renderApi.resources->image(outputWriteHandle);

    if (sourceInfo.sampleCount != targetInfo.sampleCount) {
        CO_CORE_ASSERT(sourceInfo.sampleCount != Gpu::SampleCountFlagBits::Samples1Bit &&
                           targetInfo.sampleCount == Gpu::SampleCountFlagBits::Samples1Bit,
                       "Unsupported sample count conversion in copyToTarget: {} -> {}",
                       static_cast<uint32_t>(sourceInfo.sampleCount),
                       static_cast<uint32_t>(targetInfo.sampleCount));
        renderApi.cmd->resolveTexture(Gpu::TextureResolveOptions{
            .srcTexture = srcImage,
            .srcLayout = Gpu::TextureLayout::TransferSrcOptimal,
            .dstTexture = dstImage,
            .dstLayout = Gpu::TextureLayout::TransferDstOptimal,
            .regions = {Gpu::TextureResolveRegion{
                .srcSubresource = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit},
                .dstSubresource = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit},
                .extent = extent,
            }}});
    }
    else {
        renderApi.cmd->copyTextureToTexture(
            {.srcTexture = srcImage,
             .srcLayout = Gpu::TextureLayout::TransferSrcOptimal,
             .dstTexture = dstImage,
             .dstLayout = Gpu::TextureLayout::TransferDstOptimal,
             .regions = {Gpu::TextureCopyRegion{
                 .srcSubresource = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit},
                 .srcOffset = {0, 0, 0},
                 .dstSubresource = {.aspectMask = Gpu::TextureAspectFlagBits::ColorBit},
                 .dstOffset = {0, 0, 0},
                 .extent = extent,
             }}});
    }
}
} // namespace Cory::StandardRenderTasks
