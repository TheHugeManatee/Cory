#include "vk/swapchain.h"

#include "vk/enum_utils.h"
#include "vk/graphics_context.h"
#include "vk/utils.h"

#include "utils/container.h"

#include "Log.h"

namespace cory {
namespace vk {

swapchain::swapchain(graphics_context &ctx, swapchain_builder &builder)
    : ctx_{ctx}
    , image_format_{builder.info_.imageFormat}
    , extent_{builder.info_.imageExtent}
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
    CO_CORE_DEBUG("    Extent:            {}x{}", extent_.width, extent_.height);

    VkSwapchainKHR vk_swapchain;
    VK_CHECKED_CALL(vkCreateSwapchainKHR(ctx.device(), &builder.info_, nullptr, &vk_swapchain),
                    "Could not initialize swapchain!");

    swapchain_ptr_ = make_shared_resource(vk_swapchain, [dev = ctx.device()](VkSwapchainKHR s) {
        vkDestroySwapchainKHR(dev, s, nullptr);
    });

    create_image_views();
}

void swapchain::create_image_views()
{
    // m_swapChainImages = m_ctx.device->getSwapchainImagesKHR(m_swapChain);

    // m_swapChainImageViews.resize(m_swapChainImages.size());

    // for (size_t i = 0; i < m_swapChainImages.size(); ++i) {
    //    vk::ImageViewCreateInfo createInfo{};
    //    createInfo.image = m_swapChainImages[i];
    //    createInfo.viewType = vk::ImageViewType::e2D;
    //    createInfo.format = m_swapChainImageFormat;
    //    createInfo.components.r = vk::ComponentSwizzle::eIdentity;
    //    createInfo.components.g = vk::ComponentSwizzle::eIdentity;
    //    createInfo.components.b = vk::ComponentSwizzle::eIdentity;
    //    createInfo.components.a = vk::ComponentSwizzle::eIdentity;

    //    // for stereographic, we could create separate image views for the array
    //    // layers here
    //    createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    //    createInfo.subresourceRange.baseMipLevel = 0;
    //    createInfo.subresourceRange.levelCount = 1;
    //    createInfo.subresourceRange.baseArrayLayer = 0;
    //    createInfo.subresourceRange.layerCount = 1;

    //    m_swapChainImageViews[i] = m_ctx.device->createImageView(createInfo);
    //}
    // CO_CORE_ASSERT(false, "create_image_views not implemented");
}

} // namespace vk
} // namespace cory