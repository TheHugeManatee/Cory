#define STB_IMAGE_IMPLEMENTATION

#include "Utils.h"

#include <fmt/format.h>
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
    SingleTimeCommandBuffer copyCmdBuffer(ctx);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(copyCmdBuffer.buffer(), buffer(), rhs.buffer(), 1, &copyRegion);
}

void device_buffer::copy_to(graphics_context &ctx, const device_image &rhs)
{
    SingleTimeCommandBuffer cmdBuf(ctx);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {rhs.size().x, rhs.size().y, 1};

    vkCmdCopyBufferToImage(cmdBuf.buffer(), m_buffer, rhs.image(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

stbi_image::stbi_image(const std::string &file)
{
    data = stbi_load(file.c_str(), &width, &height, &channels, STBI_rgb_alpha);
}

stbi_image::~stbi_image() { stbi_image_free(data); }

device_image::device_image() {}

void device_image::create(graphics_context &ctx, glm::uvec3 size, VkImageType type, VkFormat format,
                          VkImageTiling tiling, VkImageUsageFlags usage,
                          VkMemoryPropertyFlags properties)
{
    m_size = size;
    m_mipLevels = 1;
    m_format = format;
    m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = type; // i.e. 1D/2D/3D
    imageInfo.extent.width = size.x;
    imageInfo.extent.height = size.y;
    imageInfo.extent.depth = size.z;
    imageInfo.mipLevels = m_mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = m_currentLayout;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(ctx.device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(ctx.device, m_image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        findMemoryType(ctx.physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(ctx.device, &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(ctx.device, m_image, m_imageMemory, 0);
}

void device_image::destroy(graphics_context &ctx)
{
    vkDestroyImage(ctx.device, m_image, nullptr);
    vkFreeMemory(ctx.device, m_imageMemory, nullptr);
}

void device_image::upload(graphics_context &ctx, const void *srcData, VkDeviceSize size,
                          VkDeviceSize offset /*= 0*/)
{
    void *mappedData;
    vkMapMemory(ctx.device, m_imageMemory, offset, size, 0,
                &mappedData); // alternately use VK_WHOLE_SIZE
    memcpy(mappedData, srcData, (size_t)size);
    vkUnmapMemory(ctx.device, m_imageMemory);
}

void device_image::transitionLayout(graphics_context &ctx, VkImageLayout newLayout)
{
    if (m_currentLayout == newLayout)
        return;

    SingleTimeCommandBuffer cmdBuf(ctx);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = m_currentLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (m_currentLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (m_currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        throw std::runtime_error(fmt::format("unsupported layout transition: from {} to {}",
                                             m_currentLayout, newLayout));
    }

    vkCmdPipelineBarrier(cmdBuf.buffer(), sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr,
                         1, &barrier);

    m_currentLayout = newLayout;
}

device_image::~device_image() {}

SingleTimeCommandBuffer::SingleTimeCommandBuffer(graphics_context &ctx)
    : m_ctx{ctx}
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_ctx.transientCmdPool;
    allocInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(m_ctx.device, &allocInfo, &m_commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
}

SingleTimeCommandBuffer::~SingleTimeCommandBuffer()
{

    vkEndCommandBuffer(m_commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;

    vkQueueSubmit(m_ctx.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_ctx.graphicsQueue);

    vkFreeCommandBuffers(m_ctx.device, m_ctx.transientCmdPool, 1, &m_commandBuffer);
}
