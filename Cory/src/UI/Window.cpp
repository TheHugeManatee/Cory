#include "Cory/UI/Window.hpp"

#include <GLFW/glfw3.h>
#include <MagnumExternal/Vulkan/flextVkGlobal.h>

namespace Cory {

Window::Window(glm::ivec2 dimensions, std::string windowName)
    : windowName_{std::move(windowName)}
{
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