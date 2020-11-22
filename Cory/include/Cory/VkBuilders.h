#pragma once

#include "Shader.h"

#include <vector>
#include <vulkan/vulkan.hpp>

namespace Cory {
class GraphicsContext;
class Mesh;

class PipelineBuilder {
  public:
    PipelineBuilder() = default;

    PipelineBuilder &setShaders(std::vector<Shader> shaders);

    PipelineBuilder &setVertexInput(const Mesh &mesh);

    PipelineBuilder &
    setVertexInput(const vk::VertexInputBindingDescription &bindingDescriptor,
                   const std::vector<vk::VertexInputAttributeDescription> &attributeDescriptors,
                   vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList);

    PipelineBuilder &setViewport(vk::Extent2D swapChainExtent);
    PipelineBuilder &setDefaultRasterizer();
    PipelineBuilder &setMultisampling(vk::SampleCountFlagBits samples);
    PipelineBuilder &setDefaultDepthStencil();
    PipelineBuilder &
    setAttachmentBlendStates(std::vector<vk::PipelineColorBlendAttachmentState> blendStates);
    PipelineBuilder &setDefaultDynamicStates();
    PipelineBuilder &setPipelineLayout(vk::PipelineLayout pipelineLayout);
    PipelineBuilder &setRenderPass(vk::RenderPass renderPass);

    vk::UniquePipeline create(GraphicsContext &ctx);

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

class RenderPassBuilder {
  public:
    void addColorAttachment(vk::Format format, vk::SampleCountFlagBits samples);

    void addDepthAttachment(vk::Format format, vk::SampleCountFlagBits samples);

    void addResolveAttachment(vk::Format format);

    vk::RenderPass create(GraphicsContext &ctx);

  private:
    vk::AttachmentReference addAttachment(vk::AttachmentDescription desc, vk::ImageLayout layout);

  private:
    std::vector<vk::AttachmentDescription> m_attachments;
    std::vector<vk::AttachmentReference> m_colorAttachmentRefs;
    std::vector<vk::AttachmentReference> m_resolveAttachmentRefs;
    vk::AttachmentReference m_depthStencilAttachmentRef;
};

} // namespace Cory