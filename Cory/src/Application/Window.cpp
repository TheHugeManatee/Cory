#include <Cory/Application/Window.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/APIConversion.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/SingleShotCommandBuffer.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/DeviceProperties.h>
#include <Magnum/Vk/ImageCreateInfo.h>
#include <Magnum/Vk/ImageViewCreateInfo.h>
#include <Magnum/Vk/Instance.h>
#include <Magnum/Vk/Queue.h>

// clang-format off
#include <Magnum/Vk/Vulkan.h>
#define VK_VERSION_1_0
#include <GLFW/glfw3.h>
// clang-format on

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/transform.hpp>

#include <thread>

namespace Vk = Magnum::Vk;

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

    glfwInit();

    // prevent OpenGL usage - vulkan all the way baybeee
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    createGlfwWindow();

    surface_ = createSurface();
    swapchain_ = createSwapchain();

    colorFormat_ = swapchain_->colorFormat();
    depthFormat_ = Vk::PixelFormat::Depth32F; // TODO implement dynamic selection instead
                                              //      of hardcoded format
    createColorAndDepthResources();
}

Window::~Window() { CO_CORE_TRACE("Destroying Cory::Window {}", windowName_); }

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(const_cast<GLFWwindow *>(handle()));
}

BasicVkObjectWrapper<VkSurfaceKHR> Window::createSurface()
{
    VkSurfaceKHR surfaceHandle;
    auto ret = glfwCreateWindowSurface(ctx_.instance(), handle(), nullptr, &surfaceHandle);

    BasicVkObjectWrapper<VkSurfaceKHR> surface{
        surfaceHandle, [&instance = ctx_.instance()](auto s) {
            instance->DestroySurfaceKHR(instance, s, nullptr);
        }};

    CO_CORE_ASSERT(ret == VK_SUCCESS, "Could not create surface!");
    nameVulkanObject(ctx_.device(), surface, fmt::format("SURF_{}", windowName_));

    return surface;
}

