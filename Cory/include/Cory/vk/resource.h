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

} // namespace vk
} // namespace cory