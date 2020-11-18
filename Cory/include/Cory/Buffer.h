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

    void create(graphics_context &ctx, vk::DeviceSize size, vk::BufferUsageFlags usage,
                DeviceMemoryUsage memUsage);

    void destroy(graphics_context &ctx);

    void upload(graphics_context &ctx, const void *srcData, vk::DeviceSize size);
    void download(graphics_context &ctx, host_buffer &buf);

    void copyTo(graphics_context &ctx, Buffer &rhs, vk::DeviceSize size);
    void copyTo(graphics_context &ctx, const Image &rhs);

    vk::Buffer buffer() { return m_buffer; };

  private:
    vk::Buffer m_buffer{};
    vk::DeviceSize m_size{};
    VmaAllocation m_allocation{};
    void *m_mappedMemory{};
};

} // namespace Cory