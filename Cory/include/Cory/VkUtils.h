#pragma once

#include "Shader.h"

#include <optional>
#include <ranges>
#include <type_traits>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace Cory {

class GraphicsContext;

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
    SingleTimeCommandBuffer(GraphicsContext &ctx);
    ~SingleTimeCommandBuffer();

    vk::CommandBuffer &buffer() { return *m_commandBuffer; }

    vk::CommandBuffer *operator->() { return &*m_commandBuffer; };

  private:
    GraphicsContext &m_ctx;
    vk::UniqueCommandBuffer m_commandBuffer;
};

namespace VkDefaults {

class PipelineCreator {
  public:
    PipelineCreator() {}

    PipelineCreator &setShaders(std::vector<Shader> shaders);;

    template <class VertexClass>
    PipelineCreator &
    setVertexInput(vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList)
    {
        m_vertexBindingDescription = typename VertexClass::getBindingDescription();
        m_vertexAttributeDescriptions = typename VertexClass::getAttributeDescriptions();

        m_vertexInputInfo.vertexBindingDescriptionCount = 1;
        m_vertexInputInfo.pVertexBindingDescriptions = &m_vertexBindingDescription;
        m_vertexInputInfo.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(m_vertexAttributeDescriptions.size());
        m_vertexInputInfo.pVertexAttributeDescriptions = m_vertexAttributeDescriptions.data();

        m_inputAssembly.topology = topology;
        m_inputAssembly.primitiveRestartEnable =
            false; // allows to break primitive lists with 0xFFFF index
        return *this;
    }

    PipelineCreator &setViewport(vk::Extent2D swapChainExtent);

    PipelineCreator &setDefaultRasterizer();

    PipelineCreator &setMultisampling(vk::SampleCountFlagBits samples);

    PipelineCreator &setDefaultDepthStencil();

    PipelineCreator &
    setAttachmentBlendStates(std::vector<vk::PipelineColorBlendAttachmentState> blendStates);

    PipelineCreator &setDefaultDynamicStates();;

    PipelineCreator &setPipelineLayout(vk::PipelineLayout pipelineLayout);

    PipelineCreator& setRenderPass(vk::RenderPass renderPass);

    vk::UniquePipeline create(GraphicsContext& ctx);

  private:
    // shaders
    std::vector<vk::PipelineShaderStageCreateInfo> m_shaderCreateInfos{};
    std::vector<Shader> m_shaders;
    // Vertex and input assembly
    vk::VertexInputBindingDescription m_vertexBindingDescription{};
    std::vector<vk::VertexInputAttributeDescription> m_vertexAttributeDescriptions{};
    vk::PipelineVertexInputStateCreateInfo m_vertexInputInfo{};
    vk::PipelineInputAssemblyStateCreateInfo m_inputAssembly{};
    // viewport and scissor
    vk::Viewport m_viewport{};
    vk::Rect2D m_scissor{};
    vk::PipelineViewportStateCreateInfo m_viewportState{};
    //
    vk::PipelineRasterizationStateCreateInfo m_rasterizer{};
    vk::PipelineMultisampleStateCreateInfo m_multisampling{};
    vk::PipelineDepthStencilStateCreateInfo m_depthStencil{};
    std::vector<vk::PipelineColorBlendAttachmentState> m_attachmentBlendStates{};
    vk::PipelineColorBlendStateCreateInfo m_colorBlending{};
    //
    std::vector<vk::DynamicState> m_dynamicStates;
    vk::PipelineDynamicStateCreateInfo m_dynamicState{};

    vk::RenderPass m_renderPass;
    vk::PipelineLayout m_pipelineLayout;
};


vk::Viewport Viewport(vk::Extent2D swapChainExtent);
vk::PipelineViewportStateCreateInfo ViewportState(vk::Viewport &viewport, vk::Rect2D scissor);
vk::PipelineRasterizationStateCreateInfo Rasterizer();
vk::PipelineMultisampleStateCreateInfo
Multisampling(vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);
vk::PipelineDepthStencilStateCreateInfo DepthStencil();
vk::PipelineColorBlendAttachmentState AttachmentBlendDisabled();
vk::PipelineColorBlendStateCreateInfo
BlendState(std::vector<vk::PipelineColorBlendAttachmentState> *attachmentStages);
vk::PipelineLayoutCreateInfo PipelineLayout(vk::DescriptorSetLayout &layout);

} // namespace VkDefaults

} // namespace Cory