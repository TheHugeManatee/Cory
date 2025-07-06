#include <Cory/Application/Window.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/SingleShotCommandRecorder.hpp>

#include <KDGpu/instance.h>
#include <KDGpu/swapchain_options.h>
#include <KDGpu/texture_options.h>
#include <KDGpuKDGui/view.h>
#include <KDGui/gui_application.h>

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/transform.hpp>

#include <thread>

namespace Cory {

Window::Window(Context &context,
               glm::i32vec2 dimensions,
               std::string windowName,
               int32_t sampleCount)
    : ctx_{context}
    , sampleCount_{sampleCount}
    , dimensions_(dimensions)
    , windowName_{std::move(windowName)}
    , fpsCounter_{std::chrono::milliseconds{2000}}
{
    CO_CORE_ASSERT(!ctx_.isHeadless(), "Cannot initialize window with a headless context!");

    createWindow();

    surface_ = ctx_.createSurface(windowName_, *window_);
    swapchain_ = createSwapchain();

    createColorAndDepthResources();

    samples.valueChanged()
        .connect([this]() {
            CO_CORE_INFO("Samples changed to {}", samples());
            createColorAndDepthResources();
        })
        .release();
}

Window::~Window() { CO_CORE_TRACE("Destroying Cory::Window {}", windowName_); }

bool Window::shouldClose() const { return !window_->visible(); }

KDGpu::Swapchain Window::createSwapchain()
{
    auto &device = ctx_.device();

    using namespace KDGpu;
    const AdapterSwapchainProperties swapchainProperties =
        device.adapter()->swapchainProperties(surface_);
    const SurfaceCapabilities &surfaceCapabilities = swapchainProperties.capabilities;

    swapchainSetup_.extent = {
        .width = std::clamp(window_->width(),
                            surfaceCapabilities.minImageExtent.width,
                            surfaceCapabilities.maxImageExtent.width),
        .height = std::clamp(window_->height(),
                             surfaceCapabilities.minImageExtent.height,
                             surfaceCapabilities.maxImageExtent.height),
    };

    // Create a swapchain of images that we will render to.
    const SwapchainOptions swapchainOptions = {
        .surface = surface_,
        .format = swapchainSetup_.format,
        .minImageCount = getSuitableImageCount(surfaceCapabilities),
        .imageExtent = swapchainSetup_.extent,
        .imageUsageFlags = swapchainSetup_.usageFlags,
        .compositeAlpha = swapchainSetup_.compositeAlpha,
        .presentMode = swapchainSetup_.presentMode,
        .oldSwapchain = swapchain_,
    };

    auto swapchain = device.createSwapchain(swapchainOptions);

    const auto &swapchainTextures = swapchain_.textures();
    const auto swapchainTextureCount = swapchainTextures.size();

    swapchainViews_.clear();
    swapchainViews_.reserve(swapchainTextureCount);
    for (uint32_t i = 0; i < swapchainTextureCount; ++i) {
        auto view = swapchainTextures[i].createView({.format = swapchainOptions.format});
        swapchainViews_.push_back(std::move(view));
    }

    createColorAndDepthResources();

    return swapchain;
}

void Window::determineSwapchainOptions()
{
    auto &device = ctx_.device();
    auto &adapter = *device.adapter();

    using namespace KDGpu;
    const AdapterSwapchainProperties swapchainProperties =
        device.adapter()->swapchainProperties(surface_);

    // Choose a presentation mode from the ones supported
    constexpr std::array<PresentMode, 4> preferredPresentModes = {
        PresentMode::Mailbox, PresentMode::FifoRelaxed, PresentMode::Fifo, PresentMode::Immediate};
    const auto &availableModes = swapchainProperties.presentModes;
    for (const auto &presentMode : preferredPresentModes) {
        const auto it = std::find(availableModes.begin(), availableModes.end(), presentMode);
        if (it != availableModes.end()) {
            swapchainSetup_.presentMode = presentMode;
            break;
        }
    }

    // Try to ensure that the chosen format is supported
    swapchainSetup_.format = [this, &swapchainProperties]() {
        for (const auto &availableFormat : swapchainProperties.formats) {
            if (availableFormat.format == swapchainSetup_.format &&
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
            swapchainSetup_.depthFormat = depthFormat;
            break;
        }
    }

    // Try to ensure that the chosen alpha composite mode is supported
    swapchainSetup_.compositeAlpha = [this, &swapchainProperties]() {
        const auto supportedCompositeAlpha =
            swapchainProperties.capabilities.supportedCompositeAlpha;

        if (supportedCompositeAlpha.testFlag(swapchainSetup_.compositeAlpha))
            return swapchainSetup_.compositeAlpha;

        // Try to return a single, known, supported alpha bit
        constexpr std::array compositeAlphaBits = {CompositeAlphaFlagBits::OpaqueBit,
                                                   CompositeAlphaFlagBits::PreMultipliedBit,
                                                   CompositeAlphaFlagBits::PostMultipliedBit,
                                                   CompositeAlphaFlagBits::InheritBit};
        for (const auto alphaBit : compositeAlphaBits) {
            if (supportedCompositeAlpha.testFlag(alphaBit)) return alphaBit;
        }

        // If all else fails, do not change
        return swapchainSetup_.compositeAlpha;
    }();

    swapchainSetup_.capabilitiesString =
        surfaceCapabilitiesToString(device.adapter()->swapchainProperties(surface_).capabilities);

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
                swapchainSetup_.supportedSampleCounts.push_back(sample);
        }
    }

