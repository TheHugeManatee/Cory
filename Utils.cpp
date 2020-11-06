#define STB_IMAGE_IMPLEMENTATION

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

device_buffer::device_buffer() {}
device_buffer::~device_buffer() {}

void device_buffer::create(graphics_context &ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags properties)
{
    // Note: ideally, we would not allocate each buffer individually but instead allocate a big
    // chunk and assign it to individual buffers using the vkBindBufferMemory see also
    // https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    m_size = size;
    m_usage = usage;
    m_properties = properties;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode =
        VK_SHARING_MODE_EXCLUSIVE; // sharing between queue families - we don't do that atm

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

void device_buffer::destroy(graphics_context &ctx)
{
    vkDestroyBuffer(ctx.device, m_buffer, nullptr);
    vkFreeMemory(ctx.device, m_bufferMemory, nullptr);
}

void device_buffer::upload(graphics_context &ctx, const void *srcData, VkDeviceSize size,
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

void device_buffer::download(graphics_context &ctx, host_buffer &buf)
{
    throw std::runtime_error("Function not implemented.");
}

void device_buffer::copy_to(graphics_context &ctx, device_buffer &rhs, VkDeviceSize size)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = ctx.transientCmdPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer copyCmdBuffer;
    vkAllocateCommandBuffers(ctx.device, &allocInfo, &copyCmdBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // this is a throwaway buffer

    vkBeginCommandBuffer(copyCmdBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(copyCmdBuffer, buffer(), rhs.buffer(), 1, &copyRegion);

    vkEndCommandBuffer(copyCmdBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &copyCmdBuffer;

    vkQueueSubmit(ctx.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx.graphicsQueue); // alternative: wait for a fence that is signaled

    vkFreeCommandBuffers(ctx.device, ctx.transientCmdPool, 1, &copyCmdBuffer);
}

stbi_image::stbi_image(const std::string &file) {
    data = stbi_load(file.c_str(), &width, &height, &channels, STBI_rgb_alpha);
}

stbi_image::~stbi_image() { stbi_image_free(data); }
