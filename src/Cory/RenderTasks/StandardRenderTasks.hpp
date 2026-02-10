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
 * @brief A render task that resolves the window color image to the swapchain image.
 *
 * This render task takes single- or multisampled image and schedules a resolve or blit operation
 * to copy its contents to the given target image.
 * If the input image is multisampled, it performs a resolve operation to convert it to a
 * single-sampled image suitable for presentation.
 * If the input image is already single-sampled, it performs a blit operation.
 *
 * The task output is the updated/written to target image, which can then be used further e.g. for
 * presentation.
 */
RenderTaskDeclaration<TransientTextureHandle> resolve(RenderTaskBuilder builder,
                                                      TransientTextureHandle sourceImage,
                                                      TransientTextureHandle targetImage);

} // namespace Cory::StandardRenderTasks
