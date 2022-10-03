#include "Cory/UI/Window.hpp"

#include <Core/APIConversion.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Core/Context.hpp>
#include <Cory/Core/VulkanUtils.hpp>
#include <Cory/UI/SwapChain.hpp>

#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/Instance.h>
// clang-format off
#include <MagnumExternal/Vulkan/flextVk.h>
#define VK_VERSION_1_0
#include <GLFW/glfw3.h>
// clang-format on

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

    // no resizing for now - SwapChain is complicated
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
    static constexpr uint32_t FRAMES_IN_FLIGHT = 3;

    SwapChainSupportDetails swapChainSupport =
        SwapChainSupportDetails::query(ctx_, surface_.handle());

    VkSwapchainCreateInfoKHR createInfo{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};

    createInfo.presentMode = swapChainSupport.chooseSwapPresentMode();
    // createInfo.f =  swapChainSupport.chooseSwapSurfaceFormat();
    createInfo.imageExtent = swapChainSupport.chooseSwapExtent(toVkExtent2D(dimensions_));

    throw std::logic_error{"SwapChain not yet created!"};
    // SwapChain chain(ctx_, FRAMES_IN_FLIGHT, surface_, createInfo);
}
} // namespace Cory