#pragma once

#include "Utils.h"
#include "Context.h"
#include "VkUtils.h"

#include <vulkan/vulkan.hpp>

class device_image;

class device_buffer {
  public:
    device_buffer();
    ~device_buffer();

    // don't copy this thing
    device_buffer(const device_buffer &rhs) = delete;
    device_buffer &operator=(const device_buffer &rhs) = delete;

    // we could move technically
    device_buffer(device_buffer &&rhs) = default;
    device_buffer &operator=(device_buffer &&rhs) = default;

    void create(graphics_context &ctx, vk::DeviceSize size, vk::BufferUsageFlags usage,
                DeviceMemoryUsage memUsage);

    void destroy(graphics_context &ctx);

    void upload(graphics_context &ctx, const void *srcData, vk::DeviceSize size);
    void download(graphics_context &ctx, host_buffer &buf);

    void copy_to(graphics_context &ctx, device_buffer &rhs, vk::DeviceSize size);
    void copy_to(graphics_context &ctx, const device_image &rhs);

    vk::Buffer buffer() { return m_buffer; };

  private:
    vk::Buffer m_buffer{};
    vk::DeviceSize m_size{};
    VmaAllocation m_allocation{};
    void *m_mappedMemory{};
};