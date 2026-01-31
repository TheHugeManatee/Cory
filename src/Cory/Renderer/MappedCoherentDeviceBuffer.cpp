#include <Cory/Renderer/MappedCoherentDeviceBuffer.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>

#include <KDGpu/vulkan/vulkan_adapter.h>
#include <KDGpu/vulkan/vulkan_device.h>
#include <KDGpu/vulkan/vulkan_graphics_api.h>
#include <KDGpu/vulkan/vulkan_resource_manager.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include <cstddef>
#include <cstdint>

namespace Cory {

namespace {

uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                        uint32_t typeBits,
                        VkMemoryPropertyFlags requiredFlags)
{
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);

    for (uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
        if ((typeBits & (1u << index)) == 0) continue;
        const auto flags = properties.memoryTypes[index].propertyFlags;
        if ((flags & requiredFlags) == requiredFlags) return index;
    }

    return UINT32_MAX;
}

void setDebugName(KDGpu::VulkanDevice *device,
                  VkObjectType type,
                  uint64_t handle,
                  std::string_view name)
{
    if (!device || !device->vkSetDebugUtilsObjectNameEXT || name.empty()) return;

    const VkDebugUtilsObjectNameInfoEXT nameInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = nullptr,
        .objectType = type,
        .objectHandle = handle,
        .pObjectName = name.data(),
    };
    device->vkSetDebugUtilsObjectNameEXT(device->device, &nameInfo);
}

} // namespace

MappedCoherentDeviceBuffer::MappedCoherentDeviceBuffer(
    Gpu::Device &device, const MappedCoherentDeviceBufferCreateInfo &info)
{
    if (info.size == 0) {
        CO_CORE_ERROR("MappedCoherentDeviceBuffer: size must be non-zero.");
        return;
    }
    CO_CORE_ASSERT(info.usage != 0,
                   "MappedCoherentDeviceBuffer: usage must be non-zero (provide buffer usage).");

    auto &resourceManager = *device.graphicsApi()->resourceManager();
    auto *vulkanDevice = resourceManager.getDevice(device.handle());
    CO_CORE_DEBUG_ASSERT(vulkanDevice, "MappedCoherentDeviceBuffer: Vulkan device was null.");

    auto *adapter = resourceManager.getAdapter(vulkanDevice->adapterHandle);
    CO_CORE_ASSERT(adapter, "MappedCoherentDeviceBuffer: Vulkan adapter was null.");

    device_ = vulkanDevice->device;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = info.size;
    bufferInfo.usage = info.usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (const auto result = vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer_);
        result != VK_SUCCESS) {
        CO_CORE_ERROR("MappedCoherentDeviceBuffer: Failed to create buffer (VkResult {}).",
                      vk::to_string(vk::Result{result}));
        reset();
        return;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(device_, buffer_, &memoryRequirements);

    constexpr VkMemoryPropertyFlags requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    memoryTypeIndex_ =
        findMemoryType(adapter->physicalDevice, memoryRequirements.memoryTypeBits, requiredFlags);
    if (memoryTypeIndex_ == UINT32_MAX) {
        CO_CORE_ERROR("MappedCoherentDeviceBuffer: No memory type satisfies required flags of {} "
                      "for allocation of size {}.",
                      vk::to_string(vk::MemoryPropertyFlags{requiredFlags}),
                      Log::asMemorySize(info.size));
        CO_CORE_ASSERT(false,
                       "MappedCoherentDeviceBuffer: Required memory flags not supported on this "
                       "system (expecting ReBAR-capable hardware).");
        reset();
        return;
    }

    const VkMemoryAllocateFlagsInfo allocateFlags = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
    };

    const VkMemoryAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &allocateFlags,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = memoryTypeIndex_,
    };

    if (const auto result = vkAllocateMemory(device_, &allocateInfo, nullptr, &memory_);
        result != VK_SUCCESS) {
        CO_CORE_ERROR("MappedCoherentDeviceBuffer: Failed to allocate memory (VkResult {}).",
                      vk::to_string(vk::Result{result}));
        reset();
        return;
    }

    if (const auto result = vkBindBufferMemory(device_, buffer_, memory_, 0);
        result != VK_SUCCESS) {
        CO_CORE_ERROR("MappedCoherentDeviceBuffer: Failed to bind buffer memory (VkResult {}).",
                      vk::to_string(vk::Result{result}));
        reset();
        return;
    }

    if (const auto result = vkMapMemory(device_, memory_, 0, memoryRequirements.size, 0, &mapped_);
        result != VK_SUCCESS) {
        CO_CORE_ERROR("MappedCoherentDeviceBuffer: Failed to map buffer memory (VkResult {}).",
                      vk::to_string(vk::Result{result}));
        reset();
        return;
    }

    size_ = info.size;

    setDebugName(
        vulkanDevice, VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(buffer_), info.label);

    const VkBufferDeviceAddressInfo addressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer_,
    };
    deviceAddress_ = vkGetBufferDeviceAddress(device_, &addressInfo);
}

MappedCoherentDeviceBuffer::~MappedCoherentDeviceBuffer()
{
    reset();
}

MappedCoherentDeviceBuffer::MappedCoherentDeviceBuffer(MappedCoherentDeviceBuffer &&rhs) noexcept
    : device_(rhs.device_)
    , buffer_(rhs.buffer_)
    , mapped_(rhs.mapped_)
    , size_(rhs.size_)
    , deviceAddress_(rhs.deviceAddress_)
    , memory_(rhs.memory_)
    , memoryTypeIndex_(rhs.memoryTypeIndex_)
{
    rhs.device_ = VK_NULL_HANDLE;
    rhs.buffer_ = VK_NULL_HANDLE;
    rhs.mapped_ = nullptr;
    rhs.size_ = 0;
    rhs.deviceAddress_ = 0;
    rhs.memory_ = VK_NULL_HANDLE;
    rhs.memoryTypeIndex_ = UINT32_MAX;
}

MappedCoherentDeviceBuffer &
MappedCoherentDeviceBuffer::operator=(MappedCoherentDeviceBuffer &&rhs) noexcept
{
    if (this == &rhs) return *this;

    reset();
    device_ = rhs.device_;
    buffer_ = rhs.buffer_;
    mapped_ = rhs.mapped_;
    size_ = rhs.size_;
    deviceAddress_ = rhs.deviceAddress_;
    memory_ = rhs.memory_;
    memoryTypeIndex_ = rhs.memoryTypeIndex_;

    rhs.device_ = VK_NULL_HANDLE;
    rhs.buffer_ = VK_NULL_HANDLE;
    rhs.mapped_ = nullptr;
    rhs.size_ = 0;
    rhs.deviceAddress_ = 0;
    rhs.memory_ = VK_NULL_HANDLE;
    rhs.memoryTypeIndex_ = UINT32_MAX;
    return *this;
}

void MappedCoherentDeviceBuffer::reset() noexcept
{
    if (buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer_, nullptr);
    }
    if (memory_ != VK_NULL_HANDLE) {
        if (mapped_) {
            vkUnmapMemory(device_, memory_);
        }
        vkFreeMemory(device_, memory_, nullptr);
    }

    device_ = VK_NULL_HANDLE;
    buffer_ = VK_NULL_HANDLE;
    mapped_ = nullptr;
    size_ = 0;
    deviceAddress_ = 0;
    memory_ = VK_NULL_HANDLE;
    memoryTypeIndex_ = UINT32_MAX;
}

} // namespace Cory
