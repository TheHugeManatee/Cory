

#include <Cory/Log.h>

#include <Cory/vk/enum_utils.h>
#include <Cory/vk/graphics_context.h>
#include <Cory/vk/instance.h>
#include <Cory/vk/utils.h>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <cstdlib>
#include <optional>

VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
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
        break;
    case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        CO_CORE_ERROR("Vulkan validation layer: {}", pCallbackData->pMessage);
        __debugbreak();
        break;
    }

    return false;
}

int main()
{
    Cory::Log::Init();
    // Cory::Log::GetCoreLogger()->set_level(spdlog::level::trace);

    VkApplicationInfo app_info{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                               .pApplicationName = "CoryAPITester",
                               .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                               .pEngineName = "Cory",
                               .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                               .apiVersion = VK_API_VERSION_1_2};

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;

    CO_CORE_DEBUG("Supported Vulkan Extensions/Layers:");
    for (const auto &ext : cory::vk::extension_properties()) {
        CO_CORE_DEBUG("  {0}", ext.extensionName);
    }

    // collect all required extensions
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    CO_CORE_INFO("GLFW requires {} instance extensions", glfwExtensionCount);
    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // create the instance with our nice builder pattern
    cory::vk::instance instance =
        cory::vk::instance_builder()
            .application_info(app_info)
            .enabled_extensions(extensions)
            .next(cory::vk::debug_utils_messenger_builder()
                      .message_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
                      .message_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
                      .user_callback(debugCallback)
                      .ptr())
            .create();

    // list/pick physical device
    const auto devices = instance.physical_devices();
    std::optional<cory::vk::physical_device_info> pickedDevice;
    for (const auto &info : devices) {
        if (!pickedDevice && info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            pickedDevice = info;
        }
    }

    CO_APP_INFO("Testing pretty-printing of vulkan enums");
    VkImageTiling tiling{VK_IMAGE_TILING_LINEAR};
    VkResult result{VK_INCOMPLETE};
    CO_APP_INFO("Image tiling: {}, was {}, rtsgt={}",
                tiling,
                result,
                VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR);
    VkQueueFlags qflags{VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT};
    CO_APP_INFO("Queue flags: {}", cory::vk::flag_bits_to_string<VkQueueFlagBits>(qflags));

    // queues
    CO_APP_INFO("Listing the available queues for the selected device");
    for (const auto &qfp : pickedDevice->queue_family_properties) {
        CO_APP_INFO("{} queues: {}",
                    qfp.queueCount,
                    cory::vk::flag_bits_to_string<VkQueueFlagBits>(qfp.queueFlags));
    }

    // create a logical device
    cory::vk::device device = [&]() {
        using namespace cory::vk;

        // just enable everything! :)
        VkPhysicalDeviceFeatures enabledFeatures = pickedDevice->features;
        // build the device
        return device_builder(pickedDevice->device)
            .queue_create_infos({queue_builder().queue_family_index(1).queue_priorities({1.0f}),
                                 queue_builder().queue_family_index(2).queue_priorities({1.0f})})
            .enabled_features(enabledFeatures)
            .create();
    }();

    // create a context
    cory::vk::graphics_context ctx(instance, pickedDevice->device);

    auto img = ctx.build_image()
                   .image_type(VK_IMAGE_TYPE_3D)
                   .extent({1, 2, 3})
                   .format(VK_FORMAT_R8G8B8A8_UNORM)
                   .memory_usage(cory::vk::device_memory_usage::eGpuOnly)
                   .usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                   .name("test image")
                   .create();

    return EXIT_SUCCESS;
}
