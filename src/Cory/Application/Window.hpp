#pragma once

#include <Cory/Application/Event.hpp>
#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/KDGpuFwd.hpp>

#include <kdbindings/property.h>
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

    [[nodiscard]] glm::i32vec2 dimensions() const;

    [[nodiscard]] KDGpu::Swapchain &swapchain();

    [[nodiscard]] FrameContext nextSwapchainImage();
    void submitAndPresent(FrameContext &frameCtx);

    /// pixel format of the offscreen color images
    [[nodiscard]] KDGpu::Format colorFormat() const noexcept;
    /// pixel format of the offscreen depth images
    [[nodiscard]] KDGpu::Format depthFormat() const noexcept;

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
    friend struct WindowPrivate;
    std::unique_ptr<WindowPrivate> data_;
};

} // namespace Cory
