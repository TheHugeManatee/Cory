#pragma once

#include "utils.h"

#include <glm.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <string_view>

namespace cory {
namespace vk {

class image;
class graphics_context;

class image_view : public basic_vk_wrapper<VkImageView> {
  public:
    // default-constructor constructs an empty view
    image_view()
        : basic_vk_wrapper(nullptr){};

    image_view(vk_shared_ptr vk_ptr,
               VkImageViewType view_type,
               VkFormat format,
               glm::uvec3 size,
               uint32_t mip_levels,
               uint32_t layers)
        : basic_vk_wrapper(vk_ptr)
        , view_type_{view_type}
        , format_{format}
        , size_{size}
        , mip_levels_{mip_levels}
        , layers_{layers}
    {
    }

    [[nodiscard]] auto view_type() const noexcept { return view_type_; }
    [[nodiscard]] auto format() const noexcept { return format_; }
    [[nodiscard]] auto size() const noexcept { return size_; }
    [[nodiscard]] auto mip_levels() const noexcept { return mip_levels_; }
    [[nodiscard]] auto layers() const noexcept { return layers_; }

  private:
    VkImageViewType view_type_{VK_IMAGE_VIEW_TYPE_MAX_ENUM};
    VkFormat format_{VK_FORMAT_UNDEFINED};
    glm::uvec3 size_;
    uint32_t mip_levels_{};
    uint32_t layers_{};
};

class image_view_builder {
  public:
    friend image_view;
    image_view_builder(graphics_context &context, const cory::vk::image &img);

    [[nodiscard]] image_view_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    [[nodiscard]] image_view_builder &flags(VkImageViewCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    [[nodiscard]] image_view_builder &image(VkImage image) noexcept
    {
        info_.image = image;
        return *this;
    }

    [[nodiscard]] image_view_builder &view_type(VkImageViewType viewType) noexcept
    {
        info_.viewType = viewType;
        return *this;
    }

    [[nodiscard]] image_view_builder &format(VkFormat format) noexcept
    {
        info_.format = format;
        return *this;
    }

    [[nodiscard]] image_view_builder &components(VkComponentMapping components) noexcept
    {
        info_.components = components;
        return *this;
    }

    [[nodiscard]] image_view_builder &
    subresource_range(VkImageSubresourceRange subresourceRange) noexcept
    {
        info_.subresourceRange = subresourceRange;
        return *this;
    }

    [[nodiscard]] image_view create();

  private:
    graphics_context &ctx_;
    VkImageViewCreateInfo info_{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                .components =
                                    VkComponentMapping{.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                       .a = VK_COMPONENT_SWIZZLE_IDENTITY}};
    const cory::vk::image &image_;
    std::string_view name_;
};

} // namespace vk
} // namespace cory
