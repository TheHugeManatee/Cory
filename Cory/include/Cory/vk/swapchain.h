#pragma once

#include <vulkan/vulkan.h>

#include <memory>

namespace cory {
namespace vk {

    class graphics_context;

class swapchain {
  public:
    swapchain(std::shared_ptr<VkSwapchainKHR_T> swapchain_ptr)
        : swapchain_ptr_{swapchain_ptr} {};
  
    private:
        std::shared_ptr<VkSwapchainKHR_T> swapchain_ptr_;
};

class swapchain_builder {
  public:
    friend class graphics_context;
    swapchain_builder(graphics_context &context)
        : ctx_{context}
    {
    }

    [[nodiscard]] swapchain_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    [[nodiscard]] swapchain_builder &flags(VkSwapchainCreateFlagsKHR flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    [[nodiscard]] swapchain_builder &surface(VkSurfaceKHR surface) noexcept
    {
        info_.surface = surface;
        return *this;
    }

    [[nodiscard]] swapchain_builder &min_image_count(uint32_t minImageCount) noexcept
    {
        info_.minImageCount = minImageCount;
        return *this;
    }

    [[nodiscard]] swapchain_builder &image_format(VkFormat imageFormat) noexcept
    {
        info_.imageFormat = imageFormat;
        return *this;
    }

    [[nodiscard]] swapchain_builder &image_color_space(VkColorSpaceKHR imageColorSpace) noexcept
    {
        info_.imageColorSpace = imageColorSpace;
        return *this;
    }

    [[nodiscard]] swapchain_builder &image_extent(VkExtent2D imageExtent) noexcept
    {
        info_.imageExtent = imageExtent;
        return *this;
    }

    [[nodiscard]] swapchain_builder &image_array_layers(uint32_t imageArrayLayers) noexcept
    {
        info_.imageArrayLayers = imageArrayLayers;
        return *this;
    }

    [[nodiscard]] swapchain_builder &image_usage(VkImageUsageFlags imageUsage) noexcept
    {
        info_.imageUsage = imageUsage;
        return *this;
    }

    [[nodiscard]] swapchain_builder &image_sharing_mode(VkSharingMode imageSharingMode) noexcept
    {
        info_.imageSharingMode = imageSharingMode;
        return *this;
    }

    [[nodiscard]] swapchain_builder &
    queue_family_index_count(uint32_t queueFamilyIndexCount) noexcept
    {
        info_.queueFamilyIndexCount = queueFamilyIndexCount;
        return *this;
    }

    [[nodiscard]] swapchain_builder &
    queue_family_indices(const uint32_t *pQueueFamilyIndices) noexcept
    {
        info_.pQueueFamilyIndices = pQueueFamilyIndices;
        return *this;
    }

    [[nodiscard]] swapchain_builder &
    pre_transform(VkSurfaceTransformFlagBitsKHR preTransform) noexcept
    {
        info_.preTransform = preTransform;
        return *this;
    }

    [[nodiscard]] swapchain_builder &
    composite_alpha(VkCompositeAlphaFlagBitsKHR compositeAlpha) noexcept
    {
        info_.compositeAlpha = compositeAlpha;
        return *this;
    }

    [[nodiscard]] swapchain_builder &present_mode(VkPresentModeKHR presentMode) noexcept
    {
        info_.presentMode = presentMode;
        return *this;
    }

    [[nodiscard]] swapchain_builder &clipped(VkBool32 clipped) noexcept
    {
        info_.clipped = clipped;
        return *this;
    }

    [[nodiscard]] swapchain_builder &old_swapchain(VkSwapchainKHR oldSwapchain) noexcept
    {
        info_.oldSwapchain = oldSwapchain;
        return *this;
    }

    [[nodiscard]] swapchain create();

  private:
    graphics_context &ctx_;
    VkSwapchainCreateInfoKHR info_{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    };
};


}
}