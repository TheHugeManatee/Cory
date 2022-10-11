#include <Cory/Renderer/Swapchain.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/RenderCore/APIConversion.hpp>
#include <Cory/RenderCore/Context.hpp>
#include <Cory/RenderCore/VulkanUtils.hpp>

#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/CommandPool.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/DeviceProperties.h>
#include <Magnum/Vk/ImageCreateInfo.h>
#include <Magnum/Vk/ImageViewCreateInfo.h>
#include <Magnum/Vk/Instance.h>
#include <Magnum/Vk/Queue.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

namespace Vk = Magnum::Vk;

namespace Cory {

SwapchainSupportDetails SwapchainSupportDetails::query(Context &ctx, VkSurfaceKHR surface)
{
    SwapchainSupportDetails details;
    Vk::Instance &instance = ctx.instance();
    Vk::DeviceProperties &physicalDevice = ctx.physicalDevice();

    instance->GetPhysicalDeviceSurfaceCapabilitiesKHR(
        physicalDevice, surface, &details.capabilities);

    uint32_t formatCount;
    instance->GetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        instance->GetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    instance->GetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        instance->GetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice, surface, &presentModeCount, details.presentModes.data());
    }

    // enumerate all queue families that have present support
    VkBool32 presentSupport = false;
    auto queueFamilyProperties = physicalDevice.queueFamilyProperties();
    for (uint32_t i = 0; i < physicalDevice.queueFamilyCount(); ++i) {
        instance->GetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if (queueFamilyProperties[i].queueFamilyProperties.queueCount > 0 && presentSupport) {
            details.presentFamilies.push_back(i);
        }
    }

    return details;
}

