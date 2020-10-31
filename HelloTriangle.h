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

	static const int MAX_FRAMES_IN_FLIGHT{2};

	static VKAPI_ATTR VkBool32 VKAPI_CALL
	debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
				  VkDebugUtilsMessageTypeFlagsEXT messageType,
				  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

	void run();

  public:
	// handling window resizes happens automatically based on the result values of
	// vkAcquireNextFrameKHR and vkQueuePresentKHR. however, it might not be reliable on some
	// drivers, so we use this flag to check the glfw resize events explicitly
	bool framebufferResized{};

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
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
	VkSurfaceFormatKHR
	chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
	VkPresentModeKHR
	chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

	void createSwapChain();
	void createImageViews();

	VkShaderModule createShaderModule(const std::vector<char> &code);

	void createGraphicsPipeline();

	void createRenderPass();

	void createFramebuffers();

	void createCommandPool();

	void createCommandBuffers();

	void drawFrame();

	void createSyncObjects();

	void recreateSwapChain();

	void cleanupSwapChain();

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
	VkSwapchainKHR m_swapChain;
	std::vector<VkImage> m_swapChainImages;
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	std::vector<VkImageView> m_swapChainImageViews;

	VkRenderPass m_renderPass;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline;
	std::vector<VkFramebuffer> m_swapChainFramebuffers;

	VkCommandPool m_commandPool;
	std::vector<VkCommandBuffer> m_commandBuffers;

	size_t m_currentFrame{};

	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;
	std::vector<VkFence> m_imagesInFlight;

	VkDebugUtilsMessengerEXT m_debugMessenger;
};
