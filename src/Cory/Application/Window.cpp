#include <Cory/Application/Window.hpp>

#include <Cory/Base/Callback.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Profiling.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/SingleShotCommandRecorder.hpp>

#include <KDGpu/fence.h>
#include <KDGpu/gpu_semaphore.h>
#include <KDGpu/instance.h>
#include <KDGpu/surface.h>
#include <KDGpu/swapchain.h>
#include <KDGpu/swapchain_options.h>
#include <KDGpu/texture_options.h>
#include <KDGpuKDGui/view.h>
#include <KDGpuUtils/resource_deleter.h>
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
    } swapchainSetup;
    KDGpu::Swapchain swapchain;
    std::vector<KDGpu::TextureView> swapchainViews;

    std::vector<KDGpu::Texture> colorImages;
    std::vector<KDGpu::TextureView> colorImageViews;
    std::vector<KDGpu::Texture> depthImages;
    std::vector<KDGpu::TextureView> depthImageViews;
    std::vector<KDGpu::GpuSemaphore> presentCompleteSemaphores;
    std::vector<KDGpu::GpuSemaphore> renderCompleteSemaphores;

    uint32_t currentSwapchainImageIndex_{0};
    uint32_t inFlightIndex_{0};
    std::array<KDGpu::Fence, Window::FRAMES_IN_FLIGHT> frameCompletedFences;

    LapTimer fpsCounter{std::chrono::milliseconds{2000}};
    std::unique_ptr<KDGpuUtils::ResourceDeleter> resourceDeleter;
    // Command buffers for each frame in flight - stored here so we can keep them alive
    // until the commands have executed
    std::array<std::optional<KDGpu::CommandBuffer>, Window::FRAMES_IN_FLIGHT> commandBuffers;
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
    determineSwapchainOptions();
    data_->swapchain = createSwapchain();

    createColorAndDepthResources();

    samples.valueChanged()
        .connect([this]() {
            CO_CORE_INFO("Samples changed to {}", samples());
            createColorAndDepthResources();
        })
        .release();

    data_->resourceDeleter =
        std::make_unique<KDGpuUtils::ResourceDeleter>(&context.device(), FRAMES_IN_FLIGHT);
}

Window::~Window() { CO_CORE_TRACE("Destroying Cory::Window {}", data_->windowName); }

bool Window::shouldClose() const { return !data_->window->visible(); }

glm::i32vec2 Window::dimensions() const
{
    return {data_->window->width(), data_->window->height()};
}

KDGpu::Swapchain &Window::swapchain() { return data_->swapchain; }

KDGpu::Swapchain Window::createSwapchain()
{
    auto &device = data_->ctx->device();

    using namespace KDGpu;
    const AdapterSwapchainProperties swapchainProperties =
        device.adapter()->swapchainProperties(data_->surface);
    const SurfaceCapabilities &surfaceCapabilities = swapchainProperties.capabilities;

    auto &swapchainSetup = data_->swapchainSetup;

    swapchainSetup.extent = {
        .width = std::clamp(data_->window->width(),
                            surfaceCapabilities.minImageExtent.width,
                            surfaceCapabilities.maxImageExtent.width),
        .height = std::clamp(data_->window->height(),
                             surfaceCapabilities.minImageExtent.height,
                             surfaceCapabilities.maxImageExtent.height),
    };

    // Create a swapchain of images that we will render to.
    const SwapchainOptions swapchainOptions = {
        .surface = data_->surface,
        .format = swapchainSetup.format,
        .minImageCount = getSuitableImageCount(surfaceCapabilities),
        .imageExtent = swapchainSetup.extent,
        .imageUsageFlags = swapchainSetup.usageFlags,
        .compositeAlpha = swapchainSetup.compositeAlpha,
        .presentMode = swapchainSetup.presentMode,
        .oldSwapchain = data_->swapchain,
    };

    auto swapchain = device.createSwapchain(swapchainOptions);

    const auto &swapchainTextures = swapchain.textures();
    const auto swapchainTextureCount = swapchainTextures.size();

    data_->swapchainViews.clear();
    data_->presentCompleteSemaphores.clear();
    data_->renderCompleteSemaphores.clear();
    data_->swapchainViews.reserve(swapchainTextureCount);
    data_->presentCompleteSemaphores.reserve(swapchainTextureCount);
    data_->renderCompleteSemaphores.reserve(swapchainTextureCount);
    for (uint32_t i = 0; i < swapchainTextureCount; ++i) {
        auto view = swapchainTextures[i].createView({.format = swapchainOptions.format});
        data_->swapchainViews.push_back(std::move(view));
        data_->presentCompleteSemaphores.push_back(device.createGpuSemaphore(
            {.label = fmt::format("PRESENT-COMPLETE-{}-{}", data_->windowName, i)}));
        data_->renderCompleteSemaphores.push_back(device.createGpuSemaphore(
            {.label = fmt::format("RENDER-COMPLETE-{}-{}", data_->windowName, i)}));
    }

    createColorAndDepthResources();

    return swapchain;
}

