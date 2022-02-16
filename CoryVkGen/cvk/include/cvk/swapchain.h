#pragma once

#include "fence.h"
#include "image.h"
#include "image_view.h"
#include "semaphore.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace cvk {

class context;
class swapchain_builder;

struct frame_context {
    uint32_t index;
    image_view view;
    fence in_flight;
    semaphore acquired;
    semaphore rendered;
    bool should_recreate_swapchain;
};

class swapchain {
  public:
    swapchain(uint32_t max_frames_in_flight, context &ctx, swapchain_builder &builder);

    [[nodiscard]] auto get() noexcept { return swapchain_ptr_.get(); }
    [[nodiscard]] auto &images() const noexcept { return images_; }
    [[nodiscard]] auto format() const noexcept { return image_format_; }
    [[nodiscard]] auto extent() const noexcept { return extent_; }
    [[nodiscard]] auto &views() const noexcept { return image_views_; }
    [[nodiscard]] auto size() const noexcept { return images_.size(); }

    /**
     * acquire the next image. this method will obtain a swapchain image index from the underlying
     * swapchain. it will then wait for work on the image from a previous frame to be completed by
     * waiting for the corresponding fence.
     *
     * upon acquiring the next image through this method and before calling the corresponding
     * present(), a client application MUST:
     *  - schedule work that outputs to the image to wait for the `acquired` semaphore (at least the
     *    COLOR_ATTACHMENT_OUTPUT stage)
     *  - signal the `rendered` semaphore with the last command buffer that writes to the image
     *  - signal the `in_flight` fence when submitting the last command buffer
     */
    [[nodiscard]] frame_context next_image();

    /**
     * call vkQueuePresentKHR for the current frame. note the requirements that have to be fulfilled
     * for the synchronization objects of the passed @b fc.
     * present will wait for the semaphore @b fc.rendered for correct ordering.
     *
     * @see next_image()
     */
    [[nodiscard]] void present(frame_context &fc);

  private:
    void create_image_views();

  private:
    context &ctx_;

    std::vector<image> images_{};
    VkFormat image_format_{};
    glm::uvec2 extent_{};
    std::vector<image_view> image_views_{};

    std::shared_ptr<VkSwapchainKHR_T> swapchain_ptr_;

    // manage frame resources currently in flight
    const uint32_t max_frames_in_flight_;
    uint32_t next_frame_in_flight_{};
    std::vector<fence> in_flight_fences_{};
    std::vector<fence> image_fences_{};
    std::vector<semaphore> image_acquired_{};
    std::vector<semaphore> image_rendered_{};
};

} // namespace cvk