std::unique_ptr<Swapchain> Window::createSwapchain()
{
    SwapchainSupportDetails swapchainSupport =
        SwapchainSupportDetails::query(ctx_, surface_.handle());

    uint32_t imageCount = swapchainSupport.chooseImageCount();

    VkSwapchainCreateInfoKHR createInfo{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};

    auto surfaceFormat = swapchainSupport.chooseSwapSurfaceFormat();

    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapchainSupport.chooseSwapExtent(toVkExtent2D(dimensions_));
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (!ranges::contains(swapchainSupport.presentFamilies, ctx_.graphicsQueueFamily())) {
        // TODO need to fix this at some point, but currently we don't spend the effort to
        //      create a separate present queue if necessary - needs fixing in the Context
        //      constructor!
        // uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};
        // createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        // createInfo.queueFamilyIndexCount = 2;
        // createInfo.pQueueFamilyIndices = queueFamilyIndices;
        throw std::runtime_error(fmt::format("graphics queue does not support presenting"));
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;     // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = swapchainSupport.chooseSwapPresentMode();
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    return std::make_unique<Swapchain>(ctx_, surface_, createInfo, sampleCount_);
}

FrameContext Window::nextSwapchainImage()
{
    const Cory::ScopeTimer s{"Window/NextSwapchainImage"};
    FrameContext frameCtx = swapchain_->nextImage();

    // if the swapchain needs resizing, we wait for
    if (frameCtx.shouldRecreateSwapchain) {
        // wait until the surface dimensions are non-zero - this might happen
        // while the app is minimized or the window has been resized to zero height
        // or width, in which case we don't render anything
        do {
            glfwPollEvents();
            VkSurfaceCapabilitiesKHR capabilities{};
            ctx_.instance()->GetPhysicalDeviceSurfaceCapabilitiesKHR(
                ctx_.physicalDevice(), surface_, &capabilities);
            dimensions_ = {capabilities.currentExtent.width, capabilities.currentExtent.height};
            std::this_thread::yield();
        } while (dimensions_.x == 0 || dimensions_.y == 0);

        ctx_.device()->DeviceWaitIdle(ctx_.device());

        // recreate the necessary resized resources and notify client code via
        // the onSwaphcainResized callback
        swapchain_.reset();
        swapchain_ = createSwapchain();
        createColorAndDepthResources();
        onSwapchainResized.emit(dimensions_);

        // retry the whole thing
        return nextSwapchainImage();
    }

    frameCtx.colorImage = &colorImage_;
    frameCtx.colorImageView = &colorImageView_;
    frameCtx.depthImage = &depthImages_[frameCtx.index];
    frameCtx.depthImageView = &depthImageViews_[frameCtx.index];

    return frameCtx;
}
void Window::submitAndPresent(FrameContext &frameCtx)
{
    {
        const Cory::ScopeTimer s{"Window/Submit"};

        std::vector<VkSemaphore> waitSemaphores{*frameCtx.acquired};
        std::vector<VkPipelineStageFlags> waitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        std::vector<VkSemaphore> signalSemaphores{*frameCtx.rendered};

        Magnum::Vk::SubmitInfo submitInfo{};
        submitInfo.setCommandBuffers({*frameCtx.commandBuffer});
        submitInfo->pWaitSemaphores = waitSemaphores.data();
        submitInfo->waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
        submitInfo->pWaitDstStageMask = waitStages.data();
        submitInfo->pSignalSemaphores = signalSemaphores.data();
        submitInfo->signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());

        ctx_.graphicsQueue().submit({submitInfo}, *frameCtx.inFlight);
    }
    {
        const Cory::ScopeTimer s{"Window/Present"};
        swapchain_->present(frameCtx);
    }

    if (fpsCounter_.lap()) {
        auto s = fpsCounter_.stats();
        auto fps = fmt::format("{} FPS: {:3.2f} ({:3.2f} ms)",
                               windowName_,
                               float(1'000'000'000) / float(s.avg),
                               float(s.avg) / 1'000'000);
        CO_CORE_INFO(fps);
        glfwSetWindowTitle(handle(), fps.c_str());
    }
}

void Window::createColorAndDepthResources()
{
    const auto extent = swapchain_->extent();
    const Magnum::Vector2i size(extent.x, extent.y);
    const int levels = 1;

    // COLOR image - only one for now because we don't need more (so far)
    // ImageUsage::TransferSource is needed to be able to resolve from the image
    // ImageUsage::Sampled so it can be use it in debug views and so we can use in subsequent frames
    const auto colorImgUsage =
        Vk::ImageUsage::ColorAttachment | Vk::ImageUsage::TransferSource | Vk::ImageUsage::Sampled;
    colorImage_ =
        Vk::Image{ctx_.device(),
                  Vk::ImageCreateInfo2D{colorImgUsage, colorFormat_, size, levels, sampleCount_},
                  Vk::MemoryFlag::DeviceLocal};
    nameVulkanObject(ctx_.device(), colorImage_, fmt::format("TEX_WndCol {} (IMG)", extent));

    colorImageView_ = Vk::ImageView{ctx_.device(), Vk::ImageViewCreateInfo2D{colorImage_}};
    nameVulkanObject(ctx_.device(), colorImageView_, fmt::format("TEX_WndCol {} (VIEW)", extent));

    // DEPTH images
    depthImages_ =
        ranges::views::indices(swapchain().images().size()) |
        ranges::views::transform([&](auto idx) {
            // ImageUsage::Sampled so we can create debug views
            auto usage = Vk::ImageUsage::DepthStencilAttachment | Vk::ImageUsage::Sampled;
            Vk::Image img{ctx_.device(),
                          Vk::ImageCreateInfo2D{usage, depthFormat_, size, levels, sampleCount_},
                          Vk::MemoryFlag::DeviceLocal};

            nameVulkanObject(
                ctx_.device(), img, fmt::format("TEX_WndDepth[{}] {} (IMG)", idx, extent));
            return img;
        }) |
        ranges::to<std::vector<Vk::Image>>;

    depthImageViews_ =
        ranges::views::enumerate(depthImages_) | ranges::views::transform([&](auto it) {
            auto [idx, depthImage] = it;
            Vk::ImageView view{ctx_.device(), Vk::ImageViewCreateInfo2D{depthImage}};
            nameVulkanObject(
                ctx_.device(), view, fmt::format("TEX_WndDepth[{}] {} (VIEW)", idx, extent));
            return view;
        }) |
        ranges::to<std::vector<Vk::ImageView>>;

    // transition the images to ATTACHMENT_OPTIMAL
    {
        SingleShotCommandBuffer setInitialLayoutCmds{ctx_};
        { // color image
            const VkImageMemoryBarrier2 imageMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask = VK_ACCESS_2_NONE,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_NONE,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .srcQueueFamilyIndex = ctx_.graphicsQueueFamily(),
                .dstQueueFamilyIndex = ctx_.graphicsQueueFamily(),
                .image = colorImage_,
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                }};
            const VkDependencyInfo dependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                                  .pNext = nullptr,
                                                  .dependencyFlags = {}, // ?
                                                  .memoryBarrierCount = 0,
                                                  .pMemoryBarriers = nullptr,
                                                  .bufferMemoryBarrierCount = 0,
                                                  .pBufferMemoryBarriers = nullptr,
                                                  .imageMemoryBarrierCount = 1,
                                                  .pImageMemoryBarriers = &imageMemoryBarrier};
            ctx_.device()->CmdPipelineBarrier2(setInitialLayoutCmds, &dependencyInfo);
        }
        { // depth image
            const VkImageMemoryBarrier2 imageMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask = VK_ACCESS_2_NONE,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_NONE,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .srcQueueFamilyIndex = ctx_.graphicsQueueFamily(),
                .dstQueueFamilyIndex = ctx_.graphicsQueueFamily(),
                .image = depthImages_[0],
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                }};
            const VkDependencyInfo dependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                                  .pNext = nullptr,
                                                  .dependencyFlags = {}, // ?
                                                  .memoryBarrierCount = 0,
                                                  .pMemoryBarriers = nullptr,
                                                  .bufferMemoryBarrierCount = 0,
                                                  .pBufferMemoryBarriers = nullptr,
                                                  .imageMemoryBarrierCount = 1,
                                                  .pImageMemoryBarriers = &imageMemoryBarrier};
            ctx_.device()->CmdPipelineBarrier2(setInitialLayoutCmds, &dependencyInfo);
        }
    }
}

