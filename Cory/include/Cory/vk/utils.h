#pragma once

#include <fmt/format.h>
#include <vulkan/vulkan.h>

#include <optional>
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
    do {                                                                                           \
        if (auto code = (x); code != VK_SUCCESS) {                                                 \
            throw std::runtime_error(fmt::format(#x " failed with {}: {}", code, (err)));          \
        }                                                                                          \
    } while (0)

const std::vector<VkExtensionProperties> &extension_properties();

constexpr VkSampleCountFlagBits
get_max_usable_sample_count(const VkPhysicalDeviceProperties &props) noexcept
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

struct swap_chain_support {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

swap_chain_support query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR surface);

template <typename ScoringFunctor>
std::optional<uint32_t>
find_best_queue_family(const std::vector<VkQueueFamilyProperties> &queue_family_properties,
                       ScoringFunctor scoring_func)
{
    // if eligible, score is 32 - number of total set bits
    // the thought is the lower the number of set bits,
    // the more "specialized" the family is and therefore more optimal
    std::vector<int> scores;
    std::transform(queue_family_properties.begin(),
                   queue_family_properties.end(),
                   std::back_inserter(scores),
                   scoring_func);
    auto best_it = std::max_element(scores.begin(), scores.end());
    if (*best_it == 0) return {};
    return static_cast<uint32_t>(std::distance(scores.begin(), best_it));
}

// create a shared pointer to a vulkan resource, for ex. to VkImage_T (because VkImage_T* ==
// VkImage) with a custom deallocation function that destroys the resource appropriately, for
// example by calling the VkDestroy* functions. by wrapping the objects in a shared_ptr, we get
// reference-counted semantics without manually introducing new types for each of those types
template <typename VkResourceType, typename DeletionFunctor>
std::shared_ptr<std::remove_pointer_t<typename VkResourceType>>
make_shared_resource(VkResourceType resource, DeletionFunctor &&deletionFunctor)
{
    return {resource, std::forward<DeletionFunctor>(deletionFunctor)};
}


VKAPI_ATTR VkBool32 VKAPI_CALL
default_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData);

} // namespace vk
} // namespace cory