void Window::determineSwapchainOptions()
{
    auto &device = data_->ctx->device();
    auto &adapter = *device.adapter();
    auto &swapchainSetup = data_->swapchainSetup;

    using namespace KDGpu;
    const AdapterSwapchainProperties swapchainProperties =
        device.adapter()->swapchainProperties(data_->surface);

    // Choose a presentation mode from the ones supported
    constexpr std::array<PresentMode, 4> preferredPresentModes = {
        PresentMode::Mailbox, PresentMode::FifoRelaxed, PresentMode::Fifo, PresentMode::Immediate};
    const auto &availableModes = swapchainProperties.presentModes;
    for (const auto &presentMode : preferredPresentModes) {
        const auto it = std::find(availableModes.begin(), availableModes.end(), presentMode);
        if (it != availableModes.end()) {
            swapchainSetup.presentMode = presentMode;
            break;
        }
    }

    // Try to ensure that the chosen format is supported
    swapchainSetup.format = [this, &swapchainProperties, &swapchainSetup]() {
        for (const auto &availableFormat : swapchainProperties.formats) {
            if (availableFormat.format == swapchainSetup.format &&
                availableFormat.colorSpace == ColorSpace::SRgbNonlinear) {
                return availableFormat.format;
            }
        }
        // Fallback to first listed available format
        return swapchainProperties.formats[0].format;
    }();

    // Choose a depth format from the ones supported
    constexpr std::array preferredDepthFormat = {
        Format::D24_UNORM_S8_UINT,
        Format::D16_UNORM_S8_UINT,
        Format::D32_SFLOAT_S8_UINT,
        Format::D16_UNORM,
        Format::D32_SFLOAT,
    };
    for (const auto &depthFormat : preferredDepthFormat) {
        const FormatProperties formatProperties = adapter.formatProperties(depthFormat);
        if (formatProperties.optimalTilingFeatures &
            FormatFeatureFlagBit::DepthStencilAttachmentBit) {
            swapchainSetup.depthFormat = depthFormat;
            break;
        }
    }

    // Try to ensure that the chosen alpha composite mode is supported
    swapchainSetup.compositeAlpha = [this, &swapchainProperties, &swapchainSetup]() {
        const auto supportedCompositeAlpha =
            swapchainProperties.capabilities.supportedCompositeAlpha;

        if (supportedCompositeAlpha.testFlag(swapchainSetup.compositeAlpha))
            return swapchainSetup.compositeAlpha;

        // Try to return a single, known, supported alpha bit
        constexpr std::array compositeAlphaBits = {CompositeAlphaFlagBits::OpaqueBit,
                                                   CompositeAlphaFlagBits::PreMultipliedBit,
                                                   CompositeAlphaFlagBits::PostMultipliedBit,
                                                   CompositeAlphaFlagBits::InheritBit};
        for (const auto alphaBit : compositeAlphaBits) {
            if (supportedCompositeAlpha.testFlag(alphaBit)) return alphaBit;
        }

        // If all else fails, do not change
        return swapchainSetup.compositeAlpha;
    }();

    swapchainSetup.capabilitiesString = surfaceCapabilitiesToString(
        device.adapter()->swapchainProperties(data_->surface).capabilities);

    constexpr std::array availableSampleCounts{
        SampleCountFlagBits::Samples1Bit,
        SampleCountFlagBits::Samples2Bit,
        SampleCountFlagBits::Samples4Bit,
        SampleCountFlagBits::Samples8Bit,
        SampleCountFlagBits::Samples16Bit,
        SampleCountFlagBits::Samples32Bit,
        SampleCountFlagBits::Samples64Bit,
    };

    // get all of the supported sample counts for the hardware
    {
        auto supported = device.adapter()->properties().limits.framebufferColorSampleCounts.toInt();
        assert(supported);

        for (auto sample : availableSampleCounts) {
            if (static_cast<int>(sample) & supported)
                swapchainSetup.supportedSampleCounts.push_back(sample);
        }
    }

    // since FRAMES_IN_FLIGHT does not change, the present and complete semaphores only need to
    // be created once here. Create the present complete and render complete semaphores
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        // data_->presentCompleteSemaphores[i] = device.createGpuSemaphore();
        // data_->renderCompleteSemaphores[i] = device.createGpuSemaphore();
        data_->frameCompletedFences[i] = device.createFence({
            .label = fmt::format("FENCE-{}-{}", data_->windowName, i),
            // have to create signalled, so that we don't have to treat the
            // first frames as special cases, but simply always wait on the fence
            .createSignalled = true,
        });
    }
}

