#pragma once

#include "Shader.h"

#include <optional>
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

  PipelineBuilder &setVertexInput(
      const vk::VertexInputBindingDescription &bindingDescriptor,
      const std::vector<vk::VertexInputAttributeDescription>
          &attributeDescriptors,
      vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList);

  PipelineBuilder &setViewport(vk::Extent2D swapChainExtent);
  PipelineBuilder &setDefaultRasterizer();
  PipelineBuilder &setMultisampling(vk::SampleCountFlagBits samples);
  PipelineBuilder &setDefaultDepthStencil();
  PipelineBuilder &setAttachmentBlendStates(
      std::vector<vk::PipelineColorBlendAttachmentState> blendStates);
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
  std::vector<vk::VertexInputAttributeDescription>
      m_vertexAttributeDescriptions{};
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
  vk::AttachmentReference addColorAttachment(vk::Format format,
                                             vk::SampleCountFlagBits samples);
  vk::AttachmentReference
  addColorAttachment(vk::AttachmentDescription colorAttachment);

  vk::AttachmentReference addDepthAttachment(vk::Format format,
                                             vk::SampleCountFlagBits samples);

  vk::AttachmentReference addResolveAttachment(
      vk::Format format,
      vk::ImageLayout finalLayout = vk::ImageLayout::ePresentSrcKHR);

  /**
   * Add a default configured graphics subpass that has all color, depth and
   * resolve attachments that the builder knows about
   */
  uint32_t addDefaultSubpass();

  void addPreviousFrameSubpassDepencency()
  {
    //****************** Subpass dependencies ******************
    // this sets up the render pass to wait for the
    // STAGE_COLOR_ATTACHMENT_OUTPUT stage to ensure the images are available
    // and the swap chain is not still reading the image
    vk::SubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput |
        vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.srcAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead; // not sure here..
    dependency.dstStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput |
        vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                               vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    m_subpassDependencies.emplace_back(std::move(dependency));
  }

  void addSubpassDependency(vk::SubpassDependency dependency)
  {
    m_subpassDependencies.emplace_back(std::move(dependency));
  }

  // NOTE: the order of attachments directly corresponds to the
  // 'layout(location=0) out vec4 color' index in the fragment shader
  // pInputAttachments: attachments that are read from a shader
  // pResolveAttachments: attachments used for multisampling color attachments
  // pDepthStencilAttachment: attachment for depth and stencil data
  // pPreserveAttachments: attachments that are not currently used by the
  // subpass but for which the data needs to be preserved.
  uint32_t addSubpass(vk::SubpassDescription subpassDesc);

  vk::RenderPass create(GraphicsContext &ctx);

private:
  vk::AttachmentReference addAttachment(vk::AttachmentDescription desc,
                                        vk::ImageLayout layout);

private:
  std::vector<vk::AttachmentDescription> m_attachments;
  std::vector<vk::AttachmentReference> m_colorAttachmentRefs;
  std::vector<vk::AttachmentReference> m_resolveAttachmentRefs;
  std::optional<vk::AttachmentReference> m_depthStencilAttachmentRef;
  std::vector<vk::SubpassDependency> m_subpassDependencies;

  std::vector<vk::SubpassDescription> m_subpasses;
};

} // namespace Cory