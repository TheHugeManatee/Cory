#include <Cory/Cory.hpp>

#include <Magnum/Vk/Version.h>
#include <MagnumExternal/Vulkan/flextVkGlobal.h>

#include <Corrade/Containers/StringView.h>
#include <Magnum/Vk/ExtensionProperties.h>
#include <Magnum/Vk/Extensions.h>
#include <Magnum/Vk/InstanceCreateInfo.h>
#include <Magnum/Vk/LayerProperties.h>

#include <fmt/core.h>

#include <Cory/Core/Log.hpp>

namespace Cory {
int test_function() { return 42; }
std::string queryVulkanInstanceVersion()
{
    auto version = Magnum::Vk::enumerateInstanceVersion();
    return fmt::format(
        "{}.{}.{}", versionMajor(version), versionMinor(version), versionPatch(version));
}

VkBool32 debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                     VkDebugUtilsMessageTypeFlagsEXT messageType,
                                     const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                     void *pUserData)
{
    CO_APP_INFO("Debug message received: {}", pCallbackData->pMessage);
    return VK_TRUE;
}

void main()
{
    namespace MVk = Magnum::Vk;

    const Corrade::Containers::StringView app_name{"My Vulkan Application"};

    MVk::LayerProperties layers = MVk::enumerateLayerProperties();
    CO_CORE_INFO("Supported layers: {}", layers.count());
    for (const auto name : layers.names()) {
        CO_CORE_INFO("    {:<25}: {}", name.data(), layers.isSupported(name));
    }
    MVk::InstanceExtensionProperties extensions =
        /* ... including extensions exposed only by the extra layers */
        MVk::enumerateInstanceExtensionProperties(layers.names());
    CO_CORE_INFO("Supported extensions: {}", extensions.count());
    for (const auto name : extensions.names()) {
        CO_CORE_INFO("    {:<25}: {}", name.data(), extensions.isSupported(name));
    }

    MVk::Instance instance{Corrade::NoCreate};
    instance.create(
        MVk::InstanceCreateInfo{}
            .setApplicationInfo(app_name, MVk::version(1, 2, 3))
            .addEnabledLayers({Corrade::Containers::StringView{"VK_LAYER_KHRONOS_validation"}})
            .addEnabledExtensions<Magnum::Vk::Extensions::EXT::debug_utils>());

    VkDebugUtilsMessengerCreateInfoEXT dbgMessengerCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = debugUtilsMessengerCallback,
        .pUserData = nullptr};
    VkDebugUtilsMessengerEXT debugMessenger{};
    instance.populateGlobalFunctionPointers();

    vkCreateDebugUtilsMessengerEXT(
        instance.handle(), &dbgMessengerCreateInfo, nullptr, &debugMessenger);

    instance.release();
}

} // namespace Cory