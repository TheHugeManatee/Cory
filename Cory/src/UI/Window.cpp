#include "Cory/UI/Window.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Core/Context.hpp>
#include <Cory/Core/VulkanUtils.hpp>

#include <Magnum/Vk/Instance.h>
#include <Magnum/Vk/Device.h>
// clang-format off
#include <MagnumExternal/Vulkan/flextVk.h>
#include <MagnumExternal/Vulkan/flextVkGlobal.h>
#define VK_VERSION_1_0
#include <GLFW/glfw3.h>
// clang-format on

namespace Cory {

Window::Window(Context &context, glm::ivec2 dimensions, std::string windowName)
    : ctx_{context}
    , windowName_{std::move(windowName)}
{
    CO_CORE_ASSERT(!ctx_.isHeadless(), "Cannot initialize window with a headless context!");

    glfwInit();

    // prevent OpenGL usage - vulkan all the way baybeee
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // no resizing for now - SwapChain is complicated
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window_ = glfwCreateWindow(dimensions.x, dimensions.y, windowName_.c_str(), nullptr, nullptr);

    createSurface();
}

Window::~Window()
{
    vkDestroySurfaceKHR(ctx_.instance().handle(), surface_, nullptr);
    glfwDestroyWindow(window_);
    glfwTerminate();
}

void Window::createSurface()
{
    auto ret = glfwCreateWindowSurface(ctx_.instance().handle(), window_, nullptr, &surface_);
    CO_CORE_ASSERT(ret == VK_SUCCESS, "Could not create surface!");
    nameRawVulkanObject(ctx_.device().handle(), surface_, "Main Window Surface");
}

bool Window::shouldClose() const { return glfwWindowShouldClose(window_); }
} // namespace Cory