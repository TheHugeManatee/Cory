#pragma once

#include "Context.h"
#include "Image.h"

#include <ranges>
#include <vulkan/vulkan.hpp>

class GLFWwindow;

namespace Cory {

class Application {
  public:
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

#ifdef NDEBUG
    static constexpr bool enableValidationLayers = false;
#else
    static constexpr bool enableValidationLayers = true;
#endif

  protected:
    void requestLayers(std::vector<const char *> layers);
    void requestExtensions(std::vector<const char *> extensions);

  protected: // soon to be private after refactoring is complete
    void initWindow(vk::Extent2D extent);

    void setupInstance();
    void createSurface();

    void pickPhysicalDevice();
    // set up the logical device. this creates the queues and instantiates the features
    void createLogicalDevice();
    bool checkDeviceExtensionSupport(const vk::PhysicalDevice &device);

    void setupDebugMessenger();
    void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT &createInfo);

    void createMemoryAllocator();
    void createCommandPools();

    void createSyncObjects(uint32_t maxFramesInFlight);

    void createColorResources();
    void createDepthResources();
    void createFramebuffers(vk::RenderPass renderPass);

    bool isDeviceSuitable(const vk::PhysicalDevice &device);

  protected: // members
    GLFWwindow *m_window{};

    GraphicsContext m_ctx{};

    vk::SurfaceKHR m_surface{};
    std::unique_ptr<SwapChain> m_swapChain;

    // per frame resources
    vk::SampleCountFlagBits m_msaaSamples{vk::SampleCountFlagBits::e1};

    // handling window resizes happens automatically based on the result values of
    // vkAcquireNextFrameKHR and vkQueuePresentKHR. however, it might not be reliable on some
    // drivers, so we use this flag to check the glfw resize events explicitly
    bool m_framebufferResized{};

    DepthBuffer m_depthBuffer;
    RenderBuffer m_renderTarget;

    std::vector<vk::Framebuffer> m_swapChainFramebuffers;

    std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
    std::vector<vk::UniqueFence> m_inFlightFences;
    std::vector<vk::Fence> m_imagesInFlight;

    vk::DebugUtilsMessengerEXT m_debugMessenger{};

  private:
    // setup of validation layers
    bool checkValidationLayerSupport();
    std::vector<const char *> getRequiredExtensions();

    std::vector<const char *> m_requestedLayers;     // requested validation layers
    std::vector<const char *> m_requestedExtensions; // requested device extensions
};

} // namespace Cory