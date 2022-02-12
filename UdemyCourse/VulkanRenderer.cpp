#include "VulkanRenderer.h"

#include <cvk/FmtStruct.h>
#include <cvk/context.h>
#include <cvk/debug_utils_messenger_builder.h>
#include <cvk/instance_builder.h>

#include <Cory/Log.h>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <stdexcept>

// utility to shorten the typical enumerate pattern you need in c++
template <typename ReturnT, typename FunctionT, typename... FunctionParameters>
std::vector<ReturnT> vk_enumerate(FunctionT func, FunctionParameters... parameters)
{
    uint32_t count = 0;
    func(parameters..., &count, nullptr);
    std::vector<ReturnT> values{size_t(count)};
    func(parameters..., &count, values.data());
    return values;
}

VulkanRenderer::VulkanRenderer(GLFWwindow *window)
    : window_{window}
    , instance_{createInstance()}
    , context_{createContext()}
{
}
VulkanRenderer::~VulkanRenderer() = default;

cvk::instance VulkanRenderer::createInstance()
{
    VkApplicationInfo app_info{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                               .pApplicationName = "Udemy Vulkan Renderer",
                               .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                               .pEngineName = "Cory",
                               .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                               .apiVersion = VK_API_VERSION_1_2};

    // collect all required extensions
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    CO_APP_INFO("GLFW requires {} instance extensions", glfwExtensionCount);

    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    const auto unsupported_extensions = cvk::instance::unsupported_extensions(extensions);
    if (!unsupported_extensions.empty()) {
        throw std::runtime_error(
            fmt::format("Some required instances are not supported: {}", unsupported_extensions));
    }

    auto debug_message = cvk::debug_utils_messenger_builder()
                             .message_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
                             .message_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
                             .user_callback(cvk::default_debug_callback);

    const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

    auto instance = cvk::instance_builder()
                        .application_info(app_info)
                        .enabled_extensions(extensions)
                        .enabled_layers(validationLayers)
                        .next(debug_message.ptr())
                        .create();

    CO_APP_INFO("Instance created successfully!");
    return instance;
}

cvk::context VulkanRenderer::createContext() { return cvk::context(instance_); }
