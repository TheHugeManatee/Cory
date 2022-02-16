#include <cvk/fence.h>
#include <cvk/context.h>

#include <cvk/log.h>

namespace cvk {

void fence::reset()
{
    CVK_ASSERT(has_value(), "Trying to reset empty fence!");

    VkFence f = get();
    vkResetFences(ctx_->vk_device(), 1, &f);
}


VkResult fence::wait(uint64_t timeout /*= UINT64_MAX*/) const {
    CVK_ASSERT(has_value(), "Trying to reset empty fence!");
    VkFence f = get();
    return vkWaitForFences(ctx_->vk_device(), 1, &f, VK_TRUE, timeout);
}


} // namespace cory::vk
