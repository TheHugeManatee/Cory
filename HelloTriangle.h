#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Utils.h"

#include <cstdint>
#include <vector>

class HelloTriangleApplication {
public:
	static constexpr uint32_t WIDTH{ 800 };
	static constexpr uint32_t HEIGHT = { 600 };

#ifdef NDEBUG
	static constexpr bool enableValidationLayers = false;
#else
	static constexpr bool enableValidationLayers = true;
#endif

	static const std::vector<const char*> validationLayers;

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	void run();

private:
	void initWindow();
	void initVulkan();

	void setupInstance();

	void mainLoop();
	void cleanup();

	// setup of validation layers
	bool checkValidationLayerSupport();
	std::vector<const char*> getRequiredExtensions();

	// set up of debug callback
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	void setupDebugMessenger();

	// list all vulkan devices and pick one that is suitable for our purposes.
	void pickPhysicalDevice();

	// figure out which queue families are supported (like memory transfer, compute, graphics etc.)
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

private:
	GLFWwindow* m_window;
	VkInstance m_instance;
	VkPhysicalDevice m_physicalDevice{ VK_NULL_HANDLE };

	VkDebugUtilsMessengerEXT m_debugMessenger;
	bool isDeviceSuitable(const  VkPhysicalDevice& device);
};
