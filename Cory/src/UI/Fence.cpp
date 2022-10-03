#include <Cory/UI/Fence.hpp>

#include "Cory/Base/Log.hpp"
#include <Cory/Core/Context.hpp>

#include <Magnum/Vk/Device.h>

namespace Cory {
void Fence::reset()
{
    CO_CORE_ASSERT(has_value(), "Trying to reset empty fence!");

    VkFence f = handle();
    ctx_->device()->ResetFences(ctx_->device(), 1, &f);
}

FenceWaitResult Fence::wait(uint64_t timeout /*= UINT64_MAX*/)
{
    CO_CORE_ASSERT(has_value(), "Trying to reset empty fence!");
    VkFence f = handle();
    auto result = ctx_->device()->WaitForFences(ctx_->device(), 1, &f, VK_TRUE, timeout);
    if (result == VK_SUCCESS) { return FenceWaitResult::Success; }
    if (result == VK_TIMEOUT) { return FenceWaitResult::Timeout; }

    throw std::runtime_error(
        fmt::format("Unexpected result value for vkWaitForFences: {}", result));
}

} // namespace Cory