FrameContext Window::nextSwapchainImage()
{
    const Cory::ScopeTimer s{"Window/NextSwapchainImage"};
    static uint64_t frameCounter = 0;
    auto &ctx = *data_->ctx;
    auto nextFrameIndex = static_cast<uint32_t>(frameCounter % FRAMES_IN_FLIGHT);

    auto recorder = ctx.device().createCommandRecorder(KDGpu::CommandRecorderOptions{
        .queue = ctx.graphicsQueue().handle(),
        .level = KDGpu::CommandBufferLevel::Primary,
    });

    data_->frameCompletedFences[nextFrameIndex].wait();
    data_->frameCompletedFences[nextFrameIndex].reset();
    // Todo clear up resources here via resource deleter

    uint32_t swapchainImageIndex{};
    const KDGpu::AcquireImageResult result = data_->swapchain.getNextImageIndex(
        swapchainImageIndex, data_->presentCompleteSemaphores[nextFrameIndex]);

    if (result != KDGpu::PresentResult::Success && result != KDGpu::PresentResult::OutOfDate) {
        CO_CORE_ERROR("Failed to acquire next swapchain image: {}", result);
        throw std::runtime_error(fmt::format("Resizing logic not implemented yet", result));
    }

    FrameContext frameCtx{
        .index = nextFrameIndex,
        .swapchainImageIndex = swapchainImageIndex,
        .frameNumber = ++frameCounter,
        .shouldRecreateSwapchain = false,
        .swapchainImage = &data_->swapchain.textures()[swapchainImageIndex],
        .swapchainImageView = &data_->swapchainViews[swapchainImageIndex],
        .colorImage = &data_->colorImages[nextFrameIndex],
        .colorImageView = &data_->colorImageViews[nextFrameIndex],
        .depthImage = &data_->depthImages[nextFrameIndex],
        .depthImageView = &data_->depthImageViews[nextFrameIndex],
        .inFlight = &data_->frameCompletedFences[nextFrameIndex],
        .acquired = &data_->presentCompleteSemaphores[nextFrameIndex],
        .rendered = &data_->renderCompleteSemaphores[swapchainImageIndex],
        .commandBuffer = std::move(recorder),
        .resourceDeleter = data_->resourceDeleter.get(),
    };

    ++frameCounter;

    return frameCtx;

    // FrameContext frameCtx = swapchain_->nextImage();
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
    // }
}

