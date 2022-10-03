#include <Cory/UI/SwapChain.hpp>

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Core/APIConversion.hpp>
#include <Cory/Core/Context.hpp>
#include <Cory/Core/VulkanUtils.hpp>

#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/ImageCreateInfo.h>
#include <Magnum/Vk/ImageViewCreateInfo.h>
#include <MagnumExternal/Vulkan/flextVkGlobal.h>

namespace Vk = Magnum::Vk;

namespace Cory {

SwapChain::SwapChain(uint32_t maxFramesInFlight,
                     Context &ctx,
                     VkSurfaceKHR surface,
                     VkSwapchainCreateInfoKHR createInfo)
    : maxFramesInFlight_{maxFramesInFlight}
    , ctx_{ctx}
    , imageFormat_{fromVk(createInfo.imageFormat)}
    , extent_{createInfo.imageExtent.width, createInfo.imageExtent.height}
{
    CO_CORE_DEBUG("SwapChain configuration:");
    CO_CORE_DEBUG("    Surface Format:    {}, {}", imageFormat_, createInfo.imageColorSpace);
    CO_CORE_DEBUG("    Present Mode:      {}", createInfo.presentMode);
    CO_CORE_DEBUG("    Extent:            {}x{}", extent_.x, extent_.y);

    VkSwapchainKHR vkSwapchain;
    VK_CHECKED_CALL(vkCreateSwapchainKHR(ctx.device().handle(), &createInfo, nullptr, &vkSwapchain),
                    "Could not initialize SwapChain!");

    swapchain_ptr_ = std::shared_ptr<VkSwapchainKHR_T>(
        vkSwapchain, [dev = ctx.device().handle()](VkSwapchainKHR s) {
            vkDestroySwapchainKHR(dev, s, nullptr);
        });

    createImageViews();

    // create all the semaphores needed to manage each parallel frame in flight
    for (uint32_t i = 0; i < maxFramesInFlight_; ++i) {
        imageAcquired_.emplace_back(ctx_.createSemaphore());
        imageRendered_.emplace_back(ctx_.createSemaphore());
        inFlightFences_.emplace_back(ctx_.createFence(FenceCreateMode::Signaled));
    }

    // initialize an array of (empty) fences, one for each image in the swap chain
    imageFences_.resize(image_views_.size());
}

FrameContext SwapChain::nextImage()
{
    // advance the image index
    nextFrameInFlight_ = (nextFrameInFlight_ + 1) % maxFramesInFlight_;

    FrameContext fc;
    VkResult result = vkAcquireNextImageKHR(ctx_.device().handle(),
                                            swapchain_ptr_.get(),
                                            UINT64_MAX,
                                            imageAcquired_[nextFrameInFlight_].get(),
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
    if (imageFences_[fc.index].has_value()) { imageFences_[fc.index].wait(); }

    // assign the image a new fence and reset it
    imageFences_[fc.index] = inFlightFences_[nextFrameInFlight_];
    fc.inFlight = inFlightFences_[nextFrameInFlight_];
    fc.inFlight.reset();

    // assign the semaphores to the struct
    fc.inFlight = inFlightFences_[nextFrameInFlight_];
    fc.acquired = imageAcquired_[nextFrameInFlight_];
    fc.rendered = imageRendered_[nextFrameInFlight_];

    // get the image view
    fc.view = image_views_[fc.index];

    return fc;
}

void SwapChain::present(FrameContext &fc)
{
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    VkSemaphore waitSemaphores[1] = {fc.rendered.get()};
    presentInfo.pWaitSemaphores = waitSemaphores;

    VkSwapchainKHR swapChains[] = {swapchain_ptr_.get()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &fc.index;

    throw std::logic_error{"need to figure out present queue first!"};
    //vkQueuePresentKHR(ctx_.present_queue().get(), &presentInfo);
}

void SwapChain::createImageViews()
{
    uint32_t num_images;
    vkGetSwapchainImagesKHR(ctx_.device().handle(), swapchain_ptr_.get(), &num_images, nullptr);
    std::vector<VkImage> swapchainImages(num_images);
    vkGetSwapchainImagesKHR(
        ctx_.device().handle(), swapchain_ptr_.get(), &num_images, swapchainImages.data());

    // create a vk::image for each of the SwapChain images
    std::ranges::transform(swapchainImages, std::back_inserter(images_), [&](VkImage img) {
        return Vk::Image::wrap(ctx_.device(), img, imageFormat_);
    });

    // create an image view for each of the SwapChain images
    std::ranges::transform(images_, std::back_inserter(image_views_), [&](Vk::Image &img) {
        return Vk::ImageView{ctx_.device(), Vk::ImageViewCreateInfo2D{img}};
    });

    CO_CORE_DEBUG("    Images:            {}", images_.size());
}

} // namespace Cory