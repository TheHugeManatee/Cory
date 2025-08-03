#include <Cory/Application/Window.hpp>

#include <Cory/Application/GLFWUtils.hpp>
#include <Cory/Base/Callback.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Primitives.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/SingleShotCommandRecorder.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <KDGpu/instance.h>
#include <KDGpu/surface.h>
#include <KDGpu/texture_options.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>

// clang-format off
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
// clang-format on

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/transform.hpp>

#include <optional>
#include <thread>

namespace Cory {

struct WindowPrivate {
    Context *ctx;
    std::shared_ptr<GLFWwindow> window;
    std::shared_ptr<VkSurfaceKHR_T> surfaceHandle; // The surface handle has to be stored and
                                                   // destroyed separately from the KDGpu::Surface
    KDGpu::Surface surface{};
    std::unique_ptr<Swapchain> swapchain;

    LapTimer fpsCounter{std::chrono::milliseconds{2000}};

    void recreateSwapchain();
};

Window::Window(Context &context,
               glm::i32vec2 dimensions,
               std::string windowName,
               int32_t sampleCount)
    : data_{std::make_unique<WindowPrivate>()}
{
    data_->ctx = &context;
    this->title = std::move(windowName);

    samples = static_cast<KDGpu::SampleCountFlagBits>(sampleCount);

    glfwInit();

    // prevent OpenGL usage - vulkan all the way baybeee
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    createWindow();

    VkSurfaceKHR surfaceHandle;
    auto instance_handle = context.instance().handle();
    auto *instance = context.resources().getInstance(instance_handle);

    if (auto ret = glfwCreateWindowSurface(
            instance->instance, data_->window.get(), nullptr, &surfaceHandle);
        ret != VK_SUCCESS) {
        CO_CORE_ERROR(
            "Failed to create Vulkan window surface for window '{}', error code: {}", title(), ret);
        throw std::runtime_error(fmt::format(
            "glfwCreateWindowSurface failed for window '{}', error code: {}", title(), ret));
    }
    data_->surfaceHandle = std::shared_ptr<VkSurfaceKHR_T>{
        surfaceHandle, [instance = instance->instance](VkSurfaceKHR_T *surfaceHandle) {
            CO_CORE_TRACE("Destroying GLFW surface");
            if (surfaceHandle != nullptr) { vkDestroySurfaceKHR(instance, surfaceHandle, nullptr); }
        }};

    this->dimensions = dimensions;

    data_->surface =
        context.graphicsApi().createSurfaceFromExistingVkSurface(instance_handle, surfaceHandle);
    context.setupDevice(data_->surface);

    data_->swapchain = std::make_unique<Swapchain>(context,
                                                   data_->surface,
                                                   SwapchainCreateInfo{
                                                       .label = title(),
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

    title.valueChanged().connect([this](const std::string_view newTitle) { updateTitle(); });
}

Window::~Window() { CO_CORE_TRACE("Destroying Cory::Window {}", title()); }

bool Window::shouldClose() const { return glfwWindowShouldClose(data_->window.get()); }

Swapchain &Window::swapchain() { return *data_->swapchain; }

FrameContext Window::nextSwapchainImage()
{
    const Cory::ScopeTimer s{"Window/NextSwapchainImage"};

    auto nextImageResult = data_->swapchain->nextImage();
    const auto dims = dimensions();
    if (!nextImageResult.has_value() || (dims.x == 0 || dims.y == 0)) {
        auto error = nextImageResult.error();
        if (error == SwapchainError::Unknown) {
            throw std::runtime_error(fmt::format(
                "Failed to acquire next swapchain image for window '{}': {}", title(), error));
        }

        // wait until the surface dimensions are non-zero - this might happen
        // while the app is minimized or the window has been resized to zero height
        // or width, in which case we don't render anything
        // do {
        //     glfwPollEvents();
        //     // VkSurfaceCapabilitiesKHR capabilities{};
        //     // data_->ctx->instance()->GetPhysicalDeviceSurfaceCapabilitiesKHR(
        //     //     data_->ctx->physicalDevice(), surface_, &capabilities);
        //     // size = {capabilities.currentExtent.width, capabilities.currentExtent.height};
        //     std::this_thread::yield();
        // } while (dimensions().x == 0 || dimensions().y == 0);

        // Hard sync to make sure no commands are in flight before recreating the swapchain
        data_->ctx->device().waitUntilIdle();
        // recreate the necessary resized resources and notify client code via
        // the onSwaphcainResized callback
        data_->swapchain.reset();
        CO_CORE_INFO("Recreating swapchain for window {} with size {}", title(), dimensions());
        data_->swapchain = std::make_unique<Swapchain>(*data_->ctx,
                                                       data_->surface,
                                                       SwapchainCreateInfo{
                                                           .label = title(),
                                                           .size = dimensions(),
                                                           .samples = samples(),
                                                       });
        onSwapchainResized.emit(SwapchainResizedEvent{.size{dimensions()}});

        // retry the whole thing
        return nextSwapchainImage();
    }

    return std::move(nextImageResult).value();
}

void Window::submitAndPresent(FrameContext &frameCtx)
{

    data_->swapchain->present(frameCtx);

    if (data_->fpsCounter.lap()) { updateTitle(); }
}
KDGpu::Format Window::colorFormat() const noexcept { return data_->swapchain->colorFormat(); }
KDGpu::Format Window::depthFormat() const noexcept { return data_->swapchain->depthFormat(); }

gsl::not_null<GLFWwindow *> Window::getGlfwWindow() const { return data_->window.get(); }

void Window::createWindow()
{

    auto dims = dimensions();
    auto windowHandle = glfwCreateWindow(dims.x, dims.y, title().c_str(), nullptr, nullptr);

    std::shared_ptr<GLFWwindow> window(windowHandle, [=](auto *ptr) {
        CO_CORE_TRACE("Destroying GLFW context");
        glfwDestroyWindow(ptr);
        glfwTerminate();
    });
    glfwSetWindowUserPointer(window.get(), this);
    glfwSetCursorPosCallback(window.get(), [](GLFWwindow *window, double mouseX, double mouseY) {
        Window &self = *reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        self.onMouseMoved.emit({.position = {mouseX, mouseY},
                                .button = GLFWUtils::getMouseButtonState(window),
                                .modifiers = GLFWUtils::getModifierState(window)});
    });
    glfwSetMouseButtonCallback(
        window.get(), [](GLFWwindow *window, int button, int action, int mods) {
            Window &self = *reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            self.onMouseButton.emit(MouseButtonEvent{
                .position = glm::vec2{mouseX, mouseY},
                .button = GLFWUtils::getMouseButtonState(window),
                .action = action == GLFW_PRESS ? ButtonAction::Press : ButtonAction::Release,
                .modifiers = GLFWUtils::getModifierState(window)});
        });
    glfwSetScrollCallback(window.get(), [](GLFWwindow *window, double xOffset, double yOffset) {
        Window &self = *reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        self.onMouseScrolled.emit({.position = {mouseX, mouseY},
                                   .scrollDelta = {xOffset, yOffset},
                                   .modifiers = GLFWUtils::getModifierState(window)});
    });
    glfwSetKeyCallback(
        window.get(), [](GLFWwindow *window, int key, int scancode, int action, int mods) {
            Window &self = *reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
            self.onKeyCallback.emit(
                KeyEvent{.key = key, .scanCode = scancode, .action = action, .modifiers = mods});
        });

    data_->window = std::move(window);
}

void Window::updateTitle()
{
    auto s = data_->fpsCounter.stats();
    auto fpsTitle = fmt::format("{} {} FPS: {:3.2f} ({:3.2f} ms)",
                                title(),
                                dimensions(),
                                float(1'000'000'000) / float(s.avg),
                                float(s.avg) / 1'000'000);
    CO_CORE_INFO(fpsTitle);

    glfwSetWindowTitle(data_->window.get(), fpsTitle.data());
}

} // namespace Cory