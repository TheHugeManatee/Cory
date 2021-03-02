#pragma once

#include "utils.h"

#include <vulkan/vulkan.hpp>

namespace cory {
namespace vk {

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

class debug_utils_messenger_builder {
  public:
    friend class instance_builder;
    [[nodiscard]] debug_utils_messenger_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    [[nodiscard]] debug_utils_messenger_builder &
    flags(VkDebugUtilsMessengerCreateFlagsEXT flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    [[nodiscard]] debug_utils_messenger_builder &
    message_severity(VkDebugUtilsMessageSeverityFlagsEXT messageSeverity) noexcept
    {
        info_.messageSeverity = messageSeverity;
        return *this;
    }

    [[nodiscard]] debug_utils_messenger_builder &
    message_type(VkDebugUtilsMessageTypeFlagsEXT messageType) noexcept
    {
        info_.messageType = messageType;
        return *this;
    }

    [[nodiscard]] debug_utils_messenger_builder &
    user_callback(PFN_vkDebugUtilsMessengerCallbackEXT pfnUserCallback) noexcept
    {
        info_.pfnUserCallback = pfnUserCallback;
        return *this;
    }

    [[nodiscard]] debug_utils_messenger_builder &user_data(void *pUserData) noexcept
    {
        info_.pUserData = pUserData;
        return *this;
    }

    [[nodiscard]] void* ptr() const noexcept {
        return (void*)&info_;
    }

  private:
    VkDebugUtilsMessengerCreateInfoEXT info_{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    };
};


struct physical_device_info {
    VkPhysicalDevice device;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    std::vector<VkQueueFamilyProperties> queue_family_properties;
    VkSampleCountFlagBits max_usable_sample_count;
};

class instance {
  public:
    instance(std::shared_ptr<struct VkInstance_T> instance_ptr)
        : instance_ptr_{instance_ptr}
    {
    }

    physical_device_info device_info(VkPhysicalDevice device)
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

    /**
     * @brief list info about all physical devices
     * @return
     */
    std::vector<physical_device_info> physical_devices()
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

    VkInstance get() { return instance_ptr_.get(); }

  private:
    std::shared_ptr<struct VkInstance_T> instance_ptr_;
};

class instance_builder {
  public:
    friend class graphics_context;

    [[nodiscard]] instance_builder &next(const void *pNext)
    {
        info_.pNext = pNext;
        return *this;
    }

    [[nodiscard]] instance_builder &flags(VkInstanceCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    [[nodiscard]] instance_builder &
    application_info(const VkApplicationInfo *pApplicationInfo) noexcept
    {
        info_.pApplicationInfo = pApplicationInfo;
        return *this;
    }

    [[nodiscard]] instance_builder &enabled_layers(std::vector<const char *> enabledLayers) noexcept
    {
        enabled_layers_ = enabledLayers;
        info_.enabledLayerCount = static_cast<uint32_t>(enabled_layers_.size());
        info_.ppEnabledLayerNames = enabled_layers_.data();
        return *this;
    }

    [[nodiscard]] instance_builder &
    enabled_extensions(std::vector<const char *> enabledExtensions) noexcept
    {
        enabled_extensions_ = enabledExtensions;
        info_.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions_.size());
        info_.ppEnabledExtensionNames = enabled_extensions_.data();
        return *this;
    }

    [[nodiscard]] instance_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }

    [[nodiscard]] instance create()
    {
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

        auto instance_sptr = std::shared_ptr<VkInstance_T>{
            inst, [=](VkInstance inst) {
                if (debugMessenger)
                    detail::DestroyDebugUtilsMessengerEXT(inst, debugMessenger, nullptr);
                vkDestroyInstance(inst, nullptr);
            }};

        return instance{instance_sptr};
    }

  private:
    VkInstanceCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    };
    std::string_view name_;
    std::vector<const char *> enabled_extensions_;
    std::vector<const char *> enabled_layers_;
};

} // namespace vk
} // namespace cory