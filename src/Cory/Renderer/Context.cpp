#include "DescriptorSets.hpp"

#include <Cory/Renderer/Context.hpp>

#include <Cory/Base/Debugger.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <Cory/Renderer/VulkanUtils.hpp>

#include <KDGpu/graphics_api.h>
#include <KDGpu/instance.h>
#include <KDGpu/resource_manager.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>
#include <KDGpuKDGui/view.h>
#include <KDGui/gui_application.h>

#include <vulkan/vulkan_win32.h>

namespace Cory {

struct ContextPrivate {
    std::string name;
    bool isHeadless{true};

    Gpu::GraphicsApi api;
    Gpu::Instance instance;

    Gpu::Surface surface;
    Gpu::Adapter *adapter;
    Gpu::Device device;
    Gpu::Queue queue;

    ShaderManager shaders;
    DescriptorSets descriptorSets;
    std::unique_ptr<PipelineCache> pipelineCache;

    inline static Function<void(const DebugMessageInfo &)> validationMessageCallback;

    static void receiveDebugUtilsMessage(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                         VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                         const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData);
};

Context::Context(ContextCreationInfo creationInfo)
    : data_{std::make_unique<ContextPrivate>()}
{
    data_->name = "CCtx";
    const auto app_name{"Cory-based Vulkan Application"};

    // for dynamic rendering, we need:
    //  - KHR_get_physical_device_properties2 instance extension
    //  - KHR_dynamic_rendering device extension
    //  - enable dynamic_rendering feature via VkPhysicalDeviceDynamicRenderingFeatures
    Gpu::InstanceOptions instanceOptions = {
        .applicationName = app_name,
        .applicationVersion = KDGPU_MAKE_API_VERSION(0, 1, 0, 0),
        .apiVersion = KDGPU_MAKE_API_VERSION(0, 1, 3, 0),
        .layers = {},
        .extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        }};
    if (creationInfo.validation == ValidationLayers::Enabled) {
        instanceOptions.layers.push_back("VK_LAYER_KHRONOS_validation");
        instanceOptions.extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        Gpu::VulkanGraphicsApi::setCustomValidationHandler(
            ContextPrivate::receiveDebugUtilsMessage);
    }
    data_->instance = data_->api.createInstance(instanceOptions);
    data_->shaders.setContext(*this);
}

Context::Context(Context &&rhs) noexcept
{
    std::swap(rhs.data_, data_);
}
Context &Context::operator=(Context &&rhs) noexcept
{
    if (this != &rhs) {
        std::swap(rhs.data_, data_);
    }
    return *this;
}

Context::~Context()
{
    if (data_) {
        CO_CORE_TRACE("Destroying Cory::Context {}", data_->name);
    }
}

std::string Context::name() const
{
    return data_->name;
}

Gpu::GpuSemaphore Context::createSemaphore(std::string_view name)
{
    auto &device = data_->device;
    Gpu::GpuSemaphoreOptions options{
        .label = name,
    };

    return device.createGpuSemaphore(options);
}

Gpu::Fence Context::createFence(std::string_view name, FenceCreateMode mode)
{
    auto &device = data_->device;

    Gpu::FenceOptions options{
        .label = name,
        .createSignalled = (mode == FenceCreateMode::Signaled),
        .externalFenceHandleType{Gpu::ExternalFenceHandleTypeFlagBits::None},
    };
    return device.createFence(options);
}

void Context::onVulkanDebugMessageReceived(Function<void(const DebugMessageInfo &)> callback)
{
    ContextPrivate::validationMessageCallback = std::move(callback);
}

bool Context::isHeadless() const
{
    return data_->isHeadless;
}

