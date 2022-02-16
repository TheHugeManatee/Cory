#include <cvk/image_builder.h>

#include <cvk/context.h>
#include <cvk/image.h>

#include <vk_mem_alloc.h>

namespace cvk {
image image_builder::create()
{

    VkImage vkImage;
    VmaAllocation allocation;
    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = static_cast<VmaMemoryUsage>(usage_);
    VmaAllocationInfo allocInfo;

    VmaAllocator allocator = ctx_.vk_allocator();

    VK_CHECKED_CALL(
        vmaCreateImage(allocator, &info_, &allocCreateInfo, &vkImage, &allocation, &allocInfo),
        "Could not allocate image device memory from memory allocator");

    auto resource = make_shared_resource(
        vkImage, [allocator, allocation](auto *p) { vmaDestroyImage(allocator, p, allocation); });

    return cvk::image{ctx_,
                      std::move(resource),
                      info_.imageType,
                      info_.format,
                      glm::uvec3{info_.extent.width, info_.extent.height, info_.extent.depth},
                      info_.mipLevels,
                      name_};
}
} // namespace cvk