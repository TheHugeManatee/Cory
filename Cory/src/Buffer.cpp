#include "Buffer.h"

#include "Image.h"

#include <fmt/format.h>

#if defined(_MSC_VER) && !defined(_WIN64)
#define NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(type, x) static_cast<type>(x)
#else
#define NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(type, x)                                            \
    reinterpret_cast<uint64_t>(static_cast<type>(x))
#endif

namespace Cory {

device_buffer::device_buffer() {}
device_buffer::~device_buffer() {}

void device_buffer::create(graphics_context &ctx, vk::DeviceSize size, vk::BufferUsageFlags usage,
                           DeviceMemoryUsage memUsage)
{
    m_size = size;

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = static_cast<VkBufferUsageFlags>(usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // vk::SharingMode::eExclusive; // sharing between queue families - we don't do that atm

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = static_cast<VmaMemoryUsage>(memUsage);
    if (usage == vk::BufferUsageFlagBits::eUniformBuffer) {
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VmaAllocationInfo allocInfo;

    VkBuffer buffer;
    VkResult result = vmaCreateBuffer(ctx.allocator, &bufferInfo, &allocCreateInfo, &buffer,
                                      &m_allocation, &allocInfo);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate buffer from memory allocator!");
    }

    m_buffer = buffer;
    if (usage == vk::BufferUsageFlagBits::eUniformBuffer) {
        m_mappedMemory = allocInfo.pMappedData;
    }

#ifndef NDEBUG
    std::string name = fmt::format("Buffer [{}]", formatBytes(m_size));
    vk::DebugUtilsObjectNameInfoEXT debugUtilsObjectNameInfo(
        vk::ObjectType::eBuffer, NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkBuffer, m_buffer),
        name.c_str());
    ctx.device->setDebugUtilsObjectNameEXT(debugUtilsObjectNameInfo, ctx.dl);
#endif
}

void device_buffer::destroy(graphics_context &ctx)
{
    vmaDestroyBuffer(ctx.allocator, m_buffer, m_allocation);
}

void device_buffer::upload(graphics_context &ctx, const void *srcData, vk::DeviceSize size)
{
    // uniform buffers might already be mapped
    if (m_mappedMemory) {
        memcpy(m_mappedMemory, srcData, (size_t)size);
        return;
    }

    void *mappedData;
    vmaMapMemory(ctx.allocator, m_allocation, &mappedData);
    memcpy(mappedData, srcData, (size_t)size);
    vmaUnmapMemory(ctx.allocator, m_allocation);

    // NOTE 2: CPU-GPU transfer happens in the background and is guaranteed to complete before the
    // next vkQueueSubmit()
}

void device_buffer::download(graphics_context &ctx, host_buffer &buf)
{
    throw std::runtime_error("Function not implemented.");
}

void device_buffer::copy_to(graphics_context &ctx, device_buffer &rhs, vk::DeviceSize size)
{
    SingleTimeCommandBuffer copyCmdBuffer(ctx);

    vk::BufferCopy copyRegion{};
    copyRegion.size = size;
    copyCmdBuffer.buffer().copyBuffer(m_buffer, rhs.buffer(), {copyRegion});
}

void device_buffer::copy_to(graphics_context &ctx, const device_image &rhs)
{
    SingleTimeCommandBuffer cmdBuf(ctx);

    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{rhs.size().x, rhs.size().y, 1};

    cmdBuf.buffer().copyBufferToImage(m_buffer, rhs.image(), vk::ImageLayout::eTransferDstOptimal,
                                      {region});
}

} // namespace Cory