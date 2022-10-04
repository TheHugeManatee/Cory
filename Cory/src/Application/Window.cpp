#include <Cory/Application/Window.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/RenderCore/APIConversion.hpp>
#include <Cory/RenderCore/Context.hpp>
#include <Cory/RenderCore/VulkanUtils.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/Instance.h>
#include <Magnum/Vk/Queue.h>
// clang-format off
#include <MagnumExternal/Vulkan/flextVk.h>
#define VK_VERSION_1_0
#include <GLFW/glfw3.h>
// clang-format on

#include <range/v3/algorithm/contains.hpp>

namespace Cory {

Window::Window(Context &context, glm::i32vec2 dimensions, std::string windowName)
    : ctx_{context}
    , dimensions_(dimensions)
    , windowName_{std::move(windowName)}
{
    CO_CORE_ASSERT(!ctx_.isHeadless(), "Cannot initialize window with a headless context!");

    glfwInit();

    // prevent OpenGL usage - vulkan all the way baybeee
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // no resizing for now - Swapchain is complicated
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window_ = glfwCreateWindow(dimensions_.x, dimensions_.y, windowName_.c_str(), nullptr, nullptr);

    createSurface();
    createSwapChain();
}

Window::~Window()
{
    glfwDestroyWindow(window_);
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(window_); }

void Window::createSurface()
{
    VkSurfaceKHR surface;
    auto ret = glfwCreateWindowSurface(ctx_.instance(), window_, nullptr, &surface);
    surface_ =
        std::shared_ptr<VkSurfaceKHR_T>{surface, [&instance = ctx_.instance()](auto s) {
                                            instance->DestroySurfaceKHR(instance, s, nullptr);
                                        }};

    CO_CORE_ASSERT(ret == VK_SUCCESS, "Could not create surface!");
    nameVulkanObject(ctx_.device(), surface_, "Main Window Surface");
}

void Window::createSwapChain()
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

    swapchain_ = std::make_unique<Swapchain>(ctx_, surface_, createInfo);
}

} // namespace Cory