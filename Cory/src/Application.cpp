#include "Application.h"

#include "ImGuiLayer.h"
#include "Log.h"
#include "Profiling.h"
#include "VkUtils.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <set>
#include <thread>

namespace Cory {

VKAPI_ATTR VkBool32 VKAPI_CALL Application::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
    CO_CORE_ERROR("Vulkan validation layer: {}", pCallbackData->pMessage);

    return false;
}

void Application::cursorPosCallback(GLFWwindow *window, double mouseX, double mouseY)
{
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    Cory::CameraManipulator::MouseButton mouseButton =
        (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
            ? Cory::CameraManipulator::MouseButton::Left
            : (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
                  ? Cory::CameraManipulator::MouseButton::Middle
                  : (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
                        ? Cory::CameraManipulator::MouseButton::Right
                        : Cory::CameraManipulator::MouseButton::None;
    if (mouseButton != Cory::CameraManipulator::MouseButton::None) {
        Cory::CameraManipulator::ModifierFlags modifiers;
        if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
            modifiers |= Cory::CameraManipulator::ModifierFlagBits::Alt;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            modifiers |= Cory::CameraManipulator::ModifierFlagBits::Ctrl;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            modifiers |= Cory::CameraManipulator::ModifierFlagBits::Shift;
        }

        Cory::CameraManipulator &cameraManipulator =
            reinterpret_cast<Application *>(glfwGetWindowUserPointer(window))->cameraManipulator;
        cameraManipulator.mouseMove(glm::ivec2(static_cast<int>(mouseX), static_cast<int>(mouseY)),
                                    mouseButton, modifiers);
    }
}

void Application::framebufferSizeCallback(GLFWwindow *window, int w, int h)
{
    Cory::CameraManipulator &cameraManipulator =
        reinterpret_cast<Application *>(glfwGetWindowUserPointer(window))->cameraManipulator;
    cameraManipulator.setWindowSize(glm::ivec2(w, h));
}

void Application::mouseButtonCallback(GLFWwindow *window, int /*button*/, int /*action*/,
                                      int /*mods*/)
{
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    Cory::CameraManipulator &cameraManipulator =
        reinterpret_cast<Application *>(glfwGetWindowUserPointer(window))->cameraManipulator;
    cameraManipulator.setMousePosition(glm::ivec2(static_cast<int>(xpos), static_cast<int>(ypos)));
}

void Application::scrollCallback(GLFWwindow *window, double /*xoffset*/, double yoffset)
{
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    Cory::CameraManipulator &cameraManipulator =
        reinterpret_cast<Application *>(glfwGetWindowUserPointer(window))->cameraManipulator;
    cameraManipulator.wheel(static_cast<int>(yoffset));
}

void Application::keyCallback(GLFWwindow *window, int key, int /*scancode*/, int action,
                              int /*mods*/)
{
    if (ImGui::GetIO().WantCaptureKeyboard)
        return;

    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
        case 'Q':
            glfwSetWindowShouldClose(window, 1);
            break;
        }
    }
}

Application::Application()
{

    // Cory framework init
    Log::Init();
    CO_CORE_INFO("Cory framework initialized.");
}

void Application::run()
{
    initWindow();

    initVulkan();

    m_imgui = std::make_unique<ImGuiLayer>();
    m_imgui->init(window(), ctx(), msaaSamples(), colorBuffer().view(), swapChain());

    // client app resources
    init();
    createSwapchainDependentResources();

    mainLoop();

    cleanup();

    if (enableValidationLayers) {
        m_ctx.instance->destroyDebugUtilsMessengerEXT(m_debugMessenger, nullptr, m_ctx.dl);
    }
}

void Application::initVulkan()
{
    setupInstance();
    setupDebugMessenger();

    createSurface();

    pickPhysicalDevice();
    createLogicalDevice();

    createMemoryAllocator();

    createCommandPools();

    m_swapChain = std::make_unique<SwapChain>(m_ctx, m_window, m_surface);
    createSyncObjects(MAX_FRAMES_IN_FLIGHT);

    createColorResources();
    createDepthResources();
}

void Application::mainLoop()
{
    CO_CORE_INFO("Entering main loop.");
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        drawFrame();
    }

    m_ctx.device->waitIdle();

    CO_CORE_DEBUG("Leaving main loop.");
}

void Application::cleanup()
{
    CO_CORE_INFO("Cleaning up Vulkan and GLFW..");

    cleanupSwapChain();
    m_swapChain = {};

    m_ctx.instance->destroySurfaceKHR(m_surface);

    deinit();

    vmaDestroyAllocator(m_ctx.allocator);

    glfwDestroyWindow(m_window);
    glfwTerminate();

    CO_CORE_INFO("Application shut down.");
}

