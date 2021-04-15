#pragma once

#include "pipeline_components.h"
#include "shader.h"
#include "render_pass.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <string_view>
#include <vector>

namespace cory::vk {

class graphics_pipeline_builder;
class graphics_context;

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

    graphics_pipeline_builder &
    stages(std::vector<VkPipelineShaderStageCreateInfo> vk_stages) noexcept
    {
        shader_ci_ = std::move(vk_stages);
        return *this;
    }

    graphics_pipeline_builder &
    vertex_input_state(VkPipelineVertexInputStateCreateInfo vertexInputState) noexcept
    {
        vertex_input_ci_ = vertexInputState;
        return *this;
    }

    graphics_pipeline_builder &
    input_assembly_state(VkPipelineInputAssemblyStateCreateInfo inputAssemblyState) noexcept
    {
        input_assembly_ci_ = inputAssemblyState;
        return *this;
    }

    graphics_pipeline_builder &
    tessellation_state(VkPipelineTessellationStateCreateInfo tessellationState) noexcept
    {
        tesselation_state_ci_ = tessellationState;
        return *this;
    }

    graphics_pipeline_builder &
    viewport_state(VkPipelineViewportStateCreateInfo viewportState) noexcept
    {
        viewport_state_ci_ = viewportState;
        return *this;
    }

    graphics_pipeline_builder &
    rasterization_state(VkPipelineRasterizationStateCreateInfo rasterizationState) noexcept
    {
        rasterizer_ci_ = rasterizationState;
        return *this;
    }

    graphics_pipeline_builder &
    multisample_state(VkPipelineMultisampleStateCreateInfo multisampleState) noexcept
    {
        multisampling_ci_ = multisampleState;
        return *this;
    }

    graphics_pipeline_builder &
    depth_stencil_state(VkPipelineDepthStencilStateCreateInfo depthStencilState) noexcept
    {
        depth_stencil_ci_ = depthStencilState;
        return *this;
    }

    graphics_pipeline_builder &
    color_blend_state(VkPipelineColorBlendStateCreateInfo colorBlendState) noexcept
    {
        color_blend_state_ci_ = colorBlendState;
        return *this;
    }

    graphics_pipeline_builder &
    dynamic_state(VkPipelineDynamicStateCreateInfo dynamicState) noexcept
    {
        dynamic_state_ci_ = dynamicState;
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
    [[nodiscard]] graphics_pipeline create() { 
        return graphics_pipeline(*this); 
    }

  private:
    graphics_context &ctx_;
    VkGraphicsPipelineCreateInfo info_{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    };

    // shaders
    std::vector<VkPipelineShaderStageCreateInfo> shader_ci_{};
    std::vector<cory::vk::shader> shaders_;
    // Vertex and input assembly
    VkVertexInputBindingDescription vertex_binding_desc_{};
    std::vector<VkVertexInputAttributeDescription> vertex_attribute_descs_{};
    VkPipelineVertexInputStateCreateInfo vertex_input_ci_{};
    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci_{};
    // viewport and scissor
    VkViewport viewport_{};
    VkRect2D scissor_{};
    VkPipelineViewportStateCreateInfo viewport_state_ci_{};
    //
    VkPipelineRasterizationStateCreateInfo rasterizer_ci_{};
    VkPipelineMultisampleStateCreateInfo multisampling_ci_{};
    VkPipelineDepthStencilStateCreateInfo depth_stencil_ci_{};
    std::vector<VkPipelineColorBlendAttachmentState> attachment_blend_states_{};
    VkPipelineColorBlendStateCreateInfo color_blend_state_ci_{};
    //
    std::vector<VkDynamicState> dynamic_states_;
    VkPipelineDynamicStateCreateInfo dynamic_state_ci_{};

    cory::vk::render_pass render_pass_;
    VkPipelineLayout m_pipelineLayout;

    std::string_view name_;
    VkPipelineTessellationStateCreateInfo tesselation_state_ci_;
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