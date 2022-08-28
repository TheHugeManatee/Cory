#pragma once

#include <glm.h>

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <string_view>

namespace cvk {

class context;

/**
 * @brief named vulkan resource attached to a context
 * @tparam Derived the concrete resource type. implements CRTP.
 */
template <typename Derived, typename ResourcePtr> class resource {
  public:
    using resource_ptr_t = ResourcePtr;

    resource(context &ctx, ResourcePtr resource, std::string_view name)
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
    cvk::context &ctx_;
    ResourcePtr resource_;
};

} // namespace cvk