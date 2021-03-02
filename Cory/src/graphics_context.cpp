#include <vk_mem_alloc.h>
#include "graphics_context.h"

#include "resource_builders.h"

namespace cory {
namespace vk {


cory::vk::image graphics_context::create_image(const image_builder &builder)
{
    VkImage vkImage;
    VmaAllocation allocation;
    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = static_cast<VmaMemoryUsage>(builder.usage_);
    VmaAllocationInfo allocInfo;
    VkResult result = vmaCreateImage(
        vma_allocator_, &builder.info_, &allocCreateInfo, &vkImage, &allocation, &allocInfo);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate image device memory from memory allocator");
    }
    // create a shared pointer to VkImage_T (because VkImage_T* == VkImage)
    // with a custom deallocation function that destroys the image in the VMA
    // this way we get reference-counted
    std::shared_ptr<VkImage_T> image_sptr(
        vkImage, [=](auto *p) { vmaDestroyImage(vma_allocator_, p, allocation); });

    static_assert(std::is_same<decltype(image_sptr.get()), VkImage>::value);

    return cory::vk::image(*this,
                           std::move(image_sptr),
                           builder.name_,
                           builder.info_.imageType,
                           glm::uvec3{builder.info_.extent.width,
                                      builder.info_.extent.height,
                                      builder.info_.extent.depth},
                           builder.info_.format);
}

cory::vk::buffer graphics_context::create_buffer(const buffer_builder &buffer) {}

}
}