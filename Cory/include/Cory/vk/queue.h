#pragma once

#include "Cory/Log.h"

#include "Cory/utils/executor.h"

#include <vulkan/vulkan.h>

#include <thread>
#include <atomic>
#include <fmt/core.h>

namespace cory::vk {

class queue {
  public:
    explicit queue(std::string_view name) : name_{name}, queue_executor_{fmt::format("{} queue executor", name_)} {};

    queue &operator=(VkQueue vk_queue)
    {
        CO_CORE_ASSERT(vk_queue_ == nullptr, "queue can be assigned only once!");
        vk_queue_ = vk_queue;
        return *this;
    }

  private:
    VkQueue vk_queue_{};
    std::string name_;
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