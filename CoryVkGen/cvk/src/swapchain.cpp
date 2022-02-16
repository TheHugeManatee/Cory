#include <cvk/swapchain.h>

#include <cvk/FmtEnum.h>
#include <cvk/context.h>
#include <cvk/image_view_builder.h>
#include <cvk/swapchain_builder.h>
#include <cvk/util/container.h>
#include <cvk/queue.h>

#include <cvk/log.h>

namespace cvk {

swapchain::swapchain(uint32_t max_frames_in_flight, context &ctx, swapchain_builder &builder)
    : max_frames_in_flight_{max_frames_in_flight}
    , ctx_{ctx}
    , image_format_{builder.info_.imageFormat}
    , extent_{builder.info_.imageExtent.width, builder.info_.imageExtent.height}
{
    const auto vk_surface = ctx.vk_surface();
    const auto vk_physical_device = ctx.vk_physical_device();
    CVK_ASSERT(vk_surface && vk_physical_device,
                   "Context surface or physical device not initialized!");

    CVK_DEBUG("SwapChain configuration:");
    CVK_DEBUG("    Surface Format:    {}, {}", image_format_, builder.info_.imageColorSpace);
    CVK_DEBUG("    Present Mode:      {}", builder.info_.presentMode);
    CVK_DEBUG("    Extent:            {}x{}", extent_.x, extent_.y);

    VkSwapchainKHR vk_swapchain;
    VK_CHECKED_CALL(vkCreateSwapchainKHR(ctx.vk_device(), &builder.info_, nullptr, &vk_swapchain),
                    "Could not initialize swapchain!");

    swapchain_ptr_ = make_shared_resource(vk_swapchain, [dev = ctx.vk_device()](VkSwapchainKHR s) {
        vkDestroySwapchainKHR(dev, s, nullptr);
    });

    create_image_views();

    // create all the semaphores needed to manage each parallel frame in flight
    for (uint32_t i = 0; i < max_frames_in_flight_; ++i) {
        image_acquired_.emplace_back(ctx_.create_semaphore());
        image_rendered_.emplace_back(ctx_.create_semaphore());
        in_flight_fences_.emplace_back(ctx_.create_fence(VK_FENCE_CREATE_SIGNALED_BIT));
    }

    // initialize an array of (empty) fences, one for each image in the swap chain
    image_fences_.resize(image_views_.size());
}

frame_context swapchain::next_image()
{
    // advance the image index
    next_frame_in_flight_ = (next_frame_in_flight_ + 1) % max_frames_in_flight_;

    frame_context fc;
    VkResult result = vkAcquireNextImageKHR(ctx_.vk_device(),
                                            swapchain_ptr_.get(),
                                            UINT64_MAX,
                                            image_acquired_[next_frame_in_flight_].get(),
                                            nullptr,
                                            &fc.index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        fc.should_recreate_swapchain = true;
        return fc;
    }
    CVK_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR,
               "failed to acquire swap chain image: {}",
               result);

    fc.should_recreate_swapchain = false;

    // wait for the fence of the previous frame operating on that image
    if (image_fences_[fc.index].has_value()) { image_fences_[fc.index].wait(); }

    // assign the image a new fence and reset it
    image_fences_[fc.index] = in_flight_fences_[next_frame_in_flight_];
    fc.in_flight = in_flight_fences_[next_frame_in_flight_];
    fc.in_flight.reset();

    // assign the semaphores to the struct
    fc.in_flight = in_flight_fences_[next_frame_in_flight_];
    fc.acquired = image_acquired_[next_frame_in_flight_];
    fc.rendered = image_rendered_[next_frame_in_flight_];

    // get the image view
    fc.view = image_views_[fc.index];

    return fc;
}

void swapchain::present(frame_context &fc)
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

    vkQueuePresentKHR(ctx_.present_queue().get(), &presentInfo);
}

void swapchain::create_image_views()
{
    uint32_t num_images;
    vkGetSwapchainImagesKHR(ctx_.vk_device(), swapchain_ptr_.get(), &num_images, nullptr);
    std::vector<VkImage> swapchain_images(num_images);
    vkGetSwapchainImagesKHR(
        ctx_.vk_device(), swapchain_ptr_.get(), &num_images, swapchain_images.data());

    // create a vk::image for each of the swapchain images
    std::ranges::transform(swapchain_images, std::back_inserter(images_), [&](VkImage img) {
        // empty deletion function because the images are managed by (i.e. their
        // lifetime is tied to) the swapchain
        auto image_ptr = make_shared_resource(img, [](VkImage img) {});
        return cvk::image(
            ctx_, image_ptr, VK_IMAGE_TYPE_2D, image_format_, {extent_.x, extent_.y, 1}, 0);
    });

    // create an image view for each of the swapchain images
    std::ranges::transform(images_, std::back_inserter(image_views_), [&](const image &img) {
        return image_view_builder{ctx_, img}
            .subresource_range({.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .baseMipLevel = 0,
                                .levelCount = 1,
                                .baseArrayLayer = 0,
                                .layerCount = 1})
            .create();
    });

    CVK_DEBUG("    Images:            {}", images_.size());
}

} // namespace cvk
