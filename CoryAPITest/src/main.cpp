

#include <Cory/Log.h>

#include <Cory/vk/graphics_context.h>
#include <Cory/vk/instance.h>
#include <Cory/vk/utils.h>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <optional>
#include <cstdlib>

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

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback};

    // create the instance
    cory::vk::instance instance = cory::vk::instance_builder()
                                      .application_info(&app_info)
                                      .enabled_extensions(extensions)
                                      .next(&debugCreateInfo)
                                      .create();

    const auto devices = instance.physical_devices();
    std::optional<VkPhysicalDevice> pickedDevice;
    for (const auto &[device, props] : devices) {
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            pickedDevice = device;
            break;
        }
    }

    /*      cory::vk::graphics_context app_context;

      auto img = app_context.image().extent({1, 1, 1}).format(VK_FORMAT_R8G8B8A8_USCALED).create();

      try {
          app.run();
      }
      catch (const std::exception &e) {
          CO_APP_FATAL("Unhandled exception: {}", e.what());

          return EXIT_FAILURE;
      }

      */
    return EXIT_SUCCESS;
}
