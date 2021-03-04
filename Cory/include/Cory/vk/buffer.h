#pragma once

#include "resource.h"
#include "utils.h"

#include <vulkan/vulkan.h>

namespace cory {
namespace vk {
class graphics_context;

class buffer : public resource<buffer, std::shared_ptr<struct VkBuffer_T>> {
  public:
    friend class graphics_context;

  private:
    /// private constructor - create through @a graphics_context
    buffer(graphics_context &context, resource_ptr_t ptr, std::string_view name)
        : resource(context, ptr, name)
    {
    }
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
    [[nodiscard]] operator buffer();

    [[nodiscard]] buffer create();

  private:
    graphics_context &ctx_;
    VkBufferCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    };
    std::string_view name_;
    device_memory_usage usage_{device_memory_usage::eGpuOnly};
};

} // namespace vk
} // namespace cory