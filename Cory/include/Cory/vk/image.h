#pragma once

#include "resource.h"
#include "utils.h"

#include <glm.h>

#include <vulkan/vulkan.h>

namespace cory {
namespace vk {

class graphics_context;
class image_builder;

class image : public resource<image, std::shared_ptr<VkImage_T>> {
  public:
    // create from builder
    image(image_builder &builder);

    // initialize from existing VkImage object
    image(graphics_context &ctx_,
          std::shared_ptr<VkImage_T>& vk_resource_ptr,
          VkImageType image_type,
          VkFormat image_format,
          glm::uvec3 image_size,
          uint32_t image_mip_levels = 0,
          std::string_view name = {});

    [[nodiscard]] VkImageType type() const noexcept { return type_; }
    [[nodiscard]] const auto &size() const noexcept { return size_; }
    [[nodiscard]] auto format() const noexcept { return format_; }
    [[nodiscard]] auto mip_levels() const noexcept { return mip_levels_; }
    [[nodiscard]] VkImage get() const noexcept { return resource_.get(); }

  private:
    VkImageType type_;
    glm::uvec3 size_;
    VkFormat format_;
    uint32_t mip_levels_;
};

class image_builder {
  public:
    friend class image;
    image_builder(graphics_context &context)
        : ctx_{context}
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
    [[nodiscard]] image create() { return image(*this); }

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
    device_memory_usage usage_{device_memory_usage::eGpuOnly};
};

} // namespace vk
} // namespace cory