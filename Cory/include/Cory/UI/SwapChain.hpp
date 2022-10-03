#pragma once

#include <Cory/UI/Fence.hpp>

#include "Cory/Base/Common.hpp"
#include <Cory/Core/Semaphore.hpp>

#include <Magnum/Vk/Fence.h>
#include <Magnum/Vk/Image.h>
#include <Magnum/Vk/ImageView.h>

#include <glm/vec2.hpp>

#include <cstdint>
#include <vector>

namespace Cory {

class Context;

struct FrameContext {
    uint32_t index{};
    Magnum::Vk::ImageView *view{};
    Magnum::Vk::Fence *inFlight{};
    Semaphore *acquired{};
    Semaphore *rendered{};
    bool shouldRecreateSwapChain{false};
};

class SwapChain : public BasicVkObjectWrapper<VkSwapchainKHR>, NoCopy, NoMove {
  public:
    SwapChain(uint32_t max_frames_in_flight,
              Context &ctx,
              VkSurfaceKHR surface,
              VkSwapchainCreateInfoKHR createInfo);

    [[nodiscard]] auto &images() const noexcept { return images_; }
    [[nodiscard]] auto format() const noexcept { return imageFormat_; }
    [[nodiscard]] auto extent() const noexcept { return extent_; }
    [[nodiscard]] auto &views() const noexcept { return imageViews_; }
    [[nodiscard]] auto size() const noexcept { return images_.size(); }

    /**
     * acquire the next image. this method will obtain a SwapChain image index from the underlying
     * SwapChain. it will then wait for work on the image from a previous frame to be completed by
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

  private:
    Context &ctx_;

    std::vector<Magnum::Vk::Image> images_{};
    Magnum::Vk::PixelFormat imageFormat_{};
    glm::uvec2 extent_{};
    std::vector<Magnum::Vk::ImageView> imageViews_{};

    // manage frame resources currently in flight
    const uint32_t maxFramesInFlight_;
    uint32_t nextFrameInFlight_{};
    std::vector<Magnum::Vk::Fence> inFlightFences_{};
    std::vector<Magnum::Vk::Fence *> imageFences_{};
    std::vector<Semaphore> imageAcquired_{};
    std::vector<Semaphore> imageRendered_{};
};

} // namespace Cory
