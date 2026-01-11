#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Common.hpp>

#include <vulkan/vulkan.h>

#include <string_view>

namespace Cory {

class Context;

struct MappedCoherentDeviceBufferCreateInfo {
    std::string_view label{};
    VkDeviceSize size{};
    VkBufferUsageFlags usage{}; // SHADER_DEVICE_ADDRESS_BIT is added automatically
};

class MappedCoherentDeviceBuffer : NoCopy {
  public:
    MappedCoherentDeviceBuffer() = default;
    MappedCoherentDeviceBuffer(Context &ctx, const MappedCoherentDeviceBufferCreateInfo &info);
    ~MappedCoherentDeviceBuffer();

    MappedCoherentDeviceBuffer(MappedCoherentDeviceBuffer &&rhs) noexcept;
    MappedCoherentDeviceBuffer &operator=(MappedCoherentDeviceBuffer &&rhs) noexcept;

    [[nodiscard]] bool isValid() const { return buffer_ != VK_NULL_HANDLE; }
    [[nodiscard]] VkBuffer buffer() const { return buffer_; }
    [[nodiscard]] void *mappedData() const { return mapped_; }
    [[nodiscard]] VkDeviceSize size() const { return size_; }
    [[nodiscard]] BufferDeviceAddress deviceAddress() const { return deviceAddress_; }

  private:
    void reset() noexcept;

    VkDevice device_{VK_NULL_HANDLE};
    VkBuffer buffer_{VK_NULL_HANDLE};
    void *mapped_{nullptr};
    VkDeviceSize size_{0};
    BufferDeviceAddress deviceAddress_{0};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    uint32_t memoryTypeIndex_{UINT32_MAX};
};

} // namespace Cory
