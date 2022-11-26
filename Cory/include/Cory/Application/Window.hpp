#pragma once

#include <Cory/Base/Callback.hpp>
#include <Cory/Base/Common.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Renderer/Swapchain.hpp>
#include <Cory/Renderer/VulkanUtils.hpp>

#include <Magnum/Vk/Image.h>
#include <Magnum/Vk/ImageView.h>

#include <kdbindings/signal.h>

#include <glm/vec2.hpp>
#include <memory>
#include <string>

struct GLFWwindow;
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;

namespace Cory {

class Context;

class Window : NoCopy, NoMove {
  public:
    Window(Context &context,
           glm::i32vec2 dimensions,
           std::string windowName,
           int32_t sampleCount = 1);
    ~Window();

    [[nodiscard]] bool shouldClose() const;

    glm::i32vec2 dimensions() const { return dimensions_; }

    Swapchain &swapchain() { return *swapchain_; };

    [[nodiscard]] FrameContext nextSwapchainImage();
    void submitAndPresent(FrameContext &&frameCtx);

    [[nodiscard]] GLFWwindow *handle() { return window_.get(); }
    [[nodiscard]] const GLFWwindow *handle() const { return window_.get(); }

    /// the MSAA samples
    [[nodiscard]] int32_t sampleCount() const noexcept { return sampleCount_; }
    /// pixel format of the offscreen color images
    [[nodiscard]] Magnum::Vk::PixelFormat colorFormat() const noexcept { return colorFormat_; }
    /// access the offscreen color image
    [[nodiscard]] auto &colorImage() const noexcept { return colorImage_; }
    /// access the offscreen color image view
    [[nodiscard]] auto &colorView() noexcept { return colorImageView_; }
    /// pixel format of the offscreen depth images
    [[nodiscard]] Magnum::Vk::PixelFormat depthFormat() const noexcept { return depthFormat_; }
    /// access the offscreen depth images
    [[nodiscard]] auto &depthImages() const noexcept { return depthImages_; }
    /// access the offscreen depth image views
    [[nodiscard]] auto &depthViews() noexcept { return depthImageViews_; }

    /**
     * This signal is emitted whenever the swapchain is resized and the application should
     * create new, appropriately sized resources.
     *
     * It is called from within `nextSwapchainImage()` if a swapchain resize event is detected.
     */
    KDBindings::Signal<glm::i32vec2> onSwapchainResized;

    /// emitted when the mouse has moved over the window
    KDBindings::Signal<glm::vec2> onMouseMoved;

    struct MouseButtonData {
        glm::vec2 position;
        int button; // todo: wrap these more nicely into enums
        int action;
        int modifiers;
    };
    /// emitted when a mouse button is pressed or released
    KDBindings::Signal<MouseButtonData> onMouseButton;

    struct ScrollData {
        glm::vec2 position;
        glm::vec2 scrollDelta;
    };
    /// emitted when the mouse is scrolling, providing
    KDBindings::Signal<ScrollData> onMouseScrolled;

    struct KeyData {
        int key;
        int scanCode;
        int action;
        int modifiers;
    };
    struct KDBindings::Signal<KeyData> onKeyCallback;

  private:
    [[nodiscard]] BasicVkObjectWrapper<VkSurfaceKHR> createSurface();
    [[nodiscard]] std::unique_ptr<Swapchain> createSwapchain();
    // create the (multisampled) color images
    void createColorAndDepthResources();
    void createGlfwWindow();

  private:
    Context &ctx_;
    std::string windowName_;
    int32_t sampleCount_;
    glm::i32vec2 dimensions_;
    std::shared_ptr<GLFWwindow> window_{};

    BasicVkObjectWrapper<VkSurfaceKHR> surface_{};
    std::unique_ptr<Swapchain> swapchain_;
    Magnum::Vk::PixelFormat colorFormat_;
    Magnum::Vk::PixelFormat depthFormat_;
    Magnum::Vk::Image colorImage_{Corrade::NoCreate};
    Magnum::Vk::ImageView colorImageView_{Corrade::NoCreate};
    std::vector<Magnum::Vk::Image> depthImages_;
    std::vector<Magnum::Vk::ImageView> depthImageViews_;

    LapTimer fpsCounter_;
};

} // namespace Cory
