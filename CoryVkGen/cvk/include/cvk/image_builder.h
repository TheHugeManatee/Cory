#pragma once

#include "image.h"

#include <vulkan/vulkan.h>

namespace cvk {

class context;

class image_builder {
  public:
    friend class image;
    image_builder(context &ctx)
        : ctx_{ctx}
    {
    }

    image_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    image_builder &flags(VkImageCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    image_builder &image_type(VkImageType imageType) noexcept
    {
        info_.imageType = imageType;
        return *this;
    }

    image_builder &format(VkFormat format) noexcept
    {
        info_.format = format;
        return *this;
    }

    /**
     * 1D texture dimension - sets imageType accordingly
     */
    image_builder &extent(uint32_t extent) noexcept
    {
        info_.imageType = VK_IMAGE_TYPE_1D;
        info_.extent = VkExtent3D{extent, 1, 1};
        return *this;
    }
    /**
     * 2D texture dimension - sets imageType accordingly
     */
    image_builder &extent(glm::uvec2 extent) noexcept
    {
        info_.imageType = VK_IMAGE_TYPE_2D;
        info_.extent = VkExtent3D{extent.x, extent.y, 1};
        return *this;
    }
    /**
     * 3D texture dimension - sets imageType accordingly
     */
    image_builder &extent(glm::uvec3 extent) noexcept
    {
        info_.imageType = VK_IMAGE_TYPE_3D;
        info_.extent = VkExtent3D{extent.x, extent.y, extent.z};
        return *this;
    }

    image_builder &mip_levels(uint32_t mipLevels) noexcept
    {
        info_.mipLevels = mipLevels;
        return *this;
    }

    image_builder &array_layers(uint32_t arrayLayers) noexcept
    {
        info_.arrayLayers = arrayLayers;
        return *this;
    }

    image_builder &samples(VkSampleCountFlagBits samples) noexcept
    {
        info_.samples = samples;
        return *this;
    }

    image_builder &tiling(VkImageTiling tiling) noexcept
    {
        info_.tiling = tiling;
        return *this;
    }

    image_builder &usage(VkImageUsageFlags usage) noexcept
    {
        info_.usage = usage;
        return *this;
    }

    image_builder &sharing_mode(VkSharingMode sharingMode) noexcept
    {
        info_.sharingMode = sharingMode;
        return *this;
    }

    image_builder &queue_family_index_count(uint32_t queueFamilyIndexCount) noexcept
    {
        info_.queueFamilyIndexCount = queueFamilyIndexCount;
        return *this;
    }

    image_builder &queue_family_indices(const uint32_t *pQueueFamilyIndices) noexcept
    {
        info_.pQueueFamilyIndices = pQueueFamilyIndices;
        return *this;
    }

    image_builder &initial_layout(VkImageLayout initialLayout) noexcept
    {
        info_.initialLayout = initialLayout;
        return *this;
    }

    image_builder &memory_usage(device_memory_usage usage) noexcept
    {
        usage_ = usage;
        return *this;
    }
    image_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] image create();

  private:
    context &ctx_;
    VkImageCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .extent = {1, 1, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
    };
    std::string_view name_;
    device_memory_usage usage_{device_memory_usage::eGpuOnly};
};
} // namespace cvk