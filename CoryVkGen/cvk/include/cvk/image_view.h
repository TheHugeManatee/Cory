#pragma once

#include "core.h"

#include <glm.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <string_view>

namespace cvk {

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
        : basic_vk_wrapper(std::move(vk_ptr))
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

} // namespace cvk
