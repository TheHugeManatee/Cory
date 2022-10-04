#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/RenderCore/Semaphore.hpp>

#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/Fence.h>
#include <Magnum/Vk/Image.h>
#include <Magnum/Vk/ImageView.h>
#include <Magnum/Vk/Vulkan.h>

#include <glm/vec2.hpp>

#include <cstdint>
#include <vector>

namespace Cory {

class Context;

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

struct FrameContext {
    uint32_t index{};
    uint64_t frameNumber{};
    bool shouldRecreateSwapchain{false};
    Magnum::Vk::ImageView *colorView{};
    Magnum::Vk::ImageView *depthView{};
    Magnum::Vk::Fence *inFlight{};
    Semaphore *acquired{};
    Semaphore *rendered{};
    Magnum::Vk::CommandBuffer commandBuffer{Magnum::NoCreate};
};

class Swapchain : public BasicVkObjectWrapper<VkSwapchainKHR> {
  public:
    Swapchain(Context &ctx, VkSurfaceKHR surface, VkSwapchainCreateInfoKHR createInfo);

    [[nodiscard]] auto &images() const noexcept { return images_; }
    [[nodiscard]] auto colorFormat() const noexcept { return imageFormat_; }
    [[nodiscard]] auto &imageViews() noexcept { return imageViews_; }
    [[nodiscard]] auto &depthImages() const noexcept { return depthImages_; }
    [[nodiscard]] auto depthFormat() const noexcept { return depthFormat_; }
    [[nodiscard]] auto &depthViews() noexcept { return depthImageViews_; }
    [[nodiscard]] auto extent() const noexcept { return extent_; }
    [[nodiscard]] auto size() const noexcept { return images_.size(); }

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
    void createDepthResources();
    void createImageViews();
    void createSyncObjects();

  private:
    Context *ctx_{};

    // general information about the swapchain setup
    Magnum::Vk::PixelFormat imageFormat_{};
    Magnum::Vk::PixelFormat depthFormat_{};
    glm::u32vec2 extent_{};
    const uint32_t maxFramesInFlight_{};
    uint64_t nextFrameNumber_{};

    // these are images with memory owned by the swapchain
    std::vector<Magnum::Vk::Image> images_{};
    std::vector<Magnum::Vk::ImageView> imageViews_{};
    // these are created separately through createDepthResources()
    std::vector<Magnum::Vk::Image> depthImages_{};
    std::vector<Magnum::Vk::ImageView> depthImageViews_{};

    // for each frame in flight, we also keep a set of additional resources
    std::vector<Magnum::Vk::Fence> inFlightFences_{};
    std::vector<Magnum::Vk::Fence *> imageFences_{};
    std::vector<Semaphore> imageAcquired_{};
    std::vector<Semaphore> imageRendered_{};
};

} // namespace Cory
