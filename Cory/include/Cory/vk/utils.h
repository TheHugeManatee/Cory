#pragma once

#include <fmt/format.h>
#include <vulkan/vulkan.h>

#include <optional>
#include <type_traits>
#include <vector>

#define VK_CHECKED_CALL(x, err)                                                                    \
    do {                                                                                           \
        if (auto code = (x); code != VK_SUCCESS) {                                                 \
            throw std::runtime_error(fmt::format(#x " failed with {}: {}", code, (err)));          \
        }                                                                                          \
    } while (0)

namespace cory {
namespace vk {

template <typename WrappedVkType> class basic_vk_wrapper {
  public:
    using vk_type = WrappedVkType;
    using vk_opaque_type = std::remove_pointer_t<WrappedVkType>;
    using vk_shared_ptr = std::shared_ptr<vk_opaque_type>;

    basic_vk_wrapper(vk_shared_ptr vk_resource_ptr = {})
        : vk_resource_ptr_{vk_resource_ptr}
    {
    }

    const vk_type get() const { return vk_resource_ptr_.get(); }
    bool has_value() const { return vk_resource_ptr_.get() != nullptr; }

  private:
    vk_shared_ptr vk_resource_ptr_;
};

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

[[nodiscard]] swap_chain_support query_swap_chain_support(VkPhysicalDevice device,
                                                          VkSurfaceKHR surface);

[[nodiscard]] VkFormat find_supported_format(VkPhysicalDevice device,
                                             const std::vector<VkFormat> &candidates,
                                             VkImageTiling tiling,
                                             VkFormatFeatureFlags features) noexcept;

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

/**
 * @brief helper to "extract" the contained raw vulkan types from a vector of cory wrapper objects.
 *
 * returns a new vector by calling .get() on each of objects in the provided std::vector.
 *
 * @code{.cpp}
 *   std::vector<semaphore> my_semaphores = ...;
 *   std::vector<VkSemaphore> vk_semaphore_objects = collect_vk_objects(my_semaphores);
 * @endcode
 */
template <typename CoryWrapperType,
          typename VkContainedType = decltype(std::declval<CoryWrapperType>().get())>
std::vector<VkContainedType>
collect_vk_objects(const std::vector<CoryWrapperType> &vector_of_wrappers)
{

    std::vector<VkContainedType> vk_objects(vector_of_wrappers.size());
    std::transform(vector_of_wrappers.begin(),
                   vector_of_wrappers.end(),
                   vk_objects.begin(),
                   [](const auto &wrapper) { return wrapper.get(); });

    return vk_objects;
}

} // namespace vk
} // namespace cory