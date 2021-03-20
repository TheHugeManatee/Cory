#pragma once

#include "Cory/Log.h"

#include <vulkan/vulkan.h>

#include <thread>
#include <atomic>

namespace cory::vk {

class queue {
  public:
    queue(){};

    queue &operator=(VkQueue vk_queue)
    {
        CO_CORE_ASSERT(vk_queue_ == nullptr, "queue can be assigned only once!");
        vk_queue_ = vk_queue;
        return *this;
    }

  private:
    VkQueue vk_queue_{};

};
} // namespace cory::vk