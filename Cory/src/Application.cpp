#include "Application.h"

#include "VkUtils.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include <set>

namespace Cory {

VKAPI_ATTR VkBool32 VKAPI_CALL Application::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
    spdlog::error("validation layer: {}", pCallbackData->pMessage);

    return false;
}

void Application::requestLayers(std::vector<const char *> layers)
{
    m_requestedLayers.insert(end(m_requestedLayers), begin(layers), end(layers));
}

void Application::requestExtensions(std::vector<const char *> extensions)
{
    m_requestedExtensions.insert(end(m_requestedExtensions), begin(extensions), end(extensions));
}

void Application::initWindow(vk::Extent2D extent)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_window = glfwCreateWindow(extent.width, extent.height, "Cory Application", nullptr, nullptr);
    glfwSetWindowUserPointer(m_window, this);

    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow *window, int width, int height) {
        auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
        app->m_framebufferResized = true;
    });
}

void Application::setupInstance()
{

    vk::ApplicationInfo appInfo("Hello Triangle", VK_MAKE_VERSION(1, 0, 0), "No Engine",
                                VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_1);

    vk::InstanceCreateInfo createInfo({}, &appInfo);

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    spdlog::info("GLFW requires {} extensions", glfwExtensionCount);

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount = 0;

    auto extensions = vk::enumerateInstanceExtensionProperties();

    spdlog::info("available extensions:");
    for (const auto &extension : extensions) {
        spdlog::info("\t{}", extension.extensionName);
    }

    // enable optional extensions
    auto requiredExtensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    // validation layers
    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;

    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_requestedLayers.size());
        createInfo.ppEnabledLayerNames = m_requestedLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
    }
    else {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    m_ctx.instance = vk::createInstanceUnique(createInfo);

    vk::DynamicLoader dl;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
        dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    m_ctx.dl = vk::DispatchLoaderDynamic(*m_ctx.instance, vkGetInstanceProcAddr);
}

void Application::createSurface()
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*m_ctx.instance, m_window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Could not create window surface!");
    }
    m_surface = vk::SurfaceKHR(surface);
}

void Application::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(m_ctx.physicalDevice, m_surface);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                              indices.presentFamily.value()};

    float queuePriority = 1.0f;

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCreateInfo;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        queueCreateInfos.push_back(queueCreateInfo);
    }

    vk::PhysicalDeviceFeatures deviceFeatures{};
    // specify device features here
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE;

    vk::DeviceCreateInfo createInfo;

    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());

    createInfo.pEnabledFeatures = &deviceFeatures;

    // device-specific extensions
    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_requestedExtensions.size());
    createInfo.ppEnabledExtensionNames = m_requestedExtensions.data();

    // device-specific layers - these are already covered by the instance layers, not
    // strictly needed again here but seems to be good practice.
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_requestedLayers.size());
        createInfo.ppEnabledLayerNames = m_requestedLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    m_ctx.device = m_ctx.physicalDevice.createDeviceUnique(createInfo);

    // store the handle to the graphics and present queues
    m_ctx.graphicsQueue = m_ctx.device->getQueue(indices.graphicsFamily.value(), 0);
    m_ctx.presentQueue = m_ctx.device->getQueue(indices.presentFamily.value(), 0);
}

void Application::populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT &createInfo)
{

    createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                 vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                 vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;

    createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                             vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                             vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

    createInfo.pfnUserCallback = debugCallback;
}

void Application::createMemoryAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    allocatorInfo.physicalDevice = m_ctx.physicalDevice;
    allocatorInfo.device = *m_ctx.device;
    allocatorInfo.instance = *m_ctx.instance;

    vmaCreateAllocator(&allocatorInfo, &m_ctx.allocator);
}

void Application::createCommandPools()
{
    // create a second command pool for transient operations
    vk::CommandPoolCreateInfo poolInfo{};
    auto queueFamilyIndices = findQueueFamilies(m_ctx.physicalDevice, m_surface);
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;

    m_ctx.transientCmdPool = m_ctx.device->createCommandPoolUnique(poolInfo);

    // permanent command pool
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = vk::CommandPoolCreateFlagBits(
        0); // for re-recording of command buffers, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
            // or VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT might be necessary

    m_ctx.permanentCmdPool = m_ctx.device->createCommandPoolUnique(poolInfo);
}

