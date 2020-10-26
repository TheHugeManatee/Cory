#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>


class HelloTriangleApplication {
public:
	static constexpr uint32_t WIDTH{ 800 };
	static constexpr uint32_t HEIGHT = { 600 };

	void run();

private:
	void initWindow();
	void initVulkan();
	void mainLoop();
	void cleanup();

	GLFWwindow* m_window;
	VkInstance m_instance;
};
