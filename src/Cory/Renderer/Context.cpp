
#include <Cory/Renderer/Context.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
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

using InstanceHandle = Gpu::Handle<Gpu::Instance_t>;
using DeviceHandle = Gpu::Handle<Gpu::Device_t>;
using AdapterHandle = Gpu::Handle<Gpu::Adapter_t>;
using QueueHandle = Gpu::Handle<Gpu::Queue_t>;
using SwapchainHandle = Gpu::Handle<Gpu::Swapchain_t>;
using SurfaceHandle = Gpu::Handle<Gpu::Surface_t>;
using TextureHandle = Gpu::Handle<Gpu::Texture_t>;
using TextureViewHandle = Gpu::Handle<Gpu::TextureView_t>;
using ShaderModuleHandle = Gpu::Handle<Gpu::ShaderModule_t>;
using RenderPassHandle = Gpu::Handle<Gpu::RenderPass_t>;
using PipelineLayoutHandle = Gpu::Handle<Gpu::PipelineLayout_t>;
using GraphicsPipelineHandle = Gpu::Handle<Gpu::GraphicsPipeline_t>;
using ComputePipelineHandle = Gpu::Handle<Gpu::ComputePipeline_t>;
using RenderPassCommandRecorderHandle = Gpu::Handle<Gpu::RenderPassCommandRecorder_t>;
using GpuSemaphoreHandle = Gpu::Handle<Gpu::GpuSemaphore_t>;
using ComputePassCommandRecorderHandle = Gpu::Handle<Gpu::ComputePassCommandRecorder_t>;
using CommandBufferHandle = Gpu::Handle<Gpu::CommandBuffer_t>;
using BindGroupHandle = Gpu::Handle<Gpu::BindGroup_t>;
using BindGroupLayoutHandle = Gpu::Handle<Gpu::BindGroupLayout_t>;
using FenceHandle = Gpu::Handle<Gpu::Fence_t>;

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
    }
    data_->instance = data_->api.createInstance(instanceOptions);
}

Context::Context(Context &&rhs) noexcept { std::swap(rhs.data_, data_); }
Context &Context::operator=(Context &&rhs) noexcept
{
    if (this != &rhs) { std::swap(rhs.data_, data_); }
    return *this;
}

Context::~Context()
{
    if (data_) { CO_CORE_TRACE("Destroying Cory::Context {}", data_->name); }
}

std::string Context::name() const { return data_->name; }

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
    Gpu::VulkanGraphicsApi::setCustomValidationHandler(ContextPrivate::receiveDebugUtilsMessage);
}

bool Context::isHeadless() const { return data_->isHeadless; }

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
    CO_CORE_INFO("Supported adapter extensions:");
    for (const auto &extension : adapterExtensions) {
        CO_CORE_INFO("  - {} Version {}", extension.name, extension.version);
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
}

Gpu::Instance &Context::instance() { return data_->instance; }
Gpu::GraphicsApi &Context::graphicsApi() { return data_->api; }
const Gpu::AdapterProperties &Context::physicalDevice() { return data_->adapter->properties(); }
Gpu::Device &Context::device() { return data_->device; }

Gpu::Queue &Context::graphicsQueue() { return data_->queue; }

Gpu::VulkanResourceManager &Context::resources() { return *data_->api.resourceManager(); }
const Gpu::VulkanResourceManager &Context::resources() const
{
    return *data_->api.resourceManager();
}
ShaderManager &Context::shaders() { return data_->shaders; }
const ShaderManager &Context::shaders() const { return data_->shaders; }

void ContextPrivate::receiveDebugUtilsMessage(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData)
{
    if (!validationMessageCallback) { return; }

    DebugMessageInfo info{.severity = static_cast<DebugMessageSeverity>(messageSeverity),
                          .messageType = static_cast<DebugMessageType>(messageTypes),
                          .messageIdNumber = pCallbackData->messageIdNumber,
                          .message = pCallbackData->pMessage ? pCallbackData->pMessage : ""};
    validationMessageCallback(info);
}

} // namespace Cory