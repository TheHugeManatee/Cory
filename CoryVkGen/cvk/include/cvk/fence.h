#pragma once

#include "core.h"

#include <vulkan/vulkan.h>

namespace cvk {

class context;

class fence : public basic_vk_wrapper<VkFence> {

  public:
    // creates an empty fence object
    fence() = default;

    fence(context &ctx, vk_shared_ptr vk_ptr)
        : basic_vk_wrapper(vk_ptr)
        , ctx_{&ctx}
    {
    }

    void reset();
    VkResult wait(uint64_t timeout = UINT64_MAX) const;

  private:
    context *ctx_;
};

} // namespace cvk
