
#include "Cory/vk/fence.h"
#include "Cory/vk/graphics_context.h"

#include "Cory/Log.h"

namespace cory::vk {

void fence::reset()
{
    CO_CORE_ASSERT(has_value(), "Trying to reset empty fence!");

    VkFence f = get();
    vkResetFences(ctx_->device(), 1, &f);
}


VkResult fence::wait(uint64_t timeout /*= UINT64_MAX*/) const {
    CO_CORE_ASSERT(has_value(), "Trying to reset empty fence!");
    VkFence f = get();
    return vkWaitForFences(ctx_->device(), 1, &f, VK_TRUE, timeout);
}


} // namespace cory::vk
