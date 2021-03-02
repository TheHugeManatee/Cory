#pragma once

#include "graphics_context.h"
#include "resource.h"
#include "vk_utils.h"

#include <vulkan/vulkan.h>

namespace cory {
namespace vk {

class image_builder {
  public:
    friend class graphics_context;
    image_builder(graphics_context &context)
        : ctx_{context}
    {
    }

    [[nodiscard]] image_builder &p_next(const void *pNext)
    {
        info_.pNext = pNext;
        return *this;
    }

    [[nodiscard]] image_builder &flags(VkImageCreateFlags flags)
    {
        info_.flags = flags;
        return *this;
    }

    [[nodiscard]] image_builder &image_type(VkImageType imageType)
    {
        info_.imageType = imageType;
        return *this;
    }

    [[nodiscard]] image_builder &format(VkFormat format)
    {
        info_.format = format;
        return *this;
    }

    [[nodiscard]] image_builder &extent(glm::uvec3 extent)
    {
        info_.extent = VkExtent3D{extent.x, extent.y, extent.z};
        return *this;
    }

    [[nodiscard]] image_builder &mip_levels(uint32_t mipLevels)
    {
        info_.mipLevels = mipLevels;
        return *this;
    }

    [[nodiscard]] image_builder &array_layers(uint32_t arrayLayers)
    {
        info_.arrayLayers = arrayLayers;
        return *this;
    }

    [[nodiscard]] image_builder &samples(VkSampleCountFlagBits samples)
    {
        info_.samples = samples;
        return *this;
    }

    [[nodiscard]] image_builder &tiling(VkImageTiling tiling)
    {
        info_.tiling = tiling;
        return *this;
    }

    [[nodiscard]] image_builder &sharing_mode(VkSharingMode sharingMode)
    {
        info_.sharingMode = sharingMode;
        return *this;
    }

    [[nodiscard]] image_builder &queue_family_index_count(uint32_t queueFamilyIndexCount)
    {
        info_.queueFamilyIndexCount = queueFamilyIndexCount;
        return *this;
    }

    [[nodiscard]] image_builder &p_queue_family_indices(const uint32_t *pQueueFamilyIndices)
    {
        info_.pQueueFamilyIndices = pQueueFamilyIndices;
        return *this;
    }

    [[nodiscard]] image_builder &initial_layout(VkImageLayout initialLayout)
    {
        info_.initialLayout = initialLayout;
        return *this;
    }

    [[nodiscard]] image_builder &usage(device_memory_usage usage)
    {
        usage_ = usage;
        return *this;
    }
    [[nodiscard]] image_builder &name(std::string_view name)
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] operator image() { return ctx_.create_image(*this); }

    [[nodiscard]] image create() { return ctx_.create_image(*this); }

  private:
    graphics_context &ctx_;
    VkImageCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .extent = {1, 1, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
    };
    std::string_view name_;
    device_memory_usage usage_{VMA_MEMORY_USAGE_GPU_ONLY};
};

class buffer_builder {
  public:
    friend class graphics_context;
    buffer_builder(graphics_context &context)
        : ctx_{context}
    {
    }

    [[nodiscard]] buffer_builder &p_next(const void *pNext)
    {
        info_.pNext = pNext;
        return *this;
    }

    [[nodiscard]] buffer_builder &flags(VkBufferCreateFlags flags)
    {
        info_.flags = flags;
        return *this;
    }

    [[nodiscard]] buffer_builder &size(VkDeviceSize size)
    {
        info_.size = size;
        return *this;
    }

    [[nodiscard]] buffer_builder &sharing_mode(VkSharingMode sharingMode)
    {
        info_.sharingMode = sharingMode;
        return *this;
    }

    [[nodiscard]] buffer_builder &queue_family_index_count(uint32_t queueFamilyIndexCount)
    {
        info_.queueFamilyIndexCount = queueFamilyIndexCount;
        return *this;
    }

    [[nodiscard]] buffer_builder &p_queue_family_indices(const uint32_t *pQueueFamilyIndices)
    {
        info_.pQueueFamilyIndices = pQueueFamilyIndices;
        return *this;
    }

    [[nodiscard]] buffer_builder &usage(device_memory_usage usage)
    {
        usage_ = usage;
        return *this;
    }
    [[nodiscard]] buffer_builder &name(std::string_view name)
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] operator buffer() { return ctx_.create_buffer(*this); }

    [[nodiscard]] buffer create() { return ctx_.create_buffer(*this); }

  private:
    graphics_context &ctx_;
    VkBufferCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    };
    std::string_view name_;
    device_memory_usage usage_{VMA_MEMORY_USAGE_GPU_ONLY};
};

} // namespace vk
} // namespace cory