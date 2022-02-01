#include "vk/instance.h"

namespace cory {
namespace vk {

std::vector<const char *> instance::unsupported_extensions(std::vector<const char *> extensions)
{
    auto instanceExtensions =
        vk_enumerate<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, nullptr);

    std::vector<const char *> unsupportedExtensions;

    for (const auto &ext : extensions) {
        const auto match_ext = [&](const VkExtensionProperties &ep) {
            return std::string_view{ep.extensionName} == ext;
        };
        if (std::ranges::find_if(instanceExtensions, match_ext) ==
            std::ranges::end(instanceExtensions)) {
            unsupportedExtensions.push_back(ext);
        }
    }
    return unsupportedExtensions;
}

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

physical_device_info instance::device_info(VkPhysicalDevice device)
{
    physical_device_info dev_info;
    dev_info.device = device;
    vkGetPhysicalDeviceProperties(device, &dev_info.properties);
    vkGetPhysicalDeviceFeatures(device, &dev_info.features);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    dev_info.queue_family_properties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        device, &queueFamilyCount, dev_info.queue_family_properties.data());

    dev_info.max_usable_sample_count = get_max_usable_sample_count(dev_info.properties);
    return dev_info;
}

std::vector<physical_device_info> instance::physical_devices()
{
    uint32_t numDevices;
    vkEnumeratePhysicalDevices(instance_ptr_.get(), &numDevices, nullptr);
    std::vector<VkPhysicalDevice> devices(numDevices);
    vkEnumeratePhysicalDevices(instance_ptr_.get(), &numDevices, devices.data());

    std::vector<physical_device_info> props(numDevices);
    for (const auto &p : devices) {
        props.push_back(device_info(p));
    }

    return props;
}

instance instance_builder::create()
{
    info_.pApplicationInfo = &application_info_;

    info_.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions_.size());
    info_.ppEnabledExtensionNames = enabled_extensions_.data();

    info_.enabledLayerCount = static_cast<uint32_t>(enabled_layers_.size());
    info_.ppEnabledLayerNames = enabled_layers_.data();

    VkInstance inst;
    VK_CHECKED_CALL(vkCreateInstance(&info_, nullptr, &inst), "Failed to create instance!");

    VkDebugUtilsMessengerEXT debugMessenger;
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

} // namespace vk
} // namespace cory