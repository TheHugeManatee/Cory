#pragma once

#include <Cory/Core/VulkanUtils.hpp>

#include <cstdint>

using VkFence = struct VkFence_T *;

namespace Cory {

class Context;

enum class FenceWaitResult {
    Success,
    Timeout
};

class Fence : public BasicVkObjectWrapper<VkFence> {

  public:
    // creates an empty fence object
    Fence() = default;

    Fence(Context &ctx, VkSharedPtr vk_ptr)
        : BasicVkObjectWrapper(vk_ptr)
        , ctx_{&ctx}
    {
    }

    void reset();
    FenceWaitResult wait(uint64_t timeout = UINT64_MAX);

  private:
    Context *ctx_;
};

} // namespace Cory