void Application::createSyncObjects(uint32_t maxFramesInFlight)
{
    m_imageAvailableSemaphores.resize(maxFramesInFlight);
    m_renderFinishedSemaphores.resize(maxFramesInFlight);
    m_inFlightFences.resize(maxFramesInFlight);
    m_imagesInFlight.resize(m_swapChain->images().size());

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    for (size_t i{}; i < maxFramesInFlight; ++i) {
        m_imageAvailableSemaphores[i] = m_ctx.device->createSemaphoreUnique(semaphoreInfo);
        m_renderFinishedSemaphores[i] = m_ctx.device->createSemaphoreUnique(semaphoreInfo);
        m_inFlightFences[i] = m_ctx.device->createFenceUnique(fenceInfo);
    }
}

void Application::createColorResources()
{
    m_renderTarget.create(m_ctx, {m_swapChain->extent().width, m_swapChain->extent().height, 1},
                          m_swapChain->format(), m_msaaSamples);
}

void Application::createDepthResources()
{
    vk::Format depthFormat = findDepthFormat(m_ctx.physicalDevice);

    m_depthBuffer.create(m_ctx, {m_swapChain->extent().width, m_swapChain->extent().height, 1},
                         depthFormat, m_msaaSamples);
    m_depthBuffer.transitionLayout(m_ctx, vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

void Application::createFramebuffers(vk::RenderPass renderPass)
{
    m_swapChainFramebuffers.resize(m_swapChain->views().size());

    for (size_t i{0}; i < m_swapChain->views().size(); ++i) {
        std::array<vk::ImageView, 3> attachments = {m_renderTarget.view(), m_depthBuffer.view(),
                                                    m_swapChain->views()[i]};

        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_swapChain->extent().width;
        framebufferInfo.height = m_swapChain->extent().height;
        framebufferInfo.layers = 1;

        m_swapChainFramebuffers[i] = m_ctx.device->createFramebuffer(framebufferInfo);
    }
}

void Application::setupDebugMessenger()
{
    if (!enableValidationLayers)
        return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    m_debugMessenger = m_ctx.instance->createDebugUtilsMessengerEXT(createInfo, nullptr, m_ctx.dl);
}

std::vector<const char *> Application::getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

bool Application::checkDeviceExtensionSupport(const vk::PhysicalDevice &device)
{
    auto availableExtensions = device.enumerateDeviceExtensionProperties();

    std::set<std::string> requiredExtensions(m_requestedExtensions.begin(),
                                             m_requestedExtensions.end());

    for (const auto &extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

bool Application::checkValidationLayerSupport()
{
    auto availableLayers = vk::enumerateInstanceLayerProperties();

    // TODO: refactor using std::includes maybe?
    spdlog::debug("Supported Vulkan Layers:");
    for (const char *layerName : m_requestedLayers) {
        bool layerFound = false;

        spdlog::debug("  {0}", layerName);

        for (const auto &layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

void Application::pickPhysicalDevice()
{
    auto devices = m_ctx.instance->enumeratePhysicalDevices();
    if (devices.empty()) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }
    spdlog::info("Found {} vulkan devices", devices.size());

    for (const auto &device : devices) {
        if (isDeviceSuitable(device)) {
            m_ctx.physicalDevice = device;
            m_msaaSamples = getMaxUsableSampleCount(m_ctx.physicalDevice);
            break;
        }
    }

    if (m_ctx.physicalDevice == vk::PhysicalDevice()) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

bool Application::isDeviceSuitable(const vk::PhysicalDevice &device)
{
    auto properties = device.getProperties();
    auto features = device.getFeatures();

    spdlog::info("Found vulkan device: {}", properties.deviceName);
    // spdlog::info("  {} Driver {}, API {}", deviceProperties, deviceProperties.driverVersion,
    // deviceProperties.apiVersion);

    auto qfi = findQueueFamilies(device, m_surface);
    spdlog::info("  Queue Families: Graphics {}, Compute {}, Transfer {}, Present {}",
                 qfi.graphicsFamily.has_value(), qfi.computeFamily.has_value(),
                 qfi.transferFamily.has_value(), qfi.presentFamily.has_value());

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        auto swapChainDetails = querySwapChainSupport(device, m_surface);
        swapChainAdequate =
            !swapChainDetails.formats.empty() && !swapChainDetails.presentModes.empty();
    }

    // supported features
    vk::PhysicalDeviceFeatures supportedFeatures = device.getFeatures();

    return qfi.graphicsFamily.has_value() && qfi.presentFamily.has_value() && extensionsSupported &&
           swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

} // namespace Cory