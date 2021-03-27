#pragma once

#include "utils.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <type_traits>

namespace cory::vk {


using surface = basic_vk_wrapper<VkSurfaceKHR>;

using semaphore = basic_vk_wrapper<VkSemaphore>;

} // namespace cory::vk
