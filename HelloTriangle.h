#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Utils.h"

#include <cstdint>
#include <vector>

class HelloTriangleApplication {
  public:
	static constexpr uint32_t WIDTH{800};
	static constexpr uint32_t HEIGHT = {600};

#ifdef NDEBUG
	static constexpr bool enableValidationLayers = false;
#else
	static constexpr bool enableValidationLayers = true;
#endif

	static const std::vector<const char *> validationLayers;
	static const std::vector<const char *> deviceExtensions;

	static VKAPI_ATTR VkBool32 VKAPI_CALL
	debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
				  VkDebugUtilsMessageTypeFlagsEXT messageType,
				  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

	void run();

  private:
	void initWindow();
	void initVulkan();

	void mainLoop();
	void cleanup();

	void setupInstance();

	// setup of validation layers
	bool checkValidationLayerSupport();
	std::vector<const char *> getRequiredExtensions();

	// set up of debug callback
	void setupDebugMessenger();
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);

	void createSurface();

	// list all vulkan devices and pick one that is suitable for our purposes.
	void pickPhysicalDevice();

	// figure out which queue families are supported (like memory transfer, compute, graphics etc.)
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

	// set up the logical device. this creates the queues and instantiates the features
	void createLogicalDevice();

	bool checkDeviceExtensionSupport(const VkPhysicalDevice &device);

  private:
	bool isDeviceSuitable(const VkPhysicalDevice &device);

  private:
	GLFWwindow *m_window;
	VkInstance m_instance;
	VkDevice m_device;
	VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
	VkSurfaceKHR m_surface;
	VkQueue m_graphicsQueue;
	VkQueue m_presentQueue;

	VkDebugUtilsMessengerEXT m_debugMessenger;
};
