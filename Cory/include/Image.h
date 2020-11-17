#pragma once

#include "Context.h"
#include "VkUtils.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

class device_image {
  public:
    device_image();
    ~device_image();

    // don't copy this thing
    device_image(const device_image &rhs) = delete;
    device_image &operator=(const device_image &rhs) = delete;

    // we could move technically
    device_image(device_image &&rhs) = default;
    device_image &operator=(device_image &&rhs) = default;

    void destroy(graphics_context &ctx);

    void transitionLayout(graphics_context &ctx, vk::ImageLayout newLayout);

    const vk::Image image() const { return m_image; };
    vk::DeviceMemory memory() const { return m_deviceMemory; }
    vk::ImageView view() const { return m_imageView; }
    vk::Sampler sampler() const { return m_sampler; }
    glm::uvec3 size() const { return m_size; }

  protected:
    vk::Image m_image{};
    vk::DeviceMemory m_deviceMemory{};
    VmaAllocation m_allocation{};
    glm::uvec3 m_size{};
    uint32_t m_mipLevels{};
    vk::Format m_format{};
    vk::ImageLayout m_currentLayout{};
    vk::ImageView m_imageView{};
    vk::Sampler m_sampler{};
    vk::SampleCountFlagBits m_samples{vk::SampleCountFlagBits::e1};

    std::string m_name;
};

class device_texture : public device_image {
  public:
    void create(graphics_context &ctx, glm::uvec3 size, uint32_t mipLevels, vk::ImageType type,
                vk::Format format, vk::ImageTiling tiling, vk::Filter filter,
                vk::SamplerAddressMode addressMode, vk::ImageUsageFlags usage,
                DeviceMemoryUsage memoryUsage);

    void upload(graphics_context &ctx, const void *srcData, vk::DeviceSize size);
    // void download(graphics_context &ctx, host_buffer &buf);
    // void copy_to(graphics_context &ctx, device_buffer &rhs, vk::DeviceSize size);

    /**
     * generates mipmaps for a texture. dstLayout and dstAccess specify the configuration that the
     * texture should be transitioned to after the mipmap generation
     */
    void generate_mipmaps(graphics_context &ctx,
                          vk::ImageLayout dstLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::AccessFlags dstAccess = vk::AccessFlagBits::eShaderRead);

  private:
};

class render_target : public device_image {
  public:
    void create(graphics_context &ctx, glm::uvec3 size, vk::Format format,
                vk::SampleCountFlagBits msaaSamples);
};

class depth_buffer : public device_image {
  public:
    void create(graphics_context &ctx, glm::uvec3 size, vk::Format format,
                vk::SampleCountFlagBits msaaSamples);
};