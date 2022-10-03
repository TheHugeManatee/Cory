#include <Cory/UI/SwapChain.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Core/APIConversion.hpp>
#include <Cory/Core/Context.hpp>
#include <Cory/Core/VulkanUtils.hpp>

#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/DeviceProperties.h>
#include <Magnum/Vk/ImageCreateInfo.h>
#include <Magnum/Vk/ImageViewCreateInfo.h>
#include <Magnum/Vk/Instance.h>

namespace Vk = Magnum::Vk;

namespace Cory {

SwapChainSupportDetails SwapChainSupportDetails::query(Context &ctx, VkSurfaceKHR surface)
{
    SwapChainSupportDetails details;
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
    return details;
}

VkSurfaceFormatKHR SwapChainSupportDetails::chooseSwapSurfaceFormat() const
{
    for (const auto &availableFormat : formats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return formats[0];
}

VkPresentModeKHR SwapChainSupportDetails::chooseSwapPresentMode() const
{
    for (const auto &availablePresentMode : presentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            CO_CORE_DEBUG("Present mode: Mailbox");
            return availablePresentMode;
        }
    }

    CO_CORE_DEBUG("Present mode: V-Sync (FIFO)");
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChainSupportDetails::chooseSwapExtent(VkExtent2D windowExtent) const
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

SwapChain::SwapChain(Context &ctx,
                     uint32_t maxFramesInFlight,
                     VkSurfaceKHR surface,
                     VkSwapchainCreateInfoKHR createInfo)
    : maxFramesInFlight_{maxFramesInFlight}
    , ctx_{&ctx}
    , imageFormat_{toMagnum(createInfo.imageFormat)}
    , extent_{createInfo.imageExtent.width, createInfo.imageExtent.height}
{
    CO_CORE_DEBUG("SwapChain configuration:");
    CO_CORE_DEBUG("    Surface Format:    {}, {}", imageFormat_, createInfo.imageColorSpace);
    CO_CORE_DEBUG("    Present Mode:      {}", createInfo.presentMode);
    CO_CORE_DEBUG("    Extent:            {}x{}", extent_.x, extent_.y);

    VkSwapchainKHR vkSwapchain;

    THROW_ON_ERROR(
        ctx_->device()->CreateSwapchainKHR(ctx_->device(), &createInfo, nullptr, &vkSwapchain),
        "Could not initialize SwapChain!");

    wrap(vkSwapchain, [ctx = ctx_](VkSwapchainKHR s) {
        ctx->device()->DestroySwapchainKHR(ctx->device(), s, nullptr);
    });

    createImageViews();

    // create all the semaphores needed to manage each parallel frame in flight
    for (uint32_t i = 0; i < maxFramesInFlight_; ++i) {
        imageAcquired_.emplace_back(ctx_->createSemaphore());
        imageRendered_.emplace_back(ctx_->createSemaphore());
        inFlightFences_.emplace_back(ctx_->createFence(FenceCreateMode::Signaled));
    }

    // initialize an array of (empty) fences, one for each image in the swap chain
    imageFences_.resize(imageViews_.size(), nullptr);
}

FrameContext SwapChain::nextImage()
{
    // advance the image index
    nextFrameInFlight_ = (nextFrameInFlight_ + 1) % maxFramesInFlight_;

    FrameContext fc;
    VkResult result =
        ctx_->device()->AcquireNextImageKHR(ctx_->device(),
                                            handle(),
                                            UINT64_MAX,
                                            imageAcquired_[nextFrameInFlight_].handle(),
                                            nullptr,
                                            &fc.index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        fc.shouldRecreateSwapChain = true;
        return fc;
    }
    CO_CORE_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR,
                   "failed to acquire swap chain image: {}",
                   result);

    fc.shouldRecreateSwapChain = false;

    // wait for the fence of the previous frame operating on that image
    if (imageFences_[fc.index] != nullptr) { imageFences_[fc.index]->wait(); }

    // assign the image a new fence and reset it
    imageFences_[fc.index] = &inFlightFences_[nextFrameInFlight_];
    fc.inFlight = &inFlightFences_[nextFrameInFlight_];
    fc.inFlight->reset();

    // assign the semaphores to the struct
    fc.inFlight = &inFlightFences_[nextFrameInFlight_];
    fc.acquired = &imageAcquired_[nextFrameInFlight_];
    fc.rendered = &imageRendered_[nextFrameInFlight_];

    // get the image view
    fc.view = &imageViews_[fc.index];

    return fc;
}

void SwapChain::present(FrameContext &fc)
{
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    VkSemaphore waitSemaphores[1] = {fc.rendered->handle()};
    presentInfo.pWaitSemaphores = waitSemaphores;

    VkSwapchainKHR swapChains[] = {handle()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &fc.index;

    throw std::logic_error{"need to figure out present queue first!"};
    // vkQueuePresentKHR(ctx_.present_queue().get(), &presentInfo);
}

void SwapChain::createImageViews()
{
    uint32_t num_images;
    auto &device = ctx_->device();
    device->GetSwapchainImagesKHR(device, handle(), &num_images, nullptr);
    std::vector<VkImage> swapchainImages(num_images);
    device->GetSwapchainImagesKHR(device, handle(), &num_images, swapchainImages.data());

    // create a vk::image for each of the SwapChain images
    std::ranges::transform(swapchainImages, std::back_inserter(images_), [&](VkImage img) {
        return Vk::Image::wrap(device, img, imageFormat_);
    });

    // create an image view for each of the SwapChain images
    std::ranges::transform(images_, std::back_inserter(imageViews_), [&](Vk::Image &img) {
        return Vk::ImageView{device, Vk::ImageViewCreateInfo2D{img}};
    });

    CO_CORE_DEBUG("    Images:            {}", images_.size());
}
} // namespace Cory