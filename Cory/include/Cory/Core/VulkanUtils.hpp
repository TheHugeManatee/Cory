#pragma once

#include <memory>
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

#define VK_CHECKED_CALL(x, err)                                                                    \
    do {                                                                                           \
        if (auto code = (x); code != VK_SUCCESS) {                                                 \
            throw std::runtime_error(fmt::format(#x " failed with {}: {}", code, (err)));          \
        }                                                                                          \
    } while (0)

template <typename WrappedVkType> class BasicVkObjectWrapper {
  public:
    using VkType = WrappedVkType;
    using VkOpaqueType = std::remove_pointer_t<VkType>;
    using VkSharedPtr = std::shared_ptr<VkOpaqueType>;

    BasicVkObjectWrapper(VkSharedPtr vkResourcePtr = {})
        : vkResourcePtr_{vkResourcePtr}
    {
    }

    VkType get() { return vkResourcePtr_.get(); }
    bool has_value() const { return vkResourcePtr_.get() != nullptr; }
    VkType handle() { return *vkResourcePtr_; }

  private:
    VkSharedPtr vkResourcePtr_;
};

} // namespace Cory