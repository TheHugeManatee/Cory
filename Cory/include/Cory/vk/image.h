#pragma once

#include "resource.h"
#include "utils.h"

#include <glm.h>

#include <vulkan/vulkan.h>

namespace cory {
namespace vk {

class graphics_context;

class image : public resource<image, std::shared_ptr<struct VkImage_T>> {
  public:
    friend class graphics_context;

    [[nodiscard]] VkImageType type() const noexcept { return type_; }
    [[nodiscard]] const auto &size() const noexcept { return size_; }
    [[nodiscard]] auto format() const noexcept { return format_; }

  private:
    /// private constructor - create through @a graphics_context
    image(graphics_context &context,
          resource_ptr_t ptr,
          std::string_view name,
          VkImageType type,
          glm::uvec3 size,
          VkFormat format)
        : resource(context, ptr, name)
        , type_{type}
        , size_{size}
        , format_{format}
    {
    }

    VkImageType type_;
    glm::uvec3 size_;
    VkFormat format_;
};

class image_builder {
  public:
    friend class graphics_context;
    image_builder(graphics_context &context)
        : ctx_{context}
    {
    }

    [[nodiscard]] image_builder &p_next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    [[nodiscard]] image_builder &flags(VkImageCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    [[nodiscard]] image_builder &image_type(VkImageType imageType) noexcept
    {
        info_.imageType = imageType;
        return *this;
    }

    [[nodiscard]] image_builder &format(VkFormat format) noexcept
    {
        info_.format = format;
        return *this;
    }

    [[nodiscard]] image_builder &extent(glm::uvec3 extent) noexcept
    {
        info_.extent = VkExtent3D{extent.x, extent.y, extent.z};
        return *this;
    }

    [[nodiscard]] image_builder &mip_levels(uint32_t mipLevels) noexcept
    {
        info_.mipLevels = mipLevels;
        return *this;
    }

    [[nodiscard]] image_builder &array_layers(uint32_t arrayLayers) noexcept
    {
        info_.arrayLayers = arrayLayers;
        return *this;
    }

    [[nodiscard]] image_builder &samples(VkSampleCountFlagBits samples) noexcept
    {
        info_.samples = samples;
        return *this;
    }

    [[nodiscard]] image_builder &tiling(VkImageTiling tiling) noexcept
    {
        info_.tiling = tiling;
        return *this;
    }

    [[nodiscard]] image_builder &sharing_mode(VkSharingMode sharingMode) noexcept
    {
        info_.sharingMode = sharingMode;
        return *this;
    }

    [[nodiscard]] image_builder &queue_family_index_count(uint32_t queueFamilyIndexCount) noexcept
    {
        info_.queueFamilyIndexCount = queueFamilyIndexCount;
        return *this;
    }

    [[nodiscard]] image_builder &
    p_queue_family_indices(const uint32_t *pQueueFamilyIndices) noexcept
    {
        info_.pQueueFamilyIndices = pQueueFamilyIndices;
        return *this;
    }

    [[nodiscard]] image_builder &initial_layout(VkImageLayout initialLayout) noexcept
    {
        info_.initialLayout = initialLayout;
        return *this;
    }

    [[nodiscard]] image_builder &usage(VkImageUsageFlags usage) noexcept
    {
        info_.usage = usage;
        return *this;
    }

    [[nodiscard]] image_builder &memory_usage(device_memory_usage usage) noexcept
    {
        memory_usage_ = usage;
        return *this;
    }

    [[nodiscard]] image_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }

    [[nodiscard]] operator image();

    [[nodiscard]] image create();

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
    device_memory_usage memory_usage_{device_memory_usage::eGpuOnly};
};

} // namespace vk
} // namespace cory