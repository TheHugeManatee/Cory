#include <Cory/Renderer/Context.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/DescriptorSets.hpp>
#include <Cory/Renderer/ResourceManager.hpp>
#include <Cory/Renderer/VulkanUtils.hpp>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StringStlView.h>
#include <Magnum/Vk/BufferCreateInfo.h>
#include <Magnum/Vk/CommandPoolCreateInfo.h>
#include <Magnum/Vk/DescriptorPoolCreateInfo.h>
#include <Magnum/Vk/DescriptorSetLayoutCreateInfo.h>
#include <Magnum/Vk/DescriptorType.h>
#include <Magnum/Vk/DeviceCreateInfo.h>
#include <Magnum/Vk/DeviceFeatures.h>
#include <Magnum/Vk/DeviceProperties.h>
#include <Magnum/Vk/ExtensionProperties.h>
#include <Magnum/Vk/Extensions.h>
#include <Magnum/Vk/FenceCreateInfo.h>
#include <Magnum/Vk/InstanceCreateInfo.h>
#include <Magnum/Vk/MeshLayout.h>
#include <Magnum/Vk/PipelineLayoutCreateInfo.h>
#include <Magnum/Vk/Queue.h>
#include <Magnum/Vk/Result.h>
#include <Magnum/Vk/SamplerCreateInfo.h>
#include <Magnum/Vk/Version.h>
#include <Magnum/Vk/VertexFormat.h>

namespace Vk = Magnum::Vk;

namespace Cory {

struct ContextPrivate {
    std::string name;
    bool isHeadless{false};
    Vk::Instance instance{Corrade::NoCreate};
    BasicVkObjectWrapper<VkDebugUtilsMessengerEXT> debugMessenger{};
    Vk::DeviceProperties physicalDevice{Corrade::NoCreate};
    Vk::Device device{Corrade::NoCreate};

    Vk::Queue graphicsQueue{Corrade::NoCreate};
    uint32_t graphicsQueueFamily{};
    Vk::Queue computeQueue{Corrade::NoCreate};
    uint32_t computeQueueFamily{};

    Vk::CommandPool commandPool{Corrade::NoCreate};

    ResourceManager resources;

    Callback<const DebugMessageInfo &> onVulkanDebugMessageReceived;

    DescriptorSets descriptorSetManager;
    /// todo pipeline layout should probably belong to the framegraph instead of the context
    Magnum::Vk::PipelineLayout defaultPipelineLayout{Corrade::NoCreate};
    Magnum::Vk::MeshLayout defaultMeshLayout{Corrade::NoInit};
    /// mesh layout with no bindings, to implement dynamic vertex generation/pulling
    Magnum::Vk::MeshLayout emptyMeshLayout{Corrade::NoInit};
    SamplerHandle defaultSampler;

    void receiveDebugUtilsMessage(DebugMessageSeverity severity,
                                  DebugMessageType messageType,
                                  const VkDebugUtilsMessengerCallbackDataEXT *callbackData);
};

namespace detail {
PNextChain<> setupRequiredDeviceFeatures(Vk::DeviceCreateInfo &info, ContextPrivate &data);
Magnum::Vk::PipelineLayout
createDefaultPipelineLayout(Context &ctx, Vk::DescriptorSetLayout &descriptorSetLayout);
Magnum::Vk::MeshLayout createDefaultMeshLayout();
VkBool32 debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                     VkDebugUtilsMessageTypeFlagsEXT messageType,
                                     const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                     void *pUserData);
} // namespace detail

