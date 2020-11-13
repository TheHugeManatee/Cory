#include "HelloTriangle.h"

#include <spdlog/spdlog.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>

const std::vector<const char *> HelloTriangleApplication::validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> HelloTriangleApplication::deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

static std::vector<char> readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error(fmt::format("failed to open file {}", filename));
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VKAPI_ATTR VkBool32 VKAPI_CALL HelloTriangleApplication::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{

    spdlog::error("validation layer: {}", pCallbackData->pMessage);

    return VK_FALSE;
}

void HelloTriangleApplication::run()
{
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void HelloTriangleApplication::initWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_window = glfwCreateWindow(WIDTH, HEIGHT, "VK Tutorial", nullptr, nullptr);
    glfwSetWindowUserPointer(m_window, this);

    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow *window, int width, int height) {
        auto app = reinterpret_cast<HelloTriangleApplication *>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    });
}

void HelloTriangleApplication::initVulkan()
{
    setupInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();

    createTransientCommandPool();

    createSwapChain();
    createImageViews();
    createRenderPass();

    createColorResources();
    createDepthResources();
    createFramebuffers();
    createSyncObjects();

    m_texture = createTextureImage(RESOURCE_DIR "/viking_room.png", VK_FILTER_LINEAR,
                                   VK_SAMPLER_ADDRESS_MODE_REPEAT);
    m_texture2 = createTextureImage(RESOURCE_DIR "/sunglasses.png", VK_FILTER_NEAREST,
                                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
    createGeometry();

    createDescriptorSetLayout();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createGraphicsPipeline();

    createAppCommandPool();
    createCommandBuffers();
}

void HelloTriangleApplication::createTransientCommandPool()
{
    // create a second command pool for transient operations
    VkCommandPoolCreateInfo poolInfo{};
    auto queueFamilyIndices = findQueueFamilies(m_ctx.physicalDevice);
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    if (vkCreateCommandPool(m_ctx.device, &poolInfo, nullptr, &m_ctx.transientCmdPool) !=
        VK_SUCCESS) {
        throw std::runtime_error("Could not create transient command pool!");
    }
}

void HelloTriangleApplication::setupInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    spdlog::info("GLFW requires {} extensions", glfwExtensionCount);

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount = 0;

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    spdlog::info("available extensions:");
    for (const auto &extension : extensions) {
        spdlog::info("\t{}", extension.extensionName);
    }

    // enable optional extensions
    auto requiredExtensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    // validation layers
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
    }
    else {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_ctx.instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

void HelloTriangleApplication::mainLoop()
{
    spdlog::info("Entering main loop.");
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        drawFrame();
    }

    vkDeviceWaitIdle(m_ctx.device);

    spdlog::info("Leaving main loop.");
}

void HelloTriangleApplication::cleanup()
{
    spdlog::info("Cleaning up Vulkan and GLFW..");

    cleanupSwapChain();

    vkDestroyDescriptorSetLayout(m_ctx.device, m_descriptorSetLayout, nullptr);

    m_vertexBuffer.destroy(m_ctx);
    m_indexBuffer.destroy(m_ctx);

    m_texture.destroy(m_ctx);
    m_texture2.destroy(m_ctx);
    // vkDestroyBuffer(m_ctx.device, m_vertexBuffer, nullptr);
    // vkFreeMemory(m_ctx.device, m_vertexBufferMemory, nullptr);

    for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(m_ctx.device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_ctx.device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_ctx.device, m_inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(m_ctx.device, m_commandPool, nullptr);
    vkDestroyCommandPool(m_ctx.device, m_ctx.transientCmdPool, nullptr);

    vkDestroySurfaceKHR(m_ctx.instance, m_surface, nullptr);
    vkDestroyDevice(m_ctx.device, nullptr);

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(m_ctx.instance, m_debugMessenger, nullptr);
    }

    vkDestroyInstance(m_ctx.instance, nullptr);
    glfwDestroyWindow(m_window);
    glfwTerminate();

    spdlog::info("Application shut down.");
}

