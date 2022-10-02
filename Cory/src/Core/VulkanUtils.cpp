#include <Cory/Core/VulkanUtils.hpp>

#include <MagnumExternal/Vulkan/flextVkGlobal.h>
#include <type_traits>

namespace Cory {
template <typename VulkanObjectHandle>
void setObjectName(VkDevice device, VulkanObjectHandle handle, std::string_view name)
{
    VkDebugUtilsObjectNameInfoEXT objectNameInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = nullptr,
        .objectType = VK_OBJECT_TYPE_UNKNOWN,
        .objectHandle = (uint64_t)(handle),
        .pObjectName = name.data(),
    };

    if constexpr (std::is_same_v<VulkanObjectHandle, VkInstance>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_INSTANCE;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkDevice>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_DEVICE;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkBuffer>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
    }

    vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo);
}

// explicitly instantiate for known types
#define INSTANTIATE(type) template void setObjectName<type>(VkDevice device, type handle, std::string_view name)

INSTANTIATE(VkDevice);
INSTANTIATE(VkInstance);
INSTANTIATE(VkBuffer);

} // namespace Cory