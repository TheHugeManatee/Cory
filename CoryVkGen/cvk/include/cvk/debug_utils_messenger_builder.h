#pragma once

#include <vulkan/vulkan.h>

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

}