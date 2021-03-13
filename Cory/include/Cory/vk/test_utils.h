#pragma once

#include "graphics_context.h"
#include "utils.h"

#include <vulkan/vulkan.h>

namespace cory {

namespace vk {

VKAPI_ATTR VkBool32 VKAPI_CALL
test_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                    void *pUserData);

// access to a global vulkan instance singleton the instance has validation layers enabled and any
// validation warning or error will throw an exception.
instance &test_instance();

// access to a global context singleton. the context will be created as a headless context, i.e. no
// associated surface and no present capabilities!
graphics_context &test_context();

// initialize the vulkan instance and device to run the tests.
void test_init();

} // namespace vk
} // namespace cory