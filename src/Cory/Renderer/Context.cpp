#include "DescriptorSets.hpp"

#include <Cory/Renderer/AsyncUploader.hpp>
#include <Cory/Renderer/Context.hpp>

#include <Cory/Base/Debugger.hpp>
#include <Cory/Base/FileWatchManager.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/PipelineCache.hpp>
#include <Cory/Renderer/ShaderManager.hpp>
#include <Cory/Renderer/VulkanUtils.hpp>

#include <KDGpu/instance.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>
#include <KDGpuKDGui/view.h>
#include <KDGui/gui_application.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <vulkan/vulkan_xcb.h>
#endif

namespace Cory {

namespace {

struct QueueFamilySelection {
    uint32_t graphicsComputeFamily{0};
    std::optional<uint32_t> transferFamily{};
};

std::optional<uint32_t> findQueueFamilyWithFlags(const Gpu::Adapter &adapter,
                                                 Gpu::QueueFlags requiredFlags,
                                                 const Gpu::Surface *surface = nullptr)
{
    auto queueTypes = adapter.queueTypes();
    for (uint32_t i = 0; i < queueTypes.size(); ++i) {
        const bool supportsRequiredFlags = queueTypes[i].supportsFeature(requiredFlags);
        if (!supportsRequiredFlags) {
            continue;
        }
        const bool requiresPresentation = surface != nullptr;
        const bool supportsPresentation =
            !requiresPresentation || adapter.supportsPresentation(*surface, i);
        if (supportsPresentation) {
            return i;
        }
    }
    return std::nullopt;
}

std::vector<Gpu::QueueRequest> buildQueueRequests(const QueueFamilySelection &selection)
{
    std::vector<Gpu::QueueRequest> requests;
    requests.push_back(Gpu::QueueRequest{
        .queueTypeIndex = selection.graphicsComputeFamily, .count = 1, .priorities = {1.0f}});
    if (selection.transferFamily && *selection.transferFamily != selection.graphicsComputeFamily) {
        requests.push_back(Gpu::QueueRequest{
            .queueTypeIndex = *selection.transferFamily, .count = 1, .priorities = {0.8f}});
    }
    return requests;
}

Gpu::Queue *findQueueByTypeIndex(std::span<Gpu::Queue> queues, uint32_t queueTypeIndex)
{
    for (auto &queue : queues) {
        if (queue.queueTypeIndex() == queueTypeIndex) {
            return &queue;
        }
    }
    return nullptr;
}

Function<void(const DebugMessageInfo &)> &validationMessageCallback()
{
    static auto *callback = new Function<void(const DebugMessageInfo &)>{};
    return *callback;
}

uint32_t makeApiVersion(uint32_t variant, uint32_t major, uint32_t minor, uint32_t patch)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
    return KDGPU_MAKE_API_VERSION(variant, major, minor, patch);
#pragma clang diagnostic pop
}

} // namespace

struct ContextPrivate {
    std::string name;
    bool isHeadless{true};

    FileWatchManager fileWatchManager;

    Gpu::GraphicsApi api;
    Gpu::Instance instance;

    Gpu::Surface surface;
    Gpu::Adapter *adapter;
    Gpu::Device device;
    Gpu::Queue *graphicsQueue{nullptr};
    Gpu::Queue *computeQueue{nullptr};
    Gpu::Queue *transferQueue{nullptr};
    uint32_t graphicsQueueTypeIndex{std::numeric_limits<uint32_t>::max()};
    uint32_t computeQueueTypeIndex{std::numeric_limits<uint32_t>::max()};
    uint32_t transferQueueTypeIndex{std::numeric_limits<uint32_t>::max()};
    std::unique_ptr<AsyncUploader> uploader;

    ShaderManager shaders;
    DescriptorSets descriptorSets;
    std::unique_ptr<PipelineCache> pipelineCache;

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
        .applicationVersion = makeApiVersion(0, 1, 0, 0),
        .apiVersion = makeApiVersion(0, 1, 3, 0),
        .layers = {},
        .extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        }};
