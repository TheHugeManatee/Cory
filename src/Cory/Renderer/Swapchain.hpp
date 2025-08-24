#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Gpu.hpp>
#include <Cory/Renderer/Semaphore.hpp>

#include <KDGpu/gpu_core.h>

#include <glm/vec2.hpp>

#include <expected>

namespace Cory {

struct SwapchainCreateInfo {
    std::string label;
    glm::u32vec2 size;
    Gpu::SampleCountFlagBits samples;
};

enum class SwapchainError {
    OutOfDate, ///< The swapchain is out of date and needs to be recreated
    Lost,      ///< The swapchain has been lost and cannot be used anymore
    Unknown,   ///< An unknown error occurred
};

/**
 * @brief Swapchain wrapper that provides a high-level interface to the underlying Gpu::Swapchain.
 *
 * This class manages image acquisition and presentation as well as the basic view resources
 * required for a frame.
 *
 * It wraps the underlying swapchain, but also allocates and manages additional resources  for each
 * frame, such as:
 *  - multisampled color and depth images
 *  - semaphores for synchronization
 *  - fences to wait for the GPU to finish work on the swapchain image
 *
 */
class Swapchain {
  public:
    Swapchain(Context &ctx, const Gpu::Surface &surface, SwapchainCreateInfo createInfo);
    ~Swapchain();

    [[nodiscard]] Gpu::Format colorFormat() const noexcept;
    [[nodiscard]] Gpu::Format depthFormat() const noexcept;
    [[nodiscard]] glm::u32vec2 extent() const noexcept;
    [[nodiscard]] size_t size() const noexcept; // number of images in the swapchain

    /**
     * acquire the next image. this method will obtain a Swapchain image index from the underlying
     * Swapchain. it will then wait for work on the image from a previous frame to be completed by
     * waiting for the corresponding fence.
     *
     * upon acquiring the next image through this method and before calling the corresponding
     * present(), a client application MUST:
     *  - schedule work that outputs to the image to wait for the `acquired` semaphore (at least the
     *    COLOR_ATTACHMENT_OUTPUT stage)
     *  - signal the `rendered` semaphore with the last command buffer that writes to the image
     *  - signal the `in_flight` fence when submitting the last command buffer
     */
    [[nodiscard]] std::expected<FrameContext, SwapchainError> nextImage();

    /**
     * call vkQueuePresentKHR for the current frame. note the requirements that have to be fulfilled
     * for the synchronization objects of the passed @b frameCtx.
     *
     * This will submit the current render commands stored in frameCtx.commandBuffer to the
     * queue, and then present the swapchain image to the surface.
     *
     * Command submission will wait for the @b frameCtx.acquired semaphore to execute the commands
     * only when the swapchain image is actually available. Present will wait for the semaphore
     * @b frameCtx.rendered for correct ordering.
     *
     * @see nextImage()
     */
    void present(FrameContext &frameCtx);

  private:
    friend struct SwapchainPrivate;
    std::unique_ptr<SwapchainPrivate> data_;
};

} // namespace Cory
