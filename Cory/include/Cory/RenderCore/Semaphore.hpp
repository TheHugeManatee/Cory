#pragma once

#include <Cory/RenderCore/VulkanUtils.hpp>

using VkSemaphore = struct VkSemaphore_T *;

namespace Cory {

using Semaphore = BasicVkObjectWrapper<VkSemaphore>;

}