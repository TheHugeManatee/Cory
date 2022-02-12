#pragma once

#include <cvk/utils.h>
#include <cvk/context.h>

#include <vulkan/vulkan.h>

namespace cvk {
class instance;
}
namespace cvkt {

VKAPI_ATTR VkBool32 VKAPI_CALL
test_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                    void *pUserData);

/**
 * @brief global vulkan instance singleton for unit tests
 *
 * access to a global vulkan instance singleton the instance has validation layers enabled and any
 * validation warning or error will throw an exception.
 *
 * @note each invocation of this function will reset the count reported by debug_message_count
 */
cvk::instance &test_instance();

/**
 * @brief Query the number of debug messages from the validation layers since the last call to
 * test_instance
 */
uint64_t debug_message_count();

// access to a global context singleton. the context will be created as a headless context, i.e. no
// associated surface and no present capabilities!
cvk::context test_context();

/// initialize the vulkan instance and device to run the tests.
void test_init();

} // namespace cvkt