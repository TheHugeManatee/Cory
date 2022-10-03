#pragma once

#include <Cory/Base/Common.hpp>

#include <memory>
#include <string_view>

// copied definition to avoid vulkan header
typedef struct VkDevice_T *VkDevice;

// detects a vulkan object handle (Magnum::Vk or BasicVkObjectWrapper)
template <typename T>
concept isMagnumVulkanHandle = requires(T v) { v.handle(); };

namespace Cory {
//// set an object name on a "raw" vulkan handle
template <typename DeviceHandle, typename VulkanObjectHandle>
void nameRawVulkanObject(DeviceHandle &device, VulkanObjectHandle handle, std::string_view name);

// set an object name on a magnum vulkan handle (magnum vulkan handles always have a .handle()
// function)
template <typename DeviceHandle, typename MagnumVulkanObjectHandle>
void nameVulkanObject(DeviceHandle &device,
                      MagnumVulkanObjectHandle &handle,
                      std::string_view name);

#define THROW_ON_ERROR(x, err)                                                                     \
    do {                                                                                           \
        if (auto code = (x); code != VK_SUCCESS) {                                                 \
            throw std::runtime_error(fmt::format(#x " failed with {}: {}", code, (err)));          \
        }                                                                                          \
    } while (0)

template <typename WrappedVkType> class BasicVkObjectWrapper : NoCopy {
  public:
    using VkType = WrappedVkType;
    using VkOpaqueType = std::remove_pointer_t<VkType>;
    using VkSharedPtr = std::shared_ptr<VkOpaqueType>;

    template <typename DeleterFn>
    BasicVkObjectWrapper(VkType resource, DeleterFn &&deleter)
        : vkResourcePtr_{resource, std::forward<DeleterFn>(deleter)}
    {
    }

    /* implicit */ BasicVkObjectWrapper(VkSharedPtr vkResourcePtr = {})
        : vkResourcePtr_{vkResourcePtr}
    {
    }

    template <typename DeleterFn> void wrap(VkType resource, DeleterFn &&deleter)
    {
        vkResourcePtr_ = {resource, std::forward<DeleterFn>(deleter)};
    }

    // moveable
    BasicVkObjectWrapper(BasicVkObjectWrapper &&rhs) = default;
    BasicVkObjectWrapper &operator=(BasicVkObjectWrapper &&) = default;
    // copyable
    BasicVkObjectWrapper(const BasicVkObjectWrapper &rhs) = default;
    BasicVkObjectWrapper &operator=(const BasicVkObjectWrapper &rhs) = default;

    // implicit conversion to vulkan handle type
    operator VkType() { return vkResourcePtr_.get(); }

    // access handle explicitly
    VkType handle() { return vkResourcePtr_.get(); }
    // check if it is valid
    bool has_value() const { return vkResourcePtr_.get() != nullptr; }

  protected:
    VkSharedPtr vkResourcePtr_;
};

} // namespace Cory