#if defined(_WIN32)
    instanceOptions.extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
    instanceOptions.extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
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
    validationMessageCallback() = std::move(callback);
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
    if (!selectedAdapter && deviceType == AdapterDeviceType::Default) {
        selectedAdapter = data_->instance.selectAdapter(AdapterDeviceType::Cpu);
    }
    if (!selectedAdapter && deviceType == AdapterDeviceType::Default) {
        selectedAdapter = data_->instance.selectAdapter(AdapterDeviceType::VirtualGpu);
    }
    if (!selectedAdapter && deviceType == AdapterDeviceType::Default) {
        selectedAdapter = data_->instance.selectAdapter(AdapterDeviceType::Other);
    }
    if (!selectedAdapter) {
        CO_CORE_FATAL("Unable to find a suitable Adapter. Aborting...");
        return {};
    }

    const auto graphicsComputeFamily = findQueueFamilyWithFlags(
        *selectedAdapter,
        QueueFlags(QueueFlagBits::GraphicsBit) | QueueFlags(QueueFlagBits::ComputeBit),
        &surface);
    if (!graphicsComputeFamily.has_value()) {
        CO_CORE_FATAL("Selected adapter has no queue family supporting "
                      "graphics+compute+presentation. Aborting.");
        return {};
    }
    CO_CORE_TRACE("Selected graphics queue family: {}", *graphicsComputeFamily);

    const auto transferFamily =
        findQueueFamilyWithFlags(*selectedAdapter, QueueFlags(QueueFlagBits::TransferBit), nullptr);
    if (transferFamily.has_value()) {
        CO_CORE_TRACE("Selected transfer queue family: {}", *transferFamily);
    }

    // We are now able to query the adapter for swapchain properties and presentation support
    // with the window surface
    const auto swapchainProperties = selectedAdapter->swapchainProperties(surface);
    CO_CORE_TRACE("Supported swapchain present modes ({}):",
                  swapchainProperties.presentModes.size());
    for ([[maybe_unused]] const auto &mode : swapchainProperties.presentModes) {
        CO_CORE_TRACE("||  - {}", presentModeToString(mode));
    }

    const bool supportsPresentation =
        selectedAdapter->supportsPresentation(surface, *graphicsComputeFamily);
    CO_CORE_TRACE("Selected graphics queue family supports presentation: {}", supportsPresentation);

    const auto adapterExtensions = selectedAdapter->extensions();
    CO_CORE_TRACE("Supported adapter extensions ({}):", adapterExtensions.size());
    for ([[maybe_unused]] const auto &extension : adapterExtensions) {
        CO_CORE_TRACE("||  - {} Version {}", extension.name, extension.version);
    }

    if (!supportsPresentation) {
        CO_CORE_FATAL("Selected graphics queue family does not support presentation. Aborting.");
        return {};
    }

    data_->graphicsQueueTypeIndex = *graphicsComputeFamily;
    data_->computeQueueTypeIndex = *graphicsComputeFamily;
    data_->transferQueueTypeIndex = transferFamily.value_or(*graphicsComputeFamily);
    CO_CORE_TRACE("Feature support: ");
    [[maybe_unused]] const bool supportsMultiView = selectedAdapter->features().multiView;
    CO_CORE_TRACE("|| - multiview: {}", supportsMultiView);

    [[maybe_unused]] const bool supportsUBOIndexing =
        selectedAdapter->features().shaderUniformBufferArrayNonUniformIndexing &&
        selectedAdapter->features().bindGroupBindingUniformBufferUpdateAfterBind;
    CO_CORE_TRACE("|| - Uniform Bind Group Dynamic Indexing: {}", supportsUBOIndexing);

    [[maybe_unused]] const bool supportsAccelerationStructures =
        selectedAdapter->features().accelerationStructures;
    CO_CORE_TRACE("|| - acceleration structures: {}", supportsAccelerationStructures);

    [[maybe_unused]] const bool supportsRayTracing = selectedAdapter->features().rayTracingPipeline;
    CO_CORE_TRACE("|| - raytracing: {}", supportsRayTracing);

    [[maybe_unused]] const bool supportsMeshShader = selectedAdapter->features().meshShader;
    [[maybe_unused]] const bool supportsTaskShader = selectedAdapter->features().taskShader;
    CO_CORE_TRACE("|| - meshShader: {}", supportsMeshShader);
    CO_CORE_TRACE("|| - taskShader: {}", supportsTaskShader);

    [[maybe_unused]] const bool supportsHostToImageCopy = selectedAdapter->features().hostImageCopy;
    CO_CORE_TRACE("|| - host to image copy: {}", supportsHostToImageCopy);

    // Now we can create a device from the selected adapter that we can then use to interact
    // with the GPU.

    auto device = selectedAdapter->createDevice(DeviceOptions{
        .label = "Main Device",
        .apiVersion = makeApiVersion(0, 1, 3, 0),
        .layers = {},
        .extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                       VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                       VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
                       VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
                       VK_EXT_SHADER_OBJECT_EXTENSION_NAME},
        .queues = buildQueueRequests(QueueFamilySelection{
            .graphicsComputeFamily = data_->graphicsQueueTypeIndex,
            .transferFamily = transferFamily,
        }),
        .requestedFeatures =
            features == DeviceFeatures::All ? selectedAdapter->features() : getRequiredFeatures(),
        .adapterGroup = {},
    });

    return {selectedAdapter, std::move(device)};
}
Gpu::AdapterFeatures Context::getRequiredFeatures()
{
    Gpu::AdapterFeatures features{};
    // Dynamic rendering extensions to avoid pipeline permutations
    features.dynamicRendering = true;
    features.shaderObjectDynamicRendering = true;
    features.logicOp = true;

    // synchronization2 is automatically enabled by kdgpu

    // Features for modern bindless resource access
    features.bindGroupBindingUniformBufferUpdateAfterBind = true;
    features.bindGroupBindingSampledImageUpdateAfterBind = true;
    features.bindGroupBindingStorageBufferUpdateAfterBind = true;
    features.bindGroupBindingStorageImageUpdateAfterBind = true;
    features.bindGroupBindingPartiallyBound = true;
    features.runtimeBindGroupArray = true;
    features.shaderSampledImageArrayNonUniformIndexing = true;
    features.shaderStorageBufferArrayNonUniformIndexing = true;
    features.bufferDeviceAddress = true;

    // Enable shader storage image multisampling
    features.shaderStorageImageMultisample = true;
    // Sample rate shading to enable MSAA on e.g. raymarched volumes
    features.sampleRateShading = true;
    // Other features
    features.wideLines = true;
    features.largePoints = true;
    return features;
}