void Application::requestLayers(std::vector<const char *> layers)
{
    m_requestedLayers.insert(end(m_requestedLayers), begin(layers), end(layers));
}

void Application::requestExtensions(std::vector<const char *> extensions)
{
    m_requestedExtensions.insert(end(m_requestedExtensions), begin(extensions), end(extensions));
}

void Application::setInitialWindowSize(uint32_t width, uint32_t height)
{
    m_initialWindowSize = vk::Extent2D{width, height};
}

void Application::initWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_window = glfwCreateWindow(m_initialWindowSize.width, m_initialWindowSize.height,
                                "Cory Application", nullptr, nullptr);
    glfwSetWindowUserPointer(m_window, this);

    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow *window, int width, int height) {
        auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
        app->m_framebufferResized = true;
    });

    // install some callbacks
    glfwSetCursorPosCallback(m_window, cursorPosCallback);
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetScrollCallback(m_window, scrollCallback);

    cameraManipulator.setMode(CameraManipulator::Mode::Examine);
    cameraManipulator.setWindowSize(
        glm::u32vec2(m_initialWindowSize.width, m_initialWindowSize.height));
    glm::vec3 sceneMin{-.5f};
    glm::vec3 sceneMax{0.5f};
    glm::vec3 sceneExtents = sceneMax - sceneMin;
    glm::vec3 diagonal = 3.0f * sceneExtents;
    cameraManipulator.setLookat(1.0f * diagonal, (sceneMin + sceneMax) / 2.f, glm::vec3(0, 1, 0));
    glfwSetWindowUserPointer(m_window, this);
}

