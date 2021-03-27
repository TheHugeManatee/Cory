#pragma once

#include "fence.h"
#include "image.h"
#include "image_view.h"

#include "misc.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace cory {
namespace vk {

class graphics_context;
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
    swapchain(uint32_t max_frames_in_flight, graphics_context &ctx, swapchain_builder &builder);

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
    graphics_context &ctx_;

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

class swapchain_builder {
  public:
    friend class swapchain;
    swapchain_builder(graphics_context &context)
        : ctx_{context}
    {
    }

    swapchain_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    swapchain_builder &flags(VkSwapchainCreateFlagsKHR flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    swapchain_builder &surface(VkSurfaceKHR surface) noexcept
    {
        info_.surface = surface;
        return *this;
    }

    swapchain_builder &min_image_count(uint32_t minImageCount) noexcept
    {
        info_.minImageCount = minImageCount;
        return *this;
    }

    swapchain_builder &image_format(VkFormat imageFormat) noexcept
    {
        info_.imageFormat = imageFormat;
        return *this;
    }

    swapchain_builder &image_color_space(VkColorSpaceKHR imageColorSpace) noexcept
    {
        info_.imageColorSpace = imageColorSpace;
        return *this;
    }

    swapchain_builder &image_extent(glm::uvec2 imageExtent) noexcept
    {
        info_.imageExtent = VkExtent2D{imageExtent.x, imageExtent.y};
        return *this;
    }

    swapchain_builder &image_array_layers(uint32_t imageArrayLayers) noexcept
    {
        info_.imageArrayLayers = imageArrayLayers;
        return *this;
    }

    swapchain_builder &image_usage(VkImageUsageFlags imageUsage) noexcept
    {
        info_.imageUsage = imageUsage;
        return *this;
    }

    swapchain_builder &image_sharing_mode(VkSharingMode imageSharingMode) noexcept
    {
        info_.imageSharingMode = imageSharingMode;
        return *this;
    }

    swapchain_builder &
    queue_family_indices(const std::vector<uint32_t> &queueFamilyIndices) noexcept
    {
        queue_family_indices_ = queueFamilyIndices;
        return *this;
    }

    swapchain_builder &pre_transform(VkSurfaceTransformFlagBitsKHR preTransform) noexcept
    {
        info_.preTransform = preTransform;
        return *this;
    }

    swapchain_builder &composite_alpha(VkCompositeAlphaFlagBitsKHR compositeAlpha) noexcept
    {
        info_.compositeAlpha = compositeAlpha;
        return *this;
    }

    swapchain_builder &present_mode(VkPresentModeKHR presentMode) noexcept
    {
        info_.presentMode = presentMode;
        return *this;
    }

    swapchain_builder &clipped(VkBool32 clipped) noexcept
    {
        info_.clipped = clipped;
        return *this;
    }

    swapchain_builder &old_swapchain(VkSwapchainKHR oldSwapchain) noexcept
    {
        info_.oldSwapchain = oldSwapchain;
        return *this;
    }

    swapchain_builder &max_frames_in_flight(uint32_t max_frames) noexcept
    {
        max_frames_in_flight_ = max_frames;
        return *this;
    }

    [[nodiscard]] swapchain create()
    {
        info_.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_indices_.size());
        info_.pQueueFamilyIndices = queue_family_indices_.data();
        return swapchain{max_frames_in_flight_, ctx_, *this};
    };

  private:
    graphics_context &ctx_;
    VkSwapchainCreateInfoKHR info_{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .minImageCount = 3,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    std::vector<uint32_t> queue_family_indices_;
    uint32_t max_frames_in_flight_{2};
};

} // namespace vk
} // namespace cory