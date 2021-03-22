#pragma once

#include "pipeline_components.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <string_view>
#include <vector>

namespace cory::vk {

    class graphics_pipeline_builder;

class graphics_pipeline {
  public:
    graphics_pipeline(graphics_pipeline_builder &builder) {}

  private:
    std::shared_ptr<VkPipeline> vk_pipeline_ptr_;
};

class graphics_pipeline_builder {
  public:
    friend class graphics_pipeline;
    graphics_pipeline_builder(graphics_context &context)
        : ctx_{context}
    {
    }

    graphics_pipeline_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    graphics_pipeline_builder &flags(VkPipelineCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }


    graphics_pipeline_builder &stages(std::vector<VkPipelineShaderStageCreateInfo> vk_stages) noexcept
    {
        info_.stageCount = stageCount;
        info_.pStages = pStages;
        return *this;
    }

    graphics_pipeline_builder &
    vertex_input_state(const VkPipelineVertexInputStateCreateInfo *pVertexInputState) noexcept
    {
        info_.pVertexInputState = pVertexInputState;
        return *this;
    }

    graphics_pipeline_builder &
    input_assembly_state(const VkPipelineInputAssemblyStateCreateInfo *pInputAssemblyState) noexcept
    {
        info_.pInputAssemblyState = pInputAssemblyState;
        return *this;
    }

    graphics_pipeline_builder &
    tessellation_state(const VkPipelineTessellationStateCreateInfo *pTessellationState) noexcept
    {
        info_.pTessellationState = pTessellationState;
        return *this;
    }

    graphics_pipeline_builder &
    viewport_state(const VkPipelineViewportStateCreateInfo *pViewportState) noexcept
    {
        info_.pViewportState = pViewportState;
        return *this;
    }

    graphics_pipeline_builder &
    rasterization_state(const VkPipelineRasterizationStateCreateInfo *pRasterizationState) noexcept
    {
        info_.pRasterizationState = pRasterizationState;
        return *this;
    }

    graphics_pipeline_builder &
    multisample_state(const VkPipelineMultisampleStateCreateInfo *pMultisampleState) noexcept
    {
        info_.pMultisampleState = pMultisampleState;
        return *this;
    }

    graphics_pipeline_builder &
    depth_stencil_state(const VkPipelineDepthStencilStateCreateInfo *pDepthStencilState) noexcept
    {
        info_.pDepthStencilState = pDepthStencilState;
        return *this;
    }

    graphics_pipeline_builder &
    color_blend_state(const VkPipelineColorBlendStateCreateInfo *pColorBlendState) noexcept
    {
        info_.pColorBlendState = pColorBlendState;
        return *this;
    }

    graphics_pipeline_builder &
    dynamic_state(const VkPipelineDynamicStateCreateInfo *pDynamicState) noexcept
    {
        info_.pDynamicState = pDynamicState;
        return *this;
    }

    graphics_pipeline_builder &layout(VkPipelineLayout layout) noexcept
    {
        info_.layout = layout;
        return *this;
    }

    graphics_pipeline_builder &render_pass(VkRenderPass renderPass) noexcept
    {
        info_.renderPass = renderPass;
        return *this;
    }

    graphics_pipeline_builder &subpass(uint32_t subpass) noexcept
    {
        info_.subpass = subpass;
        return *this;
    }

    graphics_pipeline_builder &base_pipeline_handle(VkPipeline basePipelineHandle) noexcept
    {
        info_.basePipelineHandle = basePipelineHandle;
        return *this;
    }

    graphics_pipeline_builder &base_pipeline_index(int32_t basePipelineIndex) noexcept
    {
        info_.basePipelineIndex = basePipelineIndex;
        return *this;
    }

    graphics_pipeline_builder &name(std::string_view name) noexcept
    {
        name_ = name;
        return *this;
    }
    [[nodiscard]] graphics_pipeline create() { return graphics_pipeline(*this); }

  private:
    graphics_context &ctx_;
    VkGraphicsPipelineCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    };
    std::string_view name_;
};

// class pipeline_builder {
//  public:
//    pipeline_builder() = default;
//
//    pipeline_builder &setShaders(std::vector<Shader> shaders);
//
//    pipeline_builder &setVertexInput(const Mesh &mesh);
//
//    pipeline_builder &
//    setVertexInput(const vk::VertexInputBindingDescription &bindingDescriptor,
//                   const std::vector<vk::VertexInputAttributeDescription> &attributeDescriptors,
//                   vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList);
//
//    pipeline_builder &setViewport(vk::Extent2D swapChainExtent);
//    pipeline_builder &setDefaultRasterizer();
//    pipeline_builder &setMultisampling(vk::SampleCountFlagBits samples);
//    pipeline_builder &setDefaultDepthStencil();
//    pipeline_builder &
//    setAttachmentBlendStates(std::vector<vk::PipelineColorBlendAttachmentState> blendStates);
//    pipeline_builder &setDefaultDynamicStates();
//    pipeline_builder &setPipelineLayout(vk::PipelineLayout pipelineLayout);
//    pipeline_builder &setRenderPass(vk::RenderPass renderPass);
//
//    vk::UniquePipeline create(GraphicsContext &ctx);
//
//  private:
//    // shaders
//    std::vector<vk::PipelineShaderStageCreateInfo> m_shaderCreateInfos{};
//    std::vector<Shader> m_shaders;
//    // Vertex and input assembly
//    vk::VertexInputBindingDescription m_vertexBindingDescription{};
//    std::vector<vk::VertexInputAttributeDescription> m_vertexAttributeDescriptions{};
//    vk::PipelineVertexInputStateCreateInfo m_vertexInputInfo{};
//    vk::PipelineInputAssemblyStateCreateInfo m_inputAssembly{};
//    // viewport and scissor
//    vk::Viewport m_viewport{};
//    vk::Rect2D m_scissor{};
//    vk::PipelineViewportStateCreateInfo m_viewportState{};
//    //
//    vk::PipelineRasterizationStateCreateInfo m_rasterizer{};
//    vk::PipelineMultisampleStateCreateInfo m_multisampling{};
//    vk::PipelineDepthStencilStateCreateInfo m_depthStencil{};
//    std::vector<vk::PipelineColorBlendAttachmentState> m_attachmentBlendStates{};
//    vk::PipelineColorBlendStateCreateInfo m_colorBlending{};
//    //
//    std::vector<vk::DynamicState> m_dynamicStates;
//    vk::PipelineDynamicStateCreateInfo m_dynamicState{};
//
//    vk::RenderPass m_renderPass;
//    vk::PipelineLayout m_pipelineLayout;
//};

} // namespace cory::vk