void Context::setupDeviceFromSurface(const Gpu::Surface &surface)
{
    auto &device = data_->device;

    CO_CORE_ASSERT(data_->adapter == nullptr,
                   "Device already created! Multiple windows are not currently supported.");

    // Create a device and a queue to use
    auto defaultDevice = createDefaultDevice(surface);
    data_->adapter = defaultDevice.adapter;
    device = std::move(defaultDevice.device);
    if (!data_->adapter || device.queues().empty()) {
        CO_CORE_ERROR("Device has no queues!");
        throw std::runtime_error("Device has no queues!");
    }
    auto queues = data_->device.queues();
    auto *graphics = findQueueByTypeIndex(queues, data_->graphicsQueueTypeIndex);
    CO_CORE_ASSERT(graphics != nullptr,
                   "Failed to resolve graphics queue from selected queue family");
    data_->graphicsQueue = graphics;
    data_->computeQueue = graphics;
    data_->transferQueue = graphics;
    if (auto *transfer = findQueueByTypeIndex(queues, data_->transferQueueTypeIndex)) {
        data_->transferQueue = transfer;
    }

    data_->isHeadless = false;

    data_->pipelineCache = std::make_unique<PipelineCache>(
        data_->api.resourceManager(), data_->device.handle(), &data_->shaders);
    data_->uploader = std::make_unique<AsyncUploader>(*this);

    setupDescriptors();
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

    const auto graphicsComputeFamily =
        findQueueFamilyWithFlags(*selectedAdapter,
                                 Gpu::QueueFlags(Gpu::QueueFlagBits::GraphicsBit) |
                                     Gpu::QueueFlags(Gpu::QueueFlagBits::ComputeBit),
                                 nullptr);
    if (!graphicsComputeFamily.has_value()) {
        CO_CORE_FATAL(
            "Selected adapter has no queue family supporting graphics+compute. Aborting.");
        return;
    }
    CO_CORE_INFO("Selected graphics queue family: {}", *graphicsComputeFamily);
    const auto transferFamily = findQueueFamilyWithFlags(
        *selectedAdapter, Gpu::QueueFlags(Gpu::QueueFlagBits::TransferBit), nullptr);
    if (transferFamily.has_value()) {
        CO_CORE_INFO("Selected transfer queue family: {}", *transferFamily);
    }
    data_->graphicsQueueTypeIndex = *graphicsComputeFamily;
    data_->computeQueueTypeIndex = *graphicsComputeFamily;
    data_->transferQueueTypeIndex = transferFamily.value_or(*graphicsComputeFamily);

    // Create device
    auto device = selectedAdapter->createDevice(Gpu::DeviceOptions{
        .label = "Headless Device",
        .apiVersion = makeApiVersion(0, 1, 3, 0),
        .layers = {},
        .extensions = {VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                       VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
                       VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
                       VK_EXT_SHADER_OBJECT_EXTENSION_NAME},
        .queues = buildQueueRequests(QueueFamilySelection{
            .graphicsComputeFamily = data_->graphicsQueueTypeIndex,
            .transferFamily = transferFamily,
        }),
        .requestedFeatures = getRequiredFeatures(),
        .adapterGroup = {},
    });

    data_->adapter = selectedAdapter;
    data_->device = std::move(device);
    CO_CORE_ASSERT(!data_->device.queues().empty(), "Device has no queues!");
    auto queues = data_->device.queues();
    auto *graphics = findQueueByTypeIndex(queues, data_->graphicsQueueTypeIndex);
    CO_CORE_ASSERT(graphics != nullptr,
                   "Failed to resolve graphics queue from selected queue family");
    data_->graphicsQueue = graphics;
    data_->computeQueue = graphics;
    data_->transferQueue = graphics;
    if (auto *transfer = findQueueByTypeIndex(queues, data_->transferQueueTypeIndex)) {
        data_->transferQueue = transfer;
    }

    data_->isHeadless = true;

    data_->pipelineCache = std::make_unique<PipelineCache>(
        data_->api.resourceManager(), data_->device.handle(), &data_->shaders);
    data_->uploader = std::make_unique<AsyncUploader>(*this);

    setupDescriptors();
}

