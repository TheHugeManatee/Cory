#pragma once

#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>

#include <optional>

namespace Cory::StandardRenderTasks {

struct ClearPassOutputs {
    TransientTextureHandle color;
    std::optional<TransientTextureHandle> depth;
};

/**
 * @brief Clear color/depth attachments via render-pass loadOp clear.
 *
 * This is intended to be used at frame start to establish deterministic attachments before
 * downstream compute or raster passes.
 */
RenderTaskDeclaration<ClearPassOutputs>
clearAttachments(RenderTaskBuilder builder,
                 TransientTextureHandle colorTarget,
                 std::optional<TransientTextureHandle> depthTarget = std::nullopt,
                 Gpu::ColorClearValue clearColor = Gpu::ColorClearValue{0.0f, 0.0f, 0.0f, 1.0f},
                 Gpu::DepthStencilClearValue clearDepth = Gpu::DepthStencilClearValue{1.0f, 0});

/**
 * @brief Copy one color image into another for downstream usage (e.g. presentation).
 *
 * This task supports same-extent color images and chooses the transfer operation automatically:
 * - If sample counts are equal, a direct texture copy is scheduled.
 * - If source is multisampled and target is single-sampled, a resolve is scheduled.
 *
 * The task output is the written target image handle.
 */
RenderTaskDeclaration<TransientTextureHandle> copyToTarget(RenderTaskBuilder builder,
                                                           TransientTextureHandle sourceImage,
                                                           TransientTextureHandle targetImage);

} // namespace Cory::StandardRenderTasks
