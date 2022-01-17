#include "VulkanRenderer.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <Cory/Log.h>

#include <spdlog/spdlog.h>

// utility to shorten the typical enumerate pattern you need in c++
template <typename ReturnT, typename FunctionT, typename... FunctionParameters>
std::vector<ReturnT> vk_enumerate(FunctionT func, FunctionParameters... parameters)
{
    uint32_t count = 0;
    func(parameters..., &count, nullptr);
    std::vector<ReturnT> values{size_t(count)};
    func(parameters..., &count, nullptr);
    return values;
}

GLFWwindow *initWindow(const std::string &window_name, uint32_t width, uint32_t height)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // we don't do resizing yet
    return glfwCreateWindow(width, height, window_name.c_str(), nullptr, nullptr);
}

int main(int argc, char **argv)
{
    Cory::Log::Init();

    CO_APP_INFO("Compat test running");

    GLFWwindow *window = initWindow("Test window", 800, 600);

    try {
        VulkanRenderer renderer(window);

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }
    catch (const std::exception &e) {
        CO_APP_ERROR("Uncaught exception: {}", e.what());
    }
    catch (...) {
        CO_APP_ERROR("Uncaught exception in main.");
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    CO_APP_INFO("Application finished");
    return 0;
}