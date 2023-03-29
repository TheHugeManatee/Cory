#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/Semaphore.hpp>

#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/Fence.h>
#include <Magnum/Vk/Image.h>
#include <Magnum/Vk/ImageView.h>
#include <Magnum/Vk/Vulkan.h>

#include <glm/vec2.hpp>

#include <cstdint>
#include <vector>

namespace Cory {

struct SwapchainSupportDetails {
    static SwapchainSupportDetails query(Context &ctx, VkSurfaceKHR surface);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat() const;
    VkPresentModeKHR chooseSwapPresentMode() const;
    VkExtent2D chooseSwapExtent(VkExtent2D windowExtent) const;
    uint32_t chooseImageCount() const;

    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;

    std::vector<uint32_t> presentFamilies;
};

class Swapchain : public BasicVkObjectWrapper<VkSwapchainKHR> {
  public:
    Swapchain(Context &ctx,
              VkSurfaceKHR surface,
              VkSwapchainCreateInfoKHR createInfo,
              int32_t sampleCount);
    ~Swapchain();

    [[nodiscard]] auto &images() const noexcept { return images_; }
    [[nodiscard]] Magnum::Vk::PixelFormat colorFormat() const noexcept { return imageFormat_; }
    [[nodiscard]] auto &imageViews() noexcept { return imageViews_; }
    [[nodiscard]] glm::u32vec2 extent() const noexcept { return extent_; }
    [[nodiscard]] size_t size() const noexcept { return images_.size(); }
    [[nodiscard]] uint32_t maxFramesInFlight() const noexcept { return maxFramesInFlight_; };

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
    [[nodiscard]] FrameContext nextImage();

    /**
     * call vkQueuePresentKHR for the current frame. note the requirements that have to be fulfilled
     * for the synchronization objects of the passed @b fc.
     * present will wait for the semaphore @b fc.rendered for correct ordering.
     *
     * @see nextImage()
     */
    void present(FrameContext &fc);

  private:
    void createImageViews();
    void createSyncObjects();

  private:
    Context *ctx_{};

    // general information about the swapchain setup
    Magnum::Vk::PixelFormat imageFormat_{};
    int32_t sampleCount_{1};
    glm::u32vec2 extent_{};
    const uint32_t maxFramesInFlight_{};
    uint64_t nextFrameNumber_{};

    // these are images with memory owned by the swapchain
    std::vector<Magnum::Vk::Image> images_{};
    std::vector<Magnum::Vk::ImageView> imageViews_{};

    // for each frame in flight, we also keep a set of additional resources
    std::vector<Magnum::Vk::Fence> inFlightFences_{};
    std::vector<Magnum::Vk::Fence *> imageFences_{};
    std::vector<Semaphore> imageAcquired_{};
    std::vector<Semaphore> imageRendered_{};
    std::vector<Magnum::Vk::CommandBuffer> commandBuffers_{};
};

} // namespace Cory