Gpu::AdapterAndDevice Context::createDefaultDevice(const Gpu::Surface &surface,
                                                   DeviceFeatures features,
                                                   Gpu::AdapterDeviceType deviceType) const
{
    using namespace KDGpu;

    // Enumerate the adapters (physical devices) and select one to use. Here we look for
    // a discrete GPU. In a real app, we could fallback to an integrated one.
    Adapter *selectedAdapter = data_->instance.selectAdapter(deviceType);
    if (!selectedAdapter) {
        CO_CORE_FATAL("Unable to find a suitable Adapter. Aborting...");
        return {};
    }

    auto queueTypes = selectedAdapter->queueTypes();
    const bool hasGraphicsAndCompute = queueTypes[0].supportsFeature(
        QueueFlags(QueueFlagBits::GraphicsBit) | QueueFlags(QueueFlagBits::ComputeBit));
    CO_CORE_INFO("Queue family 0 graphics and compute support: {}", hasGraphicsAndCompute);

    // We are now able to query the adapter for swapchain properties and presentation support
    // with the window surface
    const auto swapchainProperties = selectedAdapter->swapchainProperties(surface);
    CO_CORE_INFO("Supported swapchain present modes:");
    for (const auto &mode : swapchainProperties.presentModes) {
        CO_CORE_INFO("  - {}", presentModeToString(mode));
    }

    const bool supportsPresentation =
        selectedAdapter->supportsPresentation(surface, 0); // Query about the 1st queue type
    CO_CORE_INFO("Queue family 0 supports presentation: {}", supportsPresentation);

    const auto adapterExtensions = selectedAdapter->extensions();
    CO_CORE_TRACE("Supported adapter extensions:");
    for ([[maybe_unused]] const auto &extension : adapterExtensions) {
        CO_CORE_TRACE("  - {} Version {}", extension.name, extension.version);
    }

    if (!supportsPresentation || !hasGraphicsAndCompute) {
        CO_CORE_FATAL("Selected adapter queue family 0 does not meet requirements. Aborting.");
        return {};
    }

    const bool supportsMultiView = selectedAdapter->features().multiView;
    CO_CORE_INFO("Supports multiview: {}", supportsMultiView);

    const bool supportsUBOIndexing =
        selectedAdapter->features().shaderUniformBufferArrayNonUniformIndexing &&
        selectedAdapter->features().bindGroupBindingUniformBufferUpdateAfterBind;
    CO_CORE_INFO("Supports Uniform Bind Group Dynamic Indexing: {}", supportsUBOIndexing);

    const bool supportsAccelerationStructures = selectedAdapter->features().accelerationStructures;
    CO_CORE_INFO("Supports acceleration structures: {}", supportsAccelerationStructures);

    const bool supportsRayTracing = selectedAdapter->features().rayTracingPipeline;
    CO_CORE_INFO("Supports raytracing: {}", supportsRayTracing);

    const bool supportsMeshShader = selectedAdapter->features().meshShader;
    const bool supportsTaskShader = selectedAdapter->features().taskShader;
    CO_CORE_INFO("Supports meshShader: {}", supportsMeshShader);
    CO_CORE_INFO("Supports taskShader: {}", supportsTaskShader);

    const bool supportsHostToImageCopy = selectedAdapter->features().hostImageCopy;
    CO_CORE_INFO("Supports host to image copy: {}", supportsHostToImageCopy);

    // Now we can create a device from the selected adapter that we can then use to interact
    // with the GPU.

    auto device = selectedAdapter->createDevice(DeviceOptions{
        .label = "Main Device",
        .apiVersion = KDGPU_MAKE_API_VERSION(0, 1, 3, 0),
        .layers = {},
        .extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                       VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                       VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
                       VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME},
        .queues = {},
        .requestedFeatures =
            features == DeviceFeatures::All ? selectedAdapter->features() : getRequiredFeatures(),
        .adapterGroup = {},
    });

    return {selectedAdapter, std::move(device)};
}
Gpu::AdapterFeatures Context::getRequiredFeatures() const
{
    Gpu::AdapterFeatures features{};
    features.sampleRateShading = true;
    // synchronization2 is automatically enabled by kdgpu

    // TODO dynamic_rendering
    // VkPhysicalDeviceDynamicRenderingFeatures{
    //     .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
    //     .dynamicRendering = VK_TRUE,
    // };

    features.bindGroupBindingUniformBufferUpdateAfterBind = true;
    features.bindGroupBindingSampledImageUpdateAfterBind = true;
    features.bindGroupBindingStorageBufferUpdateAfterBind = true;
    features.bindGroupBindingPartiallyBound = true;
    features.runtimeBindGroupArray = true;

    return features;
}

void Context::setupDevice(const Gpu::Surface &surface)
{
    auto &device = data_->device;

    CO_CORE_ASSERT(data_->adapter == nullptr,
                   "Device already created! Multiple windows are not currently supported.");

    // Create a device and a queue to use
    auto defaultDevice = createDefaultDevice(surface);
    data_->adapter = defaultDevice.adapter;
    device = std::move(defaultDevice.device);
    CO_CORE_ASSERT(!device.queues().empty(), "Device has no queues!");
    data_->queue = data_->device.queues()[0];

    data_->isHeadless = false;

    data_->pipelineCache = std::make_unique<PipelineCache>(
        data_->api.resourceManager(), data_->device.handle(), &data_->shaders);

    Gpu::ResourceBindingFlags bindless_flags;
    bindless_flags |= Gpu::ResourceBindingFlagBits::PartiallyBoundBit;
    bindless_flags |= Gpu::ResourceBindingFlagBits::UpdateAfterBindBit;

    using BindPoints = DescriptorSets::BindPoints;

    data_->descriptorSets.init(
        data_->device,
        Gpu::BindGroupLayoutOptions{
            .label = "Default Bind Group Layout",
            .bindings = {
                {
                    {
                        .binding = std::to_underlying(BindPoints::UniformBufferObject),
                        .count = 1,
                        .resourceType = Gpu::ResourceBindingType::UniformBuffer,
                        .shaderStages = Gpu::ShaderStageFlagBits::All,
                        .flags = bindless_flags,
                    },
                    {
                        .binding = std::to_underlying(BindPoints::CombinedImageSampler),
                        .count = 8,
                        .resourceType = Gpu::ResourceBindingType::CombinedImageSampler,
                        .shaderStages = Gpu::ShaderStageFlagBits::All,
                        .flags = bindless_flags,
                    },
                    {
                        .binding = std::to_underlying(BindPoints::StorageBuffer),
                        .count = 8,
                        .resourceType = Gpu::ResourceBindingType::StorageBuffer,
                        .shaderStages = Gpu::ShaderStageFlagBits::All,
                        .flags = bindless_flags,
                    },
                },
            }});
}

