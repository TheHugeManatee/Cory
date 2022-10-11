#pragma once

#include <Cory/Base/Callback.hpp>
#include <Cory/Base/Common.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/RenderCore/VulkanUtils.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <Magnum/Vk/Image.h>
#include <Magnum/Vk/ImageView.h>

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
    /// access the offscreen color images
    [[nodiscard]] auto &colorImages() const noexcept { return colorImages_; }
    /// access the offscreen color image views
    [[nodiscard]] auto &colorViews() noexcept { return colorImageViews_; }
    /// pixel format of the offscreen depth images
    [[nodiscard]] Magnum::Vk::PixelFormat depthFormat() const noexcept { return depthFormat_; }
    /// access the offscreen depth images
    [[nodiscard]] auto &depthImages() const noexcept { return depthImages_; }
    /// access the offscreen depth image views
    [[nodiscard]] auto &depthViews() noexcept { return depthImageViews_; }

    /**
     * This callback is called whenever the swapchain is resized and the application should
     * create new, appropriately sized resources.
     *
     * It is called from within `nextSwapchainImage()` if a swapchain resize event is detected.
     */
    Callback<glm::i32vec2> onSwapchainResized;

  private:
    [[nodiscard]] BasicVkObjectWrapper<VkSurfaceKHR> createSurface();
    [[nodiscard]] std::unique_ptr<Swapchain> createSwapchain();
    // create the (multisampled) color images
    void createColorAndDepthResources();

    Context &ctx_;
    std::string windowName_;
    int32_t sampleCount_;
    glm::i32vec2 dimensions_;
    std::shared_ptr<GLFWwindow> window_{};

    BasicVkObjectWrapper<VkSurfaceKHR> surface_{};
    std::unique_ptr<Swapchain> swapchain_;
    Magnum::Vk::PixelFormat colorFormat_;
    Magnum::Vk::PixelFormat depthFormat_;
    std::vector<Magnum::Vk::Image> colorImages_;
    std::vector<Magnum::Vk::ImageView> colorImageViews_;
    std::vector<Magnum::Vk::Image> depthImages_;
    std::vector<Magnum::Vk::ImageView> depthImageViews_;

    LapTimer fpsCounter_;
};

} // namespace Cory