bool HelloTriangleApplication::checkValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    spdlog::info("Supported Vulkan Layers:");
    for (const char *layerName : validationLayers) {
        bool layerFound = false;

        spdlog::info("  {0}", layerName);

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

std::vector<const char *> HelloTriangleApplication::getRequiredExtensions()
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

void HelloTriangleApplication::populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT &createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void HelloTriangleApplication::createSurface()
{
    if (glfwCreateWindowSurface(m_ctx.instance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
        throw std::runtime_error("Could not create window surface!");
    }
}

void HelloTriangleApplication::setupDebugMessenger()
{
    if (!enableValidationLayers)
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(m_ctx.instance, &createInfo, nullptr, &m_debugMessenger) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void HelloTriangleApplication::pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_ctx.instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_ctx.instance, &deviceCount, devices.data());

    spdlog::info("Found {} vulkan devices.", deviceCount);

    for (const auto &device : devices) {
        if (isDeviceSuitable(device)) {
            m_ctx.physicalDevice = device;
            m_msaaSamples = getMaxUsableSampleCount();
            break;
        }
    }

    if (m_ctx.physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

QueueFamilyIndices HelloTriangleApplication::findQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto &queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        if (queueFamily.queueCount & VK_QUEUE_COMPUTE_BIT) {
            indices.computeFamily = i;
        }
        if (queueFamily.queueCount & VK_QUEUE_TRANSFER_BIT) {
            indices.transferFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }
        i++;
    }

    return indices;
}

void HelloTriangleApplication::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(m_ctx.physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                              indices.presentFamily.value()};

    float queuePriority = 1.0f;

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};

        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    // specify device features here
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());

    createInfo.pEnabledFeatures = &deviceFeatures;

    // device-specific extensions
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // device-specific layers - these are already covered by the instance layers, not
    // strictly needed again here but seems to be good practice.
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(m_ctx.physicalDevice, &createInfo, nullptr, &m_ctx.device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device.");
    }

    // store the handle to the graphics and present queues
    vkGetDeviceQueue(m_ctx.device, indices.graphicsFamily.value(), 0, &m_ctx.graphicsQueue);
    vkGetDeviceQueue(m_ctx.device, indices.presentFamily.value(), 0, &m_ctx.presentQueue);
}

bool HelloTriangleApplication::checkDeviceExtensionSupport(const VkPhysicalDevice &device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                         availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto &extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

SwapChainSupportDetails HelloTriangleApplication::querySwapChainSupport(VkPhysicalDevice device,
                                                                        VkSurfaceKHR surface)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                                  details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR HelloTriangleApplication::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats)
{
    // BRGA8 and SRGB are the preferred formats
    for (const auto &availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR HelloTriangleApplication::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentModes)
{
    for (const auto &availableMode : availablePresentModes) {
        if (availableMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availableMode;
        }
    }
    return availablePresentModes[0];
}

VkExtent2D HelloTriangleApplication::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities)
{
    // for high DPI, the extent between pixel size and screen coordinates might not be the same.
    // in this case, we need to compute a proper viewport extent from the GLFW screen coordinates
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height);
    return actualExtent;
}

void HelloTriangleApplication::createSwapChain()
{
    const SwapChainSupportDetails swapChainSupport =
        querySwapChainSupport(m_ctx.physicalDevice, m_surface);

    const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    const VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    const VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    // we use one more image as a buffer to avoid stalls when waiting for the next image to become
    // available
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1; // this might be 2 if we are developing stereoscopic stuff
    createInfo.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // for off-screen rendering, it is possible to use
                                             // VK_IMAGE_USAGE_TRANSFER_DST_BIT instead

    QueueFamilyIndices indices = findQueueFamilies(m_ctx.physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    // if the swap and present queues are different, the swap chain images have to be shareable
    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // exclusive has better performance
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // whether the alpha channel should be used to composite
                                           // on top of other windows
    createInfo.presentMode = presentMode;
    createInfo.clipped =
        VK_TRUE; // false would force pixels to be rendered even if they are occluded. might be
                 // important if the buffer is read back somehow (screen shots etc?)

    createInfo.oldSwapchain = VK_NULL_HANDLE; // old swap chain, required when resizing etc.

    if (vkCreateSwapchainKHR(m_ctx.device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Could not create swap chain!");
    }

    vkGetSwapchainImagesKHR(m_ctx.device, m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_ctx.device, m_swapChain, &imageCount, m_swapChainImages.data());

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;
}

void HelloTriangleApplication::createImageViews()
{
    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (size_t i = 0; i < m_swapChainImages.size(); ++i) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // for stereographic, we could create separate image views for the array layers here
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_ctx.device, &createInfo, nullptr, &m_swapChainImageViews[i]) !=
            VK_SUCCESS) {
            throw std::runtime_error("Could not create swap chain image views");
        }
    }
}

