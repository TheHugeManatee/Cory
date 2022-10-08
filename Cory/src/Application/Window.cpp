#include <Cory/Application/Window.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/RenderCore/APIConversion.hpp>
#include <Cory/RenderCore/Context.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/DeviceProperties.h>
#include <Magnum/Vk/Instance.h>
#include <Magnum/Vk/Queue.h>
// clang-format off
#include <Magnum/Vk/Vulkan.h>
#define VK_VERSION_1_0
#include <GLFW/glfw3.h>
// clang-format on

#include <range/v3/algorithm/contains.hpp>

namespace Cory {

Window::Window(Context &context, glm::i32vec2 dimensions, std::string windowName)
    : ctx_{context}
    , dimensions_(dimensions)
    , windowName_{std::move(windowName)}
    , fpsCounter_{std::chrono::milliseconds{2000}}
{
    CO_CORE_ASSERT(!ctx_.isHeadless(), "Cannot initialize window with a headless context!");

    glfwInit();

    // prevent OpenGL usage - vulkan all the way baybeee
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window_ = glfwCreateWindow(dimensions_.x, dimensions_.y, windowName_.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);

    surface_ = createSurface();
    swapchain_ = createSwapchain();
}

Window::~Window()
{
    glfwDestroyWindow(window_);
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(window_); }

void Window::framebufferResized(glm::i32vec2 newDimensions)
{
    ctx_.device()->DeviceWaitIdle(ctx_.device());
    dimensions_ = newDimensions;
    swapchain_.reset(); // clear old swapchain objects before creating the new ones
    swapchain_ = createSwapchain();
    // onSwapchainResized(newDimensions);
}

BasicVkObjectWrapper<VkSurfaceKHR> Window::createSurface()
{
    VkSurfaceKHR surfaceHandle;
    auto ret = glfwCreateWindowSurface(ctx_.instance(), window_, nullptr, &surfaceHandle);

    BasicVkObjectWrapper<VkSurfaceKHR> surface{
        surfaceHandle, [&instance = ctx_.instance()](auto s) {
            instance->DestroySurfaceKHR(instance, s, nullptr);
        }};

    CO_CORE_ASSERT(ret == VK_SUCCESS, "Could not create surface!");
    nameVulkanObject(ctx_.device(), surface, "Main Window Surface");

    return surface;
}

std::unique_ptr<Swapchain> Window::createSwapchain()
{
    SwapchainSupportDetails swapchainSupport =
        SwapchainSupportDetails::query(ctx_, surface_.handle());

    uint32_t imageCount = swapchainSupport.chooseImageCount();

    VkSwapchainCreateInfoKHR createInfo{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};

    auto surfaceFormat = swapchainSupport.chooseSwapSurfaceFormat();

    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapchainSupport.chooseSwapExtent(toVkExtent2D(dimensions_));
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (!ranges::contains(swapchainSupport.presentFamilies, ctx_.graphicsQueueFamily())) {
        // TODO need to fix this at some point, but currently we don't spend the effort to
        //      create a separate present queue if necessary - needs fixing in the Context
        //      constructor!
        // uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};
        // createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        // createInfo.queueFamilyIndexCount = 2;
        // createInfo.pQueueFamilyIndices = queueFamilyIndices;
        throw std::runtime_error(fmt::format("graphics queue does not support presenting"));
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;     // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = swapchainSupport.chooseSwapPresentMode();
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    return std::make_unique<Swapchain>(ctx_, surface_, createInfo);
}

FrameContext Window::nextSwapchainImage()
{
    FrameContext frameCtx = swapchain_->nextImage();

    // if the swapchain needs resizing, we
    if (frameCtx.shouldRecreateSwapchain) {
        do {
            glfwPollEvents();
            VkSurfaceCapabilitiesKHR capabilities{};
            ctx_.instance()->GetPhysicalDeviceSurfaceCapabilitiesKHR(
                ctx_.physicalDevice(), surface_, &capabilities);
            dimensions_ = {capabilities.currentExtent.width, capabilities.currentExtent.height};
        } while (dimensions_.x == 0 || dimensions_.y == 0);

        ctx_.device()->DeviceWaitIdle(ctx_.device());
        swapchain_.reset();
        swapchain_ = createSwapchain();
        onSwapchainResized.invoke(dimensions_);
        return nextSwapchainImage();
    }

    return frameCtx;
}
void Window::present(FrameContext &&frameCtx)
{
    swapchain_->present(frameCtx);

    if (fpsCounter_.lap()) {
        auto s = fpsCounter_.stats();
        auto fps = fmt::format("FPS: {:3.2f} ({:3.2f} ms)",
                               float(1'000'000'000) / float(s.avg),
                               float(s.avg) / 1'000'000);
        CO_CORE_INFO(fps);
        glfwSetWindowTitle(window_, fps.c_str());
    }
}

} // namespace Cory