#pragma once

#include <vulkan/vulkan.h>

#include <type_traits>
#include <vector>

namespace cory {
namespace vk {

enum class device_memory_usage : int /*std::underlying_type<VmaMemoryUsage>::type*/ {
    eUnknown = 0 /*VMA_MEMORY_USAGE_UNKNOWN*/,     ///< should not be used
    eGpuOnly = 1 /*VMA_MEMORY_USAGE_GPU_ONLY*/,    ///< textures, images used as attachments
    eCpuOnly = 2 /*VMA_MEMORY_USAGE_CPU_ONLY*/,    ///< staging buffers
    eCpuToGpu = 3 /*VMA_MEMORY_USAGE_CPU_TO_GPU*/, ///< dynamic resources, i.e. vertex/uniform
                                                   ///< buffers, dynamic textures
    eGpuToCpu = 4 /*VMA_MEMORY_USAGE_GPU_TO_CPU*/, ///< transform feedback, screenshots etc.
    eCpuCopy = 5 /*VMA_MEMORY_USAGE_CPU_COPY*/,    ///< staging custom paging/residency
    eGpuLazilyAllocated =
        6 /*VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED */ ///< transient attachment images,
                                                     ///< might not be available for
                                                     ///< desktop GPUs
};

#define VK_CHECKED_CALL(x, err)                                                                    \
    if ((x) != VK_SUCCESS) { throw std::runtime_error(#x " failed: " err); }

const std::vector<VkExtensionProperties> &extension_properties();

constexpr VkSampleCountFlagBits get_max_usable_sample_count(const VkPhysicalDeviceProperties &props) noexcept
{
    auto counts = VkSampleCountFlagBits(props.limits.framebufferColorSampleCounts &
                                        props.limits.framebufferDepthSampleCounts);

    if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
    if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_64_BIT;
    if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
    if (counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
    if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;

    return VK_SAMPLE_COUNT_1_BIT;
}

} // namespace vk
} // namespace cory