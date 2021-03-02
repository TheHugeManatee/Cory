#pragma once

#include <glm.h>

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <string_view>

namespace cory {

namespace vk {

class graphics_context;

/**
 * @brief named vulkan resource attached to a context
 * @tparam Derived the concrete resource type. implements CRTP.
 */
template <typename Derived, typename ResourcePtr> class resource {
  public:
    using resource_ptr_t = ResourcePtr;

    resource(graphics_context &ctx, ResourcePtr resource, std::string_view name)
        : ctx_{ctx}
        , resource_{resource}
        , name_{name}
    {
    }
    virtual ~resource() = default;

    const auto &name() { return name_; }
    const auto &context() { return ctx_; }

  protected:
    std::string name_;
    graphics_context &ctx_;
    ResourcePtr resource_;
};

class image : public resource<image, std::shared_ptr<struct VkImage_T>> {
  public:
    friend class graphics_context;

    [[nodiscard]] VkImageType type() const noexcept { return type_; }
    [[nodiscard]] const auto &size() const noexcept { return size_; }
    [[nodiscard]] auto format() const noexcept { return format_; }

  private:
    /// private constructor - create through @a graphics_context
    image(graphics_context &context,
          resource_ptr_t ptr,
          std::string_view name,
          VkImageType type,
          glm::uvec3 size,
          VkFormat format)
        : resource(context, ptr, name)
        , type_{type}
        , size_{size}
        , format_{format}
    {
    }

    VkImageType type_;
    glm::uvec3 size_;
    VkFormat format_;
};

class buffer : public resource<buffer, std::shared_ptr<struct VkBuffer_T>> {
  public:
};

} // namespace vk
} // namespace cory