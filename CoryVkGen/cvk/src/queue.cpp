#include <cvk/queue.h>
#include <cvk/context.h>
#include <cvk/utils.h>

#include <vulkan/vulkan.h>

#include <cvk/log.h>

namespace cvk {

//cvk::future<void>
//queue::submit(executable_command_buffer cmd_buffer,
//              const std::vector<std::pair<VkPipelineStageFlags, semaphore>> &waitSemaphores,
//              const std::vector<semaphore> &signalSemaphores,
//              fence cmdbuf_fence)
//{
//    VkCommandBuffer vk_cmd_buffer = cmd_buffer.get();
//
//    // collect the semaphore stages and Vk objects into separate vectors
//    std::vector<VkPipelineStageFlags> vk_wait_stages;
//    std::vector<VkSemaphore> vk_wait_sem;
//    for (const auto &[stage, sem] : waitSemaphores) {
//        vk_wait_stages.push_back(stage);
//        vk_wait_sem.push_back(sem.get());
//    }
//
//    // collect the signal semaphores into a separate vector
//    auto vk_signal_sem = collect_vk_objects(signalSemaphores);
//
//    // fill the submit info - not much to do here
//    VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
//                            .waitSemaphoreCount = static_cast<uint32_t>(vk_wait_sem.size()),
//                            .pWaitSemaphores = vk_wait_sem.data(),
//                            .pWaitDstStageMask = vk_wait_stages.data(),
//                            .commandBufferCount = 1,
//                            .pCommandBuffers = &vk_cmd_buffer,
//                            .signalSemaphoreCount = static_cast<uint32_t>(vk_signal_sem.size()),
//                            .pSignalSemaphores = vk_signal_sem.data()};
//
//    // create a fence that will synchronize the job callback on the executor
//    if (!cmdbuf_fence.has_value()) cmdbuf_fence = ctx_.fence();
//    VK_CHECKED_CALL(
//        vkQueueSubmit(vk_queue_, 1, &submitInfo, cmdbuf_fence.get()),
//        fmt::format("failed to submit command buffer {} to queue {}", cmd_buffer.name(), name_));
//
//    // enqueue the executor task - this will a) extend the lifetime of the command_buffer until the
//    // GPU has finished executing it and b) it will signal the returned future once the GPU is
//    // finished with the submitted command buffer
//    return queue_executor_.async([cmdbuf_fence = std::move(cmdbuf_fence),
//                                  cmd_buffer = std::move(cmd_buffer),
//                                  dev = ctx_.device()]() {
//        VK_CHECKED_CALL(cmdbuf_fence.wait(SUBMISSION_TIMEOUT),
//                        "timed out while waiting for command submission to finish.");
//    });
//}
//
//cory::future<void>
//queue::submit(const std::vector<executable_command_buffer> cmd_buffers,
//              const std::vector<std::pair<VkPipelineStageFlags, semaphore>> &waitSemaphores,
//              const std::vector<semaphore> &signalSemaphores,
//              fence cmdbuf_fence)
//{
//    if (cmd_buffers.empty()) {
//        CO_CORE_WARN("queue::submit() called with empty cmd_buffers array!");
//        return {};
//    }
//
//    // collect the semaphore stages and Vk objects into separate vectors
//    std::vector<VkPipelineStageFlags> vk_wait_stages;
//    std::vector<VkSemaphore> vk_wait_sem;
//    for (const auto &[stage, sem] : waitSemaphores) {
//        vk_wait_stages.push_back(stage);
//        vk_wait_sem.push_back(sem.get());
//    }
//
//    // "extract" the actual pointers into a vector
//    std::vector<VkCommandBuffer> vk_cmd_buffers = collect_vk_objects(cmd_buffers);
//    auto vk_signal_sem = collect_vk_objects(signalSemaphores);
//
//    // fill the submit info - not much to do here
//    VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
//                            .waitSemaphoreCount = static_cast<uint32_t>(vk_wait_sem.size()),
//                            .pWaitSemaphores = vk_wait_sem.data(),
//                            .pWaitDstStageMask = vk_wait_stages.data(),
//                            .commandBufferCount = static_cast<uint32_t>(vk_cmd_buffers.size()),
//                            .pCommandBuffers = vk_cmd_buffers.data(),
//                            .signalSemaphoreCount = static_cast<uint32_t>(vk_signal_sem.size()),
//                            .pSignalSemaphores = vk_signal_sem.data()};
//
//    // create a fence that will synchronize the job callback on the executor
//    if (!cmdbuf_fence.has_value()) cmdbuf_fence = ctx_.fence();
//    vkQueueSubmit(vk_queue_, 1, &submitInfo, cmdbuf_fence.get());
//
//    // enqueue the executor task - this will a) extend the lifetime of the command_buffers until the
//    // GPU has finished executing it and b) it will signal the returned future once the GPU is
//    // finished with the submitted command buffer
//    return queue_executor_.async(
//        [cmdbuf_fence, cmd_buffers = std::move(cmd_buffers), dev = ctx_.device()]() {
//            VkFence vk_fence = cmdbuf_fence.get();
//            vkWaitForFences(dev, 1, &vk_fence, true, 0);
//        });
//}

} // namespace cory::vk

