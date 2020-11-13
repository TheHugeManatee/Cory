
#include "Utils.h"

#include <cassert>
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

VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice,
                             const std::vector<VkFormat> &candidates, VkImageTiling tiling,
                             VkFormatFeatureFlags features)
{
    for (const VkFormat &format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (props.linearTilingFeatures & features) == features) {
            return format;
        }
        if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                 (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice)
{
    return findSupportedFormat(
        physicalDevice,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
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

    if (vkCreateBuffer(*ctx.device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate buffer!");
    }

    // get the device memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(*ctx.device, m_buffer, &memRequirements);

    // allocate the physical device memory
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        findMemoryType(ctx.physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(*ctx.device, &allocInfo, nullptr, &m_bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(*ctx.device, m_buffer, m_bufferMemory, 0);
}

void device_buffer::destroy(graphics_context &ctx)
{
    vkDestroyBuffer(*ctx.device, m_buffer, nullptr);
    vkFreeMemory(*ctx.device, m_bufferMemory, nullptr);
}

void device_buffer::upload(graphics_context &ctx, const void *srcData, VkDeviceSize size,
                           VkDeviceSize offset /*= 0*/)
{
    void *mappedData;
    vkMapMemory(*ctx.device, m_bufferMemory, offset, size, 0,
                &mappedData); // alternately use VK_WHOLE_SIZE
    memcpy(mappedData, srcData, (size_t)size);
    vkUnmapMemory(*ctx.device, m_bufferMemory);

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

void device_image::destroy(graphics_context &ctx)
{
    if (m_sampler)
        vkDestroySampler(*ctx.device, m_sampler, nullptr);
    if (m_imageView)
        vkDestroyImageView(*ctx.device, m_imageView, nullptr);
    if (m_image)
        vkDestroyImage(*ctx.device, m_image, nullptr);
    if (m_imageMemory)
        vkFreeMemory(*ctx.device, m_imageMemory, nullptr);
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
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = m_mipLevels;

    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencilComponent(m_format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }

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
    else if (m_currentLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
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

void device_texture::create(graphics_context &ctx, glm::uvec3 size, uint32_t mipLevels,
                            VkImageType type, VkFormat format, VkImageTiling tiling,
                            VkFilter filter, VkSamplerAddressMode addressMode,
                            VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
    m_size = size;
    m_mipLevels = mipLevels;
    m_format = format;
    m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // create image object
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

    if (vkCreateImage(*ctx.device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    // create and bind image memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(*ctx.device, m_image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        findMemoryType(ctx.physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(*ctx.device, &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(*ctx.device, m_image, m_imageMemory, 0);

    // image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.format = m_format;
    viewInfo.image = m_image;
    assert(type == VK_IMAGE_TYPE_2D &&
           "TODO: creating views for image types other than 2D not implemented!");
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = m_mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(*ctx.device, &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        throw std::runtime_error("Could not create image view");
    }

    // image sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = filter;
    samplerInfo.minFilter = filter;
    samplerInfo.addressModeU = addressMode;
    samplerInfo.addressModeV = addressMode;
    samplerInfo.addressModeW = addressMode;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE; // [0,1] or [0, numberOfTexels]
    samplerInfo.compareEnable = VK_FALSE;           // necessary for PCF shadow maps
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(m_mipLevels);

    if (vkCreateSampler(*ctx.device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Could not create texture sampler!");
    }
}

void device_texture::upload(graphics_context &ctx, const void *srcData, VkDeviceSize size,
                            VkDeviceSize offset /*= 0*/)
{
    void *mappedData;
    vkMapMemory(*ctx.device, m_imageMemory, offset, size, 0,
                &mappedData); // alternately use VK_WHOLE_SIZE
    memcpy(mappedData, srcData, (size_t)size);
    vkUnmapMemory(*ctx.device, m_imageMemory);
}

void device_texture::generate_mipmaps(graphics_context &ctx, VkImageLayout dstLayout,
                                      VkAccessFlags dstAccess)
{
    // check if format actually supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(ctx.physicalDevice, m_format, &formatProperties);
    if (!(formatProperties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("Image format does not support linear blitting!");
        // fallback options: compute mipmap layers in software (either CPU-side with
        // stb_image_resize etc, or GPU-side with compute shaders etc.)
    }

    // make sure everything is transitioned to TRANSFER_DST_OPTIMAL
    transitionLayout(ctx, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    SingleTimeCommandBuffer cmdBuf(ctx);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = m_image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    glm::ivec3 mipSize = m_size;
    for (uint32_t i = 1; i < m_mipLevels; ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuf.buffer(), VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipSize.x, mipSize.y, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipSize.x > 1 ? mipSize.x / 2 : 1, mipSize.y > 1 ? mipSize.y / 2 : 1,
                              1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(cmdBuf.buffer(), m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = dstLayout;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = dstAccess;

        vkCmdPipelineBarrier(cmdBuf.buffer(), VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &barrier);

        if (mipSize.x > 1)
            mipSize.x /= 2;
        if (mipSize.y > 1)
            mipSize.y /= 2;
    }

    barrier.subresourceRange.baseMipLevel = m_mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = dstLayout;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmdBuf.buffer(), VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

SingleTimeCommandBuffer::SingleTimeCommandBuffer(graphics_context &ctx)
    : m_ctx{ctx}
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_ctx.transientCmdPool;
    allocInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(*m_ctx.device, &allocInfo, &m_commandBuffer);

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

    vkFreeCommandBuffers(*m_ctx.device, m_ctx.transientCmdPool, 1, &m_commandBuffer);
}

void depth_buffer::create(graphics_context &ctx, glm::uvec3 size, VkFormat format, VkSampleCountFlagBits msaaSamples)
{
    m_size = size;
    m_mipLevels = 1;
    m_format = format;
    m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_samples = msaaSamples;

    // create image object
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = size.x;
    imageInfo.extent.height = size.y;
    imageInfo.extent.depth = size.z;
    imageInfo.mipLevels = m_mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = m_currentLayout;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = m_samples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(*ctx.device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    // create and bind image memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(*ctx.device, m_image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(ctx.physicalDevice, memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(*ctx.device, &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(*ctx.device, m_image, m_imageMemory, 0);

    // image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.format = m_format;
    viewInfo.image = m_image;

    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(*ctx.device, &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        throw std::runtime_error("Could not create image view");
    }
}

void render_target::create(graphics_context &ctx, glm::uvec3 size,
                           VkFormat format, VkSampleCountFlagBits msaaSamples)
{
    m_size = size;
    m_mipLevels = 1;
    m_format = format;
    m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_samples = msaaSamples;

    // create image object
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = size.x;
    imageInfo.extent.height = size.y;
    imageInfo.extent.depth = size.z;
    imageInfo.mipLevels = m_mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = m_currentLayout;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.samples = m_samples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(*ctx.device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    // create and bind image memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(*ctx.device, m_image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(ctx.physicalDevice, memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(*ctx.device, &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(*ctx.device, m_image, m_imageMemory, 0);

    // image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.format = m_format;
    viewInfo.image = m_image;

    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(*ctx.device, &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        throw std::runtime_error("Could not create image view");
    }
}