Context::Context(ContextCreationInfo creationInfo)
    : data_{std::make_unique<ContextPrivate>()}
{
    data_->name = "CCtx";

    const auto app_name{"Cory-based Vulkan Application"};

    // for dynamic rendering, we need:
    //  - KHR_get_physical_device_properties2 instance extension
    //  - KHR_dynamic_rendering device extension
    //  - enable dynamic_rendering feature via VkPhysicalDeviceDynamicRenderingFeatures
    Vk::InstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.setApplicationInfo(app_name, Vk::version(1, 0, 0))
        .addEnabledExtensions<Magnum::Vk::Extensions::EXT::debug_utils>()
        .addEnabledExtensions({VK_KHR_SURFACE_EXTENSION_NAME,
                               "VK_KHR_win32_surface",
                               VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME});
    if (creationInfo.validation == ValidationLayers::Enabled) {
        instanceCreateInfo.addEnabledLayers({"VK_LAYER_KHRONOS_validation"});
    }
    data_->instance.create(instanceCreateInfo);
    data_->instance.populateGlobalFunctionPointers();

    data_->physicalDevice = Vk::pickDevice(data_->instance);
    CO_APP_INFO("Using device {}", data_->physicalDevice.name());

    const Vk::ExtensionProperties extensions = data_->physicalDevice.enumerateExtensionProperties();
    Vk::DeviceCreateInfo info{data_->physicalDevice, &extensions};
    info.addEnabledExtensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                               VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                               "VK_KHR_fragment_shading_rate",
                               "VK_KHR_dynamic_rendering"});

    // configure a Graphics and a Compute queue - assumes that there is a family that
    // supports both graphics and compute, which is probably not universal
    data_->graphicsQueueFamily = data_->physicalDevice.pickQueueFamily(
        Vk::QueueFlags::Type::Graphics | Vk::QueueFlags::Type::Compute);
    info.addQueues(data_->graphicsQueueFamily, {1.0f}, {data_->graphicsQueue});

    // set up the required features
    auto pnext_chain = detail::setupRequiredDeviceFeatures(info, *data_);
    info->pNext = pnext_chain.head();

    Vk::Result deviceCreateResult = data_->device.tryCreate(data_->instance, std::move(info));
    if (deviceCreateResult != Vk::Result::Success) {
        throw std::runtime_error{
            fmt::format("Could not create logical device! result = {}", deviceCreateResult)};
    }
    data_->device.populateGlobalFunctionPointers();
    // set a debug name for the logical device and queues
    nameVulkanObject(data_->device, data_->device, fmt::format("DEV_{}", data_->name));
    nameVulkanObject(data_->device, data_->graphicsQueue, fmt::format("QUE_Gfx_{}", data_->name));
    // nameVulkanObject(data_->device, data_->computeQueue, fmt::format("QUE_Comp_{}",
    // data_->name));

    setupDebugMessenger();

    data_->commandPool =
        Vk::CommandPool{data_->device, Vk::CommandPoolCreateInfo{data_->graphicsQueueFamily}};

    // delayed-init of the resource manager
    resources().setContext(*this);

    // TODO descriptorsetmanager should move to more frontend-facing object like swapchain, window,
    // or application base class
    // default layout currently only has a single uniform buffer and eight images and buffers
    Vk::DescriptorSetLayoutBinding::Flags bindless_flags{};
    bindless_flags |= Vk::DescriptorSetLayoutBinding::Flag::PartiallyBound;
    bindless_flags |= Vk::DescriptorSetLayoutBinding::Flag::UpdateAfterBind;

    // static cast is needed because Magnum does not know about this flag yet
    Vk::DescriptorSetLayoutCreateInfo::Flags layout_flags(
        static_cast<Vk::DescriptorSetLayoutCreateInfo::Flag>(
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT));

    auto all_graphics = Vk::ShaderStage{VK_SHADER_STAGE_ALL_GRAPHICS};

    Vk::DescriptorSetLayoutCreateInfo defaultLayout{
        {
            {{0, Vk::DescriptorType::UniformBuffer, 1, all_graphics, bindless_flags}},
            {{1, Vk::DescriptorType::CombinedImageSampler, 8, all_graphics, bindless_flags}},
            {{2, Vk::DescriptorType::StorageBuffer, 8, all_graphics, bindless_flags}},
        },
        layout_flags};
    static constexpr uint32_t FRAMES_IN_FLIGHT = 4;

    data_->descriptorSetManager.init(
        data_->device, data_->resources, std::move(defaultLayout), FRAMES_IN_FLIGHT);

    // create default resources
    data_->defaultMeshLayout = detail::createDefaultMeshLayout();
    data_->emptyMeshLayout = Magnum::Vk::MeshLayout{Vk::MeshPrimitive::Triangles};
    data_->defaultPipelineLayout = detail::createDefaultPipelineLayout(
        *this, resources()[data_->descriptorSetManager.layout()]);
    data_->defaultSampler = resources().createSampler("SMPL_Default", Vk::SamplerCreateInfo{});
}

Context::Context(Context &&rhs) { std::swap(rhs.data_, data_); }
Context &Context::operator=(Context &&rhs)
{
    if (this != &rhs) { std::swap(rhs.data_, data_); }
    return *this;
}

Context::~Context()
{
    if (data_) {
        data_->resources.release(data_->defaultSampler);
        CO_CORE_TRACE("Destroying Cory::Context {}", data_->name);
    }
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
        .pUserData = data_.get()};

    VkDebugUtilsMessengerEXT messenger;
    instance()->CreateDebugUtilsMessengerEXT(
        data_->instance, &dbgMessengerCreateInfo, nullptr, &messenger);
    data_->debugMessenger.wrap(messenger, [this](auto *messenger) {
        data_->instance->DestroyDebugUtilsMessengerEXT(data_->instance, messenger, nullptr);
    });
    // this seems to crash - not sure if driver or implementation bug...
    // nameRawVulkanObject(
    //    data->device.handle(), debugMessenger, fmt::format("{} Debug Messenger", data->name));
}

std::string Context::name() const { return data_->name; }

Semaphore Context::createSemaphore(std::string_view name)
{
    VkSemaphoreCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .flags = 0};

    VkSemaphore semaphore;
    THROW_ON_ERROR(device()->CreateSemaphore(data_->device, &create_info, nullptr, &semaphore),
                   "failed to create a semaphore object");

    if (!name.empty()) { nameRawVulkanObject(data_->device, semaphore, name); }

    return Semaphore{semaphore, [&device = data_->device](VkSemaphore f) {
                         device->DestroySemaphore(device, f, nullptr);
                     }};
}

