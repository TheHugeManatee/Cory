#include "vk/utils.h"

namespace cory {
namespace vk {

const std::vector<VkExtensionProperties> &extension_properties()
{
    static const std::vector<VkExtensionProperties> extension_props = []() {
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
        return extensions;
    }();
    return extension_props;
}

cory::vk::swap_chain_support query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    swap_chain_support details;
    uint32_t count;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
    details.formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, details.formats.data());

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
    details.presentModes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, details.presentModes.data());

    return details;
}

//const std::vector<VkLayerProperties>& layer_properties() {
//    static const std::vector<VkLayerProperties> layer_props = []() {
//        uint32_t layerCount = 0;
//        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
//        std::vector<VkExtensionProperties> layers(layerCount);
//        vkEnumerateInstanceExtensionProperties(&layerCount, layers.data());
//        return layers;
//    }();
//    return layer_props;
//}

}
}