#pragma once

#include "Shader.h"

#include <vector>
#include <vulkan/vulkan.hpp>

namespace Cory {
class GraphicsContext;
class Mesh;

class PipelineCreator {
  public:
    PipelineCreator() = default;

    PipelineCreator &setShaders(std::vector<Shader> shaders);

    PipelineCreator &setVertexInput(const Mesh &mesh);

    PipelineCreator &
    setVertexInput(const vk::VertexInputBindingDescription &bindingDescriptor,
                   const std::vector<vk::VertexInputAttributeDescription> &attributeDescriptors,
                   vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList);

    PipelineCreator &setViewport(vk::Extent2D swapChainExtent);
    PipelineCreator &setDefaultRasterizer();
    PipelineCreator &setMultisampling(vk::SampleCountFlagBits samples);
    PipelineCreator &setDefaultDepthStencil();
    PipelineCreator &
    setAttachmentBlendStates(std::vector<vk::PipelineColorBlendAttachmentState> blendStates);
    PipelineCreator &setDefaultDynamicStates();
    PipelineCreator &setPipelineLayout(vk::PipelineLayout pipelineLayout);
    PipelineCreator &setRenderPass(vk::RenderPass renderPass);

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