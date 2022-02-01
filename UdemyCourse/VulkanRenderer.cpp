#include "VulkanRenderer.h"

#include <Cory/Log.h>
#include <Cory/vk/queue.h>
#include <Cory/vk/utils.h>
#include <cvk/FmtStruct.h>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <range/v3/all.hpp>

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
    , physicalDevice_{pickPhysicalDevice()}
    , device_{createLogicalDevice()}
{
}
VulkanRenderer::~VulkanRenderer()
{
    // nope
}

cory::vk::instance VulkanRenderer::createInstance()
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
    CO_CORE_INFO("GLFW requires {} instance extensions", glfwExtensionCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    if (!check_extension_support(extensions)) {
        throw std::runtime_error("Some required instances are not supported");
    }

    auto debug_message = cory::vk::debug_utils_messenger_builder()
                             .message_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
                             .message_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
                             .user_callback(cory::vk::default_debug_callback);

    const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

    auto instance = cory::vk::instance_builder()
                        .application_info(app_info)
                        .enabled_extensions(extensions)
                        .enabled_layers(validationLayers)
                        .next(debug_message.ptr())
                        .create();

    CO_APP_INFO("Instance created successfully!");
    return instance;
}
bool VulkanRenderer::check_extension_support(std::vector<const char *> extensions)
{
    auto instanceExtensions =
        vk_enumerate<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, nullptr);

    namespace views = ranges::views;
    auto supported_ext_names = instanceExtensions | views::transform([](const auto &ext) {
                                   return std::string_view{ext.extensionName};
                               });
    for (const auto &ext : extensions) {
        if (ranges::find(supported_ext_names, ext) == ranges::end(supported_ext_names)) {
            spdlog::error("Extension is not supported: {}", ext);
            return false;
        }
    }
    return true;
}
cory::vk::physical_device_info VulkanRenderer::pickPhysicalDevice()
{
    const auto devices = instance_.physical_devices();
    std::optional<cory::vk::physical_device_info> pickedDevice;

    // prefer any discrete GPU
    for (const auto &info : devices) {
        if (!pickedDevice && info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            pickedDevice = info;
        }
    }
    // if no discrete available, just use the first device available
    auto picked = pickedDevice.value_or(devices[0]);
    spdlog::info("Using {}", picked.properties.deviceName);

    return picked;
}
cory::vk::device VulkanRenderer::createLogicalDevice()
{
    // just enable everything! :)
    VkPhysicalDeviceFeatures enabledFeatures = physicalDevice_.features;
    //    spdlog::info("Available Queue families:");
    //    for (const auto &qfp : physicalDevice_.queue_family_properties) {
    //        spdlog::info("{}", qfp);
    //    }

    // build the device
    auto device = cory::vk::device_builder(physicalDevice_)
                      .add_queue(VK_QUEUE_GRAPHICS_BIT)
                      .enabled_features(enabledFeatures)
                      .create();

    spdlog::info("Logical device created");
    return device;
}