void Window::submitAndPresent(FrameContext &frameCtx)
{
    auto &ctx = *data_->ctx;
    {
        const Cory::ScopeTimer s{"Window/Submit"};

        auto command_buffer = frameCtx.commandBuffer.finish();

        KDGpu::SubmitOptions submitOptions{
            .commandBuffers = {command_buffer},
            .waitSemaphores = {*frameCtx.acquired},
            .signalSemaphores = {*frameCtx.rendered},
            .signalFence = *frameCtx.inFlight,
        };
        ctx.graphicsQueue().submit(submitOptions);

        data_->commandBuffers[frameCtx.index] = std::move(command_buffer);
    }
    {
        const Cory::ScopeTimer s{"Window/Present"};

        KDGpu::PresentOptions presentOptions = {.waitSemaphores = {*frameCtx.rendered},
                                                .swapchainInfos = {{
                                                    .swapchain = data_->swapchain,
                                                    .imageIndex = frameCtx.swapchainImageIndex,
                                                }}};

        ctx.graphicsQueue().present(presentOptions);
    }

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
KDGpu::Format Window::colorFormat() const noexcept { return data_->swapchainSetup.format; }
KDGpu::Format Window::depthFormat() const noexcept { return data_->swapchainSetup.depthFormat; }

void Window::createColorAndDepthResources()
{
    auto &device = data_->ctx->device();
    const auto &swapchainSetup = data_->swapchainSetup;

    const glm::u32vec2 extent = {swapchainSetup.extent.width, swapchainSetup.extent.height};

    // COLOR images (multisampled)
    data_->colorImages =
        ranges::views::indices(swapchain().textures().size()) |
        ranges::views::transform([this, &device, &swapchainSetup, extent](auto idx) {
            // Create a depth texture to use for depth-correct rendering

            return device.createTexture(KDGpu::TextureOptions{
                .label = fmt::format("TEX_WndColor[{}] {} (IMG)", idx, extent),
                .type = KDGpu::TextureType::TextureType2D,
                .format = swapchainSetup.format,
                .extent = {swapchainSetup.extent.width, swapchainSetup.extent.height, 1},
                .mipLevels = 1,
                .samples = samples(),
                .usage = KDGpu::TextureUsageFlagBits::ColorAttachmentBit |
                         KDGpu::TextureUsageFlagBits::TransferSrcBit |
                         KDGpu::TextureUsageFlagBits::SampledBit,
                .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
            });
        }) |
        ranges::to<std::vector>;

    data_->colorImageViews = ranges::views::enumerate(data_->colorImages) |
                             ranges::views::transform([extent](auto it) {
                                 auto [idx, depthImage] = it;

                                 return depthImage.createView(KDGpu::TextureViewOptions{
                                     .label = fmt::format("TEX_WndCol[{}] {} (VIEW)", idx, extent),
                                 });
                             }) |
                             ranges::to<std::vector>;

    // DEPTH images
    data_->depthImages =
        ranges::views::indices(swapchain().textures().size()) |
        ranges::views::transform([this, &swapchainSetup, &device, extent](auto idx) {
            // Create a depth texture to use for depth-correct rendering
            return device.createTexture(KDGpu::TextureOptions{
                .label = fmt::format("TEX_WndDepth[{}] {} (IMG)", idx, extent),
                .type = KDGpu::TextureType::TextureType2D,
                .format = swapchainSetup.depthFormat,
                .extent = {swapchainSetup.extent.width, swapchainSetup.extent.height, 1},
                .mipLevels = 1,
                .samples = samples(),
                .usage = KDGpu::TextureUsageFlagBits::DepthStencilAttachmentBit |
                         swapchainSetup.depthImageUsage_,
                .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
            });
        }) |
        ranges::to<std::vector>;

    data_->depthImageViews =
        ranges::views::enumerate(data_->depthImages) | ranges::views::transform([extent](auto it) {
            auto [idx, depthImage] = it;

            return depthImage.createView(KDGpu::TextureViewOptions{
                .label = fmt::format("TEX_WndDepth[{}] {} (VIEW)", idx, extent),
            });
        }) |
        ranges::to<std::vector>;

    // transition the images to ATTACHMENT_OPTIMAL - is this actually needed?
    // {
    //     SingleShotCommandBuffer setInitialLayoutCmds{ctx_};
    //     { // color image
    //         const VkImageMemoryBarrier2 imageMemoryBarrier{
    //             .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    //             .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
    //             .srcAccessMask = VK_ACCESS_2_NONE,
    //             .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    //             .dstAccessMask = VK_ACCESS_2_NONE,
    //             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    //             .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    //             .srcQueueFamilyIndex = ctx_.graphicsQueueFamily(),
    //             .dstQueueFamilyIndex = ctx_.graphicsQueueFamily(),
    //             .image = colorImage_,
    //             .subresourceRange = {
    //                 .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    //                 .baseMipLevel = 0,
    //                 .levelCount = 1,
    //                 .baseArrayLayer = 0,
    //                 .layerCount = 1,
    //             }};
    //         const VkDependencyInfo dependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    //                                               .pNext = nullptr,
    //                                               .dependencyFlags = {}, // ?
    //                                               .memoryBarrierCount = 0,
    //                                               .pMemoryBarriers = nullptr,
    //                                               .bufferMemoryBarrierCount = 0,
    //                                               .pBufferMemoryBarriers = nullptr,
    //                                               .imageMemoryBarrierCount = 1,
    //                                               .pImageMemoryBarriers =
    //                                               &imageMemoryBarrier};
    //         ctx_.device()->CmdPipelineBarrier2(setInitialLayoutCmds, &dependencyInfo);
    //     }
    //     { // depth image
    //         const VkImageMemoryBarrier2 imageMemoryBarrier{
    //             .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    //             .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
    //             .srcAccessMask = VK_ACCESS_2_NONE,
    //             .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    //             .dstAccessMask = VK_ACCESS_2_NONE,
    //             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    //             .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    //             .srcQueueFamilyIndex = ctx_.graphicsQueueFamily(),
    //             .dstQueueFamilyIndex = ctx_.graphicsQueueFamily(),
    //             .image = depthImages_[0],
    //             .subresourceRange = {
    //                 .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
    //                 .baseMipLevel = 0,
    //                 .levelCount = 1,
    //                 .baseArrayLayer = 0,
    //                 .layerCount = 1,
    //             }};
    //         const VkDependencyInfo dependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    //                                               .pNext = nullptr,
    //                                               .dependencyFlags = {}, // ?
    //                                               .memoryBarrierCount = 0,
    //                                               .pMemoryBarriers = nullptr,
    //                                               .bufferMemoryBarrierCount = 0,
    //                                               .pBufferMemoryBarriers = nullptr,
    //                                               .imageMemoryBarrierCount = 1,
    //                                               .pImageMemoryBarriers =
    //                                               &imageMemoryBarrier};
    //         ctx_.device()->CmdPipelineBarrier2(setInitialLayoutCmds, &dependencyInfo);
    //     }
    // }
}

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