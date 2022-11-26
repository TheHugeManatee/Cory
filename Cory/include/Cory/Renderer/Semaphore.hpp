#pragma once

#include <Cory/Renderer/VulkanUtils.hpp>

using VkSemaphore = struct VkSemaphore_T *;

namespace Cory {

using Semaphore = BasicVkObjectWrapper<VkSemaphore>;

}