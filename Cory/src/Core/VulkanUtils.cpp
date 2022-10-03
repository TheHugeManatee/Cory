#include <Cory/Core/VulkanUtils.hpp>

#include <Magnum/Vk/Buffer.h>
#include <Magnum/Vk/Device.h>

#include <type_traits>

namespace Cory {

template <typename DeviceHandle, typename VulkanObjectHandle>
void nameRawVulkanObject(DeviceHandle &device, VulkanObjectHandle handle, std::string_view name)
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
    if constexpr (std::is_same_v<VulkanObjectHandle, VkDebugUtilsMessengerEXT>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkDevice>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_DEVICE;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkBuffer>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
    }
    if constexpr (std::is_same_v<VulkanObjectHandle, VkSurfaceKHR>) {
        objectNameInfo.objectType = VK_OBJECT_TYPE_SURFACE_KHR;
    }

    device->SetDebugUtilsObjectNameEXT(device, &objectNameInfo);
}

template <typename DeviceHandle, typename MagnumVulkanObjectHandle>
void nameVulkanObject(DeviceHandle &device, MagnumVulkanObjectHandle &handle, std::string_view name)
{
    nameRawVulkanObject(device, handle.handle(), name);
}

// explicitly instantiate for known types
#define INSTANTIATE(type)                                                                          \
    template void nameRawVulkanObject<Magnum::Vk::Device, type>(                                   \
        Magnum::Vk::Device & device, type handle, std::string_view name)

#define INSTANTIATE_WRAPPED(type)                                                                  \
    template void nameVulkanObject<Magnum::Vk::Device, type>(                                      \
        Magnum::Vk::Device & device, type & handle, std::string_view name)

INSTANTIATE(VkDebugUtilsMessengerEXT);
INSTANTIATE_WRAPPED(BasicVkObjectWrapper<VkSurfaceKHR>);
INSTANTIATE_WRAPPED(Magnum::Vk::Device);
INSTANTIATE_WRAPPED(Magnum::Vk::Buffer);

} // namespace Cory