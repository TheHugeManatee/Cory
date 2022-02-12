#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace cvk {

struct physical_device {
    VkPhysicalDevice device;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    std::vector<VkQueueFamilyProperties> queue_family_properties;
    VkSampleCountFlagBits max_usable_sample_count;
};

}