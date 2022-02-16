#pragma once

#include "resource.h"
#include "utils.h"

#include <glm.h>

#include <vulkan/vulkan.h>

namespace cvk {

class context;

class image : public resource<image, std::shared_ptr<VkImage_T>> {
  public:
    // initialize from existing VkImage object
    image(cvk::context &ctx_,
          std::shared_ptr<VkImage_T> vk_resource_ptr,
          VkImageType image_type,
          VkFormat image_format,
          glm::uvec3 image_size,
          uint32_t image_mip_levels = 0,
          std::string_view name = {});

    [[nodiscard]] VkImageType type() const noexcept { return type_; }
    [[nodiscard]] const auto &size() const noexcept { return size_; }
    [[nodiscard]] auto format() const noexcept { return format_; }
    [[nodiscard]] auto mip_levels() const noexcept { return mip_levels_; }
    [[nodiscard]] VkImage get() const noexcept { return resource_.get(); }

  private:
    VkImageType type_;
    glm::uvec3 size_;
    VkFormat format_;
    uint32_t mip_levels_;
};

} // namespace cvk