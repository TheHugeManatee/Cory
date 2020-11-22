#pragma once

#include "Shader.h"

#include <vector>
#include <vulkan/vulkan.hpp>

namespace Cory {
class GraphicsContext;

class PipelineCreator {
  public:
    PipelineCreator() = default;

    PipelineCreator &setShaders(std::vector<Shader> shaders);

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