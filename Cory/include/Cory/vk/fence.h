#pragma once

#include "utils.h"

#include <vulkan/vulkan.h>

namespace cory::vk {

class graphics_context;

class fence : public basic_vk_wrapper<VkFence> {

  public:
    // creates an empty fence object
    fence() = default;

    fence(graphics_context &ctx, vk_shared_ptr vk_ptr)
        : basic_vk_wrapper(vk_ptr)
        , ctx_{&ctx}
    {
    }

    void reset();
    VkResult wait(uint64_t timeout = UINT64_MAX) const;

  private:
    graphics_context *ctx_;
};

} // namespace cory::vk
