#include "vk/swapchain.h"

#include "vk/enum_utils.h"
#include "vk/graphics_context.h"
#include "vk/utils.h"

#include "utils/container.h"

#include "Log.h"

namespace cory {
namespace vk {

swapchain::swapchain(uint32_t max_frames_in_flight,
                     graphics_context &ctx,
                     swapchain_builder &builder)
    : max_frames_in_flight_{max_frames_in_flight}
    , ctx_{ctx}
    , image_format_{builder.info_.imageFormat}
    , extent_{builder.info_.imageExtent.width, builder.info_.imageExtent.height}
{
    const auto vk_surface = ctx.surface().get();
    const auto vk_physical_device = ctx.physical_device();
    CO_CORE_ASSERT(vk_surface && vk_physical_device,
                   "Context surface or physical device not initialized!");

    const auto swapchain_support =
        cory::vk::query_swap_chain_support(vk_physical_device, vk_surface);

    // make sure the swapchain config is valid
    CO_CORE_ASSERT(utils::contains(swapchain_support.formats,
                                   [&](const VkSurfaceFormatKHR &sf) {
                                       return sf.format == builder.info_.imageFormat &&
                                              sf.colorSpace == builder.info_.imageColorSpace;
                                   }),
                   "Selected surface format {{ {},{} }} is incompatible with the surface!",
                   builder.info_.imageFormat,
                   builder.info_.imageColorSpace);

    CO_CORE_ASSERT(utils::contains(swapchain_support.presentModes, builder.info_.presentMode),
                   "Selected present mode {} is not compatible with surface!",
                   builder.info_.presentMode);

    CO_CORE_DEBUG("SwapChain configuration:");
    CO_CORE_DEBUG("    Surface Format:    {}, {}", image_format_, builder.info_.imageColorSpace);
    CO_CORE_DEBUG("    Present Mode:      {}", builder.info_.presentMode);
    CO_CORE_DEBUG("    Extent:            {}x{}", extent_.x, extent_.y);

    VkSwapchainKHR vk_swapchain;
    VK_CHECKED_CALL(vkCreateSwapchainKHR(ctx.device(), &builder.info_, nullptr, &vk_swapchain),
                    "Could not initialize swapchain!");

    swapchain_ptr_ = make_shared_resource(vk_swapchain, [dev = ctx.device()](VkSwapchainKHR s) {
        vkDestroySwapchainKHR(dev, s, nullptr);
    });

    create_image_views();

    // create all the semaphores needed to manage each parallel frame in flight
    for (uint32_t i = 0; i < max_frames_in_flight_; ++i) {
        image_acquired_.emplace_back(ctx_.semaphore());
        image_rendered_.emplace_back(ctx_.semaphore());
        in_flight_fences_.emplace_back(ctx_.fence(VK_FENCE_CREATE_SIGNALED_BIT));
    }

    // initialize an array of (empty) fences, one for each image in the swap chain
    image_fences_.resize(image_views_.size());
}

frame_context swapchain::next_image()
{
    // advance the image index
    next_frame_in_flight_ = (next_frame_in_flight_ + 1) % max_frames_in_flight_;

    frame_context fc;
    VkResult result = vkAcquireNextImageKHR(ctx_.device(),
                                            swapchain_ptr_.get(),
                                            UINT64_MAX,
                                            image_acquired_[next_frame_in_flight_].get(),
                                            nullptr,
                                            &fc.index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        fc.should_recreate_swapchain = true;
        return fc;
    }
    CO_CORE_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR,
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
    vkGetSwapchainImagesKHR(ctx_.device(), swapchain_ptr_.get(), &num_images, nullptr);
    std::vector<VkImage> swapchain_images(num_images);
    vkGetSwapchainImagesKHR(
        ctx_.device(), swapchain_ptr_.get(), &num_images, swapchain_images.data());

    // create a vk::image for each of the swapchain images
    std::transform(
        swapchain_images.begin(),
        swapchain_images.end(),
        std::back_inserter(images_),
        [&](VkImage img) {
            // empty deletion function because the images are managed by (i.e. their
            // lifetime is tied to) the swapchain
            auto image_ptr = make_shared_resource(img, [](VkImage img) {});
            return cory::vk::image(
                ctx_, image_ptr, VK_IMAGE_TYPE_2D, image_format_, {extent_.x, extent_.y, 1}, 0);
        });

    // create an image view for each of the swapchain images
    std::transform(
        images_.cbegin(), images_.cend(), std::back_inserter(image_views_), [&](const image &img) {
            return image_view_builder{ctx_, img}
                .subresource_range({.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1})
                .create();
        });

    CO_CORE_DEBUG("    Images:            {}", images_.size());
}

} // namespace vk
} // namespace cory