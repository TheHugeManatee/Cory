#include "test_utils.h"

#include <cvk/instance.h>
#include <cvk/log.h>
#include <cvk/utils.h>

#include <stdexcept>

namespace cvkt {

static uint64_t test_debug_message_count{0};

VKAPI_ATTR VkBool32 VKAPI_CALL
test_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                    void *pUserData)
{
    test_debug_message_count++;
    switch (messageSeverity) {
    case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        CVK_TRACE("Vulkan validation layer: {}", pCallbackData->pMessage);
        break;
    case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        CVK_INFO("Vulkan validation layer: {}", pCallbackData->pMessage);
        break;
    case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        CVK_WARN("Vulkan validation layer: {}", pCallbackData->pMessage);
        // throw std::runtime_error(fmt::format("Validation Warning: {}", pCallbackData->pMessage));
    case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        CVK_ERROR("Vulkan validation layer: {}", pCallbackData->pMessage);
        // throw std::runtime_error(fmt::format("Validation Error: {}", pCallbackData->pMessage));
    }

    return false;
}

cvk::instance &test_instance()
{
    static cvk::instance test_inst = []() {
        VkApplicationInfo app_info{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                   .pApplicationName = "CoryTestExecutable",
                                   .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                   .pEngineName = "Cory",
                                   .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                   .apiVersion = VK_API_VERSION_1_2};
        return cvk::instance_builder()
            .application_info(app_info)
            .enabled_extensions({VK_EXT_DEBUG_UTILS_EXTENSION_NAME})
            .next(cvk::debug_utils_messenger_builder()
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
    // we reset the debug message count - this is important so a faulty debug message count
    // does not leak from one unit test to the next.
    test_debug_message_count = 0;
    return test_inst;
}

// cvk::graphics_context test_context()
//{
//     const auto devices = test_instance().physical_devices();
//     std::optional<cvk::physical_device_info> pickedDevice;
//     for (const auto &info : devices) {
//         if (!pickedDevice && info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
//         {
//             pickedDevice = info;
//         }
//     }
//
//     // create a context
//     return graphics_context(test_instance(), pickedDevice->device);
// }

void test_init() { test_instance(); }
uint64_t debug_message_count() { return test_debug_message_count; }

} // namespace cvkt