#include "Cory/UI/Window.hpp"

#include <Cory/Core/Context.hpp>
#include <Cory/Core/Log.hpp>

// clang-format off
#include <MagnumExternal/Vulkan/flextVkGlobal.h>
#include <GLFW/glfw3.h>
// clang-format on

namespace Cory {

Window::Window(Context &context, glm::ivec2 dimensions, std::string windowName)
    : ctx_{context}
    , windowName_{std::move(windowName)}
{
    CO_CORE_ASSERT(!ctx_.isHeadless(), "Cannot initialize window with a headless context!");

    glfwInit();

    // prevent OpenGL usage
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // no resizing for now - swapchain is complicated
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window_ = glfwCreateWindow(dimensions.x, dimensions.y, windowName_.c_str(), nullptr, nullptr);
}

Window::~Window()
{
    glfwDestroyWindow(window_);
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(window_); }
} // namespace Cory