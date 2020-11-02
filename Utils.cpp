#include "Utils.h"

#include <stdexcept>

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                                                  VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i{}; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find a suitable memory type!");
}

device_buffer::device_buffer()
{

}
device_buffer::~device_buffer() {}


void device_buffer::create(Context &ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties)
{
    m_size = size;
    m_usage = usage;
    m_properties = properties;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;// sharing between queue families - we don't do that atm

    if (vkCreateBuffer(ctx.device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate buffer!");
    }

    // get the device memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(ctx.device, m_buffer, &memRequirements);

    // allocate the physical device memory
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        findMemoryType(ctx.physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(ctx.device, &allocInfo, nullptr, &m_bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(ctx.device, m_buffer, m_bufferMemory, 0);

}


void device_buffer::destroy(Context &ctx) {
    vkDestroyBuffer(ctx.device, m_buffer, nullptr);
    vkFreeMemory(ctx.device, m_bufferMemory, nullptr);
}

void device_buffer::upload(Context &ctx, void *srcData, VkDeviceSize size,
                           VkDeviceSize offset /*= 0*/)
{
    void *mappedData;
    vkMapMemory(ctx.device, m_bufferMemory, offset, size, 0,
                &mappedData); // alternately use VK_WHOLE_SIZE
    memcpy(mappedData, srcData, (size_t)size);
    vkUnmapMemory(ctx.device, m_bufferMemory);

        // NOTE: writes are not necessarily visible on the device bc/ caches.
    // either: use memory heap that is HOST_COHERENT
    // or: use vkFlushMappedMemoryRanges after writing mapped range and
    // vkInvalidateMappedMemoryRanges before reading on GPU

    // NOTE 2: CPU-GPU transfer happens in the background and is guaranteed to complete before the
    // next vkQueueSubmit()
}

void device_buffer::download(Context &ctx, host_buffer &buf)
{
    throw std::runtime_error("Function not implemented.");
}

