#include "vk/command_buffer.h"

#include "Cory/Log.h"

#include "Cory/vk/queue.h"

namespace cory::vk {

cory::future<void> executable_command_buffer::submit(
    const std::vector<std::pair<VkPipelineStageFlags, semaphore>> &waitSemaphores /*= {}*/,
    const std::vector<semaphore> &signalSemaphores /*= {}*/,
    fence cmdbuf_fence)
{
    CO_CORE_ASSERT(target_queue_ != nullptr,
                   "No target queue was specified - cannot submit() without queue");
    return target_queue_->submit(*this, waitSemaphores, signalSemaphores, cmdbuf_fence);
}

cory::future<void> executable_command_buffer::submit(
    cory::vk::queue &target_queue,
    const std::vector<std::pair<VkPipelineStageFlags, semaphore>> &waitSemaphores /*= {}*/,
    const std::vector<semaphore> &signalSemaphores /*= {}*/,
    fence cmdbuf_fence)
{
    CO_CORE_ASSERT(target_queue.family() == target_queue_->family(),
                   "command buffer was recorded for a different queue family and cannot be "
                   "submitted to this queue!");
    return target_queue.submit(*this, waitSemaphores, signalSemaphores, cmdbuf_fence);
}

} // namespace cory::vk

#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>

#include "cory/vk/graphics_context.h"
#include "cory/vk/test_utils.h"

TEST_CASE("basic command buffer usage")
{
    using namespace cory::vk;
    graphics_context ctx = test_context();

    command_pool pool = command_pool_builder(ctx).queue(ctx.graphics_queue()).create();

    command_buffer cmd_buffer = pool.allocate_buffer();

    // cmd_buffer.bind_pipeline({}, {});
    // executable_command_buffer exec_cmd_buffer = cmd_buffer.end();
}

#endif