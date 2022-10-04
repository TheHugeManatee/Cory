#include <Cory/RenderCore/Context.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/RenderCore/VulkanUtils.hpp>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StringStlView.h>
#include <Magnum/Vk/BufferCreateInfo.h>
#include <Magnum/Vk/CommandPoolCreateInfo.h>
#include <Magnum/Vk/DeviceCreateInfo.h>
#include <Magnum/Vk/DeviceProperties.h>
#include <Magnum/Vk/ExtensionProperties.h>
#include <Magnum/Vk/Extensions.h>
#include <Magnum/Vk/FenceCreateInfo.h>
#include <Magnum/Vk/InstanceCreateInfo.h>
#include <Magnum/Vk/Queue.h>
#include <Magnum/Vk/Version.h>

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
    Vk::DeviceProperties physicalDevice{Corrade::NoCreate};
    Vk::Device device{Corrade::NoCreate};

    Vk::Queue graphicsQueue{Corrade::NoCreate};
    uint32_t graphicsQueueFamily{};
    Vk::Queue computeQueue{Corrade::NoCreate};
    uint32_t computeQueueFamily{};

    Vk::CommandPool commandPool{Corrade::NoCreate};
};

Context::Context()
    : data{std::make_unique<ContextPrivate>()}
{
    data->name = "CCtx";

    const auto app_name{"Cory-based Vulkan Application"};

    data->instance.create(Vk::InstanceCreateInfo{}
                              .setApplicationInfo(app_name, Vk::version(1, 0, 0))
                              .addEnabledLayers({"VK_LAYER_KHRONOS_validation"})
                              .addEnabledExtensions<Magnum::Vk::Extensions::EXT::debug_utils>()
                              .addEnabledExtensions({"VK_KHR_surface", "VK_KHR_win32_surface"}));
    data->instance.populateGlobalFunctionPointers();

    data->physicalDevice = Vk::pickDevice(data->instance);
    CO_APP_INFO("Using device {}", data->physicalDevice.name());

    Vk::ExtensionProperties extensions = data->physicalDevice.enumerateExtensionProperties();
    Vk::DeviceCreateInfo info{data->physicalDevice, &extensions};
    info.addEnabledExtensions({"VK_KHR_swapchain"});

    // configure a Graphics and a Compute queue - assumes that there is a family that
    // supports both graphics and compute, which is probably not universal
    data->graphicsQueueFamily = data->computeQueueFamily = data->physicalDevice.pickQueueFamily(
        Vk::QueueFlags::Type::Graphics | Vk::QueueFlags::Type::Compute);
    info.addQueues(
        data->graphicsQueueFamily, {1.0f, 1.0f}, {data->graphicsQueue, data->computeQueue});

    data->device.create(data->instance, std::move(info));
    data->device.populateGlobalFunctionPointers();
    // set a debug name for the logical device and queues
    nameVulkanObject(data->device, data->device, fmt::format("[{}] Logical Device", data->name));
    nameVulkanObject(data->device, data->graphicsQueue, fmt::format("[{}] Graphics", data->name));
    nameVulkanObject(data->device, data->computeQueue, fmt::format("[{}] Compute", data->name));

    setupDebugMessenger();

    data->commandPool =
        Vk::CommandPool{data->device, Vk::CommandPoolCreateInfo{data->graphicsQueueFamily}};
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

    instance()->CreateDebugUtilsMessengerEXT(
        data->instance, &dbgMessengerCreateInfo, nullptr, &data->debugMessenger);

    VkDebugUtilsMessengerCallbackDataEXT messageCallbackData{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT};
    auto message =
        fmt::format("Cory context '{}' initialized and debug messenger attached.", data->name);
    messageCallbackData.pMessage = message.c_str();
    instance()->SubmitDebugUtilsMessageEXT(data->instance,
                                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                                           &messageCallbackData);

    // this seems to crash - not sure if driver or implementation bug...
    // nameRawVulkanObject(
    //    data->device.handle(), debugMessenger, fmt::format("{} Debug Messenger", data->name));
}

Context::~Context()
{
    instance()->DestroyDebugUtilsMessengerEXT(data->instance, data->debugMessenger, nullptr);
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

Semaphore Context::createSemaphore(std::string_view name)
{
    VkSemaphoreCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .flags = 0};

    VkSemaphore semaphore;
    THROW_ON_ERROR(device()->CreateSemaphore(data->device, &create_info, nullptr, &semaphore),
                   "failed to create a semaphore object");

    if (!name.empty()) { nameRawVulkanObject(data->device, semaphore, name); }

    return Semaphore{semaphore, [&device = data->device](VkSemaphore f) {
                         device->DestroySemaphore(device, f, nullptr);
                     }};
}

Vk::Fence Context::createFence(std::string_view name, Cory::FenceCreateMode mode)
{
    Vk::Fence fence{Corrade::NoCreate};
    if (mode == FenceCreateMode::Signaled) {
        fence = Vk::Fence{data->device, Vk::FenceCreateInfo{Vk::FenceCreateInfo::Flag::Signaled}};
    }

    fence = Vk::Fence{data->device, Vk::FenceCreateInfo{}};
    nameVulkanObject(data->device, fence, name);
    return fence;
}

bool Context::isHeadless() const { return data->isHeadless; }
Vk::Instance &Context::instance() { return data->instance; }
Magnum::Vk::DeviceProperties &Context::physicalDevice() { return data->physicalDevice; }
Vk::Device &Context::device() { return data->device; }
Vk::CommandPool &Context::commandPool() { return data->commandPool; }
Magnum::Vk::Queue &Context::graphicsQueue() { return data->graphicsQueue; }
uint32_t Context::graphicsQueueFamily() const { return data->graphicsQueueFamily; }
Magnum::Vk::Queue &Context::computeQueue() { return data->computeQueue; }
uint32_t Context::computeQueueFamily() const { return data->computeQueueFamily; }

} // namespace Cory