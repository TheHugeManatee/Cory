#include "Image.h"

#include <fmt/format.h>

#if defined(_MSC_VER) && !defined(_WIN64)
#define NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(type, x) static_cast<type>(x)
#else
#define NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(type, x)                                            \
    reinterpret_cast<uint64_t>(static_cast<type>(x))
#endif

namespace Cory {

Image::Image() {}

void Image::destroy(GraphicsContext &ctx)
{
    if (m_sampler)
        vkDestroySampler(*ctx.device, m_sampler, nullptr);
    if (m_imageView)
        vkDestroyImageView(*ctx.device, m_imageView, nullptr);

    if (m_allocation != VmaAllocation{}) {
        vmaDestroyImage(ctx.allocator, m_image, m_allocation);
        return;
    }
    if (m_image)
        vkDestroyImage(*ctx.device, m_image, nullptr);
    if (m_deviceMemory)
        vkFreeMemory(*ctx.device, m_deviceMemory, nullptr);
}

void Image::transitionLayout(GraphicsContext &ctx, vk::ImageLayout newLayout)
{
    if (m_currentLayout == newLayout)
        return;

    SingleTimeCommandBuffer cmdBuf(ctx);

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = m_currentLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = m_image;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = m_mipLevels;

    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        if (hasStencilComponent(vk::Format(m_format))) {
            barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
        }
    }

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (m_currentLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (m_currentLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else if (m_currentLayout == vk::ImageLayout::eUndefined &&
             newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead |
                                vk::AccessFlagBits::eDepthStencilAttachmentWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    }
    else {
        throw std::runtime_error(fmt::format("unsupported layout transition: from {} to {}",
                                             m_currentLayout, newLayout));
    }

    cmdBuf.buffer().pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlags{}, {}, {},
                                    {barrier});

    m_currentLayout = newLayout;
}

Image::~Image() {}

void Texture::create(GraphicsContext &ctx, glm::uvec3 size, uint32_t mipLevels,
                            vk::ImageType type, vk::Format format, vk::ImageTiling tiling,
                            vk::Filter filter, vk::SamplerAddressMode addressMode,
                            vk::ImageUsageFlags usage, DeviceMemoryUsage memoryUsage)
{
    m_size = size;
    m_mipLevels = mipLevels;
    m_format = format;
    m_currentLayout = vk::ImageLayout::eUndefined;

    // create image object
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = type; // i.e. 1D/2D/3D
    imageInfo.extent = vk::Extent3D{size.x, size.y, size.z};
    imageInfo.mipLevels = m_mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = m_currentLayout;
    imageInfo.usage = usage;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = static_cast<VmaMemoryUsage>(memoryUsage);

    VmaAllocationInfo allocInfo;
    VkResult result =
        vmaCreateImage(ctx.allocator, (VkImageCreateInfo *)&imageInfo, &allocCreateInfo,
                       (VkImage *)&m_image, &m_allocation, &allocInfo);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate image device memory from memory allocator");
    }

    m_deviceMemory = allocInfo.deviceMemory;

    // image view
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.format = m_format;
    viewInfo.image = m_image;
    assert(type == vk::ImageType::e2D &&
           "TODO: creating views for image types other than 2D not implemented!");
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = m_mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    m_imageView = ctx.device->createImageView(viewInfo);

    // image sampler
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = filter;
    samplerInfo.minFilter = filter;
    samplerInfo.addressModeU = addressMode;
    samplerInfo.addressModeV = addressMode;
    samplerInfo.addressModeW = addressMode;
    samplerInfo.anisotropyEnable = true;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = false; // [0,1] or [0, numberOfTexels]
    samplerInfo.compareEnable = false;           // necessary for PCF shadow maps
    samplerInfo.compareOp = vk::CompareOp::eAlways;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(m_mipLevels);

    m_sampler = ctx.device->createSampler(samplerInfo);

#ifndef NDEBUG
    m_name = fmt::format("Texture {}x{}x{}", m_size.x, m_size.y, m_size.z);
    vk::DebugUtilsObjectNameInfoEXT debugUtilsObjectNameInfo(
        vk::ObjectType::eImage, NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkImage, m_image),
        m_name.c_str());
    ctx.device->setDebugUtilsObjectNameEXT(debugUtilsObjectNameInfo, ctx.dl);
#endif
}

void Texture::upload(GraphicsContext &ctx, const void *srcData, vk::DeviceSize size)
{
    void *mappedData;
    vmaMapMemory(ctx.allocator, m_allocation, &mappedData);
    memcpy(mappedData, srcData, (size_t)size);
    vmaUnmapMemory(ctx.allocator, m_allocation);
}

