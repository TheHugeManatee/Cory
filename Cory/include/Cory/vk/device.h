#pragma once

#include "queue.h"

#include <vulkan/vulkan.h>

#include <vector>
#include <memory>

namespace cory::vk {

using device = std::shared_ptr<VkDevice_T>;

class device_builder {
  public:
    device_builder(VkPhysicalDevice physical_device)
        : physical_device_{physical_device}
    {
    }

    [[nodiscard]] device_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    [[nodiscard]] device_builder &flags(VkDeviceCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    [[nodiscard]] device_builder &
    queue_create_infos(std::vector<queue_builder> queueCreateInfos) noexcept
    {
        queue_builders_ = queueCreateInfos;
        return *this;
    }

    [[nodiscard]] device_builder &
    enabled_layer_names(std::vector<const char *> enabledLayerNames) noexcept
    {
        enabled_layer_names_ = enabledLayerNames;
        return *this;
    }

    [[nodiscard]] device_builder &
    enabled_extension_names(std::vector<const char *> enabledExtensionNames) noexcept
    {
        enabled_extension_names_ = enabledExtensionNames;
        return *this;
    }

    [[nodiscard]] device_builder &
    enabled_features(VkPhysicalDeviceFeatures enabledFeatures) noexcept
    {
        enabled_features_ = enabledFeatures;
        return *this;
    }

    [[nodiscard]] device create();

  private:
    VkPhysicalDevice physical_device_;
    VkDeviceCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    };
    std::vector<queue_builder> queue_builders_;
    std::vector<const char *> enabled_extension_names_;
    std::vector<const char *> enabled_layer_names_;
    VkPhysicalDeviceFeatures enabled_features_;
};
}