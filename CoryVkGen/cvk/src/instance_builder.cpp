#include <cvk/instance_builder.h>

#include <cvk/core.h>
#include <cvk/utils.h>

#include <stdexcept>

namespace cvk {

/**
 * Private detail functions that should not be relevant for the public interface
 */
namespace detail {
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                      const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator,
                                      VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) { return func(instance, pCreateInfo, pAllocator, pDebugMessenger); }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) { func(instance, debugMessenger, pAllocator); }
}
} // namespace detail

instance instance_builder::create()
{
    info_.pApplicationInfo = &application_info_;

    info_.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions_.size());
    info_.ppEnabledExtensionNames = enabled_extensions_.data();

    info_.enabledLayerCount = static_cast<uint32_t>(enabled_layers_.size());
    info_.ppEnabledLayerNames = enabled_layers_.data();

    VkInstance inst;
    VK_CHECKED_CALL(vkCreateInstance(&info_, nullptr, &inst), "Failed to create instance!");

    VkDebugUtilsMessengerEXT debugMessenger{};
    if (*((VkStructureType *)info_.pNext) ==
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT) {
        VK_CHECKED_CALL(detail::CreateDebugUtilsMessengerEXT(
                            inst,
                            (const VkDebugUtilsMessengerCreateInfoEXT *)info_.pNext,
                            nullptr,
                            &debugMessenger),
                        "Could not create debug utils messenger");
    }

    auto instance_sptr = make_shared_resource(inst, [=](VkInstance inst) {
        if (debugMessenger) detail::DestroyDebugUtilsMessengerEXT(inst, debugMessenger, nullptr);
        vkDestroyInstance(inst, nullptr);
    });

    return instance{instance_sptr};
}


}