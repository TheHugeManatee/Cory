#pragma once

#include "Context.h"
#include "Image.h"

#include <vulkan/vulkan.hpp>

class GLFWwindow;

namespace Cory {

class Application {
  public:
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

    static const int MAX_FRAMES_IN_FLIGHT{2};

#ifdef NDEBUG
    static constexpr bool enableValidationLayers = false;
#else
    static constexpr bool enableValidationLayers = true;
#endif

    struct FrameUpdateInfo {
        uint32_t swapChainImageIdx;            // index of the swap chain image
        size_t currentFrameIdx;                // current frame index
        vk::Semaphore imageAvailableSemaphore; // client needs to wait for this semaphore before
                                               // drawing to the swap chain image
        vk::Semaphore renderFinishedSemaphore; // this needs to be signaled by last client submit
                                               // call as the VkPresentKHR call waits on it
        vk::Fence
            imageInFlightFence; // fence of the image in flight, to be signaled by the final submit
    };

    Application();

    /**
     * Main entry point
     */
    void run();

    /**
     * This function has to be overridden by an application implementation in order to draw a frame
     * It should wait for the imageAvailableSemaphore before writing to the swap chain image, and it
     * must signal the renderFinishedSemaphore with a VkQueueSubmit or explicitly.
     */
    virtual void drawSwapchainFrame(FrameUpdateInfo &fui) = 0;

    /**
     * This function is designated to allocate and initialize any resoruces that are dependent on
     * the number of swap chain images and potentially needs references to the swap chain. All of
     * the resources that are created here should be destroyed in
     * `destroySwapChainDependentResources`.
     * This pair of functions will be called at app startup as well as every time the window is
     * resized, thus it should only (re-)create resources that actually depend on the window size
     * and nothing else.
     */
    virtual void createSwapchainDependentResources() = 0;
    /**
     * @see `createSwapchainDependentResources`
     */
    virtual void destroySwapchainDependentResources() = 0;

    /**
     * This function should be used by an application to initialize itself and allocate permanent
     * resources, for example load textures, shaders and other resources. These resources should be
     * destroyed in `deinit`, as the vulkan context does not exist anymore at the time of
     * destruction of the app
     */
    virtual void init() = 0;
    /**
     * @see `init`
     */
    virtual void deinit() = 0;

  public: /// Startup configuration API, i.e. only to be called before initVulkan, i.e. in the
          /// client app constructor
    void requestLayers(std::vector<const char *> layers);
    void requestExtensions(std::vector<const char *> extensions);
    void setInitialWindowSize(uint32_t width, uint32_t height);

  protected: // accessors that should only be available to app implementations
    auto &swapChain() { return *m_swapChain; }
    auto &ctx() { return m_ctx; }
    auto msaaSamples() const { return m_msaaSamples; }
    auto &colorBuffer() const { return m_renderTarget; }
    auto &depthBuffer() const { return m_depthBuffer; }

  private:
    /**
     * Internal vulkan initialization
     */
    void initVulkan();

    /**
     * The main loop, consisting of:
     * 1. Input polling and processing
     * 2. Logic update
     * 3. Frame update
     */
    void mainLoop();

    /**
     * Clean up all the resources, including vulkan resources and
     */
    void cleanup();

    void initWindow();

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

    void drawFrame();

    void cleanupSwapChain();
    void recreateSwapChain();

    bool isDeviceSuitable(const vk::PhysicalDevice &device);

  protected:
    GraphicsContext m_ctx{};

  private: // members
    GLFWwindow *m_window{};

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

    std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
    std::vector<vk::UniqueFence> m_inFlightFences;
    std::vector<vk::Fence> m_imagesInFlight;

    vk::DebugUtilsMessengerEXT m_debugMessenger{};

  private:
    size_t m_currentFrame{};

    // setup of validation layers
    bool checkValidationLayerSupport();
    std::vector<const char *> getRequiredExtensions();

    std::vector<const char *> m_requestedLayers;     // requested validation layers
    std::vector<const char *> m_requestedExtensions; // requested device extensions
    vk::Extent2D m_initialWindowSize;
};

} // namespace Cory