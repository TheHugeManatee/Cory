#pragma once

#include <cvk/log.h>
#include <cvk/util/executor.h>

#include <fmt/core.h>
#include <vulkan/vulkan.h>

#include <atomic>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace cvk {

class context;

enum class queue_type { graphics, compute, transfer, present };

class queue {
  public:
    static constexpr uint64_t SUBMISSION_TIMEOUT_NS = 2'000'000'000;

    queue(context &ctx, std::string_view name, VkQueue vk_queue, uint32_t family_index)
        : ctx_{ctx}
        , name_{name}
        , vk_queue_{vk_queue}
        , queue_family_{family_index}
        , queue_executor_{fmt::format("{} queue executor", name_)} {};

    // no copy
    queue(const queue &) = delete;
    queue &operator=(const queue &) = delete;

    // move is ok
    queue(queue &&) = default;
    queue &operator=(queue &&) = default;

    //    /**
    //     * @brief record a command buffer for this queue.
    //     *
    //     * the functor passed to this method will be executed *immediately* in order to record the
    //     * commands.
    //     *
    //     * @code
    //     *      cory::vk::queue my_queue{...};
    //     *
    //     *      executable_command_buffer cmd_buf = my_queue.record([&](command_buffer& cmd_buf)
    //     *      {
    //     *          cmd_buf.copy_buffer(...);
    //     *      });
    //     *
    //     *      // some time later, or perhaps immediately
    //     *      cmd_buf.submit();
    //     * @endcode
    //     *
    //     * @return an executable command buffer that can be submitted to any queue with the same
    //     family
    //     */
    //    template <typename Functor> executable_command_buffer record(Functor &&f)
    //    {
    //        command_pool pool = command_pool_builder(ctx_).queue(*this).create();
    //        command_buffer cmd_buffer = pool.allocate_buffer();
    //        f(cmd_buffer);
    //        return cmd_buffer.end();
    //    }
    //
    //    /**
    //     * @brief assigning a VkQueue object - can only be done once!
    //     */
    //    void set_queue(VkQueue vk_queue, uint32_t queue_family)
    //    {
    //        CVK_ASSERT(vk_queue_ == nullptr, "VkQueue can be assigned only once!");
    //        vk_queue_ = vk_queue;
    //        queue_family_ = queue_family;
    //    }
    //
    //    /**
    //     * @brief submit a single command buffer.
    //     *
    //     * the lifetime of the cmd_buffer object will be extended until the GPU has finished with
    //     the
    //     * buffer, thus the user does not need to actively keep the passed command buffer alive
    //     *
    //     * @param waitSemaphores
    //     * @param signalSemaphores
    //     * @return a future that will be fulfilled once the GPU has finished executing the
    //     commands
    //     */
    //    cory::future<void>
    //    submit(executable_command_buffer cmd_buffer,
    //           const std::vector<std::pair<VkPipelineStageFlags, semaphore>> &waitSemaphores = {},
    //           const std::vector<semaphore> &signalSemaphores = {},
    //           fence cmdbuf_fence = {});
    //
    //    /**
    //     * @brief submit multiple command buffers.
    //     */
    //    cory::future<void>
    //    submit(const std::vector<executable_command_buffer> cmd_buffers,
    //           const std::vector<std::pair<VkPipelineStageFlags, semaphore>> &waitSemaphores = {},
    //           const std::vector<semaphore> &signalSemaphores = {},
    //           fence cmdbuf_fence = {});

    [[nodiscard]] auto get() const noexcept { return vk_queue_; }
    [[nodiscard]] auto family() const noexcept { return queue_family_; }

  private:
    context &ctx_;
    uint32_t queue_family_{0xFFFFFFFF};
    std::string name_;
    VkQueue vk_queue_{};
    cvk::util::executor queue_executor_;
};

} // namespace cvk