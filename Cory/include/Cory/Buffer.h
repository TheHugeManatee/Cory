#pragma once

#include "Context.h"
#include "Utils.h"
#include "VkUtils.h"

#include <vulkan/vulkan.hpp>

namespace Cory {

class Image;

class Buffer {
  public:
    Buffer();
    ~Buffer();

    // don't copy this thing
    Buffer(const Buffer &rhs) = delete;
    Buffer &operator=(const Buffer &rhs) = delete;

    // we could move technically
    Buffer(Buffer &&rhs) = default;
    Buffer &operator=(Buffer &&rhs) = default;

    void create(GraphicsContext &ctx, vk::DeviceSize size, vk::BufferUsageFlags usage,
                DeviceMemoryUsage memUsage);

    void destroy(GraphicsContext &ctx);

    void upload(GraphicsContext &ctx, const void *srcData, vk::DeviceSize size);
    void download(GraphicsContext &ctx, host_buffer &buf);

    void copyTo(GraphicsContext &ctx, Buffer &rhs, vk::DeviceSize size);
    void copyTo(GraphicsContext &ctx, const Image &rhs);

    vk::Buffer buffer() { return m_buffer; };

  private:
    vk::Buffer m_buffer{};
    vk::DeviceSize m_size{};
    VmaAllocation m_allocation{};
    void *m_mappedMemory{};
};

} // namespace Cory