#include <cvk/image_view_builder.h>

#include <cvk/context.h>
#include <cvk/image.h>

namespace cvk {
image_view image_view_builder::create()
{
    VkImageView view;
    VK_CHECKED_CALL(vkCreateImageView(ctx_.vk_device(), &info_, nullptr, &view),
                    "Failed to create image view");
    auto vk_resource_ptr = make_shared_resource(view, [dev = ctx_.vk_device()](VkImageView view) {
        vkDestroyImageView(dev, view, nullptr);
    });
    return image_view(vk_resource_ptr,
                      info_.viewType,
                      info_.format,
                      image_.size(),
                      info_.subresourceRange.layerCount,
                      info_.subresourceRange.levelCount);
}
} // namespace cvk