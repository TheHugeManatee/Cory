#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm.h>

#include <spdlog/spdlog.h>

template <typename ReturnT, typename FunctionT, typename... FunctionParameters>
std::vector<ReturnT> vk_enumerate(FunctionT func, FunctionParameters... parameters)
{
    uint32_t count = 0;
    func(parameters..., &count, nullptr);
    std::vector<ReturnT> values{size_t(count)};
    func(parameters..., &count, nullptr);
    return values;
}

int main(int argc, char **argv)
{
    spdlog::info("Compat test running");

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow *wnd = glfwCreateWindow(800, 600, "Test Window", nullptr, nullptr);

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    auto extensions =
        vk_enumerate<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, nullptr);

    spdlog::info("Extension count: {}", extensions.size());

    glm::mat4 test_matrix(1.0f);
    glm::vec4 test_vector(1.0f);
    auto test_result = test_matrix * test_vector;
    spdlog::info("Test result: {} {} {}", test_result.x, test_result.y, test_result.z);

    while (!glfwWindowShouldClose(wnd)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(wnd);

    glfwTerminate();

    spdlog::info("Application finished");
    return 0;
}