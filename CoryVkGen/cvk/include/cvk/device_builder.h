#pragma once

#include "device.h"
#include "physical_device.h"

#include <array>
#include <set>

namespace cvk {

class device_builder {
  public:
    device_builder(const physical_device &deviceInfo)
        : device_info_{deviceInfo}
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

    // add a queue to create
    [[nodiscard]] device_builder &add_queues(const std::set<uint32_t> &familyIndices);

    [[nodiscard]] device_builder &
    enabled_layer_names(std::vector<const char *> enabledLayerNames) noexcept
    {
        enabled_layer_names_ = std::move(enabledLayerNames);
        return *this;
    }

    [[nodiscard]] device_builder &
    enabled_extension_names(std::vector<const char *> enabledExtensionNames) noexcept
    {
        enabled_extension_names_ = std::move(enabledExtensionNames);
        return *this;
    }

    [[nodiscard]] device_builder &
    enabled_features(VkPhysicalDeviceFeatures enabledFeatures) noexcept
    {
        enabled_features_ = enabledFeatures;
        return *this;
    }

    [[nodiscard]] cvk::device create();

  private:
    const physical_device &device_info_;
    VkDeviceCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    };
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos_;
    std::array<float, 16>
        queue_priorities_{}; // we assume that we don't need more than 16 queues :)
    std::vector<const char *> enabled_extension_names_;
    std::vector<const char *> enabled_layer_names_;
    VkPhysicalDeviceFeatures enabled_features_{};
};

} // namespace cvk