#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/Log.hpp>

#include <fmt/format.h>

#include <any>
#include <array>
#include <memory>
#include <string_view>

/// copied definition to avoid vulkan header
typedef struct VkDevice_T *VkDevice;

/// detects a vulkan object handle (Magnum::Vk or BasicVkObjectWrapper)
template <typename T>
concept isMagnumVulkanHandle = requires(T v) { v.handle(); };

namespace Cory {
/**
 * @brief set an object name on a "raw" vulkan handle
 *
 * "raw" in this case implies a direct vulkan API handle like VkDevice, VkImage, VkBuffer etc.
 *
 * See also @a nameVulkanObject()
 */
template <typename DeviceHandle, typename VulkanObjectHandle>
void nameRawVulkanObject(DeviceHandle &device, VulkanObjectHandle handle, std::string_view name);

/**
 * @brief set an object name on a magnum vulkan handle
 *
 * assumes that the @a DeviceHAndle has a .handle() function.
 * If you get linker errors around this function, it is because the object type
 * is not explicitly supported yet. You will need to extend VulkanUtils.cpp with
 * an explicit template instantiation for your type.
 */
template <typename DeviceHandle, typename MagnumVulkanObjectHandle>
void nameVulkanObject(DeviceHandle &device,
                      MagnumVulkanObjectHandle &handle,
                      std::string_view name);

std::string getVulkanObjectName(void* vulkanObject);

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

/// a type-erased container to keep vulkan structs alive and link up a pnext chain
template <size_t MAX_CHAIN_SIZE = 10> class PNextChain : NoCopy {
  public:
    PNextChain() = default;
    PNextChain(PNextChain &&) = default;
    PNextChain &operator=(PNextChain &&) = default;

    auto &prepend(auto next_struct)
    {
        CO_CORE_ASSERT(current_ < MAX_CHAIN_SIZE, "PNextChain is full");
        data_[current_] = next_struct;

        auto &next = any_cast<decltype(next_struct) &>(data_[current_]);

        next.pNext = std::exchange(head_, &next);
        current_++;

        return next;
    }

    /// insert something into the storage without appending it into the chain
    auto &insert(auto aux_struct)
    {
        CO_CORE_ASSERT(current_ < MAX_CHAIN_SIZE, "PNextChain is full");
        data_[current_] = aux_struct;

        auto &next = any_cast<decltype(aux_struct) &>(data_[current_]);
        current_++;

        return next;
    }

    [[nodiscard]] void *head() const { return head_; };

    [[nodiscard]] size_t size() const { return current_; };

  private:
    std::array<std::any, MAX_CHAIN_SIZE> data_;
    gsl::index current_{};
    void *head_{};
};

} // namespace Cory