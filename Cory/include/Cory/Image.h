#pragma once

#include "Context.h"
#include "VkUtils.h"

#include <cstdint>
#include <glm.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace Cory {

class Image {
  public:
    Image();
    ~Image();

    // don't copy this thing
    Image(const Image &rhs) = delete;
    Image &operator=(const Image &rhs) = delete;

    // we could move technically
    Image(Image &&rhs) = default;
    Image &operator=(Image &&rhs) = default;

    void destroy(GraphicsContext &ctx);

    void transitionLayout(GraphicsContext &ctx, vk::ImageLayout newLayout);

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

class Texture : public Image {
  public:
    void create(GraphicsContext &ctx, glm::uvec3 size, uint32_t mipLevels, vk::ImageType type,
                vk::Format format, vk::ImageTiling tiling, vk::Filter filter,
                vk::SamplerAddressMode addressMode, vk::ImageUsageFlags usage,
                DeviceMemoryUsage memoryUsage);

    void upload(GraphicsContext &ctx, const void *srcData, vk::DeviceSize size);
    // void download(graphics_context &ctx, host_buffer &buf);
    // void copy_to(graphics_context &ctx, device_buffer &rhs, vk::DeviceSize size);

    /**
     * generates mipmaps for a texture. dstLayout and dstAccess specify the configuration that the
     * texture should be transitioned to after the mipmap generation
     */
    void generateMipmaps(GraphicsContext &ctx,
                          vk::ImageLayout dstLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::AccessFlags dstAccess = vk::AccessFlagBits::eShaderRead);

  private:
};

class RenderBuffer : public Image {
  public:
    void create(GraphicsContext &ctx, glm::uvec3 size, vk::Format format,
                vk::SampleCountFlagBits msaaSamples);
};

class DepthBuffer : public Image {
  public:
    void create(GraphicsContext &ctx, glm::uvec3 size, vk::Format format,
                vk::SampleCountFlagBits msaaSamples);
};

} // namespace Cory