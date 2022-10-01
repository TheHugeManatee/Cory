#pragma once

#include <string_view>

typedef struct VkDevice_T* VkDevice;

namespace Cory {

template<typename VulkanObjectHandle>
void setObjectName(VkDevice device, VulkanObjectHandle handle, std::string_view name);

}