void Application::setupInstance()
{

    vk::ApplicationInfo appInfo("Hello Triangle", VK_MAKE_VERSION(1, 0, 0), "No Engine",
                                VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_1);

    vk::InstanceCreateInfo createInfo({}, &appInfo);

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    CO_CORE_INFO("GLFW requires {} extensions", glfwExtensionCount);

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount = 0;

    auto extensions = vk::enumerateInstanceExtensionProperties();

    CO_CORE_INFO("available extensions:");
    for (const auto &extension : extensions) {
        CO_CORE_INFO("\t{}", extension.extensionName);
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
    m_ctx.queueFamilyIndices = findQueueFamilies(m_ctx.physicalDevice, m_surface);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {m_ctx.queueFamilyIndices.graphicsFamily.value(),
                                              m_ctx.queueFamilyIndices.presentFamily.value()};

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
    m_ctx.graphicsQueue =
        m_ctx.device->getQueue(m_ctx.queueFamilyIndices.graphicsFamily.value(), 0);
    m_ctx.presentQueue = m_ctx.device->getQueue(m_ctx.queueFamilyIndices.presentFamily.value(), 0);
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

    poolInfo.queueFamilyIndex = m_ctx.queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;

    m_ctx.transientCmdPool = m_ctx.device->createCommandPoolUnique(poolInfo);

    // permanent command pool
    poolInfo.queueFamilyIndex = m_ctx.queueFamilyIndices.graphicsFamily.value();
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

void Application::drawFrame()
{
    m_ctx.device->resetCommandPool(*m_ctx.transientCmdPool, vk::CommandPoolResetFlags{});

    // fences to sync per-frame draw resources
    auto perFrameFenceResult =
        m_ctx.device->waitForFences({*m_inFlightFences[m_currentFrame]}, true, UINT64_MAX);
    if (perFrameFenceResult != vk::Result::eSuccess) {
        throw std::runtime_error(fmt::format("failed to wait for inFlightFences[{}], error: {}",
                                             m_currentFrame, perFrameFenceResult));
    }

    // acquire image
    auto [result, imageIndex] = m_ctx.device->acquireNextImageKHR(
        m_swapChain->swapchain(), UINT64_MAX, *m_imageAvailableSemaphores[m_currentFrame], nullptr);

    if (result == vk::Result::eErrorOutOfDateKHR) {
        recreateSwapChain();
        return;
    }

    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("failed to acquire swap chain image");
    }

    // Check if a previous frame is using this image (i.e. there is its fence to wait on)
    if (m_imagesInFlight[imageIndex] != vk::Fence{}) {
        result = m_ctx.device->waitForFences({m_imagesInFlight[imageIndex]}, true, UINT64_MAX);
        if (result != vk::Result::eSuccess) {
            throw std::runtime_error(fmt::format("failed to wait for inFlightFences[{}], error: {}",
                                                 imageIndex, result));
        }
    }
    // Mark the image as now being in use by this frame
    m_imagesInFlight[imageIndex] = *m_inFlightFences[m_currentFrame];

    // make sure to reset the frame-respective fence
    m_ctx.device->resetFences(*m_inFlightFences[m_currentFrame]);

    FrameUpdateInfo fui;
    fui.swapChainImageIdx = imageIndex;
    fui.currentFrameIdx = m_currentFrame;
    fui.imageAvailableSemaphore = *m_imageAvailableSemaphores[m_currentFrame];
    fui.renderFinishedSemaphore = *m_renderFinishedSemaphores[m_currentFrame];
    fui.imageInFlightFence = *m_inFlightFences[m_currentFrame];

    {
        ScopeTimer timer("ImGui prepare");
        m_imgui->newFrame(ctx());
    }

    drawSwapchainFrame(fui);

    // the main FPS counter
    static LapTimer fpsCounter;
    if (fpsCounter.lap()) {
        auto s = fpsCounter.stats();
        CO_CORE_INFO("FPS: {:3.2f} ({:3.2f} ms)", float(1'000'000'000) / float(s.avg),
                     float(s.avg) / 1'000'000);
    }

    processPerfCounters(fpsCounter);

    {
        ScopeTimer timer("ImGUI draw");
        m_imgui->drawFrame(ctx(), fui.swapChainImageIdx);
    }

    vk::Semaphore signalSemaphores[] = {*m_renderFinishedSemaphores[m_currentFrame]};

    // return image to the swap chain
    vk::PresentInfoKHR presentInfo{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores; // wait for queue to finish

    vk::SwapchainKHR swapChains[] = {m_swapChain->swapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults =
        nullptr; // can be used to check every individual swap chain is successful

    try {
        result = m_ctx.presentQueue.presentKHR(presentInfo);
    }
    catch (vk::OutOfDateKHRError) {
        m_framebufferResized = false;
        recreateSwapChain();
    }

    if (result == vk::Result::eSuboptimalKHR || m_framebufferResized) {
        m_framebufferResized = false;
        recreateSwapChain();
    }
    else if (result != vk::Result::eSuccess) {
        throw std::runtime_error("failed to present swap chain image");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::processPerfCounters(LapTimer &fpsCounter)
{
    auto s = fpsCounter.stats();
    float fps = float(1'000'000'000) / float(s.avg);
    float millis = float(s.avg) / 1'000'000;
    ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_NoTitleBar);
    ImGui::Text(fmt::format("FPS: {:>4.2f}", fps).c_str());
    ImGui::Text(fmt::format("ms:  {:>4.2f}", millis).c_str());
    auto fpsHistory = fpsCounter.hist();
    std::vector<float> fpsHistoryMs;
    std::ranges::transform(fpsHistory, std::back_inserter(fpsHistoryMs),
                           [](long long nanos) { return float(nanos) / 1'000'000; });
    ImGui::PlotLines("Frame Times", fpsHistoryMs.data(), fpsHistoryMs.size());

    // debug / performance counters
    auto recs = Profiler::GetRecords();
    for (const auto [k, v] : recs) {
        auto ps = v.stats();
        auto txt = fmt::format("{:15} {:3.2f} ({:3.2f}-{:3.2f})", k, float(ps.avg) / 1'000,
                               float(ps.min) / 1'000, float(ps.max) / 1'000);
        ImGui::Text(txt.c_str());
    }

    ImGui::End();

    ImGui::ShowDemoWindow();
}

void Application::cleanupSwapChain()
{
    m_depthBuffer.destroy(m_ctx);
    m_renderTarget.destroy(m_ctx);

    m_imgui->deinit(ctx());

    destroySwapchainDependentResources();
}

void Application::recreateSwapChain()
{
    // window might be minimized
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    if (width == height == 0)
        CO_CORE_DEBUG("Window minimized");
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }
    CO_CORE_DEBUG("Framebuffer resized");

    vkDeviceWaitIdle(*m_ctx.device);

    cleanupSwapChain();

    m_swapChain = {}; // first get rid of the old swap chain before creating the new one
    m_swapChain = std::make_unique<SwapChain>(m_ctx, m_window, m_surface);

    createColorResources();
    createDepthResources();

    m_imgui->init(window(), ctx(), msaaSamples(), colorBuffer().view(), swapChain());

    createSwapchainDependentResources();
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
    CO_CORE_DEBUG("Supported Vulkan Layers:");
    for (const char *layerName : m_requestedLayers) {
        bool layerFound = false;

        CO_CORE_DEBUG("  {0}", layerName);

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
    CO_CORE_INFO("Found {} vulkan devices", devices.size());

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

    CO_CORE_INFO("Found vulkan device: {}", properties.deviceName);
    // CO_CORE_INFO("  {} Driver {}, API {}", deviceProperties, deviceProperties.driverVersion,
    // deviceProperties.apiVersion);

    auto qfi = findQueueFamilies(device, m_surface);
    CO_CORE_DEBUG("  Queue Families: Graphics {}, Compute {}, Transfer {}, Present {}",
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