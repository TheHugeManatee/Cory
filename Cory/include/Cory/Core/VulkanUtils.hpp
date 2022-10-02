#pragma once

#include <string_view>

// copied definition to avoid vulkan header
typedef struct VkDevice_T *VkDevice;

// detects a Magnum::Vk handle
template <typename T>
concept isMagnumVulkanHandle = requires(T v) { v.handle(); };

namespace Cory {
//// set an object name on a "raw" vulkan handle
template <typename VulkanObjectHandle>
void nameRawVulkanObject(VkDevice device, VulkanObjectHandle handle, std::string_view name);

// set an object name on a magnum vulkan handle (magnum vulkan handles always have a .handle()
// function)
template <typename DeviceHandle, typename MagnumVulkanObjectHandle>
void nameVulkanObject(DeviceHandle &device,
                      MagnumVulkanObjectHandle &handle,
                      std::string_view name);

} // namespace Cory