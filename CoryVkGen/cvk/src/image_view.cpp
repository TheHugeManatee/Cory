#include <cvk/image_view_builder.h>

#include <cvk/context.h>
#include <cvk/image.h>

#include <vulkan/vulkan.h>

namespace cvk {

image_view_builder::image_view_builder(context &ctx, const cvk::image &img)
    : ctx_{ctx}
    , image_{img}
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


} // namespace cvk
