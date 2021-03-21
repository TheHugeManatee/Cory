#include "Cory/vk/test_utils.h"

#include "Cory/vk/instance.h"
#include "Cory/vk/utils.h"

#include "Cory/Log.h"

#include <stdexcept>

namespace cory {
namespace vk {

VKAPI_ATTR VkBool32 VKAPI_CALL
test_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                    void *pUserData)
{
    switch (messageSeverity) {
    case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        CO_CORE_TRACE("Vulkan validation layer: {}", pCallbackData->pMessage);
        break;
    case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        CO_CORE_INFO("Vulkan validation layer: {}", pCallbackData->pMessage);
        break;
    case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        CO_CORE_WARN("Vulkan validation layer: {}", pCallbackData->pMessage);
        throw std::runtime_error(fmt::format("Validation Warning: {}", pCallbackData->pMessage));
    case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        CO_CORE_ERROR("Vulkan validation layer: {}", pCallbackData->pMessage);
        throw std::runtime_error(fmt::format("Validation Error: {}", pCallbackData->pMessage));
    }

    return false;
}

cory::vk::instance &test_instance()
{
    static cory::vk::instance instance = []() {
        VkApplicationInfo app_info{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                   .pApplicationName = "CoryTestExecutable",
                                   .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                   .pEngineName = "Cory",
                                   .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                   .apiVersion = VK_API_VERSION_1_2};
        return cory::vk::instance_builder()
            .application_info(app_info)
            .enabled_extensions({VK_EXT_DEBUG_UTILS_EXTENSION_NAME})
            .next(cory::vk::debug_utils_messenger_builder()
                      .message_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
                      .message_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
                      .user_callback(test_debug_callback)
                      .ptr())
            .create();
    }();

    return instance;
}

cory::vk::graphics_context test_context()
{
    const auto devices = test_instance().physical_devices();
    std::optional<cory::vk::physical_device_info> pickedDevice;
    for (const auto &info : devices) {
        if (!pickedDevice && info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            pickedDevice = info;
        }
    }

    // create a context
    return graphics_context(test_instance(), pickedDevice->device);
}

void test_init()
{
    test_instance();
    // test_context();
}

} // namespace vk
} // namespace cory