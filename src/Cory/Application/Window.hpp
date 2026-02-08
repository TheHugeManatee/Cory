#pragma once

#include <Cory/Application/Event.hpp>
#include <Cory/Base/Common.hpp>
#include <Cory/Base/Primitives.hpp>
#include <Cory/Renderer/Common.hpp>
#include <Cory/Renderer/FrameGenerator.hpp>
#include <Cory/Renderer/FrameSource.hpp>
#include <Cory/Renderer/Gpu.hpp>

#include <kdbindings/property.h>
#include <kdbindings/signal.h>

#include <glm/vec2.hpp>
#include <gsl/gsl>

#include <cstddef>
#include <memory>
#include <string>

struct GLFWwindow;

namespace Cory {

class Context;
struct WindowPrivate;

class Window : public FrameSource, NoCopy, NoMove {
  public:
    Window(Context &context,
           glm::i32vec2 dimensions,
           std::string windowName,
           int32_t sampleCount = 1);
    ~Window() override;

    [[nodiscard]] bool shouldClose() const;

    [[nodiscard]] Swapchain &swapchain();

    [[nodiscard]] FrameContext nextSwapchainImage();
    void submitAndPresent(FrameContext &frameCtx);
    [[nodiscard]] FrameGenerator frames() override;

    /// pixel format of the offscreen color images
    [[nodiscard]] Gpu::Format colorFormat() const noexcept override;
    /// pixel format of the offscreen depth images
    [[nodiscard]] Gpu::Format depthFormat() const noexcept override;
    [[nodiscard]] glm::u32vec2 extent() const noexcept override;
    [[nodiscard]] Gpu::SampleCountFlagBits sampleCount() const noexcept override;
    [[nodiscard]] size_t size() const noexcept override;

    /**
     * This signal is emitted whenever the swapchain is resized and the application should
     * create new, appropriately sized resources.
     *
     * It is called from within `nextSwapchainImage()` or `frames()` if a swapchain resize event is
     * detected.
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

    /// The sample count of the window
    KDBindings::Property<Gpu::SampleCountFlagBits> samples;

    KDBindings::Property<i32vec2> dimensions{i32vec2(1024, 768)};

    KDBindings::Property<std::string> title{"Cory Window"};

    gsl::not_null<GLFWwindow *> getGlfwWindow() const;

  private:
    void createWindow();
    FrameContext acquireFrameContext();
    cppcoro::generator<FrameContext> frameGenerator();
    // Format the title out of the current window title and some stats/metadata
    void updateTitle();

    friend struct WindowPrivate;
    std::unique_ptr<WindowPrivate> data_;
};

} // namespace Cory
