#include "vk/image.h"

#include "vk/graphics_context.h"

#include <vk_mem_alloc.h>

namespace cory {
namespace vk {

image::image(image_builder &builder)
    : resource(builder.ctx_, {}, builder.name_)
    , type_{builder.info_.imageType}
    , size_{builder.info_.extent.width, builder.info_.extent.height, builder.info_.extent.depth}
    , format_{builder.info_.format}
    , mip_levels_{builder.info_.mipLevels}
{
    VkImage vkImage;
    VmaAllocation allocation;
    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = static_cast<VmaMemoryUsage>(builder.usage_);
    VmaAllocationInfo allocInfo;

    VmaAllocator allocator = ctx_.allocator();

    VK_CHECKED_CALL(
        vmaCreateImage(
            allocator, &builder.info_, &allocCreateInfo, &vkImage, &allocation, &allocInfo),
        "Could not allocate image device memory from memory allocator");

    resource_ = make_shared_resource(
        vkImage, [allocator, allocation](auto *p) { vmaDestroyImage(allocator, p, allocation); });
}

} // namespace vk
} // namespace cory