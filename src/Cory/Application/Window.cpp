#include <Cory/Application/Window.hpp>

#include <Cory/Base/Callback.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/SingleShotCommandRecorder.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <KDGpu/fence.h>
#include <KDGpu/gpu_semaphore.h>
#include <KDGpu/instance.h>
#include <KDGpu/surface.h>
#include <KDGpu/texture_options.h>
#include <KDGpuKDGui/view.h>
#include <KDGui/gui_application.h>

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/find_first_of.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/transform.hpp>

#include <optional>
#include <thread>

namespace Cory {

struct WindowPrivate {
    Context *ctx;
    std::string windowName;
    std::unique_ptr<KDGpuKDGui::View> window{};
    KDGpu::Surface surface{};

    std::unique_ptr<Swapchain> swapchain;

    LapTimer fpsCounter{std::chrono::milliseconds{2000}};
};

Window::Window(Context &context,
               glm::i32vec2 dimensions,
               std::string windowName,
               int32_t sampleCount)
    : data_{std::make_unique<WindowPrivate>()}
{
    data_->ctx = &context;
    data_->windowName = std::move(windowName);

    samples = static_cast<KDGpu::SampleCountFlagBits>(sampleCount);

    createWindow();

    data_->surface = context.createSurface(data_->windowName, *data_->window);

    data_->swapchain = std::make_unique<Swapchain>(context,
                                                   data_->surface,
                                                   SwapchainCreateInfo{
                                                       .label = data_->windowName,
                                                       .size = dimensions,
                                                       .samples = samples(),
                                                   });

    samples.valueChanged()
        .connect([this]() {
            CO_CORE_INFO("Samples changed to {}", samples());
            CO_CORE_FATAL("Changing sample count dynamically is not implemented yet!");
            // createColorAndDepthResources();
        })
        .release();
}

Window::~Window() { CO_CORE_TRACE("Destroying Cory::Window {}", data_->windowName); }

bool Window::shouldClose() const { return !data_->window->visible(); }

glm::i32vec2 Window::dimensions() const
{
    return {data_->window->width(), data_->window->height()};
}

Swapchain &Window::swapchain() { return *data_->swapchain; }

FrameContext Window::nextSwapchainImage()
{
    const Cory::ScopeTimer s{"Window/NextSwapchainImage"};

    auto nextImageResult = data_->swapchain->nextImage();
    if (!nextImageResult.has_value()) {
        CO_CORE_ASSERT(false, "Swapchain resizing not implemented yet!");
    }

    //
    // // if the swapchain needs resizing, we wait for
    // if (frameCtx.shouldRecreateSwapchain) {

    //     // wait until the surface dimensions are non-zero - this might happen
    //     // while the app is minimized or the window has been resized to zero height
    //     // or width, in which case we don't render anything
    //     do {
    //         glfwPollEvents();
    //         VkSurfaceCapabilitiesKHR capabilities{};
    //         ctx_.instance()->GetPhysicalDeviceSurfaceCapabilitiesKHR(
    //             ctx_.physicalDevice(), surface_, &capabilities);
    //         dimensions_ = {capabilities.currentExtent.width,
    //         capabilities.currentExtent.height}; std::this_thread::yield();
    //     } while (dimensions_.x == 0 || dimensions_.y == 0);
    //
    //     // Hard sync to make sure no commands are in flight before recreating the swapchain
    //     ctx_.device().waitUntilIdle();
    //
    //     // recreate the necessary resized resources and notify client code via
    //     // the onSwaphcainResized callback
    //     swapchain_ = {};
    //     swapchain_ = createSwapchain();
    //     createColorAndDepthResources();
    //     onSwapchainResized.emit({.size{dimensions_}});
    //
    //     // retry the whole thing
    //     return nextSwapchainImage();
    //}

    return std::move(nextImageResult).value();
}

void Window::submitAndPresent(FrameContext &frameCtx)
{

    data_->swapchain->present(frameCtx);

    if (data_->fpsCounter.lap()) {
        auto s = data_->fpsCounter.stats();
        auto fps = fmt::format("{} FPS: {:3.2f} ({:3.2f} ms)",
                               data_->windowName,
                               float(1'000'000'000) / float(s.avg),
                               float(s.avg) / 1'000'000);
        CO_CORE_INFO(fps);
        data_->window->title = fps;
    }
}
KDGpu::Format Window::colorFormat() const noexcept { return data_->swapchain->colorFormat(); }
KDGpu::Format Window::depthFormat() const noexcept { return data_->swapchain->depthFormat(); }

void Window::createWindow()
{
    data_->window = std::make_unique<KDGpuKDGui::View>();
    data_->window->title = data_->windowName;

    // auto windowHandle =
    //     glfwCreateWindow(dimensions_.x, dimensions_.y, windowName_.c_str(), nullptr,
    //     nullptr);
    // window_ = std::shared_ptr<GLFWwindow>(windowHandle, [=](auto *ptr) {
    //     CO_CORE_TRACE("Destroying GLFW context");
    //     glfwDestroyWindow(ptr);
    //     glfwTerminate();
    // });
    // glfwSetWindowUserPointer(window_.get(), this);
    //
    // glfwSetCursorPosCallback(window_.get(), [](GLFWwindow *window, double mouseX, double
    // mouseY)
    // {
    //     Window &self = *reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
    //     self.onMouseMoved.emit({.position = {mouseX, mouseY},
    //                             .button = detail::getMouseButtonState(window),
    //                             .modifiers = detail::getModifierState(window)});
    // });
    //
    // glfwSetMouseButtonCallback(
    //     window_.get(), [](GLFWwindow *window, int button, int action, int mods) {
    //         Window &self = *reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
    //         double mouseX, mouseY;
    //         glfwGetCursorPos(window, &mouseX, &mouseY);
    //         self.onMouseButton.emit(MouseButtonEvent{
    //             .position = glm::vec2{mouseX, mouseY},
    //             .button = detail::getMouseButtonState(window),
    //             .action = action == GLFW_PRESS ? ButtonAction::Press : ButtonAction::Release,
    //             .modifiers = detail::getModifierState(window)});
    //     });
    //
    // glfwSetScrollCallback(window_.get(), [](GLFWwindow *window, double xOffset, double
    // yOffset) {
    //     Window &self = *reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
    //     double mouseX, mouseY;
    //     glfwGetCursorPos(window, &mouseX, &mouseY);
    //     self.onMouseScrolled.emit({.position = {mouseX, mouseY},
    //                                .scrollDelta = {xOffset, yOffset},
    //                                .modifiers = detail::getModifierState(window)});
    // });
    //
    // glfwSetKeyCallback(
    //     window_.get(), [](GLFWwindow *window, int key, int scancode, int action, int mods) {
    //         Window &self = *reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
    //         self.onKeyCallback.emit(
    //             KeyEvent{.key = key, .scanCode = scancode, .action = action, .modifiers =
    //             mods});
    //     });
}
//
// namespace detail {
// MouseButton getMouseButtonState(GLFWwindow *window)
// {
//     const Cory::MouseButton mouseButton =
//         (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) ?
//         Cory::MouseButton::Left : (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) ==
//         GLFW_PRESS)
//             ? Cory::MouseButton::Middle
//         : (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
//             ? Cory::MouseButton::Right
//             : Cory::MouseButton::None;
//     return mouseButton;
// }
//
// ModifierFlags getModifierState(GLFWwindow *window)
// {
//     Cory::ModifierFlags modifiers;
//     if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
//         modifiers.set(Cory::ModifierFlagBits::Alt);
//     }
//     if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
//         modifiers.set(Cory::ModifierFlagBits::Ctrl);
//     }
//     if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
//         modifiers.set(Cory::ModifierFlagBits::Shift);
//     }
//     return modifiers;
// }
// } // namespace detail

} // namespace Cory