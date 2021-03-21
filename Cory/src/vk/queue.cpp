#include "Cory/vk/queue.h"

#include "Cory/vk/graphics_context.h"

#include <vulkan/vulkan.h>

#include "Cory/Log.h"

namespace cory::vk {

cory::future<void> queue::submit(executable_command_buffer cmd_buffer,
                                 const std::vector<semaphore> &waitSemaphores,
                                 const std::vector<semaphore> &signalSemaphores)
{
    VkCommandBuffer vk_cmd_buffer = cmd_buffer.get();
    // fill the submit info - not much to do here
    VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .waitSemaphoreCount = 0,
                            .pWaitSemaphores = nullptr,
                            .pWaitDstStageMask = nullptr,
                            .commandBufferCount = 1,
                            .pCommandBuffers = &vk_cmd_buffer,
                            .signalSemaphoreCount = 0,
                            .pSignalSemaphores = nullptr};

    // create a fence that will synchronize the job callback on the executor
    fence cmdbuf_fence = ctx_.fence();
    vkQueueSubmit(vk_queue_, 1, &submitInfo, cmdbuf_fence.get());

    // enqueue the executor task - this will a) extend the lifetime of the command_buffer until the
    // GPU has finished executing it and b) it will signal the returned future once the GPU is
    // finished with the submitted command buffer
    return queue_executor_.async([cmdbuf_fence = std::move(cmdbuf_fence),
                                  cmd_buffer = std::move(cmd_buffer),
                                  dev = ctx_.device()]() {
        VkFence vk_fence = cmdbuf_fence.get();
        VK_CHECKED_CALL(vkWaitForFences(dev, 1, &vk_fence, true, SUBMISSION_TIMEOUT),
                        "timed out while waiting for command submission to finish.");
    });
}

cory::future<void> queue::submit(const std::vector<executable_command_buffer> cmd_buffers,
                                 const std::vector<semaphore> &waitSemaphores,
                                 const std::vector<semaphore> &signalSemaphores)
{
    if (cmd_buffers.empty()) {
        CO_CORE_WARN("queue::submit() called with empty cmd_buffers array!");
        return {};
    }

    // "extract" the actual pointers into a vector
    std::vector<VkCommandBuffer> vk_cmd_buffers = collect_vk_objects(cmd_buffers);
    auto vk_wait_semaphores = collect_vk_objects(waitSemaphores);
    auto vk_signal_semaphores = collect_vk_objects(signalSemaphores);

    // fill the submit info - not much to do here
    VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .waitSemaphoreCount = 0,
                            .pWaitSemaphores = nullptr,
                            .pWaitDstStageMask = nullptr,
                            .commandBufferCount = static_cast<uint32_t>(vk_cmd_buffers.size()),
                            .pCommandBuffers = vk_cmd_buffers.data(),
                            .signalSemaphoreCount = 0,
                            .pSignalSemaphores = nullptr};

    // create a fence that will synchronize the job callback on the executor
    fence cmdbuf_fence = ctx_.fence();
    vkQueueSubmit(vk_queue_, 1, &submitInfo, cmdbuf_fence.get());

    // enqueue the executor task - this will a) extend the lifetime of the command_buffers until the
    // GPU has finished executing it and b) it will signal the returned future once the GPU is
    // finished with the submitted command buffer
    return queue_executor_.async(
        [cmdbuf_fence, cmd_buffers = std::move(cmd_buffers), dev = ctx_.device()]() {
            VkFence vk_fence = cmdbuf_fence.get();
            vkWaitForFences(dev, 1, &vk_fence, true, 0);
        });
}

} // namespace cory::vk

#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>
#include <vk/test_utils.h>

TEST_SUITE_BEGIN("cory::vk::queue");

TEST_CASE("basic queue usage")
{
    using namespace cory::vk;
    graphics_context ctx = test_context();

    // record and submit directly to a queue
    queue &compute_q = ctx.compute_queue();
    auto cmdbuf1 = compute_q.record([](command_buffer &) {});
    cmdbuf1.submit();

    // record through the context
    auto cmdbuf2 = ctx.record(
        [](command_buffer &buf) {
            // buf.blit_image({}, {}, {}, {}, {}, {}, {});
            // buf.copy_buffer({}, {}, {}, {});
        },
        ctx.compute_queue());
    auto executed = cmdbuf2.submit();

    //// variant 2 - directly on queue
    // auto buffer2 = ctx.graphics_queue().record([](command_buffer &buf) {
    //    buf.blit_image({}, {}, {}, {}, {}, {}, {});
    //    buf.copy_buffer({}, {}, {}, {});
    //});
    // ctx.graphics_queue().submit(buffer2);

    //// variant 3 - overloads
    // auto buffer3 = ctx.record_graphics(...);

    //// variant 4 - template
    // ctx.record<graphics_queue>()...;

    // wait for the GPU to finish (processing an empty command buffer, har har)
    executed.get();
}

TEST_SUITE_END();

#endif
