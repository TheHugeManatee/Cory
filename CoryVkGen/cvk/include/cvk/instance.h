#pragma once

#include <vulkan/vulkan.h>

#include <utility>
#include <vector>
#include <memory>

namespace cvk {

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

    [[nodiscard]] void *ptr() const noexcept { return (void *)&info_; }

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
    explicit instance(std::shared_ptr<struct VkInstance_T> instance_ptr)
        : instance_ptr_{std::move(instance_ptr)}
    {
    }

    physical_device_info device_info(VkPhysicalDevice device);

    /**
     * @brief list info about all physical devices
     * @return
     */
    std::vector<physical_device_info> physical_devices();

    VkInstance get() { return instance_ptr_.get(); }

    /// figure out if any of the given extensions are unsupported
    static std::vector<const char *> unsupported_extensions(std::vector<const char *> extensions);

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

    [[nodiscard]] instance_builder &application_info(VkApplicationInfo applicationInfo) noexcept
    {
        application_info_ = applicationInfo;
        return *this;
    }

    [[nodiscard]] instance_builder &enabled_layers(std::vector<const char *> enabledLayers) noexcept
    {
        enabled_layers_ = std::move(enabledLayers);
        return *this;
    }

    [[nodiscard]] instance_builder &
    enabled_extensions(std::vector<const char *> enabledExtensions) noexcept
    {
        enabled_extensions_ = std::move(enabledExtensions);
        return *this;
    }

    [[nodiscard]] instance create();

  private:
    VkInstanceCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    };
    std::vector<const char *> enabled_extensions_;
    std::vector<const char *> enabled_layers_;
    VkApplicationInfo application_info_;
};

} // namespace cvk