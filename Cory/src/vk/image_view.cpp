#include "vk/image_view.h"

#include "vk/image.h"

#include <vulkan/vulkan.h>

namespace cory {
namespace vk {

image_view_builder::image_view_builder(graphics_context &context, image &image)
    : ctx_{context}
    , image_{image}
{
    info_.format = image_.format();
    info_.image = image_.get();

    switch (image_.type()) {
    case VK_IMAGE_TYPE_1D:
        info_.viewType = VK_IMAGE_VIEW_TYPE_1D;
        break;
    case VK_IMAGE_TYPE_2D:
        info_.viewType = VK_IMAGE_VIEW_TYPE_2D;
        break;
    case VK_IMAGE_TYPE_3D:
        info_.viewType = VK_IMAGE_VIEW_TYPE_3D;
        break;
    }

    info_.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info_.subresourceRange.baseMipLevel = 0;
    info_.subresourceRange.levelCount = image_.mip_levels();
    info_.subresourceRange.baseArrayLayer = 0;
    info_.subresourceRange.layerCount = 1;
}

cory::vk::image_view image_view_builder::create()
{
    VkImageView view;
    VK_CHECKED_CALL(vkCreateImageView(ctx_.device(), &info_, nullptr, &view),
                    "Failed to create image view");

    return make_shared_resource(view,
                                [dev = ctx_.device()](VkImageView) { vkDestroyImageView(dev); });
}

} // namespace vk
} // namespace cory