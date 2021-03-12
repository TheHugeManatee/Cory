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

image::image(graphics_context &ctx_,
             std::shared_ptr<VkImage_T>& vk_resource_ptr,
             VkImageType image_type,
             VkFormat image_format,
             glm::uvec3 image_size,
             uint32_t image_mip_levels /*= 0*/,
             std::string_view name /*= {}*/)
    : resource(ctx_, vk_resource_ptr, name)
    , type_{image_type}
    , size_{image_size}
    , format_{image_format}
    , mip_levels_{image_mip_levels}
{
}

} // namespace vk
} // namespace cory