VkSurfaceFormatKHR SwapchainSupportDetails::chooseSwapSurfaceFormat() const
{
    for (const auto &availableFormat : formats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return formats[0];
}

VkPresentModeKHR SwapchainSupportDetails::chooseSwapPresentMode() const
{
    for (const auto &availablePresentMode : presentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) { return availablePresentMode; }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapchainSupportDetails::chooseSwapExtent(VkExtent2D windowExtent) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    else {
        VkExtent2D actualExtent = windowExtent;
        actualExtent.width =
            std::max(capabilities.minImageExtent.width,
                     std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height =
            std::max(capabilities.minImageExtent.height,
                     std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

uint32_t SwapchainSupportDetails::chooseImageCount() const
{
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    return imageCount;
}

Swapchain::Swapchain(Context &ctx,
                     VkSurfaceKHR surface,
                     VkSwapchainCreateInfoKHR createInfo,
                     int32_t sampleCount)
    : maxFramesInFlight_{createInfo.minImageCount}
    , ctx_{&ctx}
    , imageFormat_{toMagnum(createInfo.imageFormat)}
    , sampleCount_{sampleCount}
    , extent_{createInfo.imageExtent.width, createInfo.imageExtent.height}
{
    VkSwapchainKHR vkSwapchain;

    THROW_ON_ERROR(
        ctx_->device()->CreateSwapchainKHR(ctx_->device(), &createInfo, nullptr, &vkSwapchain),
        "Could not initialize Swapchain!");

    wrap(vkSwapchain, [ctx = ctx_](VkSwapchainKHR s) {
        ctx->device()->DestroySwapchainKHR(ctx->device(), s, nullptr);
    });
    nameRawVulkanObject(
        ctx_->device(), vkSwapchain, fmt::format("Main Swapchain {}x{}", extent_.x, extent_.y));

    createImageViews();
    createSyncObjects();

    // initialize command buffers
    commandBuffers_ = images_ | ranges::views::transform([&](Vk::Image &img) mutable {
                          return Vk::CommandBuffer{Corrade::NoCreate};
                      }) |
                      ranges::to<std::vector<Vk::CommandBuffer>>;

    CO_CORE_DEBUG("Swapchain configuration:");
    CO_CORE_DEBUG("    Surface Format:    {}, {}", imageFormat_, createInfo.imageColorSpace);
    CO_CORE_DEBUG("    Present Mode:      {}", createInfo.presentMode);
    CO_CORE_DEBUG("    Extent:            {}x{}", extent_.x, extent_.y);
    CO_CORE_DEBUG("    Images:            {}", images_.size());
}

Swapchain::~Swapchain() { CO_CORE_TRACE("Destroying Cory::Swapchain."); }

FrameContext Swapchain::nextImage()
{
    uint32_t nextFrameIndex = static_cast<uint32_t>((nextFrameNumber_ + 1) % maxFramesInFlight_);

    FrameContext fc{.index = nextFrameIndex};

    // TODO evaluate if this design is suboptimal - we essentially block here until
    //      we can acquire the next image, however the client could *potentially* already
    //      record commands into the command buffer without having that image (except for
    //      the final render pass that renders to the texture?)
    VkResult result = ctx_->device()->AcquireNextImageKHR(
        ctx_->device(), *this, UINT64_MAX, imageAcquired_[nextFrameIndex], nullptr, &fc.index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        fc.shouldRecreateSwapchain = true;
        return fc;
    }
    CO_CORE_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR,
                   "failed to acquire swap chain image: {}",
                   result);
    
    // advance the image index
    ++nextFrameNumber_;
    fc.frameNumber = nextFrameNumber_;
    fc.shouldRecreateSwapchain = false;

    // wait for the fence of the previous frame operating on that image
    if (imageFences_[fc.index] != nullptr) { imageFences_[fc.index]->wait(); }

    // assign the image a new fence and reset it
    imageFences_[fc.index] = &inFlightFences_[nextFrameIndex];
    fc.inFlight = &inFlightFences_[nextFrameIndex];
    fc.inFlight->reset();

    // assign the semaphores to the struct
    fc.inFlight = &inFlightFences_[nextFrameIndex];
    fc.acquired = &imageAcquired_[nextFrameIndex];
    fc.rendered = &imageRendered_[nextFrameIndex];

    // get the swapchain image view
    fc.swapchainImage = &imageViews_[nextFrameIndex];

    // create a command buffer
    // TODO evaluate if it is more optimal to reuse command buffers?!
    commandBuffers_[nextFrameIndex] = ctx_->commandPool().allocate();
    fc.commandBuffer = &commandBuffers_[nextFrameIndex];
    nameVulkanObject(ctx_->device(), *fc.commandBuffer, fmt::format("Frame #{}", fc.frameNumber));

    return fc;
}

void Swapchain::present(FrameContext &fc)
{
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    VkSemaphore waitSemaphores[1] = {*fc.rendered};
    presentInfo.pWaitSemaphores = waitSemaphores;

    VkSwapchainKHR swapchains[] = {*this};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;

    presentInfo.pImageIndices = &fc.index;

    ctx_->device()->QueuePresentKHR(ctx_->graphicsQueue(), &presentInfo);
}

void Swapchain::createImageViews()
{
    uint32_t num_images;
    auto &device = ctx_->device();
    device->GetSwapchainImagesKHR(device, *this, &num_images, nullptr);
    std::vector<VkImage> swapchainImages(num_images);
    device->GetSwapchainImagesKHR(device, *this, &num_images, swapchainImages.data());

    // create a Vk::Image for each of the Swapchain images - when wrapped like this,
    // Vk::Image will not attempt to destroy the image on destruction
    images_ = swapchainImages | ranges::views::transform([&](VkImage img) mutable {
                  return Vk::Image::wrap(device, img, imageFormat_);
              }) |
              ranges::to<std::vector<Vk::Image>>;

    // create a Vk::ImageView for each of the Swapchain images
    imageViews_ = images_ | ranges::views::transform([&](Vk::Image &img) mutable {
                      return Vk::ImageView{device, Vk::ImageViewCreateInfo2D{img}};
                  }) |
                  ranges::to<std::vector<Vk::ImageView>>;

    // name all objects
    for (uint32_t i = 0; i < num_images; ++i) {
        nameVulkanObject(
            device, images_[i], fmt::format("Swapchain {}x{} Image {}", extent_.x, extent_.y, i));
        nameVulkanObject(device,
                         imageViews_[i],
                         fmt::format("Swapchain {}x{} ImageView {}", extent_.x, extent_.y, i));
    }
}

void Swapchain::createSyncObjects()
{
    // create all the semaphores needed to manage each parallel frame in flight
    for (uint32_t i = 0; i < maxFramesInFlight_; ++i) {
        imageAcquired_.emplace_back(
            ctx_->createSemaphore(fmt::format("Swapchain Img {} Acquired", i)));
        imageRendered_.emplace_back(
            ctx_->createSemaphore(fmt::format("Swapchain Img {} Rendered", i)));
        inFlightFences_.emplace_back(ctx_->createFence(fmt::format("Swapchain Img {} in flight", i),
                                                       FenceCreateMode::Signaled));
    }

    // initialize an array of (empty) fences, one for each image in the swap chain
    imageFences_.resize(imageViews_.size(), nullptr);
}

} // namespace Cory