VkShaderModule HelloTriangleApplication::createShaderModule(const std::vector<char> &code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_ctx.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Could not create shader");
    }

    return shaderModule;
}

void HelloTriangleApplication::createGraphicsPipeline()
{
    //****************** Shaders ******************
    auto vertShaderCode = readFile(RESOURCE_DIR "/default-vert.spv");
    auto fragShaderCode = readFile(RESOURCE_DIR "/manatee.spv");

    auto vertShaderModule = createShaderModule(vertShaderCode);
    auto fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    // entry point -- means we can add multiple entry points in one module - yays!
    vertShaderStageInfo.pName = "main";

    // Note: pSpecializationInfo can be used to set compile time constants - kinda like macros in an
    // online compilation?

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    //****************** Vertex Input ******************
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable =
        VK_FALSE; // allows to break primitive lists with 0xFFFF index

    //****************** Viewport & Scissor ******************
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_swapChainExtent.width;
    viewport.height = (float)m_swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    //****************** Rasterizer ******************
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE; // depth clamp: depth is clamped for fragments instead
                                            // of discarding them. might be useful for shadow maps?
    rasterizer.rasterizerDiscardEnable =
        VK_FALSE; // completely disable rasterizer/framebuffer output
    rasterizer.polygonMode =
        VK_POLYGON_MODE_FILL; // _LINE and _POINT are alternatives, but require enabling GPU feature
    rasterizer.lineWidth = 1.0f; // >1.0 requires 'wideLines' GPU feature
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    //****************** Multisampling ******************
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_TRUE;
    multisampling.rasterizationSamples = m_msaaSamples;
    multisampling.minSampleShading = 0.2f; // controls how smooth the msaa
    multisampling.pSampleMask = nullptr; // ?
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    //****************** Depth and Stencil ******************
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};

    //****************** Color Blending ******************
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    // note: you can only do EITHER color blending per attachment, or logic blending. enabling logic
    // blending will override/disable the blend ops above
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    //****************** Dynamic State ******************
    // Specifies which pipeline states can be changed dynamically without recreating the pipeline
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    //****************** Pipeline Layout ******************
    // stores/manages shader uniform values
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(m_ctx.device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("Could not create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr; // skipped for now --why tho?
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // note: vulkan can have "base" and "derived"
                                                      // pipelines when they are similar
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(m_ctx.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                  &m_graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Could not create graphics pipeline");
    }

    vkDestroyShaderModule(m_ctx.device, vertShaderModule, nullptr);
    vkDestroyShaderModule(m_ctx.device, fragShaderModule, nullptr);
}

void HelloTriangleApplication::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapChainImageFormat;
    colorAttachment.samples = m_msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // care about color
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // don't care about stencil
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat(m_ctx.physicalDevice);
    depthAttachment.samples = m_msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = m_swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    //****************** Subpasses ******************
    // describe which layout each attachment should be transitioned to
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;
    // NOTE: the order of attachments directly corresponds to the 'layout(location=0) out vec4
    // color' index in the fragment shader pInputAttachments: attachments that are read from a
    // shader pResolveAttachments: attachments used for multisampling color attachments
    // pDepthStencilAttachment: attachment for depth and stencil data
    // pPreserveAttachments: attachments that are not currently used by the subpass but for which
    // the data needs to be preserved.

    //****************** Render Pass ******************
    std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment,
                                                          colorAttachmentResolve};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    //****************** Subpass dependencies ******************
    // this sets up the render pass to wait for the STAGE_COLOR_ATTACHMENT_OUTPUT stage to ensure
    // the images are available and the swap chain is not still reading the image
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_ctx.device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Could not create render pass");
    }
}

void HelloTriangleApplication::createFramebuffers()
{
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

    for (size_t i{0}; i < m_swapChainImageViews.size(); ++i) {
        std::array<VkImageView, 3> attachments = {m_renderTarget.view(), m_depthBuffer.view(),
                                                  m_swapChainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_swapChainExtent.width;
        framebufferInfo.height = m_swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_ctx.device, &framebufferInfo, nullptr,
                                &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Could not create framebuffer");
        }
    }
}

