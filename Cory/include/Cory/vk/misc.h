#pragma once

#include <vulkan/vulkan.h>

#include <memory>

namespace cory::vk {

using surface = std::shared_ptr<VkSurfaceKHR_T>;

using fence = std::shared_ptr<VkFence_T>;

using semaphore = std::shared_ptr<VkSemaphore_T>;

} // namespace cory::vk