void Context::setupDescriptors()
{
    Gpu::ResourceBindingFlags bindless_flags;
    bindless_flags |= Gpu::ResourceBindingFlagBits::PartiallyBoundBit;
    bindless_flags |= Gpu::ResourceBindingFlagBits::UpdateAfterBindBit;

    data_->descriptorSets.init(data_->device,
                               DescriptorSetOptions{.label = "Default Bind Group Layout"});
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
    CO_CORE_ASSERT(data_->graphicsQueue != nullptr, "Graphics queue is not initialized");
    return *data_->graphicsQueue;
}

Gpu::Queue &Context::computeQueue()
{
    CO_CORE_ASSERT(data_->computeQueue != nullptr, "Compute queue is not initialized");
    return *data_->computeQueue;
}

Gpu::Queue &Context::transferQueue()
{
    CO_CORE_ASSERT(data_->transferQueue != nullptr, "Transfer queue is not initialized");
    return *data_->transferQueue;
}

uint32_t Context::graphicsQueueFamilyIndex() const noexcept
{
    return data_->graphicsQueueTypeIndex;
}

uint32_t Context::computeQueueFamilyIndex() const noexcept
{
    return data_->computeQueueTypeIndex;
}

uint32_t Context::transferQueueFamilyIndex() const noexcept
{
    return data_->transferQueueTypeIndex;
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

AsyncUploader &Context::uploader()
{
    CO_CORE_ASSERT(data_->uploader != nullptr, "Uploader is not initialized");
    return *data_->uploader;
}

FileWatchManager &Context::fileWatchManager()
{
    return data_->fileWatchManager;
}
const FileWatchManager &Context::fileWatchManager() const
{
    return data_->fileWatchManager;
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

    if (pCallbackData->messageIdNumber == 0 && info.severity != DebugMessageSeverity::Error) {
        // ignore message ID 0 - this is the loader itself, usually complaining about some system
        // layers
        return;
    }

    if (validationMessageCallback()) {
        validationMessageCallback()(info);
        return;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-default"
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
#pragma clang diagnostic pop
}

} // namespace Cory
