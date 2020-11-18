#include "VkUtils.h"

#include "Context.h"

namespace Cory {

QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice &device, vk::SurfaceKHR surface)
{
    auto queueFamilies = device.getQueueFamilyProperties();

    QueueFamilyIndices indices;

    int i = 0;
    for (const auto &queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }
        if (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) {
            indices.computeFamily = i;
        }
        if (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) {
            indices.transferFamily = i;
        }

        if (device.getSurfaceSupportKHR(i, surface)) {
            indices.presentFamily = i;
        }
        i++;
    }

    return indices;
}

uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter,
                        vk::MemoryPropertyFlags properties)
{
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

    for (uint32_t i{}; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find a suitable memory type!");
}

vk::Format findSupportedFormat(vk::PhysicalDevice physicalDevice,
                               const std::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                               vk::FormatFeatureFlags features)
{
    for (const vk::Format &format : candidates) {
        vk::FormatProperties props = physicalDevice.getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear &&
            (props.linearTilingFeatures & features) == features) {
            return format;
        }
        if (tiling == vk::ImageTiling::eOptimal &&
            (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

vk::Format findDepthFormat(vk::PhysicalDevice physicalDevice)
{
    return findSupportedFormat(
        physicalDevice, {vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

bool hasStencilComponent(vk::Format format)
{
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD16UnormS8Uint ||
           format == vk::Format::eD24UnormS8Uint || format == vk::Format::eS8Uint;
}

vk::SampleCountFlagBits getMaxUsableSampleCount(vk::PhysicalDevice physicalDevice)
{
    auto physicalDeviceProperties = physicalDevice.getProperties();

    vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                                  physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & vk::SampleCountFlagBits::e64)
        return vk::SampleCountFlagBits::e64;

    if (counts & vk::SampleCountFlagBits::e32)
        return vk::SampleCountFlagBits::e32;

    if (counts & vk::SampleCountFlagBits::e16)
        return vk::SampleCountFlagBits::e16;

    if (counts & vk::SampleCountFlagBits::e8)
        return vk::SampleCountFlagBits::e8;

    if (counts & vk::SampleCountFlagBits::e4)
        return vk::SampleCountFlagBits::e4;

    if (counts & vk::SampleCountFlagBits::e2)
        return vk::SampleCountFlagBits::e2;

    return vk::SampleCountFlagBits::e1;
}

Cory::SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device,
                                                    vk::SurfaceKHR surface)
{
    SwapChainSupportDetails details;
    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    details.formats = device.getSurfaceFormatsKHR(surface);
    details.presentModes = device.getSurfacePresentModesKHR(surface);

    return details;
}

SingleTimeCommandBuffer::SingleTimeCommandBuffer(graphics_context &ctx)
    : m_ctx{ctx}
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = *m_ctx.transientCmdPool;
    allocInfo.commandBufferCount = 1;

    m_commandBuffer = std::move(m_ctx.device->allocateCommandBuffersUnique(allocInfo)[0]);

    vk::CommandBufferBeginInfo beginInfo{};

    m_commandBuffer->begin(beginInfo);
}

SingleTimeCommandBuffer::~SingleTimeCommandBuffer()
{
    m_commandBuffer->end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer.get();

    m_ctx.graphicsQueue.submit({submitInfo}, vk::Fence{});
    m_ctx.graphicsQueue.waitIdle();
}

} // namespace Cory