void Window::createGlfwWindow()
{
    auto windowHandle =
        glfwCreateWindow(dimensions_.x, dimensions_.y, windowName_.c_str(), nullptr, nullptr);
    window_ = std::shared_ptr<GLFWwindow>(windowHandle, [=](auto *ptr) {
        CO_CORE_TRACE("Destroying GLFW context");
        glfwDestroyWindow(ptr);
        glfwTerminate();
    });
    glfwSetWindowUserPointer(window_.get(), this);

    glfwSetCursorPosCallback(window_.get(), [](GLFWwindow *window, double mouseX, double mouseY) {
        Window &self = *reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        self.onMouseMoved.emit({mouseX, mouseY});
    });

    glfwSetMouseButtonCallback(
        window_.get(), [](GLFWwindow *window, int button, int action, int mods) {
            Window &self = *reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            self.onMouseButton.emit(MouseButtonData{.position = glm::vec2{mouseX, mouseY},
                                                    .button = button,
                                                    .action = action,
                                                    .modifiers = mods});
        });

    glfwSetScrollCallback(window_.get(), [](GLFWwindow *window, double xOffset, double yOffset) {
        Window &self = *reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        self.onMouseScrolled.emit({glm::vec2{mouseX, mouseY}, glm::vec2{xOffset, yOffset}});
    });

    glfwSetKeyCallback(
        window_.get(), [](GLFWwindow *window, int key, int scancode, int action, int mods) {
            Window &self = *reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
            self.onKeyCallback.emit(
                KeyData{.key = key, .scanCode = scancode, .action = action, .modifiers = mods});
        });
}

} // namespace Cory