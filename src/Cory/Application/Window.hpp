#pragma once

#include <Cory/Application/Event.hpp>
#include <Cory/Base/Callback.hpp>
#include <Cory/Base/Common.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Renderer/Common.hpp>

#include <KDGpu/fence.h>
#include <KDGpu/gpu_semaphore.h>
#include <KDGpu/surface.h>
#include <KDGpu/swapchain.h>
#include <KDGpuKDGui/view.h>

#include <kdbindings/signal.h>

#include <glm/vec2.hpp>

#include <memory>
#include <string>

namespace Cory {

class Context;

class Window : NoCopy, NoMove {
  public:
    static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

    Window(Context &context,
           glm::i32vec2 dimensions,
           std::string windowName,
           int32_t sampleCount = 1);
    ~Window();

    [[nodiscard]] bool shouldClose() const;

    glm::i32vec2 dimensions() const { return dimensions_; }

    KDGpu::Swapchain &swapchain() { return swapchain_; };

    [[nodiscard]] FrameContext nextSwapchainImage();
    void submitAndPresent(FrameContext &frameCtx);

    /// pixel format of the offscreen color images
    [[nodiscard]] KDGpu::Format colorFormat() const noexcept { return swapchainSetup_.format; }
    /// pixel format of the offscreen depth images
    [[nodiscard]] KDGpu::Format depthFormat() const noexcept { return swapchainSetup_.depthFormat; }

    /**
     * This signal is emitted whenever the swapchain is resized and the application should
     * create new, appropriately sized resources.
     *
     * It is called from within `nextSwapchainImage()` if a swapchain resize event is detected.
     */
    KDBindings::Signal<SwapchainResizedEvent> onSwapchainResized;

    /// emitted when the mouse has moved over the window
    KDBindings::Signal<MouseMovedEvent> onMouseMoved;

    /// emitted when a mouse button is pressed or released
    KDBindings::Signal<MouseButtonEvent> onMouseButton;

    /// emitted when the mouse is scrolling, providing
    KDBindings::Signal<ScrollEvent> onMouseScrolled;

    /// emitted when a keyboard key is called
    KDBindings::Signal<KeyEvent> onKeyCallback;

    // The sample count of the window
    KDBindings::Property<KDGpu::SampleCountFlagBits> samples;

  private:
    // Use the device and surface to determine swapchainOptions
    void determineSwapchainOptions();
    [[nodiscard]] KDGpu::Swapchain createSwapchain();
    // create the (multisampled) color images
    void createColorAndDepthResources();
    void createWindow();

  private:
    Context &ctx_;
    std::string windowName_;
    glm::i32vec2 dimensions_;
    std::unique_ptr<KDGpuKDGui::View> window_{};

    KDGpu::Surface surface_{};
    struct {
        KDGpu::Format format{KDGpu::Format::B8G8R8A8_UNORM};
        KDGpu::CompositeAlphaFlagBits compositeAlpha{KDGpu::CompositeAlphaFlagBits::OpaqueBit};
        KDGpu::TextureUsageFlags usageFlags{KDGpu::TextureUsageFlagBits::ColorAttachmentBit};
        KDGpu::Format depthFormat;
        KDGpu::TextureUsageFlags depthImageUsage_;
        std::vector<KDGpu::SampleCountFlagBits> supportedSampleCounts;

        KDGpu::Extent2D extent;
        bool showSurfaceCapabilities{false};
        std::string capabilitiesString;
        KDGpu::PresentMode presentMode;
    } swapchainSetup_;
    KDGpu::Swapchain swapchain_;
    std::vector<KDGpu::TextureView> swapchainViews_;

    std::vector<KDGpu::Texture> colorImages_;
    std::vector<KDGpu::TextureView> colorImageViews_;
    std::vector<KDGpu::Texture> depthImages_;
    std::vector<KDGpu::TextureView> depthImageViews_;

    uint32_t currentSwapchainImageIndex_{0};
    uint32_t inFlightIndex_{0};
    std::array<KDGpu::GpuSemaphore, FRAMES_IN_FLIGHT> presentCompleteSemaphores_;
    std::array<KDGpu::GpuSemaphore, FRAMES_IN_FLIGHT> renderCompleteSemaphores_;
    std::array<KDGpu::Fence, FRAMES_IN_FLIGHT> frameCompletedFences_;

    LapTimer fpsCounter_;
};

} // namespace Cory
