#pragma once

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

uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter,
                        vk::MemoryPropertyFlags properties);
vk::Format findSupportedFormat(vk::PhysicalDevice physicalDevice,
                               const std::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                               vk::FormatFeatureFlags features);
vk::Format findDepthFormat(vk::PhysicalDevice physicalDevice);

inline bool hasStencilComponent(vk::Format format)
{
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD16UnormS8Uint ||
           format == vk::Format::eD24UnormS8Uint || format == vk::Format::eS8Uint;
}

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