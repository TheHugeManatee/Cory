#include <Cory/Renderer/Context.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/VulkanUtils.hpp>

#include <KDGpu/graphics_api.h>
#include <KDGpu/instance.h>
#include <KDGpu/resource_manager.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>
#include <KDGpuKDGui/view.h>
#include <KDGui/gui_application.h>
#include <vulkan/vulkan_win32.h>

namespace Cory {

using InstanceHandle = KDGpu::Handle<KDGpu::Instance_t>;
using DeviceHandle = KDGpu::Handle<KDGpu::Device_t>;
using AdapterHandle = KDGpu::Handle<KDGpu::Adapter_t>;
using QueueHandle = KDGpu::Handle<KDGpu::Queue_t>;
using SwapchainHandle = KDGpu::Handle<KDGpu::Swapchain_t>;
using SurfaceHandle = KDGpu::Handle<KDGpu::Surface_t>;
using TextureHandle = KDGpu::Handle<KDGpu::Texture_t>;
using TextureViewHandle = KDGpu::Handle<KDGpu::TextureView_t>;
using ShaderModuleHandle = KDGpu::Handle<KDGpu::ShaderModule_t>;
using RenderPassHandle = KDGpu::Handle<KDGpu::RenderPass_t>;
using PipelineLayoutHandle = KDGpu::Handle<KDGpu::PipelineLayout_t>;
using GraphicsPipelineHandle = KDGpu::Handle<KDGpu::GraphicsPipeline_t>;
using ComputePipelineHandle = KDGpu::Handle<KDGpu::ComputePipeline_t>;
using RenderPassCommandRecorderHandle = KDGpu::Handle<KDGpu::RenderPassCommandRecorder_t>;
using GpuSemaphoreHandle = KDGpu::Handle<KDGpu::GpuSemaphore_t>;
using ComputePassCommandRecorderHandle = KDGpu::Handle<KDGpu::ComputePassCommandRecorder_t>;
using CommandBufferHandle = KDGpu::Handle<KDGpu::CommandBuffer_t>;
using BindGroupHandle = KDGpu::Handle<KDGpu::BindGroup_t>;
using BindGroupLayoutHandle = KDGpu::Handle<KDGpu::BindGroupLayout_t>;
using FenceHandle = KDGpu::Handle<KDGpu::Fence_t>;

struct ContextPrivate {
    std::string name;
    bool isHeadless{true};

    KDGpu::GraphicsApi api;
    KDGpu::Instance instance;

    KDGpu::Surface surface;
    KDGpu::Adapter *adapter;
    KDGpu::Device device;
    KDGpu::Queue queue;

    BasicVkObjectWrapper<VkDebugUtilsMessengerEXT> debugMessenger{};

    void receiveDebugUtilsMessage(DebugMessageSeverity severity,
                                  DebugMessageType messageType,
                                  const VkDebugUtilsMessengerCallbackDataEXT *callbackData);
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
    KDGpu::InstanceOptions instanceOptions = {
        .applicationName = KDGui::GuiApplication::instance()->applicationName(),
        .applicationVersion = KDGPU_MAKE_API_VERSION(0, 1, 0, 0),
        .apiVersion = KDGPU_MAKE_API_VERSION(0, 1, 3, 0),
        .layers = {},
        .extensions = {VK_KHR_SURFACE_EXTENSION_NAME,
                       VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
                       VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME},
    };
    if (creationInfo.validation == ValidationLayers::Enabled) {
        instanceOptions.layers.push_back("VK_LAYER_KHRONOS_validation");
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

KDGpu::GpuSemaphore Context::createSemaphore(std::string_view name)
{
    auto &device = data_->device;
    KDGpu::GpuSemaphoreOptions options{
        .label = name,
    };

    return device.createGpuSemaphore(options);
}

KDGpu::Fence Context::createFence(std::string_view name, FenceCreateMode mode)
{
    auto &device = data_->device;

    KDGpu::FenceOptions options{
        .label = name,
        .createSignalled = (mode == FenceCreateMode::Signaled),
        .externalFenceHandleType{KDGpu::ExternalFenceHandleTypeFlagBits::None},
    };
    return device.createFence(options);
}

bool Context::isHeadless() const { return data_->isHeadless; }

KDGpu::AdapterAndDevice Context::createDefaultDevice(const KDGpu::Surface &surface,
                                                     DeviceFeatures features,
                                                     KDGpu::AdapterDeviceType deviceType) const
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
KDGpu::AdapterFeatures Context::getRequiredFeatures() const
{
    KDGpu::AdapterFeatures features{};
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

KDGpu::Surface Context::createSurface(std::string_view name, KDGpuKDGui::View &view)
{
    auto &instance = data_->instance;
    auto &device = data_->device;
    auto surface = view.createSurface(instance);

    // Create a device and a queue to use
    auto defaultDevice = createDefaultDevice(surface);
    data_->adapter = defaultDevice.adapter;
    device = std::move(defaultDevice.device);
    CO_CORE_ASSERT(!device.queues().empty(), "Device has no queues!");
    data_->queue = data_->device.queues()[0];

    data_->isHeadless = false;

    return surface;
}

KDGpu::Instance &Context::instance() { return data_->instance; }
KDGpu::GraphicsApi &Context::graphicsApi() { return data_->api; }
const KDGpu::AdapterProperties &Context::physicalDevice() { return data_->adapter->properties(); }
KDGpu::Device &Context::device() { return data_->device; }

KDGpu::Queue &Context::graphicsQueue() { return data_->queue; }

KDGpu::VulkanResourceManager &Context::resources() { return *data_->api.resourceManager(); }
const KDGpu::VulkanResourceManager &Context::resources() const
{
    return *data_->api.resourceManager();
}

} // namespace Cory