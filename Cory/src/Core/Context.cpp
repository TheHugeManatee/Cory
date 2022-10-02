#include <Cory/Core/Context.hpp>

#include <Cory/Core/FmtUtils.hpp>
#include <Cory/Core/Log.hpp>
#include <Cory/Core/VulkanUtils.hpp>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StringView.h>
#include <Magnum/Vk/BufferCreateInfo.h>
#include <Magnum/Vk/DeviceCreateInfo.h>
#include <Magnum/Vk/DeviceProperties.h>
#include <Magnum/Vk/ExtensionProperties.h>
#include <Magnum/Vk/Extensions.h>
#include <Magnum/Vk/InstanceCreateInfo.h>
#include <Magnum/Vk/Queue.h>
#include <Magnum/Vk/Version.h>
#include <MagnumExternal/Vulkan/flextVkGlobal.h>

namespace Vk = Magnum::Vk;

namespace Cory {

namespace detail {

VkBool32 debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                     VkDebugUtilsMessageTypeFlagsEXT messageType,
                                     const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                     void *pUserData)
{
    Context *context = static_cast<Context *>(pUserData);
    // CO_APP_ERROR("Vulkan error: {}", pCallbackData->pMessage);
    context->receiveDebugUtilsMessage(static_cast<DebugMessageSeverity>(messageSeverity),
                                      static_cast<DebugMessageType>(messageType),
                                      pCallbackData);
    return VK_TRUE;
}
} // namespace detail

struct ContextPrivate {
    std::string name;
    bool isHeadless{false};
    Vk::Instance instance{Corrade::NoCreate};
    VkDebugUtilsMessengerEXT debugMessenger{};
    Vk::Device device{Corrade::NoCreate};

    Vk::Queue graphicsQueue{Corrade::NoCreate};
    Vk::Queue computeQueue{Corrade::NoCreate};
};

Context::Context()
    : data{std::make_unique<ContextPrivate>()}
{
    data->name = "CCtx";

    using namespace Corrade::Containers::Literals;
    const auto app_name{"Cory-based Vulkan Application"_s};

    data->instance.create(
        Vk::InstanceCreateInfo{}
            .setApplicationInfo(app_name, Vk::version(1, 0, 0))
            .addEnabledLayers({"VK_LAYER_KHRONOS_validation"_s})
            .addEnabledExtensions<Magnum::Vk::Extensions::EXT::debug_utils>()
            .addEnabledExtensions({"VK_KHR_surface"_s, "VK_KHR_win32_surface"_s}));
    data->instance.populateGlobalFunctionPointers();

    Vk::DeviceProperties properties = Vk::pickDevice(data->instance);
    CO_APP_INFO("Using device {}", properties.name());

    Vk::ExtensionProperties extensions = properties.enumerateExtensionProperties();
    Vk::DeviceCreateInfo info{properties, &extensions};
    info.addEnabledExtensions({"VK_KHR_swapchain"_s});

    // configure a Graphics and a Compute queue - assumes that there is a family that
    // supports both graphics and compute, which is probably not universal
    auto graphicsAndComputeFamily =
        properties.pickQueueFamily(Vk::QueueFlags::Type::Graphics | Vk::QueueFlags::Type::Compute);
    info.addQueues(
        graphicsAndComputeFamily, {1.0f, 1.0f}, {data->graphicsQueue, data->computeQueue});

    data->device.create(data->instance, std::move(info));
    data->device.populateGlobalFunctionPointers();
    // set a debug name for the logical device
    nameVulkanObject(data->device, data->device, fmt::format("{} Logical Device", data->name));

    setupDebugMessenger();
}

void Context::setupDebugMessenger()
{
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
        .pfnUserCallback = detail::debugUtilsMessengerCallback,
        .pUserData = nullptr};

    vkCreateDebugUtilsMessengerEXT(
        data->instance.handle(), &dbgMessengerCreateInfo, nullptr, &data->debugMessenger);

    VkDebugUtilsMessengerCallbackDataEXT messageCallbackData{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
    auto message =
        fmt::format("Cory context '{}' initialized and debug messenger attached.", data->name);
    messageCallbackData.pMessage = message.c_str();
    vkSubmitDebugUtilsMessageEXT(data->instance.handle(),
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                                 &messageCallbackData);

    // this seems to crash - not sure if driver or implementation bug...
    // nameRawVulkanObject(
    //    data->device.handle(), debugMessenger, fmt::format("{} Debug Messenger", data->name));
}

Context::~Context()
{
    vkDestroyDebugUtilsMessengerEXT(data->instance.handle(), data->debugMessenger, nullptr);
}

std::string Context::getName() const { return data->name; }

void Context::receiveDebugUtilsMessage(DebugMessageSeverity severity,
                                       DebugMessageType messageType,
                                       const VkDebugUtilsMessengerCallbackDataEXT *callbackData)
{
    auto level = [&]() {
        switch (severity) {
        case DebugMessageSeverity::Verbose:
            return spdlog::level::trace;
        case DebugMessageSeverity::Info:
            return spdlog::level::info;
        case DebugMessageSeverity::Warning:
            return spdlog::level::warn;
        case DebugMessageSeverity::Error:
            return spdlog::level::err;
        }
        return spdlog::level::debug;
    }();

    Log::GetCoreLogger()->log(level, "[VulkanDebugMsg:{}] {}", messageType, callbackData->pMessage);
#if _MSC_VER && _DEBUG
    if (severity == DebugMessageSeverity::Error) { __debugbreak(); }
#endif
}

bool Context::isHeadless() const { return data->isHeadless; }
Vk::Instance &Context::instance() const { return data->instance; }
Vk::Device &Context::device() const { return data->device; }

} // namespace Cory