void Context::setupHeadlessDevice()
{
    CO_CORE_ASSERT(data_->adapter == nullptr,
                   "Device already created! Multiple devices are not currently supported.");

    // Enumerate all adapters
    auto adapters = data_->instance.adapters();
    if (adapters.empty()) {
        CO_CORE_FATAL("No adapters found. Aborting...");
        return;
    }

    // Prefer discrete GPU, otherwise pick the first adapter
    Gpu::Adapter *selectedAdapter = nullptr;
    for (auto &adapter : adapters) {
        if (adapter->properties().deviceType == Gpu::AdapterDeviceType::DiscreteGpu) {
            selectedAdapter = adapter;
            break;
        }
    }
    if (!selectedAdapter) {
        selectedAdapter = adapters[0];
    }

    CO_CORE_INFO("Selected adapter: {}", selectedAdapter->properties().deviceName);

    auto queueTypes = selectedAdapter->queueTypes();
    const bool hasGraphicsAndCompute =
        queueTypes[0].supportsFeature(Gpu::QueueFlags(Gpu::QueueFlagBits::GraphicsBit) |
                                      Gpu::QueueFlags(Gpu::QueueFlagBits::ComputeBit));
    CO_CORE_INFO("Queue family 0 graphics and compute support: {}", hasGraphicsAndCompute);

    if (!hasGraphicsAndCompute) {
        CO_CORE_FATAL("Selected adapter queue family 0 does not meet requirements. Aborting.");
        return;
    }

    // Create device
    auto device = selectedAdapter->createDevice(Gpu::DeviceOptions{
        .label = "Headless Device",
        .apiVersion = KDGPU_MAKE_API_VERSION(0, 1, 3, 0),
        .layers = {},
        .extensions = {VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                       VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
                       VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME},
        .queues = {},
        .requestedFeatures = getRequiredFeatures(),
        .adapterGroup = {},
    });

    data_->adapter = selectedAdapter;
    data_->device = std::move(device);
    CO_CORE_ASSERT(!data_->device.queues().empty(), "Device has no queues!");
    data_->queue = data_->device.queues()[0];

    data_->isHeadless = true;

    data_->pipelineCache = std::make_unique<PipelineCache>(
        data_->api.resourceManager(), data_->device.handle(), &data_->shaders);
}

Gpu::Instance &Context::instance()
{
    return data_->instance;
}
Gpu::GraphicsApi &Context::graphicsApi()
{
    return data_->api;
}
const Gpu::AdapterProperties &Context::physicalDevice()
{
    return data_->adapter->properties();
}
Gpu::Device &Context::device()
{
    return data_->device;
}

Gpu::Queue &Context::graphicsQueue()
{
    return data_->queue;
}

Gpu::VulkanResourceManager &Context::resources()
{
    return *data_->api.resourceManager();
}
const Gpu::VulkanResourceManager &Context::resources() const
{
    return *data_->api.resourceManager();
}
PipelineCache &Context::pipelineCache()
{
    CO_CORE_ASSERT(data_->pipelineCache,
                   "Pipeline cache not ready - likely device was not created yet");
    return *data_->pipelineCache;
}

ShaderManager &Context::shaders()
{
    return data_->shaders;
}
const ShaderManager &Context::shaders() const
{
    return data_->shaders;
}

DescriptorSets &Context::descriptors()
{
    return data_->descriptorSets;
}
const DescriptorSets &Context::descriptors() const
{
    return data_->descriptorSets;
}

void ContextPrivate::receiveDebugUtilsMessage(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData)
{
    DebugMessageInfo info{.severity = static_cast<DebugMessageSeverity>(messageSeverity),
                          .messageType = static_cast<DebugMessageType>(messageTypes),
                          .messageIdNumber = pCallbackData->messageIdNumber,
                          .message = pCallbackData->pMessage ? pCallbackData->pMessage : ""};

    if (validationMessageCallback) {
        validationMessageCallback(info);
        return;
    }
    switch (info.severity) {
    case DebugMessageSeverity::Verbose:
        CO_CORE_TRACE("Vulkan Validation: {}", pCallbackData->pMessage);
        break;
    case DebugMessageSeverity::Info:
        CO_CORE_INFO("Vulkan Validation: {}", pCallbackData->pMessage);
        break;
    case DebugMessageSeverity::Warning:
        CO_CORE_WARN("Vulkan Validation: {}", pCallbackData->pMessage);
        BreakpointIfDebugging();
        break;
    case DebugMessageSeverity::Error:
        CO_CORE_ERROR("Vulkan Validation: {}", pCallbackData->pMessage);
        BreakpointIfDebugging();
        break;
    }
}

} // namespace Cory