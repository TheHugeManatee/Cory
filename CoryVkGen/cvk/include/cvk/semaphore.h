#pragma once

#include <cvk/core.h>

#include <vulkan/vulkan.h>

namespace cvk {
/// wrapper for a VkSemaphore
using semaphore = basic_vk_wrapper<VkSemaphore>;
}