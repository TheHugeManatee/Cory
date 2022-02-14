#include <cvk/utils.h>

#include "core.h"
#include <cvk/log.h>

namespace cvk {
VKAPI_ATTR VkBool32 VKAPI_CALL
default_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                       VkDebugUtilsMessageTypeFlagsEXT messageType,
                       const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                       void *pUserData)
{
    switch (messageSeverity) {
    case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        CVK_TRACE("Vulkan validation layer: {}", pCallbackData->pMessage);
        break;
    case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        CVK_INFO("Vulkan validation layer: {}", pCallbackData->pMessage);
        break;
    case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        CVK_WARN("Vulkan validation layer: {}", pCallbackData->pMessage);
        break;
    case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        CVK_ERROR("Vulkan validation layer: {}", pCallbackData->pMessage);
        __debugbreak(); // FIXME: this is currently windows-specific
        break;
    }

    return false;
}

swap_chain_support query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    swap_chain_support details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
    details.formats =
        vk_enumerate<VkSurfaceFormatKHR>(vkGetPhysicalDeviceSurfaceFormatsKHR, device, surface);
    details.present_modes =
        vk_enumerate<VkPresentModeKHR>(vkGetPhysicalDeviceSurfacePresentModesKHR, device, surface);

    return details;
}

} // namespace cvk