void HelloTriangleApplication::createAppCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_ctx.physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = 0; // for re-recording of command buffers, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
                        // or VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT might be necessary

    if (vkCreateCommandPool(m_ctx.device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Could not create command pool!");
    }
}

void HelloTriangleApplication::createCommandBuffers()
{
    // we need one command buffer per frame buffer
    m_commandBuffers.resize(m_swapChainFramebuffers.size());
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // _SECONDARY cannot be directly submitted
                                                       // but can be called from other cmd buffer
    allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

    if (vkAllocateCommandBuffers(m_ctx.device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate command buffers");
    }

    // begin all command buffers
    for (size_t i = 0; i < m_commandBuffers.size(); i++) {
        auto &cmdBuf = m_commandBuffers[i];

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags =
            0; // ONE_TIME_SUBMIT for transient cmdbuffers that are rerecorded every frame
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(cmdBuf, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // start render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass;
        renderPassInfo.framebuffer = m_swapChainFramebuffers[i];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_swapChainExtent; // should match size of attachments

        // defines what is used for VK_ATTACHMENT_LOAD_OP_CLEAR
        std::array<VkClearValue, 2> clearColors;
        clearColors[0] = {0.2f, 0.2f, 0.2f, 1.0f};
        clearColors[1] = {1.0f, 0.0f};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
        renderPassInfo.pClearValues = clearColors.data();

        vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // bind graphics pipeline
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

        // bind the vertex buffer
        VkBuffer vertexBuffers[] = {m_vertexBuffer.buffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmdBuf, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(cmdBuf, m_indexBuffer.buffer(), 0, VK_INDEX_TYPE_UINT16);

        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                                &descriptorSets[i], 0, nullptr);

        // draw three vertices
        vkCmdDrawIndexed(cmdBuf, m_numVertices, 1, 0, 0, 0);

        vkCmdEndRenderPass(cmdBuf);

        if (vkEndCommandBuffer(cmdBuf) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }
    }
}

void HelloTriangleApplication::createSyncObjects()
{
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_imagesInFlight.resize(m_swapChainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i{}; i < MAX_FRAMES_IN_FLIGHT; ++i) {

        if (vkCreateSemaphore(m_ctx.device, &semaphoreInfo, nullptr,
                              &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_ctx.device, &semaphoreInfo, nullptr,
                              &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_ctx.device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {

            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void HelloTriangleApplication::recreateSwapChain()
{
    // window might be minimized
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    if (width == height == 0)
        spdlog::info("Window minimized");
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }
    spdlog::info("Framebuffer resized");

    vkDeviceWaitIdle(m_ctx.device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createColorResources();
    createDepthResources();
    createFramebuffers();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
}

void HelloTriangleApplication::cleanupSwapChain()
{
    m_depthBuffer.destroy(m_ctx);
    m_renderTarget.destroy(m_ctx);

    for (auto framebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(m_ctx.device, framebuffer, nullptr);
    }
    vkDestroyDescriptorPool(m_ctx.device, m_descriptorPool, nullptr);

    vkFreeCommandBuffers(m_ctx.device, m_commandPool,
                         static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());

    vkDestroyPipeline(m_ctx.device, m_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_ctx.device, m_pipelineLayout, nullptr);
    vkDestroyRenderPass(m_ctx.device, m_renderPass, nullptr);

    for (auto imageView : m_swapChainImageViews) {
        vkDestroyImageView(m_ctx.device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(m_ctx.device, m_swapChain, nullptr);

    for (auto &buffer : m_uniformBuffers) {
        buffer.destroy(m_ctx);
    }
}

void HelloTriangleApplication::createGeometry()
{
    //     auto [vertices, indices] = primitives::doublequad();
    //     m_numVertices = static_cast<uint32_t>(indices.size());

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                          RESOURCE_DIR "/viking_room.obj")) {

        throw std::runtime_error(fmt::format("Could not load 3D model: {} {}", warn, err));
    }

    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    std::unordered_map<Vertex, uint32_t, Vertex::hasher> uniqueVertices;
    for (const auto &shape : shapes) {

        for (const auto &index : shape.mesh.indices) {
            Vertex v{};
            v.pos = {attrib.vertices[3 * index.vertex_index + 0],
                     attrib.vertices[3 * index.vertex_index + 1],
                     attrib.vertices[3 * index.vertex_index + 2]};
            v.texCoord = {attrib.texcoords[2 * index.texcoord_index + 0],
                          1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};
            v.color = {1.0f, 1.0f, 1.0f};

            if (uniqueVertices.count(v) == 0) {
                uniqueVertices[v] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(v);
            }

            indices.push_back(uniqueVertices[v]);
        }
    }
    m_numVertices = static_cast<uint32_t>(indices.size());

    createVertexBuffers(vertices);
    createIndexBuffer(indices);
}

void HelloTriangleApplication::createVertexBuffers(const std::vector<Vertex> &vertices)
{
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    device_buffer stagingBuffer;
    stagingBuffer.create(m_ctx, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    stagingBuffer.upload(m_ctx, vertices.data(), bufferSize);

    m_vertexBuffer.create(
        m_ctx, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    stagingBuffer.copy_to(m_ctx, m_vertexBuffer, bufferSize);

    stagingBuffer.destroy(m_ctx);
}

void HelloTriangleApplication::createIndexBuffer(const std::vector<uint16_t> &indices)
{
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    device_buffer stagingBuffer;
    stagingBuffer.create(m_ctx, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    stagingBuffer.upload(m_ctx, indices.data(), bufferSize);

    m_indexBuffer.create(m_ctx, bufferSize,
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    stagingBuffer.copy_to(m_ctx, m_indexBuffer, bufferSize);

    stagingBuffer.destroy(m_ctx);
}

void HelloTriangleApplication::createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    m_uniformBuffers.resize(m_swapChainImages.size());

    for (size_t i{}; i < m_swapChainImages.size(); ++i) {
        m_uniformBuffers[i].create(m_ctx, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
}

void HelloTriangleApplication::drawFrame()
{
    // fences to sync per-frame draw resources
    vkWaitForFences(m_ctx.device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    // acquire image
    uint32_t imageIndex;
    auto result = vkAcquireNextImageKHR(m_ctx.device, m_swapChain, UINT64_MAX,
                                        m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE,
                                        &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image");
    }

    // Check if a previous frame is using this image (i.e. there is its fence to wait on)
    if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(m_ctx.device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    // Mark the image as now being in use by this frame
    m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];

    updateUniformBuffer(imageIndex);

    // execute command buffer with that image as attachment
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // vkQueueSubmit allows to wait for a specific semaphore, which in our case waits until the
    // image is signaled available
    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];

    // vkQueueSubmit allows to signal other semaphore(s) when the rendering is finished
    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // make sure to reset the frame-respective fence
    vkResetFences(m_ctx.device, 1, &m_inFlightFences[m_currentFrame]);

    if (vkQueueSubmit(m_ctx.graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) !=
        VK_SUCCESS) // could use a fence here instead of the semaphores for synchronization
    {
        throw std::runtime_error("Could not submit draw command buffer");
    }

    // return image to the swap chain
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores; // wait for queue to finish

    VkSwapchainKHR swapChains[] = {m_swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr; // can be used to check every individual swap chai is successful

    result = vkQueuePresentKHR(m_ctx.presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void HelloTriangleApplication::updateUniformBuffer(uint32_t imageIndex)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time =
        std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo;
    ubo.model =
        glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                           glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj =
        glm::perspective(glm::radians(45.0f),
                         m_swapChainExtent.width / (float)m_swapChainExtent.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1; // NOTE: we flip this bc/ glm is written for OpenGL which has Y inverted.
                          // otherwise image will be upside down :)

    m_uniformBuffers[imageIndex].upload(m_ctx, &ubo, sizeof(ubo));
}

void HelloTriangleApplication::createDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(m_swapChainImages.size());
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(m_swapChainImages.size());

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(m_swapChainImages.size());
    // enables creation and freeing of individual descriptor sets -- we don't care for that right
    // now
    // poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(m_ctx.device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Could not create descriptor pool");
    }
}

void HelloTriangleApplication::createDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(m_swapChainImages.size(), m_descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(m_swapChainImages.size());
    allocInfo.pSetLayouts = layouts.data();

    // NOTE: descriptor sets are freed implicitly when the pool is freed.
    descriptorSets.resize(m_swapChainImages.size());
    if (vkAllocateDescriptorSets(m_ctx.device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate descriptor sets");
    }

    // populate every descriptor
    for (size_t i{}; i < m_swapChainImages.size(); ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i].buffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject); // access range, could be VK_WHOLE_SIZE

        std::array<VkDescriptorImageInfo, 2> imageInfos;
        imageInfos[0].imageView = m_texture.view();
        imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[0].sampler = m_texture.sampler();

        imageInfos[1].imageView = m_texture2.view();
        imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[1].sampler = m_texture2.sampler();

        std::array<VkWriteDescriptorSet, 2> descriptorWrites;
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;
        descriptorWrites[0].pImageInfo = nullptr;
        descriptorWrites[0].pTexelBufferView = nullptr;
        descriptorWrites[0].pNext = nullptr;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = static_cast<uint32_t>(imageInfos.size());
        descriptorWrites[1].pBufferInfo = nullptr;
        descriptorWrites[1].pImageInfo = imageInfos.data();
        descriptorWrites[1].pTexelBufferView = nullptr;
        descriptorWrites[1].pNext = nullptr;

        vkUpdateDescriptorSets(m_ctx.device, static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }
}

void HelloTriangleApplication::createColorResources()
{
    m_renderTarget.create(m_ctx, {m_swapChainExtent.width, m_swapChainExtent.height, 1},
                          m_swapChainImageFormat, m_msaaSamples);
}

void HelloTriangleApplication::createDepthResources()
{

    VkFormat depthFormat = findDepthFormat(m_ctx.physicalDevice);

    m_depthBuffer.create(m_ctx, {m_swapChainExtent.width, m_swapChainExtent.height, 1},
                         depthFormat, m_msaaSamples);
    m_depthBuffer.transitionLayout(m_ctx, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

bool HelloTriangleApplication::isDeviceSuitable(const VkPhysicalDevice &device)
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    spdlog::info("Found vulkan device: {}", deviceProperties.deviceName);
    // spdlog::info("  {} Driver {}, API {}", deviceProperties, deviceProperties.driverVersion,
    // deviceProperties.apiVersion);

    auto qfi = findQueueFamilies(device);
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
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return qfi.graphicsFamily.has_value() && qfi.presentFamily.has_value() && extensionsSupported &&
           swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

VkSampleCountFlagBits HelloTriangleApplication::getMaxUsableSampleCount()
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(m_ctx.physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                                physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) {
        return VK_SAMPLE_COUNT_64_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_32_BIT) {
        return VK_SAMPLE_COUNT_32_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_16_BIT) {
        return VK_SAMPLE_COUNT_16_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_8_BIT) {
        return VK_SAMPLE_COUNT_8_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_4_BIT) {
        return VK_SAMPLE_COUNT_4_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT) {
        return VK_SAMPLE_COUNT_2_BIT;
    }

    return VK_SAMPLE_COUNT_1_BIT;
}

device_texture HelloTriangleApplication::createTextureImage(std::string textureFilename,
                                                            VkFilter filter,
                                                            VkSamplerAddressMode addressMode)
{
    stbi_image image(textureFilename);
    device_texture texture;

    if (!image.data) {
        throw std::runtime_error("Could not load texture image from file!");
    }

    device_buffer stagingBuffer;
    stagingBuffer.create(m_ctx, image.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    stagingBuffer.upload(m_ctx, image.data, image.size());

    uint32_t mipLevels =
        static_cast<uint32_t>(std::floor(std::log2(std::max(image.width, image.height)))) + 1;
    texture.create(m_ctx, {image.width, image.height, 1}, mipLevels, VK_IMAGE_TYPE_2D,
                   VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, filter, addressMode,
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    texture.transitionLayout(m_ctx, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    stagingBuffer.copy_to(m_ctx, texture);
    stagingBuffer.destroy(m_ctx);

    texture.generate_mipmaps(m_ctx, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_ACCESS_SHADER_READ_BIT);
    // texture.transitionLayout(m_ctx, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return texture;
}

void HelloTriangleApplication::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                                  VK_SHADER_STAGE_FRAGMENT_BIT; // or VK_SHADER_STAGE_ALL_GRAPHICS
    uboLayoutBinding.pImmutableSamplers = nullptr; // idk, something with image sampling

    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 1;
    samplerBinding.descriptorCount = 2;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerBinding.pImmutableSamplers = nullptr;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_ctx.device, &layoutInfo, nullptr, &m_descriptorSetLayout) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }
}
