#include "HelloTriangleApplication.hpp"

#include <Cory/Core/Log.hpp>
#include <Cory/Cory.hpp>

#include <GLFW/glfw3.h>

HelloTriangleApplication::HelloTriangleApplication()
    : window_{{1024, 768}, "HelloTriangle"}
{
    Cory::Init();
    CO_APP_INFO("Vulkan instance version is {}", Cory::queryVulkanInstanceVersion());
}

void HelloTriangleApplication::run() {
    while(!window_.shouldClose()) {
        glfwPollEvents();
    }
}
