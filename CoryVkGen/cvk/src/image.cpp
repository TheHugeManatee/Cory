#include <cvk/image.h>

#include <cvk/context.h>

#include <vk_mem_alloc.h>


namespace cvk {

image::image(cvk::context &ctx_,
             std::shared_ptr<VkImage_T> vk_resource_ptr,
             VkImageType image_type,
             VkFormat image_format,
             glm::uvec3 image_size,
             uint32_t image_mip_levels /*= 0*/,
             std::string_view name /*= {}*/)
    : resource(ctx_, std::move(vk_resource_ptr), name)
    , type_{image_type}
    , size_{image_size}
    , format_{image_format}
    , mip_levels_{image_mip_levels}
{
}

} // namespace cvk