#pragma once

#include <Cory/Core/VulkanUtils.hpp>

using VkSemaphore = struct VkSemaphore_T *;

namespace Cory {

using Semaphore = BasicVkObjectWrapper<VkSemaphore>;

}