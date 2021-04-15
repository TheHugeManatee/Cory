#pragma once

#include <vulkan/vulkan.h>

#include "shader.h"
#include "render_pass.h"


namespace cory::vk {

class graphics_context;

class pipeline : public basic_vk_wrapper<VkPipeline>{

};

class pipeline_builder {
  public:
    pipeline_builder() = default;

    pipeline_builder &shaders(std::vector<shader> shaders);

    //pipeline_builder &setVertexInput(const Mesh &mesh);

    pipeline_builder &
    vertex_input(const VkVertexInputBindingDescription &bindingDescriptor,
                   const std::vector<VkVertexInputAttributeDescription> &attributeDescriptors,
                   VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    pipeline_builder &viewport(VkExtent2D swapChainExtent);
    pipeline_builder &default_rasterizer();
    pipeline_builder &multisampling(VkSampleCountFlagBits samples);
    pipeline_builder &default_depth_stencil();
    pipeline_builder &
    attachment_blend_states(std::vector<VkPipelineColorBlendAttachmentState> blendStates);
    pipeline_builder &default_dynamic_states();
    pipeline_builder &pipeline_layout(VkPipelineLayout pipelineLayout);
    pipeline_builder &render_pass(render_pass renderPass);

    cory::vk::pipeline create(graphics_context &ctx);

  private:
    // shaders
    std::vector<VkPipelineShaderStageCreateInfo> shader_ci_{};
    std::vector<shader> shaders_;
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
    VkPipelineColorBlendStateCreateInfo color_blending_ci_{};
    //
    std::vector<VkDynamicState> dynamic_states_;
    VkPipelineDynamicStateCreateInfo m_dynamicState{};

    cory::vk::render_pass render_pass_;
    VkPipelineLayout m_pipelineLayout;
};

}