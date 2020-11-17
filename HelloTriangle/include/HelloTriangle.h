#pragma once

#include <vulkan/vulkan.hpp>

#include <Buffer.h>
#include <Image.h>
#include <Utils.h>
#include <VkUtils.h>
#include <Mesh.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <memory>
#include <vector>

using namespace Cory;

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

    void createTransientCommandPool();

    void mainLoop();
    void cleanup();

    void setupInstance();

    // setup of validation layers
    bool checkValidationLayerSupport();
    std::vector<const char *> getRequiredExtensions();

    // set up of debug callback
    void setupDebugMessenger();
    void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT &createInfo);

    void createSurface();

    // list all vulkan devices and pick one that is suitable for our purposes.
    void pickPhysicalDevice();

    // figure out which queue families are supported (like memory transfer, compute, graphics etc.)
    QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice &device);

    // set up the logical device. this creates the queues and instantiates the features
    void createLogicalDevice();

    bool checkDeviceExtensionSupport(const vk::PhysicalDevice &device);
    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device,
                                                  vk::SurfaceKHR surface);
    vk::SurfaceFormatKHR
    chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);
    vk::PresentModeKHR
    chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities);

    void createSwapChain();
    void createImageViews();

    vk::UniqueShaderModule createShaderModule(const std::vector<char> &code);

    void createGraphicsPipeline();
    void createRenderPass();
    void createFramebuffers();
    void createAppCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void recreateSwapChain();
    void cleanupSwapChain();
    void createGeometry();
    void createVertexBuffers(const std::vector<Vertex> &vertices);
    void createIndexBuffer(const std::vector<uint16_t> &indices);
    void createUniformBuffers();
    void createDescriptorSetLayout();
    device_texture createTextureImage(std::string textureFilename, vk::Filter filter,
                                      vk::SamplerAddressMode addressMode);

    void drawFrame();

    void updateUniformBuffer(uint32_t imageIndex);

    void createDescriptorPool();
    void createDescriptorSets();
    void createColorResources();
    void createDepthResources();

    void createMemoryAllocator();

  private:
    bool isDeviceSuitable(const vk::PhysicalDevice &device);
    vk::SampleCountFlagBits getMaxUsableSampleCount();

  private:
    GLFWwindow *m_window;
    graphics_context m_ctx;

    vk::SampleCountFlagBits m_msaaSamples{vk::SampleCountFlagBits::e1};

    vk::SurfaceKHR m_surface;
    vk::SwapchainKHR m_swapChain;
    std::vector<vk::Image> m_swapChainImages;
    vk::Format m_swapChainImageFormat;
    vk::Extent2D m_swapChainExtent;
    std::vector<vk::ImageView> m_swapChainImageViews;

    vk::RenderPass m_renderPass;
    vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
    vk::UniqueDescriptorPool m_descriptorPool;
    std::vector<vk::DescriptorSet> m_descriptorSets;
    vk::UniquePipelineLayout m_pipelineLayout;
    vk::UniquePipeline m_graphicsPipeline;
    std::vector<vk::Framebuffer> m_swapChainFramebuffers;

    vk::UniqueCommandPool m_commandPool;
    std::vector<vk::UniqueCommandBuffer> m_commandBuffers;
    std::vector<device_buffer> m_uniformBuffers;

    size_t m_currentFrame{};

    std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
    std::vector<vk::UniqueFence> m_inFlightFences;
    std::vector<vk::Fence> m_imagesInFlight;

    uint32_t m_numVertices;
    device_buffer m_vertexBuffer;
    device_buffer m_indexBuffer;

    depth_buffer m_depthBuffer;
    render_target m_renderTarget;
    device_texture m_texture;
    device_texture m_texture2;

    vk::DebugUtilsMessengerEXT m_debugMessenger;
};
