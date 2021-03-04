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

int main_main()
{
    Cory::Log::Init();
    // Cory::Log::GetCoreLogger()->set_level(spdlog::level::trace);

    // initialize glfw - this has to be done early
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(1024, 768, "Hello Cory", nullptr, nullptr);

    // collect all required extensions
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    CO_CORE_INFO("GLFW requires {} instance extensions", glfwExtensionCount);
    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // set up the ApplicationInfo
    VkApplicationInfo app_info{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                               .pApplicationName = "CoryAPITester",
                               .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                               .pEngineName = "Cory",
                               .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                               .apiVersion = VK_API_VERSION_1_2};

    // create the instance
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

    // initialize the surface
    auto surface = [&, instance_ptr = instance.get()]() -> std::shared_ptr<VkSurfaceKHR_T> {
        VkSurfaceKHR surface;
        VK_CHECKED_CALL(glfwCreateWindowSurface(instance.get(), window, nullptr, &surface),
                        "Could not create window surface");
        // return a shared_ptr with custom deallocator to destroy the surface
        return {surface,
                [instance_ptr](VkSurfaceKHR s) { vkDestroySurfaceKHR(instance_ptr, s, nullptr); }};
    }();

    // create a context
    cory::vk::graphics_context ctx(instance, pickedDevice->device, surface.get());



    return EXIT_SUCCESS;
}

int main()
{
    try {
        main_main();
    }
    catch (std::runtime_error &err) {
        CO_APP_ERROR("runtime_error: {}", err.what());
    }
}