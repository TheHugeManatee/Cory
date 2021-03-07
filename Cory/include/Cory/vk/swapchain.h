#pragma once

#include "image.h"
#include "image_view.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace cory {
namespace vk {

class graphics_context;
class swapchain_builder;

class swapchain {
  public:
    swapchain(graphics_context &ctx, swapchain_builder &builder);

    [[nodiscard]] auto get() noexcept { return swapchain_ptr_.get(); }
    [[nodiscard]] auto &images() noexcept { return images_; }
    [[nodiscard]] auto format() const noexcept { return image_format_; }
    [[nodiscard]] auto extent() const noexcept { return extent_; }
    [[nodiscard]] auto &views() noexcept { return image_views_; }
    [[nodiscard]] auto size() const noexcept { return images_.size(); }

  private:
    void create_image_views();

  private:
    graphics_context &ctx_;

    std::vector<image> images_{};
    VkFormat image_format_{};
    VkExtent2D extent_{};
    std::vector<image_view> image_views_{};

    std::shared_ptr<VkSwapchainKHR_T> swapchain_ptr_;
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

    swapchain_builder &
    pre_transform(VkSurfaceTransformFlagBitsKHR preTransform) noexcept
    {
        info_.preTransform = preTransform;
        return *this;
    }

    swapchain_builder &
    composite_alpha(VkCompositeAlphaFlagBitsKHR compositeAlpha) noexcept
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

    [[nodiscard]] swapchain create()
    {
        info_.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_indices_.size());
        info_.pQueueFamilyIndices = queue_family_indices_.data();
        return swapchain{ctx_, *this};
    };

  private:
    graphics_context &ctx_;
    VkSwapchainCreateInfoKHR info_{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .clipped = VK_TRUE,
    };
    std::vector<uint32_t> queue_family_indices_;
};

} // namespace vk
} // namespace cory