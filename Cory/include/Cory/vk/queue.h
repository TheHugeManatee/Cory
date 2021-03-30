#pragma once

#include "command_buffer.h"

#include "Cory/Log.h"
#include "Cory/utils/executor.h"

#include "command_buffer.h"
#include "command_pool.h"
#include "misc.h"

#include <fmt/core.h>
#include <vulkan/vulkan.h>

#include <atomic>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace cory::vk {

class graphics_context;

enum class queue_type { graphics, compute, transfer, present };

class queue {
  public:
    static constexpr uint64_t SUBMISSION_TIMEOUT = 2'000'000'000;

    queue(graphics_context &ctx,
          std::string_view name,
          VkQueue vk_queue = {},
          uint32_t family_index = {})
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

    /**
     * @brief record a command buffer for this queue.
     *
     * the functor passed to this method will be executed *immediately* in order to record the
     * commands.
     *
     * @code
     *      cory::vk::queue my_queue{...};
     *
     *      executable_command_buffer cmd_buf = my_queue.record([&](command_buffer& cmd_buf)
     *      {
     *          cmd_buf.copy_buffer(...);
     *      });
     *
     *      // some time later, or perhaps immediately
     *      cmd_buf.submit();
     * @endcode
     *
     * @return an executable command buffer that can be submitted to any queue with the same family
     */
    template <typename Functor> executable_command_buffer record(Functor &&f)
    {
        command_pool pool = command_pool_builder(ctx_).queue(*this).create();
        command_buffer cmd_buffer = pool.allocate_buffer();
        f(cmd_buffer);
        return cmd_buffer.end();
    }

    /**
     * @brief assigning a VkQueue object - can only be done once!
     */
    void set_queue(VkQueue vk_queue, uint32_t queue_family)
    {
        CO_CORE_ASSERT(vk_queue_ == nullptr, "VkQueue can be assigned only once!");
        vk_queue_ = vk_queue;
        queue_family_ = queue_family;
    }

    /**
     * @brief submit a single command buffer.
     *
     * the lifetime of the cmd_buffer object will be extended until the GPU has finished with the
     * buffer, thus the user does not need to actively keep the passed command buffer alive
     *
     * @param waitSemaphores
     * @param signalSemaphores
     * @return a future that will be fulfilled once the GPU has finished executing the commands
     */
    cory::future<void>
    submit(executable_command_buffer cmd_buffer,
           const std::vector<std::pair<VkPipelineStageFlags, semaphore>> &waitSemaphores = {},
           const std::vector<semaphore> &signalSemaphores = {},
           fence cmdbuf_fence = {});

    /**
     * @brief submit multiple command buffers.
     */
    cory::future<void>
    submit(const std::vector<executable_command_buffer> cmd_buffers,
           const std::vector<std::pair<VkPipelineStageFlags, semaphore>> &waitSemaphores = {},
           const std::vector<semaphore> &signalSemaphores = {},
           fence cmdbuf_fence = {});

    [[nodiscard]] auto get() const noexcept { return vk_queue_; }
    [[nodiscard]] auto family() const noexcept { return queue_family_; }

  private:
    graphics_context &ctx_;
    uint32_t queue_family_{0xFFFFFFFF};
    std::string name_;
    VkQueue vk_queue_{};
    cory::utils::executor queue_executor_;
};

class queue_builder {
  public:
    friend class device_builder;

    [[nodiscard]] queue_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    [[nodiscard]] queue_builder &flags(VkDeviceQueueCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    [[nodiscard]] queue_builder &queue_family_index(uint32_t queueFamilyIndex) noexcept
    {
        info_.queueFamilyIndex = queueFamilyIndex;
        return *this;
    }

    [[nodiscard]] queue_builder &queue_priorities(std::vector<float> queuePriorities) noexcept
    {
        queue_priorities_ = queuePriorities;
        return *this;
    }

  private:
    [[nodiscard]] auto create_info()
    {
        info_.queueCount = static_cast<uint32_t>(queue_priorities_.size());
        info_.pQueuePriorities = queue_priorities_.data();
        return info_;
    }
    VkDeviceQueueCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    };
    std::vector<float> queue_priorities_;
};

} // namespace cory::vk