    // since FRAMES_IN_FLIGHT does not change, the present and complete semaphores only need to be
    // created once here. Create the present complete and render complete semaphores
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        presentCompleteSemaphores_[i] = device.createGpuSemaphore();
        renderCompleteSemaphores_[i] = device.createGpuSemaphore();
        frameCompletedFences_[i] = device.createFence({
            .label = fmt::format("FENCE-{}-{}", windowName_, i),
            .createSignalled = false,
        });
    }
}

FrameContext Window::nextSwapchainImage()
{
    const Cory::ScopeTimer s{"Window/NextSwapchainImage"};
    static uint64_t frameCounter = 0;

    auto nextFrameIndex = static_cast<uint32_t>((frameCounter + 1) % FRAMES_IN_FLIGHT);
    auto recorder = ctx_.device().createCommandRecorder(KDGpu::CommandRecorderOptions{
        .queue = ctx_.graphicsQueue().handle(), .level = KDGpu::CommandBufferLevel::Primary});

    uint32_t swapchainImageIndex{};
    const KDGpu::AcquireImageResult result = swapchain_.getNextImageIndex(
        swapchainImageIndex, presentCompleteSemaphores_[nextFrameIndex]);

    if (result != KDGpu::PresentResult::Success && result != KDGpu::PresentResult::OutOfDate) {
        CO_CORE_ERROR("Failed to acquire next swapchain image: {}", result);
        throw std::runtime_error(fmt::format("Resizing logic not implemented yet", result));
    }

    return FrameContext{
        .index = nextFrameIndex,
        .swapchainImageIndex = swapchainImageIndex,
        .frameNumber = ++frameCounter,
        .shouldRecreateSwapchain = false,
        .swapchainImage = &swapchain_.textures()[swapchainImageIndex],
        .swapchainImageView = &swapchainViews_[swapchainImageIndex],
        .colorImage = &colorImages_[nextFrameIndex],
        .colorImageView = &colorImageViews_[nextFrameIndex],
        .depthImage = &depthImages_[nextFrameIndex],
        .depthImageView = &depthImageViews_[nextFrameIndex],
        .inFlight = &frameCompletedFences_[nextFrameIndex],
        .acquired = &presentCompleteSemaphores_[nextFrameIndex],
        .rendered = &renderCompleteSemaphores_[nextFrameIndex],
        .commandBuffer = std::move(recorder),
    };

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
    //         dimensions_ = {capabilities.currentExtent.width, capabilities.currentExtent.height};
    //         std::this_thread::yield();
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
    {
        const Cory::ScopeTimer s{"Window/Submit"};

        auto command_buffer = frameCtx.commandBuffer.finish();

        KDGpu::SubmitOptions submitOptions{
            .commandBuffers = {command_buffer},
            .waitSemaphores = {*frameCtx.acquired},
            .signalSemaphores = {*frameCtx.rendered},
            .signalFence = {},
        };
        ctx_.graphicsQueue().submit(submitOptions);
    }
    {
        const Cory::ScopeTimer s{"Window/Present"};

        KDGpu::PresentOptions presentOptions = {.waitSemaphores = {*frameCtx.rendered},
                                                .swapchainInfos = {{
                                                    .swapchain = swapchain_,
                                                    .imageIndex = frameCtx.swapchainImageIndex,
                                                }}};

        ctx_.graphicsQueue().present(presentOptions);
    }

    if (fpsCounter_.lap()) {
        auto s = fpsCounter_.stats();
        auto fps = fmt::format("{} FPS: {:3.2f} ({:3.2f} ms)",
                               windowName_,
                               float(1'000'000'000) / float(s.avg),
                               float(s.avg) / 1'000'000);
        CO_CORE_INFO(fps);
        window_->title = fps;
    }
}

void Window::createColorAndDepthResources()
{
    auto &device = ctx_.device();

    const glm::u32vec2 extent = {swapchainSetup_.extent.width, swapchainSetup_.extent.height};

    // COLOR images (multisampled)
    colorImages_ =
        ranges::views::indices(swapchain().textures().size()) |
        ranges::views::transform([this, &device, extent](auto idx) {
            // Create a depth texture to use for depth-correct rendering

            return device.createTexture(KDGpu::TextureOptions{
                .label = fmt::format("TEX_WndColor[{}] {} (IMG)", idx, extent),
                .type = KDGpu::TextureType::TextureType2D,
                .format = swapchainSetup_.format,
                .extent = {swapchainSetup_.extent.width, swapchainSetup_.extent.height, 1},
                .mipLevels = 1,
                .samples = samples(),
                .usage = KDGpu::TextureUsageFlagBits::ColorAttachmentBit |
                         KDGpu::TextureUsageFlagBits::TransferSrcBit |
                         KDGpu::TextureUsageFlagBits::SampledBit,
                .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
            });
        }) |
        ranges::to<std::vector>;

    colorImageViews_ = ranges::views::enumerate(colorImages_) |
                       ranges::views::transform([extent](auto it) {
                           auto [idx, depthImage] = it;

                           return depthImage.createView(KDGpu::TextureViewOptions{
                               .label = fmt::format("TEX_WndCol[{}] {} (VIEW)", idx, extent),
                           });
                       }) |
                       ranges::to<std::vector>;

    // DEPTH images
    depthImages_ =
        ranges::views::indices(swapchain().textures().size()) |
        ranges::views::transform([this, &device, extent](auto idx) {
            // Create a depth texture to use for depth-correct rendering
            return device.createTexture(KDGpu::TextureOptions{
                .label = fmt::format("TEX_WndDepth[{}] {} (IMG)", idx, extent),
                .type = KDGpu::TextureType::TextureType2D,
                .format = swapchainSetup_.depthFormat,
                .extent = {swapchainSetup_.extent.width, swapchainSetup_.extent.height, 1},
                .mipLevels = 1,
                .samples = samples(),
                .usage = KDGpu::TextureUsageFlagBits::DepthStencilAttachmentBit |
                         swapchainSetup_.depthImageUsage_,
                .memoryUsage = KDGpu::MemoryUsage::GpuOnly,
            });
        }) |
        ranges::to<std::vector>;

    depthImageViews_ = ranges::views::enumerate(depthImages_) |
                       ranges::views::transform([extent](auto it) {
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
    //                                               .pImageMemoryBarriers = &imageMemoryBarrier};
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
    //                                               .pImageMemoryBarriers = &imageMemoryBarrier};
    //         ctx_.device()->CmdPipelineBarrier2(setInitialLayoutCmds, &dependencyInfo);
    //     }
    // }
}

void Window::createWindow()
{
    window_ = std::make_unique<KDGpuKDGui::View>();
    window_->title = windowName_;

    // auto windowHandle =
    //     glfwCreateWindow(dimensions_.x, dimensions_.y, windowName_.c_str(), nullptr, nullptr);
    // window_ = std::shared_ptr<GLFWwindow>(windowHandle, [=](auto *ptr) {
    //     CO_CORE_TRACE("Destroying GLFW context");
    //     glfwDestroyWindow(ptr);
    //     glfwTerminate();
    // });
    // glfwSetWindowUserPointer(window_.get(), this);
    //
    // glfwSetCursorPosCallback(window_.get(), [](GLFWwindow *window, double mouseX, double mouseY)
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
    // glfwSetScrollCallback(window_.get(), [](GLFWwindow *window, double xOffset, double yOffset) {
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
    //             KeyEvent{.key = key, .scanCode = scancode, .action = action, .modifiers = mods});
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