Vk::Fence Context::createFence(std::string_view name, Cory::FenceCreateMode mode)
{
    Vk::Fence fence{Corrade::NoCreate};
    if (mode == FenceCreateMode::Signaled) {
        fence = Vk::Fence{data_->device, Vk::FenceCreateInfo{Vk::FenceCreateInfo::Flag::Signaled}};
    }

    fence = Vk::Fence{data_->device, Vk::FenceCreateInfo{}};
    nameVulkanObject(data_->device, fence, name);
    return fence;
}

bool Context::isHeadless() const { return data_->isHeadless; }
Vk::Instance &Context::instance() { return data_->instance; }
Magnum::Vk::DeviceProperties &Context::physicalDevice() { return data_->physicalDevice; }
Vk::Device &Context::device() { return data_->device; }
DescriptorSets &Context::descriptorSets() { return data_->descriptorSetManager; }
Vk::CommandPool &Context::commandPool() { return data_->commandPool; }
Magnum::Vk::Queue &Context::graphicsQueue() { return data_->graphicsQueue; }
uint32_t Context::graphicsQueueFamily() const { return data_->graphicsQueueFamily; }
Magnum::Vk::Queue &Context::computeQueue() { return data_->computeQueue; }
uint32_t Context::computeQueueFamily() const { return data_->computeQueueFamily; }
ResourceManager &Context::resources() { return data_->resources; }
const ResourceManager &Context::resources() const { return data_->resources; }

void Context::onVulkanDebugMessageReceived(std::function<void(const DebugMessageInfo &)> callback)
{
    data_->onVulkanDebugMessageReceived(std::move(callback));
}

const Magnum::Vk::MeshLayout &Context::defaultMeshLayout(bool empty) const
{
    return empty ? data_->emptyMeshLayout : data_->defaultMeshLayout;
}

Magnum::Vk::PipelineLayout &Context::defaultPipelineLayout()
{
    return data_->defaultPipelineLayout;
}
SamplerHandle Context::defaultSampler() const { return data_->defaultSampler; }

void ContextPrivate::receiveDebugUtilsMessage(
    DebugMessageSeverity severity,
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

    onVulkanDebugMessageReceived.invoke({.severity = severity,
                                         .messageType = messageType,
                                         .messageIdNumber = callbackData->messageIdNumber,
                                         .message = callbackData->pMessage});
    Log::GetCoreLogger()->log(level, "[VulkanDebugMsg:{}] {}", messageType, callbackData->pMessage);
}

namespace detail {

PNextChain<> setupRequiredDeviceFeatures(Vk::DeviceCreateInfo &info, ContextPrivate &data)
{
    PNextChain chain;

    //    VkPhysicalDeviceFeatures2 deviceFeatures{.sType =
    //    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    //                                             .pNext = nullptr};
    //    data.instance->GetPhysicalDeviceFeatures2(data.physicalDevice, &deviceFeatures);

    // general enabled features
    auto &enabled_features = chain.insert(VkPhysicalDeviceFeatures{
        // sample rate shading to be able to work with multisampling properly
        .sampleRateShading = VK_TRUE,
    });
    info->pEnabledFeatures = &enabled_features;

    // synchronization2
    chain.prepend(VkPhysicalDeviceSynchronization2Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
        .synchronization2 = VK_TRUE});

    // dynamic_rendering
    chain.prepend(VkPhysicalDeviceDynamicRenderingFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = VK_TRUE,
    });

    // indexing_features (required for bindless)
    chain.prepend(VkPhysicalDeviceDescriptorIndexingFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE});

    return chain;
}

VkBool32 debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                     VkDebugUtilsMessageTypeFlagsEXT messageType,
                                     const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                     void *pUserData)
{
    ContextPrivate *contextData = static_cast<ContextPrivate *>(pUserData);
    contextData->receiveDebugUtilsMessage(static_cast<DebugMessageSeverity>(messageSeverity),
                                          static_cast<DebugMessageType>(messageType),
                                          pCallbackData);
    return VK_TRUE;
}

Magnum::Vk::PipelineLayout createDefaultPipelineLayout(Context &ctx,
                                                       Vk::DescriptorSetLayout &descriptorSetLayout)
{
    // use max guaranteed memory of 128 bytes, for all shaders
    VkPushConstantRange pushConstantRange{
        .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL, .offset = 0, .size = 128};

    // create pipeline layout
    Vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        descriptorSetLayout, descriptorSetLayout, descriptorSetLayout, descriptorSetLayout};
    pipelineLayoutCreateInfo->pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo->pPushConstantRanges = &pushConstantRange;
    return Vk::PipelineLayout(ctx.device(), pipelineLayoutCreateInfo);
}

Magnum::Vk::MeshLayout createDefaultMeshLayout()
{
    static constexpr uint32_t binding{0};
    return Vk::MeshLayout{Vk::MeshPrimitive::Triangles}
        .addBinding(binding, 10 * sizeof(float))
        .addAttribute(0, binding, Vk::VertexFormat::Vector3, 0)
        .addAttribute(1, binding, Vk::VertexFormat::Vector3, 3 * sizeof(float))
        .addAttribute(2, binding, Vk::VertexFormat::Vector4, 6 * sizeof(float));
}

} // namespace detail

} // namespace Cory