void Texture::generateMipmaps(GraphicsContext &ctx, vk::ImageLayout dstLayout,
                                      vk::AccessFlags dstAccess)
{
    // check if format actually supports linear blitting
    vk::FormatProperties formatProperties = ctx.physicalDevice.getFormatProperties(m_format);

    if (!(formatProperties.optimalTilingFeatures &
          vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
        throw std::runtime_error("Image format does not support linear blitting!");
        // fallback options: compute mipmap layers in software (either CPU-side with
        // stb_image_resize etc, or GPU-side with compute shaders etc.)
    }

    // make sure everything is transitioned to TRANSFER_DST_OPTIMAL
    transitionLayout(ctx, vk::ImageLayout::eTransferDstOptimal);

    SingleTimeCommandBuffer cmdBuf(ctx);

    vk::ImageMemoryBarrier barrier{};
    barrier.image = m_image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    glm::ivec3 mipSize = m_size;
    for (uint32_t i = 1; i < m_mipLevels; ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        cmdBuf->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits{}, {},
                                {}, {barrier});

        vk::ImageBlit blit{};
        blit.srcOffsets[0] = vk::Offset3D{0, 0, 0};
        blit.srcOffsets[1] = vk::Offset3D{mipSize.x, mipSize.y, 1};
        blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = vk::Offset3D{0, 0, 0};
        blit.dstOffsets[1] =
            vk::Offset3D{mipSize.x > 1 ? mipSize.x / 2 : 1, mipSize.y > 1 ? mipSize.y / 2 : 1, 1};
        blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        cmdBuf->blitImage(m_image, vk::ImageLayout::eTransferSrcOptimal, m_image,
                          vk::ImageLayout::eTransferDstOptimal, {blit}, vk::Filter::eLinear);

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = dstLayout;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = dstAccess;
        cmdBuf->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags{},
                                {}, {}, {barrier});

        if (mipSize.x > 1)
            mipSize.x /= 2;
        if (mipSize.y > 1)
            mipSize.y /= 2;
    }

    barrier.subresourceRange.baseMipLevel = m_mipLevels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = dstLayout;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = dstAccess;

    cmdBuf->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags{}, {},
                            {}, {barrier});
}

void DepthBuffer::create(GraphicsContext &ctx, glm::uvec3 size, vk::Format format,
                          vk::SampleCountFlagBits msaaSamples)
{
    m_size = size;
    m_mipLevels = 1;
    m_format = format;
    m_currentLayout = vk::ImageLayout::eUndefined;
    m_samples = msaaSamples;

    // create image object
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = vk::Extent3D{size.x, size.y, size.z};
    imageInfo.mipLevels = m_mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_format;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = m_currentLayout;
    imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    imageInfo.samples = m_samples;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;

    VmaAllocationInfo allocInfo;
    VkResult result =
        vmaCreateImage(ctx.allocator, (VkImageCreateInfo *)&imageInfo, &allocCreateInfo,
                       (VkImage *)&m_image, &m_allocation, &allocInfo);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate image device memory from memory allocator");
    }

    m_deviceMemory = allocInfo.deviceMemory;

    // image view
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.format = m_format;
    viewInfo.image = m_image;

    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    m_imageView = ctx.device->createImageView(viewInfo);

#ifndef NDEBUG
    m_name = fmt::format("Depth Buffer [{}x{}]", m_size.x, m_size.y);
    vk::DebugUtilsObjectNameInfoEXT debugUtilsObjectNameInfo(
        vk::ObjectType::eImage, NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkImage, m_image),
        m_name.c_str());
    ctx.device->setDebugUtilsObjectNameEXT(debugUtilsObjectNameInfo, ctx.dl);
#endif
}

void RenderBuffer::create(GraphicsContext &ctx, glm::uvec3 size, vk::Format format,
                           vk::SampleCountFlagBits msaaSamples)
{
    m_size = size;
    m_mipLevels = 1;
    m_format = format;
    m_currentLayout = vk::ImageLayout::eUndefined;
    m_samples = msaaSamples;

    // create image object
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = vk::Extent3D{size.x, size.y, size.z};
    imageInfo.mipLevels = m_mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_format;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = m_currentLayout;
    imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment;
    imageInfo.samples = m_samples;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    // create and bind image memory
    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VmaAllocationInfo allocInfo;
    VkResult result =
        vmaCreateImage(ctx.allocator, (VkImageCreateInfo *)&imageInfo, &allocCreateInfo,
                       (VkImage *)&m_image, &m_allocation, &allocInfo);

    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            "Could not allocate render target device memory from memory allocator");
    }

    m_deviceMemory = allocInfo.deviceMemory;

    // image view
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.format = m_format;
    viewInfo.image = m_image;

    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    m_imageView = ctx.device->createImageView(viewInfo);

#ifndef NDEBUG
    m_name = fmt::format("Render Buffer [{}x{}]", m_size.x, m_size.y);
    vk::DebugUtilsObjectNameInfoEXT debugUtilsObjectNameInfo(
        vk::ObjectType::eImage, NON_DISPATCHABLE_HANDLE_TO_UINT64_CAST(VkImage, m_image),
        m_name.c_str());
    ctx.device->setDebugUtilsObjectNameEXT(debugUtilsObjectNameInfo, ctx.dl);
#endif
}

} // namespace Cory