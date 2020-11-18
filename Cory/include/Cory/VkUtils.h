#pragma once

#include <optional>
#include <type_traits>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace Cory {

class graphics_context;

enum class DeviceMemoryUsage : std::underlying_type<VmaMemoryUsage>::type {
    eUnknown = VMA_MEMORY_USAGE_UNKNOWN,     ///< should not be used
    eGpuOnly = VMA_MEMORY_USAGE_GPU_ONLY,    ///< textures, images used as attachments
    eCpuOnly = VMA_MEMORY_USAGE_CPU_ONLY,    ///< staging buffers
    eCpuToGpu = VMA_MEMORY_USAGE_CPU_TO_GPU, ///< dynamic resources, i.e. vertex/uniform buffers,
                                             ///< dynamic textures
    eGpuToCpu = VMA_MEMORY_USAGE_GPU_TO_CPU, ///< transform feedback, screenshots etc.
    eCpuCopy = VMA_MEMORY_USAGE_CPU_COPY,    ///< staging custom paging/residency
    eGpuLazilyAllocated =
        VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED ///< transient attachment images, might not be
                                              ///< available for desktop GPUs
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> transferFamily;
    std::optional<uint32_t> presentFamily;
};

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

// figure out which queue families are supported (like memory transfer, compute, graphics etc.)
QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice &device, vk::SurfaceKHR surface);

uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter,
                        vk::MemoryPropertyFlags properties);
vk::Format findSupportedFormat(vk::PhysicalDevice physicalDevice,
                               const std::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                               vk::FormatFeatureFlags features);
vk::Format findDepthFormat(vk::PhysicalDevice physicalDevice);

bool hasStencilComponent(vk::Format format);

vk::SampleCountFlagBits getMaxUsableSampleCount(vk::PhysicalDevice physicalDevice);

SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

class SingleTimeCommandBuffer {
  public:
    SingleTimeCommandBuffer(graphics_context &ctx);
    ~SingleTimeCommandBuffer();

    vk::CommandBuffer &buffer() { return *m_commandBuffer; }

    vk::CommandBuffer *operator->() { return &*m_commandBuffer; };

  private:
    graphics_context &m_ctx;
    vk::UniqueCommandBuffer m_commandBuffer;
};

} // namespace Cory