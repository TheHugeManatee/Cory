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

Buffer::Buffer(Context &ctx, VkDeviceSize size, VkBufferUsageFlags usage,
               VkMemoryPropertyFlags properties)
    : m_size{size}
    , m_usage{usage}
    , m_properties{properties}
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

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

Buffer::~Buffer() {}

void Buffer::upload(void *data, uint32_t size, uint32_t offset) {}

void Buffer::download() {}
